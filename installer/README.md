# Corum Integration Windows Installer

The installer is a separate Windows client for installing, updating and launching Corum Integration. It does not replace the manually downloadable `.geode` package.

## Projects

- `CorumIntegrationInstaller.Core`: Steam/Geometry Dash discovery, GitHub Release lookup, `.geode` inspection and transactional replacement logic.
- `CorumIntegrationInstaller`: one-screen .NET 8 WinForms UI.
- `CorumIntegrationInstaller.Tests`: dependency-free executable test harness for package detection and safe update behavior.

## Local build on Windows

```powershell
dotnet run --project installer/CorumIntegrationInstaller.Tests/CorumIntegrationInstaller.Tests.csproj -c Release
dotnet publish installer/CorumIntegrationInstaller/CorumIntegrationInstaller.csproj -c Release -r win-x64 --self-contained true
```

GitHub Actions reads the release version from `corum-integration-mod/mod.json`, passes it to the installer assembly and renames the single executable to `Corum-Integration-Installer-v{VERSION}.exe`.

The installer embeds `DISCORD-DOWNLOAD-NOTICE.md` and `corum-integration-mod/PRIVACY.md` so the same repository-controlled disclosure is available before installation.
