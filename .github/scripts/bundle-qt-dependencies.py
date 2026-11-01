#!/usr/bin/env python3
"""Complete macdeployqt's Homebrew dylib dependencies before signing the app."""
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys

helpers = runpy.run_path(str(Path(__file__).with_name("osxrelocator")))
is_macho = helpers["is_macho"]
load_commands = helpers["load_commands"]


def complete_bundle(app):
    contents = app.resolve() / "Contents"
    frameworks = contents / "Frameworks"
    gst = frameworks / "GStreamer.framework"
    pending = [p for p in contents.rglob("*") if gst not in p.parents and is_macho(str(p))]
    visited = set()
    copied = 0
    while pending:
        owner = pending.pop()
        if owner in visited:
            continue
        visited.add(owner)
        _, dependencies, _ = load_commands(str(owner))
        for dependency in set(dependencies):
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue  # System libraries may live only in the dyld shared cache.
            if dependency.startswith("@rpath/"):
                suffix = dependency[len("@rpath/"):]
                candidates = [frameworks / suffix, gst / "Versions/1.0/lib" / suffix]
            elif dependency.startswith("@loader_path/"):
                candidates = [owner.parent / dependency[len("@loader_path/"):]]
            elif dependency.startswith("@executable_path/"):
                candidates = [contents / "MacOS" / dependency[len("@executable_path/"):]]
            else:
                candidates = []
            if any(p.exists() for p in candidates):
                continue

            name = Path(dependency).name
            if not name.endswith(".dylib"):
                raise RuntimeError(f"Unbundled dependency: {owner}: {dependency}")
            destination = frameworks / name
            if not destination.exists():
                sources = []
                if dependency.startswith(("/opt/homebrew/", "/usr/local/")) and Path(dependency).is_file():
                    sources.append(Path(dependency))
                for prefix in (Path("/opt/homebrew/opt"), Path("/usr/local/opt")):
                    sources.extend(prefix.glob(f"*/lib/{name}"))
                sources = sorted({p.resolve() for p in sources if p.is_file()})
                if len(sources) != 1:
                    raise RuntimeError(f"Expected one Homebrew source for {dependency}, found {sources}")
                shutil.copy2(sources[0], destination)
                destination.chmod(destination.stat().st_mode | 0o200)
                subprocess.run(["install_name_tool", "-id", f"@rpath/{name}", str(destination)], check=True)
                pending.append(destination)
                copied += 1
                print(f"Bundled {sources[0]} -> {destination}", flush=True)

            # A loader-relative reference remains valid without Homebrew's rpaths.
            relative = os.path.relpath(destination, owner.parent)
            owner.chmod(owner.stat().st_mode | 0o200)
            subprocess.run(["install_name_tool", "-change", dependency,
                            "@loader_path/" + relative, str(owner)], check=True)
    print(f"Completed Qt dependencies: {copied} additional libraries", flush=True)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: bundle-qt-dependencies.py PATH.app")
    complete_bundle(Path(sys.argv[1]))
