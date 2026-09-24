#!/usr/bin/env python3
"""Render one masterclass Markdown lesson as a styled PDF."""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path

import markdown
from weasyprint import CSS, HTML


def title_from_markdown(text: str, fallback: str) -> str:
    match = re.search(r"^#\s+(.+)$", text, re.MULTILINE)
    return match.group(1).strip() if match else fallback


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--base-url",
        type=Path,
        help="base directory used to resolve relative links and images",
    )
    parser.add_argument("--no-open", action="store_true")
    args = parser.parse_args()

    source = args.source.expanduser().resolve()
    output = (
        args.output.expanduser().resolve()
        if args.output
        else source.parents[1] / "pdf" / f"{source.stem}.pdf"
    )
    output.parent.mkdir(parents=True, exist_ok=True)

    text = source.read_text(encoding="utf-8")
    title = title_from_markdown(text, source.stem)
    body = markdown.markdown(
        text,
        extensions=["extra", "fenced_code", "codehilite", "sane_lists"],
    )

    document = f"""<!doctype html>
<html lang="en">
<head><meta charset="utf-8"><title>{title}</title></head>
<body><main>{body}</main></body>
</html>"""

    css = CSS(
        string="""
@page {
  size: A4;
  margin: 18mm 17mm 19mm;
  @bottom-left {
    content: "Systems Engineering Masterclass";
    color: #7a8491;
    font: 8pt "DejaVu Sans";
  }
  @bottom-right {
    content: "page " counter(page) " / " counter(pages);
    color: #7a8491;
    font: 8pt "DejaVu Sans";
  }
}
body {
  color: #17202a;
  font: 10.2pt/1.48 "DejaVu Sans", sans-serif;
}
main { max-width: 100%; }
h1 {
  color: #174f86;
  font-size: 22pt;
  line-height: 1.15;
  border-bottom: 2px solid #174f86;
  padding-bottom: 7px;
  margin: 0 0 15px;
}
h2 {
  color: #174f86;
  font-size: 15pt;
  line-height: 1.2;
  border-bottom: 1px solid #b9c6d3;
  padding-bottom: 4px;
  margin: 20px 0 8px;
  break-after: avoid;
}
h3 {
  color: #263746;
  font-size: 12pt;
  margin: 15px 0 6px;
  break-after: avoid;
}
p { margin: 6px 0 9px; orphans: 3; widows: 3; }
ul, ol { margin: 5px 0 10px; padding-left: 22px; }
li { margin: 3px 0; }
blockquote {
  background: #f1f5f9;
  border-left: 3px solid #2769a6;
  color: #394b5d;
  margin: 11px 0;
  padding: 8px 12px;
}
pre {
  background: #f5f7f9;
  border: 1px solid #d7dde3;
  border-left: 3px solid #2769a6;
  border-radius: 3px;
  font: 8.5pt/1.4 "DejaVu Sans Mono", monospace;
  padding: 8px 10px;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  break-inside: avoid;
}
code {
  background: #eef1f4;
  border-radius: 2px;
  font-family: "DejaVu Sans Mono", monospace;
  font-size: 0.91em;
  padding: 1px 3px;
}
pre code { background: transparent; padding: 0; }
table {
  border-collapse: collapse;
  font-size: 8.8pt;
  margin: 9px 0 13px;
  width: 100%;
}
th, td {
  border: 1px solid #cbd3db;
  padding: 5px 7px;
  text-align: left;
  vertical-align: top;
}
th { background: #e9f0f7; color: #174f86; }
hr { border: 0; border-top: 1px solid #ccd3da; margin: 17px 0; }
strong { color: #17202a; }
"""
    )

    base_url = (
        args.base_url.expanduser().resolve()
        if args.base_url
        else source.parent
    )
    HTML(string=document, base_url=str(base_url)).write_pdf(
        output, stylesheets=[css]
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
