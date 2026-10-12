/*
    OpenMCM2 - the game's settings, kept in mcm2_profile.json beside it.

    The game keeps its settings, profiles and EULA acceptance under HKLM\Software\Microsoft\
    Microsoft Games\Motocross Madness 2 (virtualised to the user's VirtualStore). The portable
    copy's launcher carries them in mcm2_profile.json and loads them into the registry before the
    game starts; this does the same, and writes them back when the game exits.

    The file holds one object per key, named by its path below the game's key ("" for the key
    itself), and each value as [type, "hex bytes"].
*/

#pragma once

/* Load mcm2_profile.json into the registry, and save it back when the game calls ExitProcess. */
void profile_install(void);
