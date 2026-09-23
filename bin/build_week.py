#!/usr/bin/env python3
"""Render a weekly overview and its daily sections into one PDF."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
LESSON_BUILDER = ROOT / "bin" / "build_lesson.py"


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
        sources = [overview, *lessons]
        parts: list[Path] = []

        for index, source in enumerate(sources):
            part = temp_dir / f"{index:02d}-{source.stem}.pdf"
            subprocess.run(
                [
                    sys.executable,
                    str(LESSON_BUILDER),
                    str(source),
                    "--output",
                    str(part),
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
