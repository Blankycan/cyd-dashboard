# CYD Dashboard — Hotaru color palette (Python side)
# Keep in sync with src/theme.h — edit PAL values there first, then mirror here.
# Used by companion scripts for any terminal/log coloring or future GUI elements.

PAL = {
    # Character "Hotaru"
    "cap":        "#E07A5F",  # warm terracotta  — mushroom cap
    "cap_shine":  "#F2CC8F",  # sun amber        — cap highlight
    "spots":      "#FEFAE0",  # warm cream       — cap spots
    "face":       "#EDF6F9",  # soft off-white   — face/body
    "eyes":       "#264653",  # deep teal        — eyes
    "stem":       "#81B29A",  # sage green       — stem/body
    "glow":       "#E9C46A",  # warm gold        — glow/sparks

    # UI
    "bg":         "#0F1F1A",  # very dark forest — main background
    "panel":      "#162820",  # dark forest+     — card/panel bg
    "border":     "#264653",  # deep teal        — panel borders
    "divider":    "#1E3A30",  # muted forest     — dividers

    # Text
    "text_pri":   "#FEFAE0",  # warm cream       — primary labels
    "text_sec":   "#81B29A",  # sage green       — secondary text
    "text_dim":   "#4A7B6F",  # muted sage       — inactive/timestamps

    # Status
    "ok":         "#81B29A",  # sage green       — normal / idle
    "warn":       "#F2CC8F",  # amber            — >70% usage
    "alert":      "#E07A5F",  # terracotta       — >90% usage / disconnect
    "bar_track":  "#1E3A30",  # dark             — progress bar track
}


def hex_to_rgb(h: str) -> tuple[int, int, int]:
    h = h.lstrip("#")
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def ansi(color_name: str, text: str) -> str:
    """Wrap text in ANSI terminal color using the palette."""
    r, g, b = hex_to_rgb(PAL[color_name])
    return f"\033[38;2;{r};{g};{b}m{text}\033[0m"
