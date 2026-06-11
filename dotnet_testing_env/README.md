# dotnetenv

Pinned, isolated, side-by-side .NET environments for CI.

One config file pins each environment (SDK + workloads + MAUI workload-set +
optional downlevel runtime + Xcode). Each environment lives in its own
directory with its own `DOTNET_ROOT` and global-tool home, so installs never
clobber each other.

## Install

```sh
pipx install dotnetenv          # from a feed
# or, from source:
pip install -e .
```

## Use

```sh
dotnetenv install 10.0          # provision (idempotent: skips if already done)
dotnetenv install 10.0 --force  # reinstall from scratch
dotnetenv exec 10.0 -- dotnet --info
eval "$(dotnetenv activate 10.0)"   # venv-style; deactivate-dotnet to undo
dotnetenv list
```

## Test devices

Bring a test simulator/emulator up or down. Pins live in the top-level
`devices` block of the config; CLI flags override per-invocation.

```sh
# iOS: <name> supplies the Xcode pin (via that env's developerDir).
# Device/runtime default to the newest installed unless pinned/overridden.
dotnetenv device up   10.0 --platform ios [-d "iPhone 15 Pro Max"] [-r 17.2]
dotnetenv device down 10.0 --platform ios

# Android: API level from config (default 24) or -l. JAVA_HOME auto-resolved on macOS.
dotnetenv device up   --platform android [-l 24]
dotnetenv device down --platform android [-l 24]
```

Both manage a sim/AVD named `dotnet_cbl_testing` (Android: `_<api>` suffix).
Idempotent: reuses an existing booted device, creates one only if absent.

`device id` prints JUST the running device's id to stdout (logs go to stderr),
for piping into another script. iOS -> simulator UDID; Android -> adb serial
(e.g. `emulator-5554`). Exits non-zero if no matching device is running.

```sh
SERIAL=$(dotnetenv device id --platform android -l 24)   # emulator-5554
UDID=$(dotnetenv device id 10.0 --platform ios)          # sim UDID
```

## Config

The canonical config ships inside the package
(`src/dotnetenv/default_config.json`) and works out-of-box: `developerDir`
follows the [Xcodes.app](https://github.com/XcodesOrg/Xcodes) convention
(`/Applications/Xcode-<ver>.app`), so it is portable across machines that use
it. Edit that file to change the pins.

Override only if you deviate from the convention. Resolution — first that
exists wins:

1. `--config <path>`
2. `DOTNETENV_CONFIG`
3. `./dotnetenv.json` (project-local)
4. `~/.config/dotnetenv/dotnetenv.json` (user-global)
5. bundled canonical config
