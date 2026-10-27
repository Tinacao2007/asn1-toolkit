"""Install a completed native Release build and run independent package consumers.

The output directory is never removed, so use a fresh directory for each run.
CMake and a C/C++ compiler must be on PATH. Linux additionally checks the C
executable's dynamic dependencies for C++ runtime leakage.
"""
import argparse
import pathlib
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", required=True, type=pathlib.Path)
parser.add_argument("--output", required=True, type=pathlib.Path)
parser.add_argument("--config", default="Release")
parser.add_argument("--consumers", nargs='+', choices=("c", "runtime", "compiler", "generated", "generated-c"),
                    default=("c", "runtime", "compiler", "generated", "generated-c"))
parser.add_argument("--host-compiler", type=pathlib.Path,
                    help="Native generator from a separate host SDK for runtime-only installations")
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
prefix = out / "install"
def run(*command):
    subprocess.run(list(map(str, command)), check=True)
# Refresh install rules after any CMake-only edits; preserve cached toolchain/options.
run("cmake", "-S", root, "-B", args.build.resolve())
run("cmake", "--install", args.build, "--config", args.config, "--prefix", prefix)
for private_name in ("gtest", "gmock"):
    if (prefix / "include" / private_name).exists():
        raise RuntimeError("Private test headers leaked into SDK: " + private_name)
for kind in args.consumers:
    build = out / kind
    command = ["cmake", "-S", root / "examples/package-consumers", "-B", build,
               "-DCMAKE_BUILD_TYPE=Release", "-DCONSUMER_KIND=" + kind,
               "-DCMAKE_PREFIX_PATH=" + str(prefix)]
    if kind in ("generated", "generated-c"):
        tool = args.host_compiler.resolve() if args.host_compiler else prefix / "bin" / ("asn1cxx.exe" if sys.platform == "win32" else "asn1cxx")
        command.append("-DASN1_HOST_COMPILER=" + str(tool))
    run(*command)
    run("cmake", "--build", build, "--config", args.config)
    run("ctest", "--test-dir", build, "-C", args.config, "--output-on-failure")
    if kind in ("c", "generated-c"):
        cache = (build / "CMakeCache.txt").read_text(encoding='utf-8')
        if "CMAKE_CXX_COMPILER:" in cache:
            raise RuntimeError("C-only SDK consumer unexpectedly enabled C++")
    if kind in ("c", "generated-c") and sys.platform.startswith("linux"):
        deps = subprocess.check_output(["ldd", str(build / "consumer")], text=True)
        if "libstdc++" in deps or "libc++" in deps:
            raise RuntimeError("Pure C consumer linked a C++ runtime: " + deps)
        print("Pure C consumer has no C++ runtime dependency", flush=True)
print("PASS installed consumers: " + ', '.join(args.consumers))
