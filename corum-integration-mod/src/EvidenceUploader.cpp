#include "EvidenceUploader.hpp"

#include "ApiClient.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/Loader.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/utils/web.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using namespace geode::prelude;

namespace {

struct PendingEvidence {
    int levelID = 0;
    int canonicalLevelID = 0;
    int accountID = 0;
    int width = 0;
    int height = 0;
    double capturedAtMs = 0.0;
    std::string username;
    std::string platform;
    std::string modVersion;
    std::string gameVersion;
    std::string geodeVersion;
    std::vector<std::string> loadedMods;
    std::filesystem::path imagePath;
};

async::TaskHolder<web::WebResponse> s_evidenceRequest;
PendingEvidence s_pendingEvidence;
bool s_uploadingEvidence = false;
corum::EvidencePreparationCallback s_evidenceCallback;

std::string evidenceStorageKey(int levelID) {
    return fmt::format("clear-evidence.{}", levelID);
}

std::string evidenceScopePrefix(int accountID, int canonicalLevelID) {
    return fmt::format(
        "clear-evidence-v2.{}.{}",
        accountID,
        canonicalLevelID
    );
}

std::string scopedKey(
    int accountID,
    int canonicalLevelID,
    char const* field
) {
    return fmt::format(
        "{}.{}",
        evidenceScopePrefix(accountID, canonicalLevelID),
        field
    );
}

std::filesystem::path pendingDirectory() {
    return Mod::get()->getSaveDir() / "evidence-pending";
}

std::filesystem::path pendingImagePath(
    int accountID,
    int canonicalLevelID
) {
    auto const filename = Mod::get()->getSavedValue<std::string>(
        scopedKey(accountID, canonicalLevelID, "file"),
        ""
    );
    if (filename.empty()) return {};
    return pendingDirectory() / filename;
}

bool evidenceCompleteForScope(int accountID, int canonicalLevelID) {
    if (accountID <= 0 || canonicalLevelID <= 0) return false;
    auto const complete = Mod::get()->getSavedValue<bool>(
        scopedKey(accountID, canonicalLevelID, "complete"),
        false
    );
    if (!complete) return false;

    auto const currentGeneration = corum::ApiClient::evidenceGeneration();
    if (currentGeneration.empty()) return false;

    auto const savedGeneration = Mod::get()->getSavedValue<std::string>(
        scopedKey(accountID, canonicalLevelID, "complete-generation"),
        ""
    );
    return !savedGeneration.empty() && savedGeneration == currentGeneration;
}

bool flushEvidenceState() {
    auto result = Mod::get()->saveData();
    if (result.isErr()) {
        log::warn(
            "Could not persist Corum pending-evidence metadata: {}",
            result.unwrapErr()
        );
        return false;
    }
    return true;
}

std::string serializeLoadedMods(std::vector<std::string> const& loadedMods) {
    std::string result;
    for (auto const& loadedMod : loadedMods) {
        if (loadedMod.empty()) continue;
        if (!result.empty()) result.push_back('\n');
        result += loadedMod;
    }
    return result;
}

std::vector<std::string> deserializeLoadedMods(std::string const& value) {
    std::vector<std::string> result;
    std::size_t offset = 0;
    while (offset <= value.size()) {
        auto const end = value.find('\n', offset);
        auto item = value.substr(
            offset,
            end == std::string::npos ? std::string::npos : end - offset
        );
        if (!item.empty()) result.push_back(std::move(item));
        if (end == std::string::npos) break;
        offset = end + 1;
    }
    return result;
}

std::string base64Encode(std::vector<std::uint8_t> const& input) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    std::size_t index = 0;
    while (index + 3 <= input.size()) {
        auto const value =
            (static_cast<std::uint32_t>(input[index]) << 16) |
            (static_cast<std::uint32_t>(input[index + 1]) << 8) |
            static_cast<std::uint32_t>(input[index + 2]);
        output.push_back(alphabet[(value >> 18) & 0x3f]);
        output.push_back(alphabet[(value >> 12) & 0x3f]);
        output.push_back(alphabet[(value >> 6) & 0x3f]);
        output.push_back(alphabet[value & 0x3f]);
        index += 3;
    }

