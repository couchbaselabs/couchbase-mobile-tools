#!/usr/bin/env python3
"""dotnetenv - pinned, isolated, side-by-side .NET environments for CI.

One config file pins each environment (SDK + workloads + MAUI workload-set +
Xcode). Each environment lives in its own directory with its own DOTNET_ROOT
and its own global-tool home, so installs never clobber each other.

Verbs:
    install <name>             provision the environment (idempotent)
    exec <name> -- <cmd...>    run a command inside it (sets env, execs)
    activate <name>            print shell code to enter it (venv-style)
    list                       list configured environments
    device up|down --platform  bring a test simulator/emulator up or down

No shell sourcing. Nothing leaks into the parent shell. CI calls `exec`.
"""

import argparse
import importlib.resources as ir
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

IS_WINDOWS = platform.system() == "Windows"
IS_MAC = platform.system() == "Darwin"

# Config search order when --config / DOTNETENV_CONFIG are not given:
#   1. ./dotnetenv.json                    (project-local; the usual case)
#   2. ~/.config/dotnetenv/dotnetenv.json  (user-global)
#   3. bundled default_config.json         (read-only template inside the wheel)
USER_CONFIG = Path.home() / ".config" / "dotnetenv" / "dotnetenv.json"


def discover_config():
    """Highest-priority on-disk config path, or None to use the bundled default."""
    for p in (Path.cwd() / "dotnetenv.json", USER_CONFIG):
        if p.exists():
            return p
    return None

INSTALL_SCRIPT_SH = "https://dot.net/v1/dotnet-install.sh"
INSTALL_SCRIPT_PS1 = "https://dot.net/v1/dotnet-install.ps1"

# XHarness is the same everywhere: the 10.0 prerelease is .NET 8 compatible too.
XHARNESS_NAME = "Microsoft.DotNet.XHarness.CLI"
XHARNESS_VERSION = "10.0.0-prerelease*"
XHARNESS_SOURCE = "https://pkgs.dev.azure.com/dnceng/public/_packaging/dotnet-eng/nuget/v3/index.json"


def log(msg):
    # Progress goes to stderr so stdout stays clean for machine-readable output
    # (e.g. `device id`, `activate`). Still visible in terminals and CI logs.
    print(f"\033[32m===== {msg} =====\033[0m" if sys.stderr.isatty() else f"===== {msg} =====",
          file=sys.stderr, flush=True)


def die(msg):
    print(f"dotnetenv: {msg}", file=sys.stderr)
    sys.exit(1)


def load_config(path):
    """path=None -> use the bundled default template shipped in the package."""
    if path is None:
        src = "bundled default_config.json"
        text = ir.files("dotnetenv").joinpath("default_config.json").read_text()
        cfg = json.loads(text)
    else:
        p = Path(path)
        if not p.exists():
            die(f"config not found: {p}")
        src = str(p)
        with open(p) as f:
            cfg = json.load(f)
    if not cfg.get("environments"):
        die(f"no 'environments' in {src}")
    return cfg


def env_config(cfg, name):
    envs = cfg["environments"]
    if name not in envs:
        die(f"unknown environment '{name}'. known: {', '.join(sorted(envs))}")
    return envs[name]


def device_config(cfg, platform_key):
    """Top-level 'devices' block for a platform ('ios' | 'android'), or {}."""
    return (cfg.get("devices") or {}).get(platform_key, {}) or {}


def env_path(name, ec):
    """Install dir for an environment: $HOME/.dotnet<major> (e.g. .dotnet10).

    Overridable per-env with 'installDir', or globally via DOTNETENV_HOME (base).
    """
    override = ec.get("installDir")
    if override:
        return Path(os.path.expanduser(override))
    major = name.split(".")[0]
    base = Path(os.environ.get("DOTNETENV_HOME", Path.home()))
    return base / f".dotnet{major}"


# ---- cross-process lock (portable, no platform branches) -------------------

