r"""MCM2:Reborn's options: build_options.py <game ui folder> <out folder> <preview dir>

OptBase is the screen round the pages: SETTINGS, a row of tabs for the pages (and Advanced,
which opens OptAdvG over it), Help, Restore Defaults and Done. Each page is drawn under the
tabs (its Place) as panels of settings, one a row: a switch, a slider, a drop-down list, a row
of choices, or the game's words. The original's labels are drawn in the pictures, and its
boxes and frames go.

Writes OPTBASE, OPTGAME, OPTGRAPH, OPTSOUND, OPTCONT, OPTMESS, OPTGAR and OPTADVG for the game's
ui\modern\, and OptReborn, MCM2:Reborn's own page of its settings (optionprocs.c, reborn_rows), its
tab after the game's.
"""
import sys
from pathlib import Path

import reborn_kit as kit
from reborn_kit import EDGE, GREY, INK, LEFT, ORANGE, PANEL, PAPER, RIGHT, box, cap_height, heading, sub
from reborn_layout import (LABEL, Screen, arrow_states, button_states, colorref, font, on_board, outline,
                           segment_states, shape, slanted, under, xywh)
from reborn_pages import chip, combo, footer, full_info, full_screen, ground, preview, radio, scroll_bar, toggle

ART = 3
TABS_TOP, TABS_H = 274, 64
PLACE = (LEFT, 380, RIGHT, 1264)            # the pages' room, under the tabs
ROW_H, PITCH = 76, 88

# The tabs in order: the control, its words (optionprocs.c's pages are by name).
TABS = [("GameSettings", "GAME"), ("Graphics", "GRAPHICS"), ("Sound", "SOUND"), ("Controls", "CONTROLS"),
        ("Messages", "MESSAGES"), ("Garage", "GARAGE"), ("Reborn", "REBORN"), ("Gamepad", "GAMEPAD")]


def columns(count, area=PLACE, gap=40):
    """`area` cut into `count` columns."""
    w = (area[2] - area[0] - (count - 1) * gap) / count
    return [(area[0] + k * (w + gap), area[1], area[0] + k * (w + gap) + w, area[3]) for k in range(count)]


# ---- A page's rows ----

def panel(board, area, title):
    """A panel and its title; the first row's top."""
    kit.panel(board, area, 230)
    heading(board, title, area[0] + 36, area[1] + 56, 28, ORANGE)
    return area[1] + 80


def rows_area(area, top, k, h=ROW_H):
    return (area[0] + 24, top + k * PITCH, area[2] - 24, top + k * PITCH + h)


def slider(screen, board, name, row, words, split=0.42):
    """A setting on a slider: its name, and the slider over the well's right."""
    chip(board, row, words)
    x0 = row[0] + (row[2] - row[0]) * split
    bar = (x0, row[1] + 16, row[2] - 24, row[3] - 16)
    screen.set(name, Anchor="", **scroll_bar(screen, board, name, bar))


def choice(screen, board, row, words, choices, chosen=0, start=None):
    """A row of choices (radio buttons), as segments across the well's right."""
    chip(board, row, words)
    left = start or row[0] + (row[2] - row[0]) * 0.42
    gap = 6
    w = (row[2] - 12 - left - (len(choices) - 1) * gap) / len(choices)
    for k, (name, label) in enumerate(choices):
        where = (left + k * (w + gap), row[1] + 12, left + k * (w + gap) + w, row[3] - 12)
        radio(screen, name, segment_states(board, where, label), where, screen.origin, chosen=k == chosen, Anchor="")


def words(screen, board, name, row, label, size=28, align="RIGHT", colour=PAPER, split=0.42):
    """A setting the game shows in words: its name, the words at the well's right."""
    chip(board, row, label)
    keys = {"Xywh": xywh((row[0] + (row[2] - row[0]) * split, row[1], row[2] - 22, row[3]), screen.origin),
            "TextLMargin": "0", "TextTMargin": "0", "Anchor": ""}
    keys.update(font(size, colour, align))
    screen.set(name, ("AnimNorm",), **keys)