    auto const remaining = input.size() - index;
    if (remaining == 1) {
        auto const value = static_cast<std::uint32_t>(input[index]) << 16;
        output.push_back(alphabet[(value >> 18) & 0x3f]);
        output.push_back(alphabet[(value >> 12) & 0x3f]);
        output.push_back('=');
        output.push_back('=');
    } else if (remaining == 2) {
        auto const value =
            (static_cast<std::uint32_t>(input[index]) << 16) |
            (static_cast<std::uint32_t>(input[index + 1]) << 8);
        output.push_back(alphabet[(value >> 18) & 0x3f]);
        output.push_back(alphabet[(value >> 12) & 0x3f]);
        output.push_back(alphabet[(value >> 6) & 0x3f]);
        output.push_back('=');
    }

    return output;
}

bool isPNG(std::vector<std::uint8_t> const& bytes) {
    static constexpr std::uint8_t signature[] {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
    };
    if (bytes.size() < sizeof(signature)) return false;
    for (std::size_t index = 0; index < sizeof(signature); ++index) {
        if (bytes[index] != signature[index]) return false;
    }
    return true;
}

void finishEvidenceUpload(
    bool success,
    std::string evidenceID = "",
    std::string error = "Could not prepare this record for upload."
) {
    auto callback = std::move(s_evidenceCallback);
    s_evidenceCallback = {};
    s_pendingEvidence = {};
    s_uploadingEvidence = false;

    if (callback) {
        callback({
            .success = success,
            .evidenceID = std::move(evidenceID),
            .error = success ? "" : std::move(error),
        });
    }
}

void handleEvidenceResponse(web::WebResponse response);

void followEvidenceRedirect(web::WebResponse response) {
    auto location = response.header("location");
    if (!location) location = response.header("Location");
    if (!location || location->empty()) {
        log::error("Corum end-screen upload returned a redirect without Location");
        finishEvidenceUpload(false);
        return;
    }

    auto request = web::WebRequest();
    request.header("User-Agent", corum::ApiClient::userAgent());
    request.followRedirects(true);
    request.timeout(std::chrono::seconds(120));
    s_evidenceRequest.spawn(
        request.get(std::string(*location)),
        [](web::WebResponse redirectedResponse) {
            handleEvidenceResponse(std::move(redirectedResponse));
        }
    );
}

void handleEvidenceResponse(web::WebResponse response) {
    if (response.code() < 200 || response.code() >= 300) {
        log::error(
            "Corum end-screen upload failed with HTTP {}: {}",
            response.code(),
            response.errorMessage()
        );
        finishEvidenceUpload(false);
        return;
    }

    auto root = response.json().unwrapOr(matjson::Value());
    if (!root.isObject() || !root["ok"].asBool().unwrapOr(false)) {
        auto code = root["error"]["code"].asString().unwrapOr("UNKNOWN");
        log::error("Corum end-screen upload API error: {}", code);
        finishEvidenceUpload(false);
        return;
    }

    auto evidenceID = root["evidence"]["id"].asString().unwrapOr("");
    if (evidenceID.empty()) {
        log::error("Corum end-screen upload response did not contain an evidence ID");
        finishEvidenceUpload(false);
        return;
    }

    Mod::get()->setSavedValue(
        scopedKey(
            s_pendingEvidence.accountID,
            s_pendingEvidence.canonicalLevelID,
            "uploaded"
        ),
        evidenceID
    );
    flushEvidenceState();
    finishEvidenceUpload(true, evidenceID);
}