class DirLock:
    """mkdir-based lock. Serializes installs across parallel CI jobs."""

    def __init__(self, path, timeout=600):
        self.path = Path(path)
        self.timeout = timeout

    def __enter__(self):
        deadline = time.time() + self.timeout
        while True:
            try:
                self.path.mkdir(parents=False, exist_ok=False)
                return self
            except FileExistsError:
                if time.time() > deadline:
                    die(f"timed out waiting for lock {self.path}")
                time.sleep(1)

    def __exit__(self, *exc):
        try:
            self.path.rmdir()
        except OSError:
            pass


# ---- environment variable construction -------------------------------------

def managed_vars(env_dir, ec):
    """The env vars dotnetenv controls (everything except PATH). Single source of
    truth shared by `exec` and `activate` so they can never diverge."""
    v = {
        "DOTNET_ROOT": str(env_dir),
        # DOTNET_CLI_HOME isolates global tools / NuGet fallback / telemetry per env.
        "DOTNET_CLI_HOME": str(env_dir),
        "DOTNET_MULTILEVEL_LOOKUP": "0",
        "DOTNET_CLI_TELEMETRY_OPTOUT": "1",
        "DOTNET_NOLOGO": "1",
        "DOTNET_SKIP_FIRST_TIME_EXPERIENCE": "1",
    }
    dev_dir = ec.get("developerDir")
    if dev_dir and IS_MAC:
        v["DEVELOPER_DIR"] = dev_dir
    for k, val in (ec.get("env") or {}).items():
        v[k] = str(val)
    return v


def build_env(name, ec):
    env_dir = env_path(name, ec)
    if not (env_dir / dotnet_exe_name()).exists():
        die(f"environment '{name}' not installed. run: dotnetenv install {name}")
    tools_dir = env_dir / ".dotnet" / "tools"
    env = dict(os.environ)
    env.update(managed_vars(env_dir, ec))
    env["PATH"] = os.pathsep.join([str(env_dir), str(tools_dir), env.get("PATH", "")])
    return env_dir, env


def dotnet_exe_name():
    return "dotnet.exe" if IS_WINDOWS else "dotnet"


# ---- install ----------------------------------------------------------------

def download(url, dest):
    with urllib.request.urlopen(url) as r, open(dest, "wb") as f:
        shutil.copyfileobj(r, f)


def run_install_script(env_dir, ec, work):
    channel = ec.get("sdk") or ec.get("channel")
    version = ec.get("sdkVersion") or ec.get("version")
    if not channel and not version:
        die("environment config needs 'sdk' (channel) or 'sdkVersion' (exact)")

    if IS_WINDOWS:
        script = work / "dotnet-install.ps1"
        download(INSTALL_SCRIPT_PS1, script)
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
               "-File", str(script), "-InstallDir", str(env_dir)]
        if version:
            cmd += ["-Version", version]
        elif channel:
            cmd += ["-Channel", channel]
    else:
        script = work / "dotnet-install.sh"
        download(INSTALL_SCRIPT_SH, script)
        os.chmod(script, 0o755)
        cmd = ["bash", str(script), "--install-dir", str(env_dir),
               "--skip-non-versioned-files"]
        if version:
            cmd += ["--version", version]
        elif channel:
            cmd += ["--channel", channel]
    subprocess.run(cmd, check=True)


def run_install_runtime(env_dir, channel, work):
    """Install a shared .NET runtime (not the SDK) into env_dir, side-by-side
    with the SDK. e.g. an 8.0 runtime under a 10.0 SDK for downlevel testing."""
    if IS_WINDOWS:
        script = work / "dotnet-install.ps1"
        download(INSTALL_SCRIPT_PS1, script)
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
               "-File", str(script), "-InstallDir", str(env_dir),
               "-Runtime", "dotnet", "-Channel", channel]
    else:
        script = work / "dotnet-install.sh"
        download(INSTALL_SCRIPT_SH, script)
        os.chmod(script, 0o755)
        cmd = ["bash", str(script), "--install-dir", str(env_dir),
               "--skip-non-versioned-files",
               "--runtime", "dotnet", "--channel", channel]
    subprocess.run(cmd, check=True)


