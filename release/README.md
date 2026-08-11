# Release builds

An older compiled binary is intentionally not included because it does not
contain the current evidence, client-version, and submission-gating fixes.

Run `.github/workflows/build-mod.yml` from the repository root. It publishes
one GitHub Actions artifact:

- `Corum-Integration-v1.0.0`

The artifact combines Win64, Android32, and Android64 binaries in one `.geode`
package with shared `v1.0.0` metadata.