void uploadPNG() {
    std::ifstream stream(s_pendingEvidence.imagePath, std::ios::binary);
    if (!stream) {
        log::error(
            "Could not read pending Corum end-screen PNG: {}",
            s_pendingEvidence.imagePath.string()
        );
        finishEvidenceUpload(false);
        return;
    }

    std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>()
    );
    if (!isPNG(bytes)) {
        log::error("Corum end-screen capture was not a PNG");
        finishEvidenceUpload(false);
        return;
    }

    matjson::Value body;
    body["action"] = "evidence";
    body["levelId"] = s_pendingEvidence.levelID;
    body["gdAccountId"] = s_pendingEvidence.accountID;
    body["gdUsername"] = s_pendingEvidence.username;
    body["mimeType"] = "image/png";
    body["width"] = s_pendingEvidence.width;
    body["height"] = s_pendingEvidence.height;
    body["imageBase64"] = base64Encode(bytes);
    body["platform"] = s_pendingEvidence.platform;
    body["modVersion"] = s_pendingEvidence.modVersion;
    body["gameVersion"] = s_pendingEvidence.gameVersion;
    body["geodeVersion"] = s_pendingEvidence.geodeVersion;
    body["loadedMods"] = matjson::Value::array();
    for (auto const& loadedMod : s_pendingEvidence.loadedMods) {
        body["loadedMods"].push(loadedMod);
    }
    body["clientTimestamp"] = s_pendingEvidence.capturedAtMs;

    auto request = web::WebRequest();
    request.header("User-Agent", corum::ApiClient::userAgent());
    request.bodyJSON(body);
    request.followRedirects(false);
    request.timeout(std::chrono::seconds(120));
    s_evidenceRequest.spawn(
        request.post(corum::ApiClient::baseURL()),
        [](web::WebResponse response) {
            if (response.redirected()) {
                followEvidenceRedirect(std::move(response));
                return;
            }
            handleEvidenceResponse(std::move(response));
        }
    );
}

void storePendingCapture(PendingEvidence const& pending) {
    auto const accountID = pending.accountID;
    auto const canonicalLevelID = pending.canonicalLevelID;
    if (
        accountID <= 0 ||
        canonicalLevelID <= 0 ||
        pending.imagePath.empty()
    ) return;

    auto const oldPath = pendingImagePath(accountID, canonicalLevelID);
    auto const prefix = evidenceScopePrefix(accountID, canonicalLevelID);

    Mod::get()->setSavedValue(
        fmt::format("{}.file", prefix),
        pending.imagePath.filename().string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.source", prefix),
        pending.levelID
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.width", prefix),
        pending.width
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.height", prefix),
        pending.height
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.username", prefix),
        pending.username
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.capture-metadata-version", prefix),
        1
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.captured-at", prefix),
        pending.capturedAtMs
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.platform", prefix),
        pending.platform
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.mod-version", prefix),
        pending.modVersion
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.game-version", prefix),
        pending.gameVersion
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.geode-version", prefix),
        pending.geodeVersion
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.loaded-mods", prefix),
        serializeLoadedMods(pending.loadedMods)
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.uploaded", prefix),
        std::string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.complete", prefix),
        false
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.complete-generation", prefix),
        std::string()
    );

    auto const persisted = flushEvidenceState();
    if (persisted && !oldPath.empty() && oldPath != pending.imagePath) {
        std::error_code error;
        std::filesystem::remove(oldPath, error);
        if (error) {
            log::warn(
                "Could not remove superseded Corum pending PNG {}: {}",
                oldPath.string(),
                error.message()
            );
        }
    }
}

} // namespace

