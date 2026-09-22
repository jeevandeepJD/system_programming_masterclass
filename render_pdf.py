#!/usr/bin/env python3
"""Render MASTERCLASS.md into a nicely styled PDF."""
import re
from pathlib import Path

import markdown
from weasyprint import HTML, CSS

ROOT = Path(__file__).resolve().parent
SRC = ROOT / "MASTERCLASS.md"
OUT = ROOT / "MASTERCLASS.pdf"

with open(SRC, "r", encoding="utf-8") as f:
    md_text = f.read()


def ensure_blank_line_before_lists(text: str) -> str:
    """python-markdown (unlike GFM/CommonMark) requires a blank line
    before a list can start, otherwise '- foo' lines glued directly to a
    preceding paragraph line get swallowed as plain text. Insert the
    missing blank line automatically."""
    list_re = re.compile(r'^(\s*)([-*+]|\d+\.)\s+')
    lines = text.split("\n")
    out = []
    for line in lines:
        if list_re.match(line):
            prev = out[-1] if out else ""
            prev_is_list = bool(list_re.match(prev))
            if prev.strip() != "" and not prev_is_list:
                out.append("")
        out.append(line)
    return "\n".join(out)


md_text = ensure_blank_line_before_lists(md_text)

html_body = markdown.markdown(
    md_text,
    extensions=["extra", "toc", "codehilite", "sane_lists", "fenced_code"],
    extension_configs={"toc": {"title": "Table of Contents"}},
)

html_doc = f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Systems Engineering Masterclass</title>
</head>
<body>
<div class="titlepage">
  <h1>Systems Engineering Masterclass</h1>
  <p class="subtitle">From First Principles to AI Infrastructure</p>
  <p class="meta">Personal handoff / continuation document</p>
</div>
<div class="content">
{html_body}
</div>
</body>
</html>
"""

css = CSS(string="""
@page {
    size: A4;
    margin: 2cm 1.8cm 2.2cm 1.8cm;
    @bottom-center {
        content: "Page " counter(page) " of " counter(pages);
        font-size: 9px;
        color: #888;
    }
}
body {
    font-family: "DejaVu Sans", sans-serif;
    font-size: 10.5px;
    line-height: 1.5;
    color: #1a1a1a;
}
.titlepage {
    text-align: center;
    margin-top: 6cm;
    page-break-after: always;
}
.titlepage h1 {
    font-size: 30px;
    margin-bottom: 0.3em;
    color: #0b3d91;
}
.titlepage .subtitle {
    font-size: 16px;
    color: #444;
    margin-bottom: 2em;
}
.titlepage .meta {
    font-size: 11px;
    color: #888;
}
h1 {
    font-size: 18px;
    color: #0b3d91;
    border-bottom: 2px solid #0b3d91;
    padding-bottom: 4px;
    margin-top: 1.6em;
    page-break-before: avoid;
    page-break-after: avoid;
}
h2 {
    font-size: 15px;
    color: #10508c;
    margin-top: 1.2em;
    border-bottom: 1px solid #ccc;
    padding-bottom: 2px;
    page-break-after: avoid;
}
h3 {
    font-size: 12.5px;
    color: #333;
    margin-top: 1em;
    page-break-after: avoid;
}
h4 {
    font-size: 11px;
    color: #333;
    margin-top: 0.8em;
    page-break-after: avoid;
}
p, li { font-size: 10.5px; orphans: 3; widows: 3; }
code {
    font-family: "DejaVu Sans Mono", monospace;
    background: #f2f2f2;
    padding: 1px 4px;
    border-radius: 3px;
    font-size: 9.5px;
}
pre {
    background: #f5f5f5;
    border: 1px solid #ddd;
    border-left: 3px solid #0b3d91;
    border-radius: 4px;
    padding: 8px 10px;
    font-size: 9px;
    line-height: 1.4;
    white-space: pre-wrap;
    word-wrap: break-word;
    page-break-inside: avoid;
}
pre code { background: none; padding: 0; }
blockquote {
    border-left: 3px solid #0b3d91;
    margin: 0.6em 0;
    padding: 2px 12px;
    color: #333;
    background: #f7f9fc;
    font-style: italic;
}
table {
    border-collapse: collapse;
    width: 100%;
    font-size: 9.5px;
    margin: 0.6em 0;
}
th, td {
    border: 1px solid #ccc;
    padding: 4px 6px;
    text-align: left;
}
th { background: #eef2f8; }
hr {
    border: none;
    border-top: 1px solid #ccc;
    margin: 1.2em 0;
}
ul, ol { margin: 0.3em 0; padding-left: 1.4em; }
li { margin: 0.15em 0; }
a { color: #0b3d91; }
.toc {
    page-break-after: always;
}
.toc ul { list-style: none; padding-left: 1em; }
.toc > ul { padding-left: 0; }
.toc a { text-decoration: none; color: #10508c; }
""")

HTML(string=html_doc, base_url=str(ROOT)).write_pdf(OUT, stylesheets=[css])
print(f"wrote {OUT}")
