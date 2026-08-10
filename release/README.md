# Release builds

An older compiled binary is intentionally not included because it does not
contain the current evidence, client-version, and submission-gating fixes.

Run `.github/workflows/build-mod.yml` from the repository root. It publishes
two GitHub Actions artifacts:

- `Corum-Integration-Windows-v1.0.0`
- `Corum-Integration-Android-v0.2.40`

The Android artifact combines Android32 and Android64. Windows is kept separate
because its release version is `v1.0.0` while Android remains `v0.2.40`.
