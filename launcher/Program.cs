// MCM2:Reborn's launcher: installs MCM2:Reborn from the player's own Motocross Madness 2, keeps it
// up to date from GitHub, shows the changelog and how many are playing, and starts the game.

using System;
using System.Windows.Forms;

namespace Mcm2Reborn.Launcher
{
    static class Program
    {
        [STAThread]
        static void Main()
        {
            Updates.TidyOldLauncher();
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Theme.Load();
            Application.Run(new LauncherForm());
        }
    }
}
