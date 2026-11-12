"""Shared helpers for the NTC thermistor figures: read function bodies from the repository.

Every helper raises when the code no longer looks the way a figure expects, so a figure fails to build instead of
drawing something the code does not do.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CONVERSION = "inc/ntc_thermistor_conversion.hpp"
IMPL = "src/ntc_thermistor.ipp"


def read(rel: str) -> str:
    return (ROOT / rel).read_text()


def need(pattern: str, text: str, what: str, flags: int = re.S) -> re.Match:
    m = re.search(pattern, text, flags)
    if not m:
        raise SystemExit(f"figure source drift: {what} not found (pattern {pattern!r})")
    return m


def body(text: str, signature: str) -> str:
    """The body of the function whose definition contains ``signature``, braces matched."""
    start = need(re.escape(signature) + r"[^{;]*\{", text, signature).end()
    depth, i = 1, start
    while depth:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    return text[start : i - 1]