namespace corum {

std::string latestEvidenceID(int levelID) {
    if (levelID <= 0) return "";
    return Mod::get()->getSavedValue<std::string>(
        evidenceStorageKey(levelID),
        ""
    );
}

bool hasPendingEvidenceForSubmission(
    int canonicalLevelID,
    int accountID
) {
    if (canonicalLevelID <= 0 || accountID <= 0) return false;
    if (evidenceCompleteForScope(accountID, canonicalLevelID)) return false;
    return !pendingImagePath(accountID, canonicalLevelID).empty();
}

void prepareEvidenceForSubmission(
    int canonicalLevelID,
    int sourceLevelID,
    int accountID,
    EvidencePreparationCallback callback
) {
    if (!callback) return;
    if (canonicalLevelID <= 0 || sourceLevelID <= 0 || accountID <= 0) {
        callback({
            .success = false,
            .error = "Could not prepare this record for upload.",
        });
        return;
    }

    if (s_uploadingEvidence) {
        callback({
            .success = false,
            .error = "Another record is still being prepared. Please try again.",
        });
        return;
    }

    if (evidenceCompleteForScope(accountID, canonicalLevelID)) {
        callback({.success = true});
        return;
    }

    auto const imagePath = pendingImagePath(accountID, canonicalLevelID);
    if (imagePath.empty()) {
        // v0.2.31 compatibility: reuse an already uploaded evidence ID when one
        // exists. Older trusted records with no evidence remain submittable.
        auto evidenceID = latestEvidenceID(sourceLevelID);
        if (evidenceID.empty() && sourceLevelID != canonicalLevelID) {
            evidenceID = latestEvidenceID(canonicalLevelID);
        }
        callback({
            .success = true,
            .evidenceID = std::move(evidenceID),
        });
        return;
    }

    std::error_code existsError;
    if (!std::filesystem::exists(imagePath, existsError) || existsError) {
        log::error(
            "Pending Corum evidence metadata points to a missing file: {}",
            imagePath.string()
        );
        callback({
            .success = false,
            .error = "Could not prepare this record for upload. Please clear the level again.",
        });
        return;
    }

    auto const uploadedID = Mod::get()->getSavedValue<std::string>(
        scopedKey(accountID, canonicalLevelID, "uploaded"),
        ""
    );
    if (!uploadedID.empty()) {
        callback({
            .success = true,
            .evidenceID = uploadedID,
        });
        return;
    }

    auto account = GJAccountManager::get();
    auto username = Mod::get()->getSavedValue<std::string>(
        scopedKey(accountID, canonicalLevelID, "username"),
        ""
    );
    if (username.empty() && account && account->m_accountID == accountID) {
        username = std::string(account->m_username);
    }

    auto storedSource = Mod::get()->getSavedValue<int>(
        scopedKey(accountID, canonicalLevelID, "source"),
        sourceLevelID
    );
    if (storedSource <= 0) storedSource = sourceLevelID;

    auto const metadataVersion = Mod::get()->getSavedValue<int>(
        scopedKey(accountID, canonicalLevelID, "capture-metadata-version"),
        0
    );
    auto capturedAtMs = Mod::get()->getSavedValue<double>(
        scopedKey(accountID, canonicalLevelID, "captured-at"),
        0.0
    );
    auto platform = Mod::get()->getSavedValue<std::string>(
        scopedKey(accountID, canonicalLevelID, "platform"),
        ""
    );
    auto modVersion = Mod::get()->getSavedValue<std::string>(
        scopedKey(accountID, canonicalLevelID, "mod-version"),
        ""
    );
    auto gameVersion = Mod::get()->getSavedValue<std::string>(
        scopedKey(accountID, canonicalLevelID, "game-version"),
        ""
    );
    auto geodeVersion = Mod::get()->getSavedValue<std::string>(
        scopedKey(accountID, canonicalLevelID, "geode-version"),
        ""
    );
    auto loadedMods = deserializeLoadedMods(
        Mod::get()->getSavedValue<std::string>(
            scopedKey(accountID, canonicalLevelID, "loaded-mods"),
            ""
        )
    );

    // v0.2.32 pending captures did not snapshot their environment. Preserve
    // compatibility by filling only those legacy captures from this session.
    if (metadataVersion < 1) {
        auto const now = std::chrono::system_clock::now().time_since_epoch();
        capturedAtMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count()
        );
        platform = GEODE_PLATFORM_NAME;
        modVersion = Mod::get()->getVersion().toVString();
        gameVersion = GEODE_GD_VERSION_STRING;
        geodeVersion = Loader::get()->getVersion().toVString();
        loadedMods = corum::ApiClient::loadedMods();
    }

    s_pendingEvidence = {
        .levelID = storedSource,
        .canonicalLevelID = canonicalLevelID,
        .accountID = accountID,
        .width = Mod::get()->getSavedValue<int>(
            scopedKey(accountID, canonicalLevelID, "width"),
            0
        ),
        .height = Mod::get()->getSavedValue<int>(
            scopedKey(accountID, canonicalLevelID, "height"),
            0
        ),
        .capturedAtMs = capturedAtMs,
        .username = std::move(username),
        .platform = std::move(platform),
        .modVersion = std::move(modVersion),
        .gameVersion = std::move(gameVersion),
        .geodeVersion = std::move(geodeVersion),
        .loadedMods = std::move(loadedMods),
        .imagePath = imagePath,
    };
    s_evidenceCallback = std::move(callback);
    s_uploadingEvidence = true;
    uploadPNG();
}