def install(name, cfg, force=False):
    ec = env_config(cfg, name)
    env_dir = env_path(name, ec)
    env_dir.mkdir(parents=True, exist_ok=True)
    marker = env_dir / ".dotnetenv.json"

    with DirLock(Path(str(env_dir) + ".lock")):
        # Re-check under the lock: a parallel job may have just finished.
        # The marker is written only on full success, so its presence means
        # SDK + runtime + XHarness + MAUI are all provisioned.
        if marker.exists() and not force:
            log(f"Environment '{name}' already provisioned ({env_dir}); "
                f"skipping. Use 'install --force' to reinstall.")
            return
        # Force: clear the marker first so a reinstall that dies partway through
        # isn't mistaken for a complete environment next time.
        marker.unlink(missing_ok=True)

        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)

            log(f"Installing .NET SDK for '{name}' -> {env_dir}")
            run_install_script(env_dir, ec, work)

            runtime = ec.get("runtime")
            if runtime:
                log(f"Installing .NET {runtime} shared runtime -> {env_dir}")
                run_install_runtime(env_dir, runtime, work)

        dotnet = str(env_dir / dotnet_exe_name())
        _, env = build_env(name, ec)

        # XHarness global tool - same version everywhere, isolated via DOTNET_CLI_HOME
        log(f"Installing XHarness {XHARNESS_VERSION}")
        subprocess.run(
            [dotnet, "tool", "install", "--global", XHARNESS_NAME,
             "--version", XHARNESS_VERSION, "--add-source", XHARNESS_SOURCE],
            env=env, check=True)

        # MAUI workload is always installed, pinned to an exact workload-set version.
        maui_ver = ec.get("mauiVersion")
        if not maui_ver:
            die(f"environment '{name}' missing required 'mauiVersion' (workload-set version)")
        log(f"Installing MAUI workload (set {maui_ver})")
        subprocess.run([dotnet, "workload", "install", "maui", "--version", maui_ver],
                       env=env, check=True)

        # record what was provisioned
        provisioned = {
            "name": name,
            "config": ec,
            "dotnet": dotnet,
        }
        marker.write_text(json.dumps(provisioned, indent=2))

    log(f"Environment '{name}' ready")
    print(f"\nUse it:\n  python3 {Path(__file__).name} exec {name} -- dotnet --info")


# ---- exec -------------------------------------------------------------------

def exec_in(name, cmd, cfg):
    if not cmd:
        die("nothing to exec. usage: dotnetenv exec <name> -- <command...>")
    ec = env_config(cfg, name)
    _, env = build_env(name, ec)

    if IS_WINDOWS:
        proc = subprocess.run(cmd, env=env)
        sys.exit(proc.returncode)
    else:
        exe = shutil.which(cmd[0], path=env["PATH"]) or cmd[0]
        os.execvpe(exe, cmd, env)


# ---- activate ---------------------------------------------------------------

def _activate_posix(name, env_dir, tools_dir, v):
    L = []
    # Snapshot originals once (guard against double-activation clobbering them).
    L.append('if [ -z "${_DOTNETENV_ACTIVE-}" ]; then')
    L.append('  _DOTNETENV_OLD_PATH="$PATH"')
    for k in v:
        L.append(f'  if [ -n "${{{k}+x}}" ]; then _DOTNETENV_HAD_{k}=1; _DOTNETENV_OLD_{k}="${k}"; '
                 f'else _DOTNETENV_HAD_{k}=0; fi')
    L.append('fi')
    L.append(f'export _DOTNETENV_ACTIVE="{name}"')
    for k, val in v.items():
        L.append(f'export {k}="{val}"')
    L.append(f'export PATH="{env_dir}:{tools_dir}:$PATH"')
    L.append('deactivate-dotnet() {')
    L.append('  export PATH="$_DOTNETENV_OLD_PATH"; unset _DOTNETENV_OLD_PATH')
    for k in v:
        L.append(f'  if [ "${{_DOTNETENV_HAD_{k}}}" = 1 ]; then export {k}="${{_DOTNETENV_OLD_{k}}}"; '
                 f'else unset {k}; fi; unset _DOTNETENV_HAD_{k} _DOTNETENV_OLD_{k}')
    L.append('  unset _DOTNETENV_ACTIVE; unset -f deactivate-dotnet')
    L.append('}')
    return L


