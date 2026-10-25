# Building and releasing SLANG

Electron packages a desktop runtime together with our bundled editor and native
interpreter. Users do not install Node.js. A Windows executable still needs its
adjacent runtime files; a macOS `.app` is a directory bundle; Linux ships an
executable and supporting files in a tar archive. Extract the complete package.

## Platform matrix

The [GitHub Actions workflow](../.github/workflows/build-and-release.yml) builds
each interpreter on its target OS and architecture, then packages it with the
matching Electron runtime:

| Target | GitHub runner | IDE download | Launch |
| --- | --- | --- | --- |
| Windows x64 | `windows-2025` | `SLANG-IDE-VERSION-win32-x64.zip` | `SLANG.exe` |
| Linux x64 | `ubuntu-22.04` | `SLANG-IDE-VERSION-linux-x64.tar.gz` | `./SLANG` |
| macOS Intel | `macos-15-intel` | `SLANG-IDE-VERSION-darwin-x64.zip` | `SLANG.app` |
| macOS Apple Silicon | `macos-15` | `SLANG-IDE-VERSION-darwin-arm64.zip` | `SLANG.app` |

Every target also produces a smaller native-only `slang-VERSION-OS-CPU` archive.
Windows uses ZIP; Unix CLI packages use tar.gz. The two macOS packages are
separate architectures, not a universal binary. Linux ARM64 and Windows ARM64
are not part of the initial CI matrix.

Windows has been tested locally. macOS and Linux execution must be confirmed by
their first successful workflow runs. The current Linux build baseline is Ubuntu
22.04; the macOS build baseline is macOS 15. These are not claims of compatibility
with every older or newer OS. Standard desktop libraries, kernel facilities,
graphics drivers, and Electron's platform requirements still apply. The Linux
Chromium sandbox needs a supported user-namespace configuration; the workflow
does not disable it.

## Build on your machine

Use Node.js **22.12+** (CI uses Node 24) and a C++20 toolchain. Windows uses the
Visual Studio Desktop development with C++ workload, Windows SDK, CMake and Ninja.
macOS needs Xcode command-line tools and CMake. Linux needs CMake and development
packages for the SDL backends; the exact apt commands are in the workflow.

From the repository root:

```sh
npm ci
node node_modules/electron/install.js
```

On Windows:

```powershell
.\scripts\build-native.ps1 -Package -VerifyParity
```

On macOS or Linux:

```sh
cmake -S . -B build/native-ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native-ninja --parallel 3
ctest --test-dir build/native-ninja --output-on-failure
cpack --config build/native-ninja/CPackConfig.cmake -C Release -B dist
```

Then, on any target platform:

```sh
npm run ide:build
npm run ide:package
node scripts/test-release-archive.mjs
```

The packager uses the current host OS and architecture. It validates that the
native interpreter's version matches the IDE. It does not cross-compile C++ or
replace it with a binary downloaded from an unrelated build. `SLANG_BUILD_DIR`
selects a different CMake build directory; it must include SDL's `_deps` source
tree for its license notices. `SLANG_DIST_DIR` selects an alternative output
directory. Existing output folders and archives are never overwritten.

`build/ide-package.json` records the packaged executable and archive. Archive
validation extracts it into a path containing spaces and Scandinavian letters,
checks licenses, runs the shipped CLI with a minimal PATH, and redirects the
subsequent UI tests to that extracted copy.

```powershell
$env:SLANG_TEST_PACKAGED = '1'
$env:SLANG_IDE_MINIMAL_PATH = '1'
npm run ide:smoke
npm run ide:themes
```

On macOS use shell environment assignments; on headless Linux also wrap each
command with `xvfb-run -a`, as in the workflow. These tests exercise the bundled
interpreter, not an override pointing back at the development executable.

## GitHub setup

1. Create a GitHub repository and push this source tree, including `.github/`,
   `package-lock.json`, and both license texts. Generated `build/`, `dist/`, and
   `node_modules/` stay ignored.
2. Allow GitHub Actions for the repository. No personal access token is needed:
   the workflow uses its built-in `GITHUB_TOKEN`. Repository/organization policy
   must permit the release job's `contents: write` permission.
3. Run **Build, test and package** from the Actions tab, or push a branch/PR.
   Each matrix job uploads its tested packages and screenshots. Branch and PR
   runs do not create releases.
4. Review the first matrix run before treating macOS/Linux packages as supported.

Adding the workflow locally does not run it or publish anything. Configure your
own GitHub repository remote and push the source before expecting Actions runs.

## Make a release

Versions are synchronized by a helper; CMake reads the root package version:

```sh
node scripts/set-version.mjs 0.6.3
npm run release:check
git add package.json ide/package.json package-lock.json
git commit -m "Release 0.6.3"
git tag v0.6.3
git push origin HEAD
git push origin v0.6.3
```

These commands assume the rest of the source/workflow is already committed and
the `origin` remote is configured. For the current version use `v0.6.2` after
committing all intended source changes. Tags must match the checked-out package
version exactly. Prerelease versions such as `0.6.3-rc.1` are supported.

On a `v*` tag, all four jobs build and test both products. Only after every job
passes, the release job downloads eight archives, generates `SHA256SUMS.txt`,
and creates a **draft GitHub Release** with generated notes. A rerun may update
that draft's assets; it refuses to overwrite a published release. Review the
draft and click **Publish release** when ready. Download checksums can be checked
with `sha256sum -c SHA256SUMS.txt` or PowerShell `Get-FileHash`.

The workflow pins third-party actions to commit hashes, uses read-only permissions
for builds, and grants write access only to the tag-gated release job. Pull
requests never receive publishing credentials. Build logs and failed UI captures
remain available under Actions. Release artifacts are retained for 14 days in
Actions; release attachments persist with the release.

## Signing and installers

The current output is portable archives. Windows publisher signing and Apple
Developer ID signing/notarization are not configured. macOS receives an ad-hoc
signature preserving Electron's entitlements, which supports local execution but
does not satisfy Gatekeeper's publisher trust requirements. Clean-machine testing
of downloads is still necessary; a local CI launch does not test quarantine.

For school-wide distribution, add a protected signing/release environment with
your Windows signing identity and Apple Developer ID/notarization credentials.
Sign native binaries and the complete app before creating archives and checksums;
notarize and staple macOS builds before distribution. Do not put signing secrets
in source or expose them to pull-request jobs. Installers such as Windows setup,
macOS DMG, or Linux DEB/AppImage can be added later with Electron Forge makers.

## Licensing

The language, IDE and original tooling use **MIT**. Original graphics example
programs/assets and the built-in drawing sample use **CC0 1.0 Universal**. The
archive includes both legal texts. SDL, Electron, Chromium, Monaco and other
dependencies retain their own licenses and notices; CC0 does not apply to them.

References: [Electron packaging](https://www.electronjs.org/docs/latest/tutorial/application-distribution),
[Electron Packager](https://github.com/electron/packager),
[GitHub runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners),
[Electron signing](https://www.electronjs.org/docs/latest/tutorial/code-signing),
[MIT](https://opensource.org/license/mit), and
[CC0](https://creativecommons.org/publicdomain/zero/1.0/).