def edit(screen, board, name, row, label, start=None):
    """An edit box over the well's right."""
    chip(board, row, label)
    area = (start or row[0] + 220, row[1] + 12, row[2] - 12, row[3] - 12)
    box(board, area, (14, 15, 18, 255), 6, EDGE + (255,))
    keys = {"Xywh": xywh(area, screen.origin), "TextLMargin": "16", "TextTMargin": "0", "Anchor": "",
            "AnimNorm": screen.picture("M%s" % name, under(board, area))}
    keys.update(font(28, PAPER))
    screen.set(name, **keys)


def drop(screen, board, name, row, label, list_h=300):
    chip(board, row, label)
    side = 52
    inset = (row[3] - row[1] - side) / 2
    combo(screen, name, board, (row[0] + (row[2] - row[0]) * 0.42, row[1] + inset, row[2] - 12 - side - 1,
                                row[3] - inset), list_h=list_h, origin=screen.origin)
    screen.set(name, Anchor="")


def switch(screen, board, name, row, label):
    toggle(screen, board, name, row, label, screen.origin)
    screen.set(name, Anchor="")


def page(data, stem):
    screen = Screen(data, stem, PLACE[:2])
    return screen, ground()


def finish_page(screen, board, hidden=()):
    """A page's ground, size and Place, the original's labels and boxes gone."""
    for name in hidden:
        screen.hide(name)
    screen.set("Set_Information", BackgroundImage=screen.picture("MBack", under(board, PLACE)),
               ScreenWidth=str(PLACE[2] - PLACE[0]), ScreenHeight=str(PLACE[3] - PLACE[1]), ArtScale=str(ART),
               NoScale="-1", Place=xywh(PLACE), Xywh=xywh(PLACE), FontName='"Helvetica"')
    return screen, board, None


# ---- OptBase ----

def tab_areas():
    x, areas = LEFT, []
    for _, words in TABS + [("Advanced", "ADVANCED")]:
        w = kit.heading_ink(words, 26) + 80
        areas.append((x, TABS_TOP, x + w, TABS_TOP + TABS_H))
        x += w + 16
    return areas


def tab_states(board, area, words):
    """A tab: an outlined ghost, filled orange when it is the page shown."""
    pictures = {}
    for state, on in (("On", True), ("Off", False)):
        canvas = on_board(board, area)
        if on:
            shape(canvas, slanted(area), ORANGE)
        else:
            shape(canvas, slanted(area), PANEL, 220)
            outline(canvas, slanted(area), EDGE, 2)
        heading(canvas, words, (area[0] + area[2]) / 2, (area[1] + area[3]) / 2 + cap_height(26) / 2, 26,
                INK if on else PAPER, anchor="ms")
        pictures[state] = under(canvas, area)
    pictures["OnGrey"], pictures["OffGrey"] = pictures["On"], pictures["Off"]
    return pictures


def build_base(data):
    screen, board = full_screen(data, "OPTBASE", "OPTIONS", "SETTINGS")
    areas = tab_areas()
    for k, ((name, words), area) in enumerate(zip(TABS, areas)):
        # REBORN and GAMEPAD are tabs of the screen's own, in the same group as the game's (AttachId 101).
        added = {} if name in dict(screen.sections) else {"AttachId": "101", "SoundPush": '"CLICK04"'}
        radio(screen, name, tab_states(board, area, words), area, (0, 0), chosen=k == 0, FitToAnimOn="0", **added)
    screen.button("Advanced", areas[-1], button_states(board, areas[-1], "ADVANCED", size=26))
    footer(screen, board, [("Help", "HELP")], [("Back", "DONE", 540, True), ("Restore", "RESTORE DEFAULTS", 560,
                                                                             False)])
    full_info(screen, board)
    return screen, board, None


# ---- The pages ----

