"""What the menus' screens are made of: survey.py <game ui folder>

For each dialog archive: its layout's size, background, and its controls by type, with how many
draw pictures and how many draw text.
"""
import collections
import sys
from pathlib import Path

from build_mainmenu import read_dtm, rs2_entries


def value(lines, key):
    for line in lines:
        if "=" in line and line.split("=", 1)[0].strip().lower() == key.lower():
            return line.split("=", 1)[1].split("'")[0].strip().strip('"')
    return None


def survey(folder):
    totals = collections.Counter()
    for dat in sorted(Path(folder).glob("*.DAT")):
        entries = rs2_entries(dat.read_bytes())
        layouts = [name for name in entries if name.endswith(".dtm")]
        if not layouts:
            print("%-13s (no layout: %d entries)" % (dat.name, len(entries)))
            continue
        for layout in layouts:
            sections = read_dtm(entries[layout].decode("latin-1"))
            info = dict(sections).get("Set_Information", [])
            kinds = collections.Counter()
            for name, lines in sections:
                kind = value(lines, "Type")
                if kind and name not in ("Set_Information",):
                    pictured = any((value(lines, k) or "0") not in ("0", "") for k in ("AnimNorm", "AnimThumb"))
                    texted = bool(value(lines, "Text") or value(lines, "LocalTextId"))
                    kinds["%s%s%s" % (kind, "+pic" if pictured else "", "+text" if texted else "")] += 1
            totals.update(kinds)
            print("%-13s %-14s %sx%s bg=%-16s noscale=%s popup=%s  %s" % (
                dat.name, layout, value(info, "ScreenWidth"), value(info, "ScreenHeight"),
                value(info, "BackgroundImage"), value(info, "NoScale"), value(info, "PopUp"),
                ", ".join("%s %d" % kv for kv in sorted(kinds.items()))))
    print()
    for kind, count in totals.most_common():
        print("%5d %s" % (count, kind))


if __name__ == "__main__":
    survey(sys.argv[1])
