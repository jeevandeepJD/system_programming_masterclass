#!/usr/bin/env python3
"""Render a weekly overview and its daily sections into one PDF."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from urllib.parse import quote


ROOT = Path(__file__).resolve().parent.parent
LESSON_BUILDER = ROOT / "bin" / "build_lesson.py"
GITHUB_BLOB_ROOT = (
    "https://github.com/jeevandeepJD/system_programming_masterclass/"
    "blob/experiment/weekly-modules"
)
HTML_PORT = 8731


def normalize_week(value: str) -> str:
    match = re.fullmatch(r"(?:week-)?(\d+)", value)
    if not match:
        raise argparse.ArgumentTypeError("use a number such as 1 or week-001")
    return f"week-{int(match.group(1)):03d}"


def slug_from_overview(path: Path) -> str:
    text = path.read_text(encoding="utf-8")
    match = re.search(r"^#\s+Week\s+\d+\s+[—-]\s+(.+)$", text, re.MULTILINE)
    title = match.group(1) if match else "complete"
    slug = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
    return slug or "complete"


def lesson_with_challenge_links(
    lesson: Path, module: Path, temp_dir: Path
) -> Path:
    day_match = re.match(r"(day-\d+)-", lesson.stem)
    if not day_match:
        return lesson

    challenges = sorted(
        path
        for path in (module / "challenges").glob(f"{day_match.group(1)}-*")
        if path.is_file()
    )
    if not challenges:
        return lesson

    labels = {
        ".html": "interactive HTML lab",
        ".py": "Python lab",
        ".c": "C lab",
        ".s": "assembly lab",
        ".sh": "shell lab",
    }
    lines = [
        "",
        "---",
        "",
        "## Challenge links for this section",
        "",
        "Run commands from the repository root. GitHub links are included only",
        "as a portable way to inspect or download the source.",
        "",
    ]
    for path in challenges:
        relative = path.relative_to(ROOT).as_posix()
        kind = labels.get(path.suffix.lower(), "challenge file")
        github_url = f"{GITHUB_BLOB_ROOT}/{quote(relative, safe='/')}"
        lines.extend([f"### {path.name}", "", f"**Type:** {kind}", ""])

        if path.suffix.lower() == ".html":
            serve_directory = path.parent.relative_to(ROOT).as_posix()
            local_url = f"http://localhost:{HTML_PORT}/{quote(path.name)}"
            lines.extend(
                [
                    "**Start the local challenge server:**",
                    "",
                    "```bash",
                    f"python3 -m http.server {HTML_PORT} --bind 127.0.0.1 "
                    f"--directory {serve_directory}",
                    "```",
                    "",
                    f"- [Open interactive lab]({local_url})",
                    f"- [View source on GitHub]({github_url})",
                    "",
                ]
            )
            continue

        command = runnable_command(path, module)
        lines.extend(
            [
                "**Copy and run:**",
                "",
                "```bash",
                command,
                "```",
                "",
                f"- [View source on GitHub]({github_url})",
                "",
            ]
        )

    enhanced = temp_dir / lesson.name
    enhanced.write_text(
        lesson.read_text(encoding="utf-8") + "\n".join(lines) + "\n",
        encoding="utf-8",
    )
    return enhanced


def runnable_command(path: Path, module: Path) -> str:
    relative = path.relative_to(ROOT).as_posix()
    suffix = path.suffix.lower()

    if suffix == ".py":
        arguments = {
            "day-003-riscv-decoder.py": " trace sum",
            "day-004-plot.py": " /tmp/memory-wall.csv",
            "day-006-fictional-isa.py": " table",
        }.get(path.name, "")
        return f"python3 {relative}{arguments}"

    if suffix == ".sh":
        return f"bash {relative}"

    if suffix == ".s":
        output = f"/tmp/{path.stem}.o"
        return f"gcc -c -g {relative} -o {output}"

    if suffix == ".c":
        if path.name == "day-005-object-demo.c":
            output = "/tmp/day-005-object-demo.o"
            return (
                "gcc -std=c17 -Wall -Wextra -O0 -g -c "
                f"{relative} -o {output} && readelf -h -S {output}"
            )

        day_prefix = re.match(r"(day-\d+)-", path.name)
        assembly_sources: list[Path] = []
        if day_prefix:
            assembly_sources = sorted(
                p
                for p in (module / "challenges").glob(
                    f"{day_prefix.group(1)}-*.s"
                )
                if p.is_file()
            )
        companions = " ".join(
            p.relative_to(ROOT).as_posix() for p in assembly_sources
        )
        output = f"/tmp/{path.stem}"
        argument = {
            "day-002-layout-callback-lab.c": " layout",
            "day-003-memory-errors.c": " safe",
            "day-004-qualifiers-ub-lab.c": " safe",
            "day-007-exception-flow-lab.c": " all",
            "day-007-startup-probe.c": " demo",
        }.get(path.name, "")
        sources = f"{relative} {companions}".strip()
        build = (
            "gcc -std=c17 -Wall -Wextra -O0 -g -pthread "
            f"{sources} -lm -o {output}"
        )
        if path.name == "day-004-memory-wall-lab.c":
            return f"{build} && {output} 64 > /tmp/memory-wall.csv"
        return f"{build} && {output}{argument}"

    return f"printf 'Open this challenge file: %s\\n' {relative}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("week", type=normalize_week)
    parser.add_argument("--no-open", action="store_true")
    args = parser.parse_args()

    module = ROOT / "weekly" / args.week
    overview = module / "overview.md"
    lessons = sorted((module / "lessons").glob("day-*.md"))

    if not overview.exists():
        raise SystemExit(f"missing weekly overview: {overview}")
    if not lessons:
        raise SystemExit(f"no daily lesson sources found under {module / 'lessons'}")

    output_dir = ROOT / "weekly" / "pdf"
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f"{args.week}-{slug_from_overview(overview)}.pdf"

    with tempfile.TemporaryDirectory(prefix=f"{args.week}-") as temp:
        temp_dir = Path(temp)
        sources = [(overview, module)]
        sources.extend(
            (
                lesson_with_challenge_links(lesson, module, temp_dir),
                lesson.parent,
            )
            for lesson in lessons
        )
        parts: list[Path] = []

        for index, (source, base_url) in enumerate(sources):
            part = temp_dir / f"{index:02d}-{source.stem}.pdf"
            subprocess.run(
                [
                    sys.executable,
                    str(LESSON_BUILDER),
                    str(source),
                    "--output",
                    str(part),
                    "--base-url",
                    str(base_url),
                    "--no-open",
                ],
                check=True,
            )
            parts.append(part)

        subprocess.run(
            ["pdfunite", *(str(part) for part in parts), str(output)],
            check=True,
        )

    print(f"wrote {output}")

    if not args.no_open:
        cursor = subprocess.run(
            ["bash", "-lc", "command -v cursor || true"],
            check=False,
            capture_output=True,
            text=True,
        ).stdout.strip()
        if cursor:
            subprocess.Popen(
                [cursor, str(output)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )


if __name__ == "__main__":
    main()