void markEvidenceSubmissionComplete(
    int canonicalLevelID,
    int accountID
) {
    if (canonicalLevelID <= 0 || accountID <= 0) return;

    auto const imagePath = pendingImagePath(accountID, canonicalLevelID);
    auto const prefix = evidenceScopePrefix(accountID, canonicalLevelID);
    auto const previousSource = Mod::get()->getSavedValue<int>(
        fmt::format("{}.source", prefix), 0
    );
    auto const previousWidth = Mod::get()->getSavedValue<int>(
        fmt::format("{}.width", prefix), 0
    );
    auto const previousHeight = Mod::get()->getSavedValue<int>(
        fmt::format("{}.height", prefix), 0
    );
    auto const previousUsername = Mod::get()->getSavedValue<std::string>(
        fmt::format("{}.username", prefix), ""
    );
    auto const previousMetadataVersion = Mod::get()->getSavedValue<int>(
        fmt::format("{}.capture-metadata-version", prefix), 0
    );
    auto const previousCapturedAt = Mod::get()->getSavedValue<double>(
        fmt::format("{}.captured-at", prefix), 0.0
    );
    auto const previousPlatform = Mod::get()->getSavedValue<std::string>(
        fmt::format("{}.platform", prefix), ""
    );
    auto const previousModVersion = Mod::get()->getSavedValue<std::string>(
        fmt::format("{}.mod-version", prefix), ""
    );
    auto const previousGameVersion = Mod::get()->getSavedValue<std::string>(
        fmt::format("{}.game-version", prefix), ""
    );
    auto const previousGeodeVersion = Mod::get()->getSavedValue<std::string>(
        fmt::format("{}.geode-version", prefix), ""
    );
    auto const previousLoadedMods = Mod::get()->getSavedValue<std::string>(
        fmt::format("{}.loaded-mods", prefix), ""
    );
    auto const previousUploaded = Mod::get()->getSavedValue<std::string>(
        fmt::format("{}.uploaded", prefix), ""
    );

    Mod::get()->setSavedValue(
        fmt::format("{}.file", prefix),
        std::string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.source", prefix),
        0
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.width", prefix),
        0
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.height", prefix),
        0
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.username", prefix),
        std::string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.capture-metadata-version", prefix),
        0
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.captured-at", prefix),
        0.0
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.platform", prefix),
        std::string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.mod-version", prefix),
        std::string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.game-version", prefix),
        std::string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.geode-version", prefix),
        std::string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.loaded-mods", prefix),
        std::string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.uploaded", prefix),
        std::string()
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.complete", prefix),
        true
    );
    Mod::get()->setSavedValue(
        fmt::format("{}.complete-generation", prefix),
        corum::ApiClient::evidenceGeneration()
    );

    if (!flushEvidenceState()) {
        Mod::get()->setSavedValue(
            fmt::format("{}.file", prefix),
            imagePath.empty() ? std::string() : imagePath.filename().string()
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.source", prefix), previousSource
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.width", prefix), previousWidth
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.height", prefix), previousHeight
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.username", prefix), previousUsername
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.capture-metadata-version", prefix),
            previousMetadataVersion
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.captured-at", prefix), previousCapturedAt
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.platform", prefix), previousPlatform
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.mod-version", prefix), previousModVersion
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.game-version", prefix), previousGameVersion
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.geode-version", prefix), previousGeodeVersion
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.loaded-mods", prefix), previousLoadedMods
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.uploaded", prefix), previousUploaded
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.complete", prefix), false
        );
        Mod::get()->setSavedValue(
            fmt::format("{}.complete-generation", prefix), std::string()
        );
        return;
    }

    if (!imagePath.empty()) {
        std::error_code error;
        std::filesystem::remove(imagePath, error);
        if (error) {
            log::warn(
                "Could not remove submitted Corum pending PNG {}: {}",
                imagePath.string(),
                error.message()
            );
        }
    }
}

} // namespace corum

class $modify(CorumEndLevelEvidenceLayer, EndLevelLayer) {
    struct Fields {
        bool captureScheduled = false;
    };

