#!/usr/bin/env python3
"""dotnetenv - pinned, isolated, side-by-side .NET environments for CI.

One config file pins each environment (SDK + workloads + MAUI workload-set +
Xcode). Each environment lives in its own directory with its own DOTNET_ROOT
and its own global-tool home, so installs never clobber each other.

Two verbs:
    install <name>           provision the environment (idempotent)
    exec <name> -- <cmd...>  run a command inside it (sets env, execs)

No shell sourcing. Nothing leaks into the parent shell. CI calls `exec`.
"""

import argparse
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

DEFAULT_CONFIG = Path(__file__).resolve().parent / "dotnetenv.json"

INSTALL_SCRIPT_SH = "https://dot.net/v1/dotnet-install.sh"
INSTALL_SCRIPT_PS1 = "https://dot.net/v1/dotnet-install.ps1"

# XHarness is the same everywhere: the 10.0 prerelease is .NET 8 compatible too.
XHARNESS_NAME = "Microsoft.DotNet.XHarness.CLI"
XHARNESS_VERSION = "10.0.0-prerelease*"
XHARNESS_SOURCE = "https://pkgs.dev.azure.com/dnceng/public/_packaging/dotnet-eng/nuget/v3/index.json"


def log(msg):
    print(f"\033[32m===== {msg} =====\033[0m" if sys.stdout.isatty() else f"===== {msg} =====",
          flush=True)


def die(msg):
    print(f"dotnetenv: {msg}", file=sys.stderr)
    sys.exit(1)


def load_config(path):
    p = Path(path)
    if not p.exists():
        die(f"config not found: {p}")
    with open(p) as f:
        cfg = json.load(f)
    envs = cfg.get("environments")
    if not envs:
        die(f"no 'environments' in {p}")
    return cfg


def env_config(cfg, name):
    envs = cfg["environments"]
    if name not in envs:
        die(f"unknown environment '{name}'. known: {', '.join(sorted(envs))}")
    return envs[name]


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


def install(name, cfg):
    ec = env_config(cfg, name)
    env_dir = env_path(name, ec)
    env_dir.mkdir(parents=True, exist_ok=True)

    with DirLock(Path(str(env_dir) + ".lock")):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)

            log(f"Installing .NET SDK for '{name}' -> {env_dir}")
            run_install_script(env_dir, ec, work)

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
        marker = {
            "name": name,
            "config": ec,
            "dotnet": dotnet,
        }
        (env_dir / ".dotnetenv.json").write_text(json.dumps(marker, indent=2))

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

    me = Path(__file__).name
    if shell == "powershell":
        how = f"python3 {me} activate {name} --shell powershell | Out-String | Invoke-Expression"
    else:
        how = f'eval "$(python3 {me} activate {name})"'
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


# ---- cli --------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(prog="dotnetenv", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--config", default=os.environ.get("DOTNETENV_CONFIG", DEFAULT_CONFIG))
    sub = p.add_subparsers(dest="cmd", required=True)

    pi = sub.add_parser("install", help="provision an environment")
    pi.add_argument("name")

    pe = sub.add_parser("exec", help="run a command inside an environment")
    pe.add_argument("name")
    pe.add_argument("rest", nargs=argparse.REMAINDER)

    pa = sub.add_parser("activate", help="print shell code to enter an environment")
    pa.add_argument("name")
    pa.add_argument("--shell", choices=["posix", "powershell", "fish"],
                    default="powershell" if IS_WINDOWS else "posix",
                    help="output dialect (default: by platform)")

    sub.add_parser("list", help="list configured environments")

    args = p.parse_args()
    cfg = load_config(args.config)

    if args.cmd == "install":
        install(args.name, cfg)
    elif args.cmd == "exec":
        rest = args.rest
        if rest and rest[0] == "--":
            rest = rest[1:]
        exec_in(args.name, rest, cfg)
    elif args.cmd == "activate":
        activate(args.name, cfg, args.shell)
    elif args.cmd == "list":
        list_envs(cfg)


if __name__ == "__main__":
    main()
