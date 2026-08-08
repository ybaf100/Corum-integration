# v0.2.35 release build

The v0.2.33 binary files that came with the input bundle are intentionally not
part of the v0.2.35 source deliverable because they contain only the old Win64
binary and report version v0.2.33.

Build `.github/workflows/build-mod.yml` from the repository root to produce a
combined Win64 + Android32 + Android64 `.geode` artifact, or use the local Geode
CLI commands documented in `ANDROID-BUILD.md`.
