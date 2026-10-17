// How many are playing MCM2:Reborn: each running game keeps a lobby of one in Epic's lobbies
// (mcm2-eos --presence, which the game starts); the helper's --count says how many there are.

using System;
using System.Diagnostics;
using System.IO;

namespace Mcm2Reborn.Launcher
{
    static class Players
    {
        const int WaitMs = 20000;

        // The count, or -1 when it could not be had (no helper, no network, Epic not answering).
        public static int Count(string folder)
        {
            string helper = Path.Combine(folder ?? "", "mcm2-eos.exe");
            if (folder == null || !File.Exists(helper))
                return -1;
            try
            {
                var start = new ProcessStartInfo(helper, "--count") {
                    WorkingDirectory = folder, UseShellExecute = false, CreateNoWindow = true,
                    RedirectStandardOutput = true,
                };
                using (Process process = Process.Start(start))
                {
                    var reading = process.StandardOutput.ReadToEndAsync();
                    string output = reading.Wait(WaitMs) ? reading.Result : null;
                    if (!process.WaitForExit(1000))
                        process.Kill();
                    return int.TryParse(output?.Trim(), out int count) ? count : -1;
                }
            }
            catch (Exception)
            {
                return -1;
            }
        }
    }
}