def _activate_fish(name, env_dir, tools_dir, v):
    L = []
    L.append('if not set -q _DOTNETENV_ACTIVE')
    L.append('  set -gx _DOTNETENV_OLD_PATH $PATH')
    for k in v:
        L.append(f'  if set -q {k}; set -gx _DOTNETENV_HAD_{k} 1; set -gx _DOTNETENV_OLD_{k} ${k}; '
                 f'else; set -gx _DOTNETENV_HAD_{k} 0; end')
    L.append('end')
    L.append(f'set -gx _DOTNETENV_ACTIVE "{name}"')
    for k, val in v.items():
        L.append(f'set -gx {k} "{val}"')
    L.append(f'set -gx PATH "{env_dir}" "{tools_dir}" $PATH')
    L.append('function deactivate-dotnet')
    L.append('  set -gx PATH $_DOTNETENV_OLD_PATH; set -e _DOTNETENV_OLD_PATH')
    for k in v:
        L.append(f'  if test "$_DOTNETENV_HAD_{k}" = 1; set -gx {k} $_DOTNETENV_OLD_{k}; else; set -e {k}; end; '
                 f'set -e _DOTNETENV_HAD_{k} _DOTNETENV_OLD_{k}')
    L.append('  set -e _DOTNETENV_ACTIVE; functions -e deactivate-dotnet')
    L.append('end')
    return L


def _activate_powershell(name, env_dir, tools_dir, v):
    L = []
    L.append('if (-not (Test-Path Env:_DOTNETENV_ACTIVE)) {')
    L.append('  $env:_DOTNETENV_OLD_PATH = $env:PATH')
    for k in v:
        L.append(f'  if (Test-Path Env:{k}) {{ $env:_DOTNETENV_HAD_{k}=1; $env:_DOTNETENV_OLD_{k}=$env:{k} }} '
                 f'else {{ $env:_DOTNETENV_HAD_{k}=0 }}')
    L.append('}')
    L.append(f'$env:_DOTNETENV_ACTIVE = "{name}"')
    for k, val in v.items():
        L.append(f'$env:{k} = "{val}"')
    L.append(f'$env:PATH = "{env_dir};{tools_dir};" + $env:PATH')
    L.append('function global:deactivate-dotnet {')
    L.append('  $env:PATH = $env:_DOTNETENV_OLD_PATH; Remove-Item Env:_DOTNETENV_OLD_PATH -EA SilentlyContinue')
    for k in v:
        L.append(f'  if ($env:_DOTNETENV_HAD_{k} -eq 1) {{ $env:{k}=$env:_DOTNETENV_OLD_{k} }} '
                 f'else {{ Remove-Item Env:{k} -EA SilentlyContinue }}')
        L.append(f'  Remove-Item Env:_DOTNETENV_HAD_{k},Env:_DOTNETENV_OLD_{k} -EA SilentlyContinue')
    L.append('  Remove-Item Env:_DOTNETENV_ACTIVE -EA SilentlyContinue; Remove-Item Function:deactivate-dotnet')
    L.append('}')
    return L