def build_game(data):
    screen, board = page(data, "OPTGAME")
    left, right = columns(2)
    top = panel(board, left, "ON SCREEN")
    for k, (name, label) in enumerate((("RaceStatsCheckBox", "RACE STATISTICS"), ("GuagesCheckBox", "GAUGES"),
                                       ("OverviewCheckBox", "OVERVIEW MAP / RADAR"),
                                       ("RiderNamesCheckBox", "RIDER NAMES"),
                                       ("RiderPositionCheckBox", "RIDER POSITION"))):
        switch(screen, board, name, rows_area(left, top, k), label)
    words(screen, board, "MultChatMode", rows_area(left, top, 5), "CHAT BOX (MULTIPLAYER)")
    top = panel(board, right, "GAME")
    for k, (name, label) in enumerate((("ChkPodiums", "END OF RACE ANIMATIONS"), ("ChkUIAnims", "MENU ANIMATIONS"),
                                       ("ChkIntroMovie", "INTRO MOVIE"), ("ChkPlayerIndicator", "PLAYER INDICATOR"),
                                       ("ChkToolTips", "TOOL TIPS"),
                                       ("ChkWreckResetOnTrack", "PRACTICE: RESET ON TRACK"))):
        switch(screen, board, name, rows_area(right, top, k), label)
    # The product's ID, as small print under the panels' rows.
    keys = {"Xywh": xywh((right[0] + 36, right[3] - 56, right[2] - 36, right[3] - 16), screen.origin),
            "TextLMargin": "0", "TextTMargin": "0"}
    keys.update(font(22, GREY, "RIGHT"))
    screen.set("TxtPID", **keys)
    return finish_page(screen, board, ("OverlaysBox", "OverlaysText", "rbwFrame1", "rbwLabel2", "MultiPlayerText",
                                       "MiscSettingsBox", "MiscText"))


def build_graphics(data):
    screen, board = page(data, "OPTGRAPH")
    left, right = columns(2)
    top = panel(board, left, "DISPLAY")
    drop(screen, board, "DisplayResolutionDropDown", rows_area(left, top, 0), "RESOLUTION", list_h=420)
    slider(screen, board, "TerrainQualitySliderBar", rows_area(left, top, 1), "OVERALL QUALITY")
    sub(board, "MORE IN ADVANCED: VISIBILITY, TERRAIN, OBJECTS AND TEXTURES", left[0] + 36, left[3] - 36, 22,
        (120, 122, 128), bold=True)
    top = panel(board, right, "FEATURES")
    for k, (name, label) in enumerate((("ShadowsCheckBox", "SHADOWS"), ("ParticlesCheckBox", "PARTICLE SYSTEMS"),
                                       ("SkyCheckBox", "SKY"))):
        switch(screen, board, name, rows_area(right, top, k), label)
    return finish_page(screen, board, ("DisplayResolutionBox", "DisplayResolutionText", "TerrainQualityBox",
                                       "TerrainQualityText", "rbwLabel6", "rbwLabel7", "GraphicFeaturesBox",
                                       "GraphicFeaturesText"))


# The REBORN page: MCM2:Reborn's settings (optionprocs.c, reborn_rows), a column for each kind.
REBORN_COLUMNS = (
    ("PICTURE", (("MultAntiAlias", "ANTI-ALIASING"), ("MultFiltering", "TEXTURE FILTERING"), ("MultVSync", "VSYNC"),
                 ("MultFxaa", "EDGE SMOOTHING"), ("MultSharpen", "SHARPENING"), ("MultVibrance", "COLOUR VIBRANCE"),
                 ("MultBrightness", "BRIGHTNESS"), ("MultFrameLimit", "FRAME RATE LIMIT"))),
    ("VIEW", (("MultHudStyle", "HUD"), ("MultCamStyle", "FIRST PERSON"), ("MultFpFov", "FIRST-PERSON FOV"),
              ("MultFpTilt", "TILT"), ("MultFpPitch", "PITCH"), ("MultFpCorner", "CORNER ANTICIPATION"),
              ("MultFpLean", "LEAN HEADING SCALE"))),
    ("RIDING", (("MultPhysStyle", "PHYSICS"), ("MultRiders", "RIDERS ON THE GATE"),
                ("MultAiRate", "COMPUTER RIDERS' STEPS"))),
)


