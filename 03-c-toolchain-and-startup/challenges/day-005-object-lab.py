#!/usr/bin/env python3
"""Build and inspect each translation stage without modifying the source tree."""

from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile


def run(command: list[str], *, capture: bool = False) -> str:
    print("+", " ".join(command))
    result = subprocess.run(
        command, check=True, text=True, capture_output=capture
    )
    return result.stdout if capture else ""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--keep", action="store_true", help="keep the temporary directory")
    args = parser.parse_args()

    source = Path(__file__).with_name("day-005-object-demo.c").resolve()
    compiler = shutil.which("gcc") or shutil.which("clang")
    if not compiler:
        raise SystemExit("Install gcc or clang first")
    for tool in ("file", "readelf", "nm", "objdump"):
        if not shutil.which(tool):
            raise SystemExit(f"Required tool not found: {tool}")

    work = Path(tempfile.mkdtemp(prefix="week3-object-"))
    print(f"workspace: {work}")
    try:
        preprocessed = work / "demo.i"
        assembly = work / "demo.s"
        object_file = work / "demo.o"

        run([compiler, "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-E",
             str(source), "-o", str(preprocessed)])
        run([compiler, "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-S",
             "-O0", str(source), "-o", str(assembly)])
        run([compiler, "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-c",
             "-O0", str(source), "-o", str(object_file)])

        print("\nArtifacts")
        run(["file", str(preprocessed), str(assembly), str(object_file)])
        print("\nELF header and sections")
        run(["readelf", "-h", "-S", str(object_file)])
        print("\nSymbols (undefined names are intentionally unresolved)")
        run(["nm", "-C", str(object_file)])
        print("\nRelocations")
        run(["readelf", "-Wr", str(object_file)])
        print("\nDisassembly with relocation annotations")
        run(["objdump", "-dr", str(object_file)])

        print("\nThe object is expected not to link by itself:")
        failed = subprocess.run(
            [compiler, str(object_file), "-o", str(work / "demo")],
            text=True, capture_output=True
        )
        if failed.returncode == 0:
            raise SystemExit("Unexpectedly linked: the demonstration lost its undefined symbols")
        print(failed.stderr.strip())
        print("\nSuccess: preprocessing, compilation, assembly, and unresolved object state observed.")
    finally:
        if args.keep:
            print(f"kept: {work}")
        else:
            shutil.rmtree(work)


if __name__ == "__main__":
    main()
