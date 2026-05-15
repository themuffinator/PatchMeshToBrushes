#!/usr/bin/env python3
"""Render README.md as a standalone, styled HTML document for releases."""

from __future__ import annotations

import argparse
import html
import pathlib
import re
import sys


def render_markdown(markdown_text: str) -> str:
    try:
        import markdown  # type: ignore
    except ImportError:
        return fallback_render(markdown_text)

    return markdown.markdown(
        markdown_text,
        extensions=["extra", "sane_lists", "toc"],
        output_format="html5",
    )


def fallback_render(markdown_text: str) -> str:
    """Small fallback renderer for local use when Python-Markdown is absent."""

    lines = markdown_text.splitlines()
    output: list[str] = []
    in_code = False
    in_list = False

    for raw_line in lines:
        line = raw_line.rstrip()
        if line.startswith("```"):
            if in_list:
                output.append("</ul>")
                in_list = False
            output.append("</code></pre>" if in_code else "<pre><code>")
            in_code = not in_code
            continue

        if in_code:
            output.append(html.escape(line))
            continue

        if line.startswith("#"):
            if in_list:
                output.append("</ul>")
                in_list = False
            level = min(len(line) - len(line.lstrip("#")), 6)
            text = line[level:].strip()
            output.append(f"<h{level}>{inline_markdown(text)}</h{level}>")
            continue

        if line.startswith("- "):
            if not in_list:
                output.append("<ul>")
                in_list = True
            output.append(f"<li>{inline_markdown(line[2:].strip())}</li>")
            continue

        if not line:
            if in_list:
                output.append("</ul>")
                in_list = False
            continue

        if in_list:
            output.append("</ul>")
            in_list = False
        output.append(f"<p>{inline_markdown(line)}</p>")

    if in_list:
        output.append("</ul>")
    if in_code:
        output.append("</code></pre>")
    return "\n".join(output)


def inline_markdown(text: str) -> str:
    escaped = html.escape(text)
    escaped = re.sub(r"`([^`]+)`", r"<code>\1</code>", escaped)
    escaped = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', escaped)
    return escaped


def standalone_html(title: str, body: str) -> str:
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{html.escape(title)}</title>
  <style>
    :root {{
      color-scheme: light dark;
      --bg: #f7f5f0;
      --text: #1f2528;
      --muted: #596268;
      --panel: #ffffff;
      --border: #d9d4c8;
      --accent: #276c68;
      --code-bg: #eef1ef;
    }}
    @media (prefers-color-scheme: dark) {{
      :root {{
        --bg: #141719;
        --text: #ece7dc;
        --muted: #a7b0ad;
        --panel: #1c2022;
        --border: #343a3d;
        --accent: #7ecdc5;
        --code-bg: #252b2d;
      }}
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font: 16px/1.6 system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI",
        sans-serif;
    }}
    main {{
      width: min(980px, calc(100% - 32px));
      margin: 40px auto;
      padding: 40px;
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 8px;
    }}
    h1, h2, h3 {{ line-height: 1.2; margin-top: 1.8em; }}
    h1 {{ margin-top: 0; font-size: clamp(2rem, 5vw, 3.5rem); }}
    h2 {{ border-top: 1px solid var(--border); padding-top: 1.2em; }}
    a {{ color: var(--accent); }}
    code {{
      padding: 0.15em 0.35em;
      background: var(--code-bg);
      border-radius: 4px;
      font-family: ui-monospace, SFMono-Regular, Consolas, monospace;
    }}
    pre {{
      overflow-x: auto;
      padding: 16px;
      background: var(--code-bg);
      border-radius: 8px;
      border: 1px solid var(--border);
    }}
    pre code {{ padding: 0; background: transparent; }}
    table {{
      width: 100%;
      border-collapse: collapse;
      margin: 1.5em 0;
    }}
    th, td {{
      padding: 10px 12px;
      border: 1px solid var(--border);
      text-align: left;
      vertical-align: top;
    }}
    blockquote {{
      margin-left: 0;
      padding-left: 1em;
      color: var(--muted);
      border-left: 4px solid var(--border);
    }}
  </style>
</head>
<body>
  <main>
{body}
  </main>
</body>
</html>
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()

    markdown_text = args.input.read_text(encoding="utf-8")
    body = render_markdown(markdown_text)
    title = "PatchMeshToBrushes"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(standalone_html(title, body), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