def activate(name, cfg, shell):
    """Print shell code to source. Defines deactivate-dotnet to restore the
    prior environment (PATH and every managed var), venv-style."""
    ec = env_config(cfg, name)
    env_dir = env_path(name, ec)
    if not (env_dir / dotnet_exe_name()).exists():
        die(f"environment '{name}' not installed. run: dotnetenv install {name}")
    tools_dir = env_dir / ".dotnet" / "tools"
    v = managed_vars(env_dir, ec)

    builders = {"posix": _activate_posix, "fish": _activate_fish, "powershell": _activate_powershell}
    for line in builders[shell](name, env_dir, tools_dir, v):
        print(line)

    if shell == "powershell":
        how = f"dotnetenv activate {name} --shell powershell | Out-String | Invoke-Expression"
    else:
        how = f'eval "$(dotnetenv activate {name})"'
    print(f"# dotnetenv: activated '{name}' ({env_dir}); 'deactivate-dotnet' to undo", file=sys.stderr)
    print(f"# usage: {how}", file=sys.stderr)


# ---- list -------------------------------------------------------------------

def list_envs(cfg):
    for name, ec in sorted(cfg["environments"].items()):
        env_dir = env_path(name, ec)
        mark = "installed" if (env_dir / dotnet_exe_name()).exists() else "not installed"
        sdk = ec.get("sdkVersion") or ec.get("sdk") or ec.get("channel") or "?"
        pin = ec.get("mauiVersion") or "MISSING"
        print(f"  {name:8} sdk={sdk:10} maui={pin:14} dir={str(env_dir):28} [{mark}]")


# ---- device: iOS simulators -------------------------------------------------

def _simctl_env(name, cfg):
    """Env for simctl/xcrun: inherit + DEVELOPER_DIR from the named env's Xcode.

    iOS sim management only needs Xcode, not an installed .NET SDK, so we read
    developerDir straight from config without requiring `install` to have run.
    """
    env = dict(os.environ)
    if name:
        dev_dir = env_config(cfg, name).get("developerDir")
        if dev_dir:
            env["DEVELOPER_DIR"] = dev_dir
    return env


