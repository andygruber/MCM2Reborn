# MCM2:Reborn 0.3.7

## Changes

- Reset the fixed-step AI cache before loading each event's bikes. Old bike pointers no longer fill its 32 entries across races, causing colliding new riders to lose their accumulated simulation time. This applies to races and Stunt/Freestyle.
- Fix Classic HUD radar markers for expanded fields: correct the relative marker lookup after moving the bike list, and enforce the eleven-marker limit at the shared update routine, including initialization.
- Continue into the main menu when its preview movie/decoder is unavailable. Pausing and stopping a missing video are safe.
- Correct the screen-capture `PORT` binding at `0x00487100`; its marker previously attached to an intervening renderer declaration with a different calling convention.
- Show Single Player, Multiplayer, Pro Circuit in that order. Opening Multiplayer no longer enumerates legacy network providers; LAN initializes when selected.
- Remove generated track numbers and retain one left margin. Add All / Base Game / Custom filters to single-player and host track pickers for every event. Empty filters cannot start a race or index a nonexistent track, including ghost selection.
- Replace the legacy controller picker and joystick reads with the modern controller path. The Controls page is now keyboard-only; controller shortcuts open Gamepad. Remove legacy presets, analog-axis, force-feedback and startup-picker options from the modern layouts.
- Poll Xbox/XInput and native Sony DualShock 4/DualSense controllers independently of the game's startup joystick list. Detect connections/reconnections in menus and during races.
- Drive menu focus, buttons, lists, dropdowns and sliders directly from sticks/D-pad and A/Cross. Preserve controller focus until the mouse moves; skip parked/hidden controls and respect modal dialogs and binding capture. B/Circle goes back, closing a focused dropdown first; Start/Options pauses. Gameplay's B/Circle stunt binding does not trigger Back.

## Supplied logs

- `openmcm2-dev (1).log`, 0.3.6: the startup fault at `006B7F21` matches dereferencing the main menu movie's null stream after opening failed.
- `openmcm2-dev (2).log`, 0.3.6: the fault at `0051E72A`, writing `0000015C`, is the radar marker position setter called with a null marker. The stack and rider index match the expanded-list marker lookup/update issues.
- `openmcm2-dev (3).log`, 0.3.5: the invalid execution address `0000001A` occurs on the buffered DirectInput path. The log does not prove the original corruption source. Legacy joystick reads now return neutral state before entering that driver path; this is not proof that every possible input-driver crash is resolved.
- The logs do not record enough AI simulation state to prove the exact cause of each stationary rider report. The retained-cache defect was independently identified in code and reproduced with the timing-cache regression fixture.

## Automated validation

On Windows with MSVC and Python:

```sh
python tests/run_regressions.py
python tests/verify_assets.py
python tools/verify.py
python tools/check_calls.py
python tools/check_ports.py
```

`verify_assets.py` requires rebuilt menu DATs in `build/modern/` and the retail reference used by the existing tools. The native fixtures execute the production AI, pad, Sony conversion and radar code; the menu/video helpers are extracted verbatim from their production sources. Hardware and surrounding game objects are simulated.

The native suite covers twenty consecutive 30-rider event loads at 144 FPS, every AI taking simulation steps; radar counts 0 through 32 and restoration of the real count; missing/valid preview video; controller connection, disconnection and reconnection; Circle/Cross, triggers and diagonal D-pad conversion; focus/activation, list and slider changes, dropdowns, modal/frozen parents, binding capture and mouse handoff; and legacy-driver bypass plus Back/Start behavior. Asset checks cover actual packed menu order, alignment, filters, removed options, capture binding and retail patch-site bytes.

The build must preserve the retail `.text`, `.rdata` and `.data` apart from references redirected to ports. Calling-convention checks must report zero wrong declarations and zero mismatched ports. Existing tail-call-only ports are reported for inspection by `check_ports.py` rather than treated as failures.

## Runtime validation still needed

These checks do not run full gameplay or physical controllers. Confirm extended 30-rider racing and Stunt/Freestyle, Classic HUD at the countdown, startup on the affected AMD PC, and Xbox/PlayStation USB/Bluetooth reconnection and every menu on the affected systems. Network session behavior also needs a live host/client check. Do not interpret the automated fixtures as hardware or gameplay certification.