    void showLayer(bool instant) {
        EndLevelLayer::showLayer(instant);

        if (m_fields->captureScheduled || s_uploadingEvidence) return;
        if (!Mod::get()->getSettingValue<bool>("enable-record-submit")) return;
        if (!m_playLayer || !m_playLayer->m_level) return;
        if (m_playLayer->m_isPracticeMode || m_playLayer->m_isTestMode) return;
        if (!m_playLayer->m_hasCompletedLevel) return;

        auto const levelID = static_cast<int>(m_playLayer->m_level->m_levelID);
        auto const map = corum::ApiClient::startupMap(levelID);
        if (levelID <= 0 || !map) return;

        auto account = GJAccountManager::get();
        auto const username = account ? std::string(account->m_username) : "";
        if (!account || account->m_accountID <= 0 || username.empty()) return;
        if (evidenceCompleteForScope(account->m_accountID, map->levelID)) return;

        m_fields->captureScheduled = true;
        scheduleOnce(
            schedule_selector(CorumEndLevelEvidenceLayer::captureEndScreen),
            0.20f
        );
    }

    void captureEndScreen(float) {
        if (!m_playLayer || !m_playLayer->m_level || s_uploadingEvidence) return;

        auto const levelID = static_cast<int>(m_playLayer->m_level->m_levelID);
        auto const map = corum::ApiClient::startupMap(levelID);
        auto account = GJAccountManager::get();
        auto const username = account ? std::string(account->m_username) : "";
        if (
            levelID <= 0 ||
            !map ||
            !account ||
            account->m_accountID <= 0 ||
            username.empty()
        ) return;
        if (evidenceCompleteForScope(account->m_accountID, map->levelID)) return;

        auto director = CCDirector::sharedDirector();
        auto scene = director ? director->getRunningScene() : nullptr;
        if (!director || !scene) return;

        auto const pixelSize = director->getWinSizeInPixels();
        auto const width = static_cast<int>(pixelSize.width);
        auto const height = static_cast<int>(pixelSize.height);
        if (width <= 0 || height <= 0) return;

        auto renderTexture = CCRenderTexture::create(width, height);
        if (!renderTexture) {
            log::error("Could not create Corum end-screen render texture");
            return;
        }

        auto const windowSize = director->getWinSize();
        auto captureLabel = CCLabelBMFont::create(
            fmt::format("{} / {}", username, map->title).c_str(),
            "bigFont.fnt"
        );
        captureLabel->setAnchorPoint({0.0f, 1.0f});
        captureLabel->setColor(ccc3(255, 255, 255));
        captureLabel->setOpacity(235);
        captureLabel->setPosition({8.0f, windowSize.height - 8.0f});
        captureLabel->limitLabelWidth(
            windowSize.width * 0.48f,
            0.30f,
            0.16f
        );
        scene->addChild(captureLabel, 100000);

        renderTexture->beginWithClear(0.0f, 0.0f, 0.0f, 1.0f);
        scene->visit();
        renderTexture->end();
        captureLabel->removeFromParentAndCleanup(true);

        auto image = renderTexture->newCCImage(true);
        if (!image) {
            log::error("Could not create Corum end-screen image from render texture");
            return;
        }

        auto directory = pendingDirectory();
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        auto const capturedAtMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
        auto imagePath = directory / fmt::format(
            "clear-{}-{}-{}.png",
            account->m_accountID,
            map->levelID,
            static_cast<std::int64_t>(capturedAtMs)
        );
        auto const saved = !error && image->saveToFile(imagePath.string().c_str(), false);
        delete image;

        if (!saved) {
            log::error("Could not write Corum end-screen PNG: {}", imagePath.string());
            return;
        }

        storePendingCapture({
            .levelID = levelID,
            .canonicalLevelID = map->levelID,
            .accountID = account->m_accountID,
            .width = width,
            .height = height,
            .capturedAtMs = capturedAtMs,
            .username = username,
            .platform = GEODE_PLATFORM_NAME,
            .modVersion = Mod::get()->getVersion().toVString(),
            .gameVersion = GEODE_GD_VERSION_STRING,
            .geodeVersion = Loader::get()->getVersion().toVString(),
            .loadedMods = corum::ApiClient::loadedMods(),
            .imagePath = std::move(imagePath),
        });
        log::info(
            "Stored Corum end-screen capture locally for account {} / map {}",
            account->m_accountID,
            map->levelID
        );
    }
};
