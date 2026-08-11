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

The one-screen UI includes mutually exclusive agreement choices and opens its built-in terms and record-verification notice in a separate read-only window. It does not depend on external Markdown files at build time or runtime.
