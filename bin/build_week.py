#!/usr/bin/env python3
"""Render a weekly overview and its daily sections into one PDF."""

from __future__ import annotations

import argparse
import ast
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from urllib.parse import quote

from pypdf import PdfReader, PdfWriter
from pypdf.generic import NameObject, TextStringObject


ROOT = Path(__file__).resolve().parent.parent
LESSON_BUILDER = ROOT / "bin" / "build_lesson.py"
GITHUB_BLOB_ROOT = (
    "https://github.com/jeevandeepJD/system_programming_masterclass/"
    "blob/master"
)


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
        (
            path
            for path in (module / "challenges").glob(
                f"{day_match.group(1)}-*"
            )
            if path.is_file()
        ),
        key=lambda path: (
            0 if "theory-check" in path.stem else 1,
            path.name,
        ),
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
        "## Now check your understanding",
        "",
        "You have finished the theory for this section. Before moving on, use",
        "the quick check below to find anything that still feels uncertain,",
        "then reinforce the idea with the practical lab when one is provided.",
        "",
    ]
    for path in challenges:
        relative = path.relative_to(ROOT).as_posix()
        kind = labels.get(path.suffix.lower(), "challenge file")
        github_url = f"{GITHUB_BLOB_ROOT}/{quote(relative, safe='/')}"
        relative_pdf_url = (
            f"repo-relative:../{module.name}/challenges/{quote(path.name)}"
        )
        friendly_name = re.sub(
            r"^day-\d+-", "", path.stem
        ).replace("-", " ").title()

        if "theory-check" in path.stem:
            lines.extend(
                [
                    "### Quick theory check",
                    "",
                    "Answer from memory first. Explanations appear only after",
                    "you submit your choices.",
                    "",
                    f"- [Start the interactive theory check]({relative_pdf_url})",
                    f"- [View quiz source on GitHub]({github_url})",
                    "",
                ]
            )
            continue

        lines.extend(
            [
                f"### Put it into practice — {friendly_name}",
                "",
                f"**Format:** {kind}",
                "",
                f"**What you will do:** {challenge_description(path)}",
                "",
            ]
        )

        if path.suffix.lower() == ".html":
            lines.extend(
                [
                    f"- [Open the interactive lab]({relative_pdf_url})",
                    f"- [View lab source on GitHub]({github_url})",
                    "",
                ]
            )
            continue

        command = runnable_command(path, module)
        has_todos = "TODO" in path.read_text(
            encoding="utf-8", errors="ignore"
        )
        if has_todos:
            lines.extend(
                [
                    "This is starter code. Open the file, read its instructions,",
                    "complete the `TODO` sections, and then run the self-checks.",
                    "",
                ]
            )
        else:
            lines.extend(
                [
                    "Predict the important result first, then run the lab and",
                    "explain any difference between prediction and observation.",
                    "",
                ]
            )
        lines.extend(
            [
                f"- [Open the lab file]({relative_pdf_url})",
                "",
                "**Copy and run:**",
                "",
                "```bash",
                command,
                "```",
                "",
                f"- [View lab source on GitHub]({github_url})",
                "",
            ]
        )

    enhanced = temp_dir / lesson.name
    enhanced.write_text(
        lesson.read_text(encoding="utf-8") + "\n".join(lines) + "\n",
        encoding="utf-8",
    )
    return enhanced


def challenge_description(path: Path) -> str:
    text = path.read_text(encoding="utf-8", errors="ignore")

    if path.suffix.lower() == ".py":
        try:
            description = ast.get_docstring(ast.parse(text))
            if description:
                return " ".join(description.splitlines()[0].split())
        except SyntaxError:
            pass

    if path.suffix.lower() == ".html":
        match = re.search(r"<title>(.*?)</title>", text, re.I | re.S)
        if match:
            return " ".join(match.group(1).split())

    if path.suffix.lower() in {".c", ".s"}:
        match = re.search(r"/\*(.*?)\*/", text, re.S)
        if match:
            for raw_line in match.group(1).splitlines():
                line = raw_line.strip().lstrip("*").strip()
                if line and not line.lower().startswith(("build", "run")):
                    return line

    if path.suffix.lower() == ".sh":
        for raw_line in text.splitlines()[1:12]:
            line = raw_line.strip().lstrip("#").strip()
            if line:
                return line

    friendly = re.sub(r"^day-\d+-", "", path.stem).replace("-", " ")
    return f"Use this {friendly} exercise to test the section's model."


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


def rewrite_relative_links(pdf: Path) -> int:
    reader = PdfReader(pdf)
    changed = 0
    marker = "repo-relative:"

    for page in reader.pages:
        for reference in page.get("/Annots", []):
            annotation = reference.get_object()
            action = annotation.get("/A")
            if not action or "/URI" not in action:
                continue
            uri = str(action["/URI"])
            if not uri.startswith(marker):
                continue
            action[NameObject("/URI")] = TextStringObject(
                uri.removeprefix(marker)
            )
            changed += 1

    if changed:
        rewritten = pdf.with_suffix(".relative-links.pdf")
        writer = PdfWriter(clone_from=reader)
        with rewritten.open("wb") as stream:
            writer.write(stream)
        rewritten.replace(pdf)

    return changed


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
        sources = [(overview, module, False)]
        for lesson in lessons:
            rendered_source = lesson_with_challenge_links(
                lesson, module, temp_dir
            )
            sources.append(
                (rendered_source, lesson.parent, rendered_source != lesson)
            )
        parts: list[Path] = []

        for index, (source, base_url, preserve_relative) in enumerate(sources):
            part = temp_dir / f"{index:02d}-{source.stem}.pdf"
            command = [
                sys.executable,
                str(LESSON_BUILDER),
                str(source),
                "--output",
                str(part),
                "--base-url",
                str(base_url),
                "--no-open",
            ]
            if preserve_relative:
                command.append("--preserve-relative-links")
            subprocess.run(
                command,
                check=True,
            )
            parts.append(part)

        subprocess.run(
            ["pdfunite", *(str(part) for part in parts), str(output)],
            check=True,
        )

    relative_links = rewrite_relative_links(output)
    print(f"wrote {output} ({relative_links} relative challenge links)")

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
