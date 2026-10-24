#!/usr/bin/env python3
"""Run an experiment command and retain its input, output and exit status."""
import argparse
import datetime
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("label")
    parser.add_argument("--remote", type=Path, help="bash script for server-lzj-161-ubuntu")
    parser.add_argument("--ssh-config", type=Path, help="explicit SSH configuration for remote runs")
    parser.add_argument("--logs", type=Path, default=Path(__file__).resolve().parents[1] / "build/bar1-64g/logs")
    parser.add_argument("--command", nargs=argparse.REMAINDER, default=[])
    args = parser.parse_args()
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
    args.logs.mkdir(parents=True, exist_ok=True)
    base = args.logs / (stamp + "-" + args.label)
    script = args.remote.read_bytes() if args.remote else None
    ssh_options = ["-F", str(args.ssh_config)] if args.ssh_config else []
    command = (["ssh", *ssh_options, "-o", "BatchMode=yes", "-o", "ConnectTimeout=10",
                "-o", "ServerAliveInterval=10", "-o", "ServerAliveCountMax=3",
                "server-lzj-161-ubuntu", "bash -s"] if args.remote else args.command)
    if not command:
        parser.error("provide --remote or a command")
    metadata = {"utc": stamp, "cwd": str(Path.cwd()), "argv": command,
                "stdin": script.decode() if script is not None else None}
    base.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")
    with base.with_suffix(".log").open("wb") as log:
        result = subprocess.run(command, input=script, stdout=log, stderr=subprocess.STDOUT)
    metadata["returncode"] = result.returncode
    base.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(base.with_suffix(".log").read_text(errors="replace"), end="")
    print(f"AUDIT: {base}; exit={result.returncode}")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
