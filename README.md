# MCM2:Reborn

**Motocross Madness 2** (Rainbow Studios, 2000), rebuilt to run and play well on today's PCs. It has
modern menus, a new race HUD, an OpenGL renderer, online play for 30 riders, lap leaderboards and
proper gamepad support. Every change can be switched back to the original's behaviour in Options.

## Installing

1. Download **MCM2Reborn-Launcher.exe** from the [latest release](https://github.com/xshxdex98/MCM2Reborn/releases/latest).
2. Run it. It looks for a copy of the game on your PC: a retail install, a portable copy, or the
   CD. If you don't have one, pick **DOWNLOAD MOTOCROSS MADNESS 2** and it fetches the game.
3. Type your rider's name and press **INSTALL**. MCM2:Reborn goes in its own folder, with desktop
   and Start menu shortcuts.
4. A short setup sets the picture quality, vsync, your controller and the look. Then press **PLAY**.

The launcher keeps MCM2:Reborn up to date by itself and shows how many riders are online. Your
settings are kept across updates.

**Linux / Steam Deck:** run the launcher and the game through **GE-Proton** (the latest), which has
what the game needs; stock Proton does not.

## What's new

**Racing**
- Up to 30 riders on the gate, in single races and online.
- The bike's physics behave the same at any frame rate, and computer riders move smoothly.
- The Pro Circuit's rider gear is in every event.
- Add as many of your own skins as you like: every `.tga` in `RIDERS\` and `BIKES\` is its own choice.

**Online**
- Lobbies through Epic Online Services, up to 30 riders each, found from the game's Internet screen.
- If you're missing the host's track or another rider's skins, the lobby offers to download them.
- Each rider's ping and a ready count show in the lobby. If the host's game drops, the session ends cleanly.

**Leaderboards**
- A board for every track, event (Baja, Nationals, Supercross, Enduro) and bike class.
- Your best lap from the second lap on counts, with one entry per rider. It changes only when you beat it.
- Open them from the main menu or the multiplayer panel. Laps are kept on your PC, and on a
  leaderboard server when one is set (`tools/leaderboard_server.py`).

**Picture**
- Modern menus and HUD, widescreen at your desktop's resolution.
- Anti-aliasing, texture filtering, vsync and a frame-rate limit.
- Edge smoothing (FXAA), sharpening, colour vibrance, brightness and bloom.

**Controls**
- XInput gamepads steer by their sticks, with response, sensitivity, smoothing and dead-zone settings.
- Every action can be bound to any button or trigger: click it, then press the button.
- Alt-tabbing no longer flips the camera or locks the controls.

## Settings

Everything is in the game's **Options**, under REBORN and GAMEPAD. The same settings are kept in
`MCM2Reborn.ini` beside the game. The launcher's **SETUP** button changes
the main ones in one go.

## Building it

MCM2:Reborn is built from the retail executable. The game is disassembled, reassembled and
relinked by its own toolchain, and new code is linked in beside it. So the game runs from the
first build, and any function can be replaced by C and checked against the original.

```sh
sh tools/ghidra/reimport.sh    # build the Ghidra database from Mcm2.exe (once)
sh tools/ghidra/cycle.sh       # export -> port -> assemble -> compile -> link -> verify
sh tools/deploy_dev.sh         # copy the build into a game folder
py tools/build_launcher.py     # the launcher
py tools/release.py            # the release package, build/release/
py tools/base_package.py       # the base game's package, from a clean copy
```

- **Unchanged sections:** the rebuilt game's `.text`, `.rdata` and `.data` match retail byte for
  byte, so every pointer in its data stays right.
- **Our code:** it lives in a section of its own after the game's. The link refuses any build in
  which one of the game's sections moved or grew.
- **Ports:** a function marked `PORT(address)` replaces the game's own. `tools/verify.py` accepts no
  difference from retail other than the calls redirected to it.
- **Rendering:** the game gets DirectDraw 7 and Direct3D 7 objects written on OpenGL, so it needs no
  wrapper.
- **The dev build:** MCM2:Reborn's own features are built under `OPENMCM2_DEV`.

The Ghidra workspace lives at `C:\mcm2ghidra\`, outside OneDrive and with no spaces in its path.
OneDrive corrupts Ghidra's project files, and `analyzeHeadless.bat` splits its arguments on spaces.

| | |
|---|---|
| `code/mcm2/` | what is linked into the game: start-up, the renderer, the menus' code and the ported functions |
| `code/dev/` | MCM2:Reborn's features: HUD, gamepads, leaderboards, cameras, online, settings |
| `code/eos/` | the Epic Online Services helper (64-bit, beside the game) |
| `launcher/` | the launcher (C#, .NET Framework 4.8) |
| `tools/` | the build, the release packages, and the checks against retail |
| `tools/modern/` | the modern menus' layouts and art |
| `docs/` | measured facts about the game: start with [`binary.md`](docs/binary.md) and [`harness.md`](docs/harness.md) |
