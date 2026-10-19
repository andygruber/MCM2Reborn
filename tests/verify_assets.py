"""Validate built menu layouts and rider patch bytes against the retail image."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/modern"))
import disasm
import port
from build_mainmenu import rs2_entries, read_dtm


def layout(stem):
    entries = rs2_entries((ROOT / "build/modern" / (stem + ".DAT")).read_bytes())
    text = next(data.decode("latin-1") for name, data in entries.items() if name.endswith(".dtm"))
    return {name: dict(line.split("=", 1) for line in lines if "=" in line)
            for name, lines in read_dtm(text)}


def keys(stem, name):
    return {key.strip().lower(): value.strip() for key, value in layout(stem)[name].items()}


def main():
    checks = 0
    mainmenu = [keys("MAINMENU", name) for name in ("MenuSingle", "MenuMulti", "ButProCircuit")]
    places = [tuple(int(part.strip()) for part in item["xywh"].split(",")) for item in mainmenu]
    assert places[0][1] < places[1][1] < places[2][1]
    assert len({place[0] for place in places}) == 1
    assert [item["tabindex"] for item in mainmenu] == ["1", "2", "3"]
    checks += 3
    track = keys("SPEVENT", "TrackList")
    assert track["textalign"] == "LEFT" and track["textlmargin"] == "24"
    checks += 1
    for stem in ("SPEVENT", "MPEVENT"):
        filters = [keys(stem, name) for name in ("TrackAll", "TrackBase", "TrackCustom")]
        assert all(item["type"] == "RADIOBUTTON" and item["attachid"] == "152" for item in filters)
        checks += 1
    for name in ("InputDeviceDDL", "ChkAnalogAxes", "ForceFeedCheck", "FeedbackSlider", "ChkGasGyro", "ChkBrakeGyro"):
        assert keys("OPTCONT", name)["xywh"] == "0, 0, 4, 4"
        checks += 1
    for name in ("ChkFilterInput", "ChkShowPickController"):
        assert keys("OPTADVG", name)["xywh"] == "0, 0, 4, 4"
        checks += 1
    assert "MultPadDirect" not in layout("OptPad")
    checks += 1
    assert port.ports_in_source()[0x00487100] == "gui_manager_capture_screen"
    checks += 1
    image, sections = disasm.load_image()
    source = (ROOT / "code/dev/riders.c").read_text(encoding="utf-8")
    sites = re.findall(r'\{\s*(0x[\dA-Fa-f]+),\s*\{([^}]+)\},\s*\{([^}]+)\},\s*(\d+)', source)
    assert sites
    for address, old, new, length in sites:
        old_bytes = bytes(int(value.strip(), 0) for value in old.split(","))
        assert len(old_bytes) == int(length)
        assert disasm.read(image, sections, int(address, 16), int(length)) == old_bytes, address
        checks += 1
    for index in range(11):
        assert 0x208 + 4 * index + 0xD0 == 0x2D8 + 4 * index
        checks += 1
    print(f"PASS {checks} asset/patch checks: menu order, left alignment, source filters, retired settings, capture binding and original rider patch bytes")


if __name__ == "__main__":
    main()