def _simctl_json(env, *args):
    out = subprocess.run(["xcrun", "simctl", *args, "-j"],
                         env=env, check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def _ios_resolve_device(env, override):
    """Return (identifier, label) for the sim device type. Newest iPhone if unset."""
    if override:
        ident = "com.apple.CoreSimulator.SimDeviceType." + override.replace(" ", "-")
        return ident, override
    types = [d for d in _simctl_json(env, "list", "devicetypes")["devicetypes"]
             if "iPhone" in d["name"]]
    if not types:
        die("no iPhone simulator device types found")
    last = types[-1]                       # matches the old `tail -1`
    return last["identifier"], last["name"]


def _ios_resolve_runtime(env, override):
    """Return (identifier, label) for the sim runtime. Newest iOS runtime if unset."""
    if override:
        ident = "com.apple.CoreSimulator.SimRuntime.iOS-" + override.replace(".", "-")
        return ident, f"iOS {override}"
    runtimes = _simctl_json(env, "list", "runtimes")["runtimes"]
    if not runtimes:
        die("no simulator runtimes found")
    last = runtimes[-1]
    return last["identifier"], last["name"]


def _ios_find_sim(env, sim_name):
    """Return (udid, state) for the named sim, or (None, None) if absent."""
    for devs in _simctl_json(env, "list", "devices")["devices"].values():
        for d in devs:
            if d.get("name") == sim_name:
                return d.get("udid"), d.get("state")
    return None, None


def _ios_up(name, dc, cfg, device_override, runtime_override):
    env = _simctl_env(name, cfg)
    sim_name = dc.get("simName", "dotnet_cbl_testing")

    udid, state = _ios_find_sim(env, sim_name)
    if udid is None:
        dev_id, dev_label = _ios_resolve_device(env, device_override or dc.get("device"))
        rt_id, rt_label = _ios_resolve_runtime(env, runtime_override or dc.get("runtime"))
        log(f"{sim_name} not found, creating {dev_label} ({rt_label}) sim")
        subprocess.run(["xcrun", "simctl", "create", sim_name, dev_id, rt_id],
                       env=env, check=True)
        udid, state = _ios_find_sim(env, sim_name)

    if state != "Booted":
        log(f"booting {sim_name}")
        subprocess.run(["xcrun", "simctl", "boot", sim_name], env=env, check=True)

    subprocess.run(["open", "-a", "simulator"], env=env, check=True)
    log(f"iOS simulator '{sim_name}' ready")


def _ios_id(name, dc, cfg):
    """Print JUST the UDID of the named sim (for scripting). Dies if absent."""
    env = _simctl_env(name, cfg)
    sim_name = dc.get("simName", "dotnet_cbl_testing")
    udid, _ = _ios_find_sim(env, sim_name)
    if udid is None:
        die(f"iOS simulator '{sim_name}' does not exist")
    print(udid)


def _ios_down(name, dc, cfg):
    env = _simctl_env(name, cfg)
    sim_name = dc.get("simName", "dotnet_cbl_testing")
    udid, state = _ios_find_sim(env, sim_name)
    if udid is None:
        log(f"iOS simulator '{sim_name}' does not exist")
        return
    if state == "Booted":
        log(f"shutting down {sim_name}")
        subprocess.run(["xcrun", "simctl", "shutdown", sim_name], env=env, check=True)
    else:
        log(f"iOS simulator '{sim_name}' already shut down")


# ---- device: Android emulators ----------------------------------------------

ANDROID_ARCHES = {"x86_64": "x86_64", "aarch64": "arm64-v8a", "arm64": "arm64-v8a"}


def _android_env(dc):
    """Env for the Android SDK tools, with JAVA_HOME resolved on macOS."""
    env = dict(os.environ)
    java_ver = dc.get("javaVersion", "17.0")
    if IS_MAC and "JAVA_HOME" not in env:
        try:
            jh = subprocess.run(["/usr/libexec/java_home", "-v", java_ver],
                                check=True, capture_output=True, text=True).stdout.strip()
            env["JAVA_HOME"] = jh
        except subprocess.CalledProcessError:
            die(f"no JDK {java_ver} found (via /usr/libexec/java_home)")
    return env


def _android_sdk_tools(env):
    """Locate the four SDK tools we need: (sdkmanager, avdmanager, emulator, adb).

    Resolution order:
      1. $ANDROID_HOME / $ANDROID_SDK_ROOT (if set and a directory) -> canonical
         cmdline-tools/latest/bin layout.
      2. avdmanager on PATH, with the SDK root derived from its real location
         (.../cmdline-tools/latest/bin/avdmanager -> ../../../).
    """
    sdk_root = None
    for var in ("ANDROID_HOME", "ANDROID_SDK_ROOT"):
        val = env.get(var)
        if val and Path(val).is_dir():
            sdk_root = Path(val)
            break

    if sdk_root is not None:
        cmdline_bin = sdk_root / "cmdline-tools" / "latest" / "bin"
        sdkmanager = cmdline_bin / "sdkmanager"
        avdmanager = cmdline_bin / "avdmanager"
        if not avdmanager.exists():
            die(f"avdmanager not found at {avdmanager} "
                "(is cmdline-tools/latest installed under the Android SDK root?)")
    else:
        avd = shutil.which("avdmanager", path=env.get("PATH"))
        if not avd:
            die("Android SDK not found: set ANDROID_HOME (or ANDROID_SDK_ROOT), "
                "or put avdmanager on PATH")
        avdmanager = Path(os.path.realpath(avd))         # .../cmdline-tools/latest/bin/avdmanager
        bin_dir = avdmanager.parent
        sdkmanager = bin_dir / "sdkmanager"
        sdk_root = bin_dir.parents[2]                     # .../  (matches ../../../)

    emulator = sdk_root / "emulator" / "emulator"
    adb = sdk_root / "platform-tools" / "adb"
    return sdkmanager, avdmanager, emulator, adb


def _adb_getprop(adb, env, serial, prop):
    """getprop a single property; '' on any error/timeout (device still booting)."""
    try:
        return subprocess.run(
            [str(adb), "-s", serial, "shell", "getprop", prop],
            env=env, capture_output=True, text=True, timeout=10).stdout.strip()
    except (subprocess.TimeoutExpired, subprocess.SubprocessError):
        return ""


def _android_find_device(adb, env, avd_name, quiet=False):
    """adb serial of a running emulator whose AVD name matches, or None."""
    out = subprocess.run([str(adb), "devices"], env=env,
                         check=True, capture_output=True, text=True).stdout
    if not quiet:
        log(f"looking for emulator running AVD '{avd_name}'")
    for line in out.splitlines():
        line = line.strip()
        if not line or line.startswith("List"):
            continue
        parts = line.split()
        serial, state = parts[0], (parts[1] if len(parts) > 1 else "")
        if state != "device":          # offline/booting: not usable yet, skip quietly
            continue
        found = _adb_getprop(adb, env, serial, "ro.kernel.qemu.avd_name")
        if found == avd_name:
            if not quiet:
                log(f"found running emulator {serial}")
            return serial
        if not quiet:
            log(f"{serial} is '{found}', not a match")
    return None


def _android_wait_boot(adb, env, avd_name, timeout=300):
    """Block until the matching emulator is in adb AND fully booted; return its
    serial. Dies on timeout. Polls quietly so it doesn't spam the log."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        serial = _android_find_device(adb, env, avd_name, quiet=True)
        if serial and _adb_getprop(adb, env, serial, "sys.boot_completed") == "1":
            return serial
        time.sleep(3)
    die(f"timed out after {timeout}s waiting for '{avd_name}' to boot")


def _android_create_avd(env, sdkmanager, avdmanager, avd_name, api_level, arch, sys_platform):
    img = f"system-images;android-{api_level};default;{arch}"
    existing = subprocess.run([str(avdmanager), "list", "avd"], env=env,
                              check=True, capture_output=True, text=True).stdout
    if avd_name in existing:
        log(f"found existing AVD {avd_name}, skipping create")
        return
    log(f"{avd_name} AVD not found, creating Android API {api_level} {arch} AVD")
    subprocess.run([str(sdkmanager), "--install", img, "platform-tools", sys_platform],
                   env=env, check=True)
    subprocess.run([str(avdmanager), "-s", "create", "avd", "-n", avd_name,
                    "-k", img, "-c", "1024M"], env=env, check=True)


def _android_launch(emulator, env, avd_name):
    if not Path(emulator).exists():
        die("emulator not installed! This script will not install it automatically "
            "because it is not versioned, and the latest version crashes a lot on "
            "Apple Silicon. Please install it before running this command.")
    log(f"launching emulator {avd_name}")
    subprocess.Popen([str(os.path.realpath(emulator)), f"@{avd_name}",
                      "-memory", "1024", "-no-snapshot", "-netspeed", "full",
                      "-netdelay", "none", "-no-boot-anim"], env=env)


def _android_avd_name(dc, api_override):
    api_level = api_override or dc.get("apiLevel", 22)
    return f"{dc.get('avdPrefix', 'dotnet_cbl_testing')}_{api_level}", api_level


def _android_up(dc, api_override):
    arch = ANDROID_ARCHES.get(platform.machine())
    if not arch:
        die(f"unsupported architecture {platform.machine()}")
    avd_name, api_level = _android_avd_name(dc, api_override)

    env = _android_env(dc)
    sdkmanager, avdmanager, emulator, adb = _android_sdk_tools(env)

    serial = _android_find_device(adb, env, avd_name)
    if serial:
        log(f"Android emulator '{avd_name}' already running ({serial})")
        return
    log("no suitable emulator found, checking AVD images")
    _android_create_avd(env, sdkmanager, avdmanager, avd_name, api_level, arch,
                        dc.get("systemImagePlatform", "platforms;android-34"))
    _android_launch(emulator, env, avd_name)
    timeout = dc.get("bootTimeout", 300)
    log(f"waiting up to {timeout}s for '{avd_name}' to finish booting")
    serial = _android_wait_boot(adb, env, avd_name, timeout)
    log(f"Android emulator '{avd_name}' ready ({serial})")


def _android_id(dc, api_override):
    """Print JUST the adb serial (e.g. emulator-5554) of the running emulator
    whose AVD matches ours (for scripting). Dies if none is running."""
    avd_name, _ = _android_avd_name(dc, api_override)
    env = _android_env(dc)
    *_, adb = _android_sdk_tools(env)
    dev = _android_find_device(adb, env, avd_name)
    if not dev:
        die(f"no running emulator for AVD '{avd_name}'")
    print(dev)


def _android_down(dc, api_override):
    avd_name, _ = _android_avd_name(dc, api_override)
    env = _android_env(dc)
    *_, adb = _android_sdk_tools(env)
    dev = _android_find_device(adb, env, avd_name)
    if not dev:
        log(f"no running emulator for '{avd_name}'")
        return
    log(f"killing emulator {dev}")
    subprocess.run([str(adb), "-s", dev, "emu", "kill"], env=env, check=True)


def device(action, name, plat, cfg, device_override, runtime_override, api_override):
    dc = device_config(cfg, plat)
    if plat == "ios":
        if not name:
            die("ios device needs an environment name (for the Xcode pin): "
                "dotnetenv device up <name> --platform ios")
        if action == "up":
            _ios_up(name, dc, cfg, device_override, runtime_override)
        elif action == "down":
            _ios_down(name, dc, cfg)
        else:
            _ios_id(name, dc, cfg)
    else:
        if action == "up":
            _android_up(dc, api_override)
        elif action == "down":
            _android_down(dc, api_override)
        else:
            _android_id(dc, api_override)


# ---- cli --------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(prog="dotnetenv", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--config", default=os.environ.get("DOTNETENV_CONFIG"),
                   help="config file (default: ./dotnetenv.json, then "
                        "~/.config/dotnetenv/dotnetenv.json, then bundled template)")
    sub = p.add_subparsers(dest="cmd", required=True)

    pi = sub.add_parser("install", help="provision an environment (idempotent)")
    pi.add_argument("name")
    pi.add_argument("--force", action="store_true",
                    help="reinstall even if already provisioned")

    pe = sub.add_parser("exec", help="run a command inside an environment")
    pe.add_argument("name")
    pe.add_argument("rest", nargs=argparse.REMAINDER)

    pa = sub.add_parser("activate", help="print shell code to enter an environment")
    pa.add_argument("name")
    pa.add_argument("--shell", choices=["posix", "powershell", "fish"],
                    default="powershell" if IS_WINDOWS else "posix",
                    help="output dialect (default: by platform)")

    sub.add_parser("list", help="list configured environments")

    pd = sub.add_parser("device", help="bring a test simulator/emulator up or down")
    pd.add_argument("action", choices=["up", "down", "id"],
                    help="up/down a device, or 'id' to print just the running device's "
                         "serial/UDID (for scripting)")
    pd.add_argument("name", nargs="?",
                    help="environment name (required for ios: supplies the Xcode pin)")
    pd.add_argument("--platform", choices=["ios", "android"], required=True)
    pd.add_argument("-d", "--device", help="ios: simulator device, e.g. 'iPhone 15 Pro Max'")
    pd.add_argument("-r", "--runtime", help="ios: simulator runtime, e.g. 17.2")
    pd.add_argument("-l", "--api-level", type=int, help="android: emulator API level")

    args = p.parse_args()
    config_path = args.config or discover_config()
    cfg = load_config(config_path)

    if args.cmd == "install":
        install(args.name, cfg, force=args.force)
    elif args.cmd == "exec":
        rest = args.rest
        if rest and rest[0] == "--":
            rest = rest[1:]
        exec_in(args.name, rest, cfg)
    elif args.cmd == "activate":
        activate(args.name, cfg, args.shell)
    elif args.cmd == "list":
        list_envs(cfg)
    elif args.cmd == "device":
        device(args.action, args.name, args.platform, cfg,
               args.device, args.runtime, args.api_level)


if __name__ == "__main__":
    main()