# The GAMEPAD page: the XInput pad's settings and each action's control (optionprocs.c, reborn_rows;
# code/dev/pad.c). Actions 0 to 3, steering and lean, are the sticks.
PAD_COLUMNS = (
    ("STICKS", (("MultPadOn", "XINPUT PADS"), ("MultPadResponse", "STEERING RESPONSE"),
                ("MultPadDeadzone", "STICK DEAD ZONE"), ("MultPadLean", "LEAN WITH"),
                ("MultPadDirect", "OTHER PADS' STICKS"))),
    ("RIDING", (("DdlPad4", "ACCELERATE"), ("DdlPad5", "BRAKE"), ("DdlPad12", "CLUTCH"),
                ("DdlPad13", "BACK UP"), ("DdlPad6", "STUNTS 1-8"), ("DdlPad7", "STUNTS 9-16"))),
    ("MORE", (("DdlPad8", "KAHUNA DUMPSTER"), ("DdlPad9", "TAUNTING"), ("DdlPad10", "THRILL CAM"),
              ("DdlPad11", "REVERSE CAM"))),
)


def rows_page(data, stem, columns_rows, note):
    """A page of rows, made from the game settings page, its controls put away."""
    screen, board = page(data, stem)
    screen.layout = stem + ".dtm"
    for name in screen.unplaced():
        screen.hide(name)
    for area, (title, rows) in zip(columns(3), columns_rows):
        top = panel(board, area, title)
        for k, (name, label) in enumerate(rows):
            if name.startswith("Ddl"):              # picked from a list
                screen.add(name, Type="DROPDOWNLIST", SoundPush='"CLICK04"')
                drop(screen, board, name, rows_area(area, top, k), label, list_h=420)
            else:                                   # clicked through
                screen.add(name, Type="MULTISTATE", SoundPush='"Beep"')
                words(screen, board, name, rows_area(area, top, k), label, size=24, split=0.5)
    sub(board, note, PLACE[0] + 36, PLACE[3] - 30, 22, (120, 122, 128), bold=True)
    return finish_page(screen, board)


def build_reborn(data):
    return rows_page(data, "OptReborn", REBORN_COLUMNS,
                     "EACH ROW: CLICK TO CHANGE. REBORN OR CLASSIC: MCM2:REBORN'S OR THE GAME'S OWN.")


def build_pad(data):
    return rows_page(data, "OptPad", PAD_COLUMNS,
                     "THE LEFT STICK STEERS, START PAUSES. CLICK A ROW TO CHANGE IT, OR PICK A CONTROL FROM ITS LIST.")


def build_sound(data):
    screen, board = page(data, "OPTSOUND")
    left, right = columns(2)
    top = panel(board, left, "SOUND")
    for k, (name, label) in enumerate((("UserInterfaceSoundsCheckBox", "MENU SOUNDS"),
                                       ("InGameSoundEffectsCheck", "IN-RACE SOUND EFFECTS"),
                                       ("SoundHardwareCheckBox", "3-D SOUND HARDWARE"), ("ChkEax", "EAX EXTENSIONS"),
                                       ("ChkWaypointChime", "WAYPOINT CHIME"))):
        switch(screen, board, name, rows_area(left, top, k), label)
    top = panel(board, right, "VOLUME")
    slider(screen, board, "UserInterfaceVolumeSlider", rows_area(right, top, 0), "MENUS")
    slider(screen, board, "InGameVolumeSlider", rows_area(right, top, 1), "IN RACE")
    choice(screen, board, rows_area(right, top, 2), "QUALITY", (("Rad8Bit", "8-BIT"), ("Rad16Bit", "16-BIT")), 1)
    return finish_page(screen, board, ("SoundSettingsBox", "SoundSettingsText", "VolumeSettingsBox",
                                       "VolumeSettingsText", "InGameVolumeText", "UserInterfaceVolumeText",
                                       "rbwLabel1"))


