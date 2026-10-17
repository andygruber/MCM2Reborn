/*
    OpenMCM2 - the game's settings, kept in mcm2_profile.json beside it.

    The game keeps its settings, profiles and EULA acceptance under HKLM\Software\Microsoft\
    Microsoft Games\Motocross Madness 2 (virtualised to the user's VirtualStore). The portable
    copy's launcher carries them in mcm2_profile.json and loads them into the registry before the
    game starts; this does the same, and writes them back when the game exits.

    The file holds one object per key, named by its path below the game's key ("" for the key
    itself), and each value as [type, "hex bytes"].

    The game's card profile (DriverInfo) of OpenMCM2's renderer is kept apart, in
    openmcm2_drivers.json the same way, and the original game's left in mcm2_profile.json and the
    registry as it was: shared, each game's run overwrote the other's, and the original game,
    given OpenMCM2's, crashed as it started.
*/

#pragma once

/* Load mcm2_profile.json into the registry, and save it back when the game calls ExitProcess. */
void profile_install(void);
