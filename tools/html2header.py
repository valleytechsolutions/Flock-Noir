#!/usr/bin/env python3
"""
html2header.py - regenerate firmware/FlockNoir/web_ui.h from web/index.html.

web/index.html is the single source of truth for the Flock Noir web UI. The
Raspberry Pi target serves it directly; the XIAO ESP32-S3 target embeds it in
flash as a C raw-string literal. Run this after editing web/index.html:

    python tools/html2header.py
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "web", "index.html")
DST = os.path.join(ROOT, "firmware", "FlockNoir", "web_ui.h")

HEADER = """\
// =============================================================================
//  Flock Noir  -  web_ui.h   (GENERATED from web/index.html - do not hand edit)
//  Single-page UI. Tabs: ALPR / Scanner / Camera / Wardrive / Settings.
//  Polls /api/status ~2x/sec. Logo served UNALTERED at /logo.png.
//  Regenerate with:  python tools/html2header.py
// =============================================================================
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(
"""

FOOTER = """)HTMLPAGE";
"""


def main():
    html = open(SRC, encoding="utf-8").read()
    if ')HTMLPAGE"' in html:
        raise SystemExit("index.html must not contain the raw-string delimiter )HTMLPAGE\"")
    with open(DST, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER)
        f.write(html if html.endswith("\n") else html + "\n")
        f.write(FOOTER)
    print("Wrote", DST, "(%d bytes of HTML)" % len(html))


if __name__ == "__main__":
    main()