def build_controls(data):
    screen, board = page(data, "OPTCONT")
    keys_col, feel, feedback = columns(3)
    top = panel(board, keys_col, "CONTROLS")
    drop(screen, board, "InputDeviceDDL", rows_area(keys_col, top, 0), "PRESET", list_h=360)
    switch(screen, board, "ChkAnalogAxes", rows_area(keys_col, top, 1), "ANALOG AXES")

    # The assignments: what each action is, and its key, side by side; a click remaps it.
    table = (keys_col[0] + 24, top + 2 * PITCH, keys_col[2] - 24, keys_col[3] - 24)
    box(board, table, (12, 12, 14, 235), 8, EDGE + (255,))
    sub(board, "ACTION", table[0] + 24, table[1] + 40, LABEL, GREY, bold=True)
    middle = (table[0] + table[2] - 56) / 2
    sub(board, "KEY  ·  CLICK TO CHANGE", middle + 12, table[1] + 40, LABEL, GREY, bold=True)
    for name, area in (("ActionListBox", (table[0] + 12, table[1] + 60, middle, table[3] - 12)),
                       ("MapKeyListBox", (middle, table[1] + 60, table[2] - 56, table[3] - 12))):
        keys = {"Xywh": xywh(area, screen.origin), "TextLMargin": "12", "TextTMargin": "0",
                "AnimNorm": screen.picture("M%s" % name, under(board, area)), "SelectColor": colorref(ORANGE)}
        keys.update(font(26, PAPER))
        screen.set(name, **keys)
    screen.set("ListboxScroll", **scroll_bar(screen, board, "ListboxScroll",
                                             (table[2] - 52, table[1] + 60, table[2] - 12, table[3] - 12)))

    top = panel(board, feel, "SENSITIVITY")
    for k, (name, label) in enumerate((("SteeringSlider", "STEERING"), ("CrossupSlider", "CROSS-UP"),
                                       ("PitchSlider", "PITCH"), ("PressThrottleSlider", "THROTTLE ON"),
                                       ("PressBrakesSlider", "BRAKES ON"), ("ReleaseThrottleSlider", "THROTTLE OFF"),
                                       ("ReleaseBrakesSlider", "BRAKES OFF"))):
        slider(screen, board, name, rows_area(feel, top, k), label, split=0.45)
    sub(board, "ON: HOW FAST IT PRESSES  ·  OFF: HOW FAST IT LETS GO", feel[0] + 36, feel[3] - 30, 20,
        (120, 122, 128), bold=True)

    top = panel(board, feedback, "FORCE FEEDBACK")
    switch(screen, board, "ForceFeedCheck", rows_area(feedback, top, 0), "FORCE FEEDBACK")
    slider(screen, board, "FeedbackSlider", rows_area(feedback, top, 1), "STRENGTH", split=0.45)
    switch(screen, board, "ChkGasGyro", rows_area(feedback, top, 2), "GAS GYRO EFFECT")
    switch(screen, board, "ChkBrakeGyro", rows_area(feedback, top, 3), "BRAKE GYRO EFFECT")
    return finish_page(screen, board, (
        "AssignmentsBox", "ControlSensitivityBox", "ClickBelowText", "CurrentAssignmentText", "ControlSensitivityText",
        "PressText", "ReleaseSensitivityText", "SteeringText", "CrossUpText", "PitchText", "PressThrottleText",
        "PressBrakesText", "ReleaseThrottleText", "releaseBrakesText", "ReleaseSlowText", "ReleaseFastText", "LessText",
        "MoreText", "WeakerText", "StrongerText", "InputDeviceText"))


def build_messages(data):
    screen, board = page(data, "OPTMESS")
    halves = columns(2)
    for half, keys in zip(halves, ((1, 2, 3, 4, 5), (6, 7, 8, 9, 0))):
        top = panel(board, half, "CHAT MESSAGES" if keys[0] == 1 else "")
        for k, key in enumerate(keys):
            edit(screen, board, "Message%d" % key, rows_area(half, top, k), "CTRL + %d" % key, start=None)
    sub(board, "Send one in a race with Ctrl and its number.", halves[0][0] + 36, halves[0][3] - 30, 24, GREY)
    return finish_page(screen, board, ["Shift%dText" % k for k in range(10)] + ["EditBelowText"])


