"""Text helpers shared by pollers that send strings to the firmware."""

from unidecode import unidecode


def to_displayable(text: str) -> str:
    """Fold characters the firmware's font can't render down to ASCII.

    The firmware font covers Basic Latin + Latin-1 Supplement (U+0000-U+00FF),
    which includes accented letters like é, ñ, Å, Ä, Ö, ü — those pass through
    untouched. Anything past that (e.g. stylized "small caps" Unicode some
    titles use) gets transliterated to its closest ASCII equivalent instead of
    showing as a missing/wrong glyph.

    Requires unidecode: pip install unidecode
    """
    return "".join(c if ord(c) <= 0xFF else unidecode(c) for c in text)


def fit_utf8(text: str, max_bytes: int) -> str:
    """Trim to at most max_bytes of UTF-8 without splitting a character, so a
    fixed-size firmware buffer never receives half a multi-byte sequence."""
    return text.encode("utf-8")[:max_bytes].decode("utf-8", errors="ignore")
