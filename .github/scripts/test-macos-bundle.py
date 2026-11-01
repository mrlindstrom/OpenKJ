#!/usr/bin/env python3
"""Launch the signed release bundle and fail on an immediate startup crash."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

app = Path(sys.argv[1]).resolve()
env = os.environ.copy()
for name in ("DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH"):
    env.pop(name, None)
env["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
env["QT_DEBUG_PLUGINS"] = "1"
with tempfile.TemporaryFile(mode="w+") as log:
    process = subprocess.Popen([str(app / "Contents/MacOS/openkj")], env=env,
                               stdout=log, stderr=subprocess.STDOUT)
    try:
        try:
            code = process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            print("Release bundle remained running after startup.")
        else:
            log.seek(0)
            sys.exit(f"Release bundle exited during startup ({code}):\n{log.read()}")
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