def build_garage(data):
    """The engine's power curve (eleven sliders, the power at each speed) and the suspension."""
    screen, board = page(data, "OPTGAR")
    curve, suspension = columns(2, gap=40)
    curve = (curve[0], curve[1], curve[0] + 1480, curve[3])
    suspension = (curve[2] + 40, suspension[1], RIGHT, suspension[3])
    top = panel(board, curve, "POWER CURVE")

    picker = rows_area(curve, top, 0)
    chip(board, picker)
    side = 52
    inset = (ROW_H - side) / 2
    left = (picker[0] + 12, picker[1] + inset, picker[0] + 12 + side, picker[1] + inset + side)
    right = (picker[2] - 12 - side, picker[1] + inset, picker[2] - 12, picker[1] + inset + side)
    screen.button("CurveLeft", left, arrow_states(board, left, "left"))
    screen.button("CurveRight", right, arrow_states(board, right, "right"))
    combo(screen, "DDLCurves", board, (left[2] + 8, picker[1] + 8, right[0] - 8, picker[3] - 8), arrow=False,
          list_h=360, size=30, align="CENTER", origin=screen.origin)

    # The bands: the power above each slider, the speed under it; the power left over at the right.
    band_top, band_bottom = picker[3] + 24, curve[3] - 24
    well = (curve[0] + 24, band_top, curve[2] - 24, band_bottom)
    box(board, well, (12, 12, 14, 235), 8, EDGE + (255,))
    sub(board, "HP", well[0] + 24, band_top + 46, LABEL, GREY, bold=True)
    sub(board, "RPM", well[0] + 24, band_bottom - 26, LABEL, GREY, bold=True)
    spare_w = 180
    first = well[0] + 110
    pitch = (well[2] - spare_w - first) / 11
    for k in range(11):
        cx = first + k * pitch + pitch / 2
        value = (cx - 44, band_top + 16, cx + 44, band_top + 64)
        for name, area, size in (("HP%d" % (k + 1), value, 26),
                                 ("Txt%d" % (k + 1), (cx - 48, band_bottom - 56, cx + 48, band_bottom - 12), 22)):
            keys = {"Xywh": xywh(area, screen.origin), "TextLMargin": "0", "TextTMargin": "0"}
            keys.update(font(size, PAPER if name.startswith("HP") else GREY, "CENTER"))
            screen.set(name, ("AnimNorm",), **keys)
        bar = (cx - 22, band_top + 76, cx + 22, band_bottom - 68)
        screen.set("SldEQ%d" % (k + 1), **scroll_bar(screen, board, "SldEQ%d" % (k + 1), bar))
    sx = well[2] - spare_w + 20
    sub(board, "UNUSED", sx, band_top + 46, LABEL, GREY, bold=True)
    box(board, (sx, band_top + 64, well[2] - 20, band_top + 120), (30, 31, 36, 255), 6, EDGE + (255,))
    keys = {"Xywh": xywh((sx, band_top + 64, well[2] - 20, band_top + 120), screen.origin), "TextLMargin": "0",
            "TextTMargin": "0"}
    keys.update(font(30, ORANGE, "CENTER"))
    screen.set("HPUnused", ("AnimNorm",), **keys)

    top = panel(board, suspension, "SUSPENSION")
    for k, (name, label) in enumerate((("SldFrontDamp", "FRONT DAMPING"), ("SldFrontComp", "FRONT COMPRESSED"),
                                       ("SldFrontDecomp", "FRONT DECOMPRESSED"), ("SldRearDamp", "REAR DAMPING"),
                                       ("SldRearComp", "REAR COMPRESSED"), ("SldRearDecomp", "REAR DECOMPRESSED"))):
        slider(screen, board, name, rows_area(suspension, top, k), label, split=0.5)
    sub(board, "LEFT WEAKER  ·  RIGHT STRONGER", suspension[0] + 36, suspension[3] - 30, 20, (120, 122, 128),
        bold=True)
    return finish_page(screen, board, ["rbwLabel%d" % n for n in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13)]
                       + ["rbwLabelxx", "TxtUnused"])


