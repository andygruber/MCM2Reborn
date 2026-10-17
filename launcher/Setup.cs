// The first-run setup: a few choices, each a set of the game's own settings, written into the ini
// the game reads beside its exe (the exe's name with .ini, as code/dev/dev.c finds it). Everything
// here is also in the game's Options, row by row; this is the quick way in.

using System.IO;
using System.Runtime.InteropServices;

namespace Mcm2Reborn.Launcher
{
    sealed class Preset
    {
        public string Name;
        public (string Section, string Key, int Value)[] Values;
    }

    sealed class SetupChoice
    {
        public string Title;
        public Preset[] Presets;
    }

    static class Setup
    {
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        static extern int GetPrivateProfileInt(string section, string key, int fallback, string file);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        static extern bool WritePrivateProfileString(string section, string key, string value, string file);

        static Preset P(string name, params (string, string, int)[] values) => new Preset { Name = name, Values = values };

        public static readonly SetupChoice[] Choices = {
            new SetupChoice { Title = "PICTURE", Presets = new[] {
                P("CLASSIC", ("graphics", "antialiasing", 0), ("graphics", "filtering", 0), ("graphics", "fxaa", 0),
                  ("graphics", "sharpen", 0), ("graphics", "vibrance", 0), ("graphics", "bloom", 0)),
                P("PERFORMANCE", ("graphics", "antialiasing", 0), ("graphics", "filtering", 1), ("graphics", "fxaa", 1),
                  ("graphics", "sharpen", 25), ("graphics", "vibrance", 0), ("graphics", "bloom", 0)),
                P("BALANCED", ("graphics", "antialiasing", 4), ("graphics", "filtering", 4), ("graphics", "fxaa", 0),
                  ("graphics", "sharpen", 25), ("graphics", "vibrance", 25), ("graphics", "bloom", 25)),
                P("QUALITY", ("graphics", "antialiasing", 8), ("graphics", "filtering", 4), ("graphics", "fxaa", 0),
                  ("graphics", "sharpen", 50), ("graphics", "vibrance", 25), ("graphics", "bloom", 50)) } },
            new SetupChoice { Title = "VSYNC", Presets = new[] {
                P("ON", ("graphics", "vsync", 1)),
                P("OFF", ("graphics", "vsync", 0)) } },
            new SetupChoice { Title = "CONTROLLER", Presets = new[] {
                P("GAMEPAD", ("gamepad", "xinput", 1)),
                P("KEYBOARD ONLY", ("gamepad", "xinput", 0)) } },
            new SetupChoice { Title = "LOOK", Presets = new[] {
                P("REBORN", ("reborn", "hud", 1), ("reborn", "cameras", 1)),
                P("CLASSIC", ("reborn", "hud", 0), ("reborn", "cameras", 0)) } },
        };

        public static string IniOf(string folder)
        {
            string exe = Install.GameExe(folder);
            return exe != null ? Path.ChangeExtension(exe, ".ini") : null;
        }

        public static bool Done(string folder)
        {
            string ini = IniOf(folder);
            return ini == null || GetPrivateProfileInt("launcher", "setup", 0, ini) == 1;
        }

        // The preset each choice's settings match now, or -1 when they are set some other way.
        public static int[] Current(string folder)
        {
            string ini = IniOf(folder);
            var chosen = new int[Choices.Length];
            for (int c = 0; c < Choices.Length; c++)
            {
                chosen[c] = -1;
                for (int p = 0; p < Choices[c].Presets.Length && chosen[c] < 0; p++)
                {
                    bool all = ini != null;
                    foreach (var (section, key, value) in Choices[c].Presets[p].Values)
                        all &= GetPrivateProfileInt(section, key, int.MinValue, ini) == value;
                    if (all)
                        chosen[c] = p;
                }
            }
            return chosen;
        }

        // The first rider's name for the game to make its first profile with ([launcher] rider, read by
        // the dev build at its "new rider" screen), when the install has no profile yet.
        public static void NameFirstRider(string folder, string name)
        {
            string ini = IniOf(folder), profiles = Path.Combine(folder, @"UI\PROFILE");
            if (ini == null || name.Length == 0
                || (Directory.Exists(profiles) && Directory.GetDirectories(profiles).Length > 0))
                return;
            WritePrivateProfileString("launcher", "rider", name, ini);
        }

        public static void Save(string folder, int[] chosen)
        {
            string ini = IniOf(folder);
            if (ini == null)
                return;
            for (int c = 0; c < Choices.Length; c++)
                if (chosen[c] >= 0)
                    foreach (var (section, key, value) in Choices[c].Presets[chosen[c]].Values)
                        WritePrivateProfileString(section, key, value.ToString(), ini);
            WritePrivateProfileString("launcher", "setup", "1", ini);
        }
    }
}
