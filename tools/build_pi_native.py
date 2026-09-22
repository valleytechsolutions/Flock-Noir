"""Build the Pi shared parser on its host architecture (run during installation)."""
import os
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[1]
suffix = ".dll" if os.name == "nt" else ".so"
output = root / "pi/flocknoir" / ("native_core" + suffix)
compiler = shlex.split(os.environ.get("CXX", "c++"))
subprocess.run(compiler + ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-fPIC", "-shared",
                          "-I", str(root / "firmware/FlockNoir"),
                          str(root / "pi/native/core.cpp"), "-o", str(output)], check=True)
print(output)