# ---- OptAdvG, over OptBase ----

def build_advanced(data):
    screen, board = full_screen(data, "OPTADVG", "OPTIONS", "ADVANCED")
    detail, card, repair = columns(3, (LEFT, 274, RIGHT, 1264))
    top = panel(board, detail, "DETAIL")
    for k, (name, label) in enumerate((("SldVisibility", "VISIBILITY"), ("SldTerrain", "TERRAIN"),
                                       ("SldObjects", "OBJECTS"), ("SldEcosystem", "ECOSYSTEM"))):
        slider(screen, board, name, rows_area(detail, top, k), label)
    choice(screen, board, rows_area(detail, top, 4), "TERRAIN PASSES",
           (("RadTerrSingle", "SINGLE"), ("RadTerrDual", "DUAL"), ("RadTerrNone", "NONE")), 1)

    top = panel(board, card, "3-D RENDERER")
    words(screen, board, "Txt3DCard", rows_area(card, top, 0), "RENDERER", size=24)
    switch(screen, board, "ChkShowPick3D", rows_area(card, top, 1), "ASK AT START")
    switch(screen, board, "ChkForceVideoMem", rows_area(card, top, 2), "FIT IN VIDEO MEMORY")

    top = panel(board, repair, "TROUBLESHOOTING")
    switch(screen, board, "ChkFilterInput", rows_area(repair, top, 0), "FILTER JOYSTICK INPUT")
    switch(screen, board, "ChkShowPickController", rows_area(repair, top, 1), "ASK CONTROLLER AT START")

    for name in ("UnusedGraphicsSlideBox", "UnusedSlide03Text", "Less03", "More03", "rbwLabel1", "rbwLabel2",
                 "rbwLabel3", "rbwLabel4", "rbwLabel5", "rbwLabel6", "rbwLabel7", "rbwLabel8", "rbwLabel9",
                 "rbwLabel10", "rbwLabel11", "rbwLabel12", "CardSelectionBox", "StartupCardSelectionText",
                 "AdvancedGraphicsTitle"):
        screen.hide(name)
    footer(screen, board, [("Help", "HELP")], [("Back", "DONE", 540, True)])
    full_info(screen, board)
    return screen, board, None


def main(ui, out, previews):
    ui, out, previews = Path(ui), Path(out), Path(previews)
    kit.fonts_from(ui / "modern" / "fonts")
    read = lambda stem: (ui / (stem + ".DAT")).read_bytes()             # noqa: E731
    base = build_base(read("OPTBASE"))
    pages = [build_game(read("OPTGAME")), build_graphics(read("OPTGRAPH")), build_sound(read("OPTSOUND")),
             build_controls(read("OPTCONT")), build_messages(read("OPTMESS")), build_garage(read("OPTGAR")),
             build_reborn(read("OPTGAME")), build_pad(read("OPTGAME"))]
    advanced = build_advanced(read("OPTADVG"))
    out.mkdir(parents=True, exist_ok=True)
    previews.mkdir(parents=True, exist_ok=True)
    for screen, _, _ in [base] + pages + [advanced]:
        data = screen.pack()
        (out / (screen.stem + ".DAT")).write_bytes(data)
        print("%-8s %3d pictures, %d bytes" % (screen.stem, len(screen.pictures), len(data)))
    photo = sorted(previews.parent.glob("menu photo *.jpg"))[0]
    for page_ in pages:
        preview(photo, [base, page_], PLACE).save(previews / ("reborn %s.png" % page_[0].stem.lower()))
    preview(photo, [advanced]).save(previews / "reborn optadvg.png")


if __name__ == "__main__":
    main(*sys.argv[1:4])
