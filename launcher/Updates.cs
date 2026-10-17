// MCM2:Reborn's releases, from GitHub (xshxdex98/OpenMCM2): the changelog the launcher shows, and
// the newest's package, which it puts over the install when it is newer than the one there.
//
// A release's package is its asset MCM2Reborn-<version>.zip: MCM2:Reborn's own files laid out as
// in the game's folder, and the launcher. Settings files already there are kept.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Web.Script.Serialization;

namespace Mcm2Reborn.Launcher
{
    sealed class Release
    {
        public string Tag, Name, Notes, PackageUrl;
        public DateTime Published;
        public Version Version;
    }

    static class Updates
    {
        const string Releases = "https://api.github.com/repos/xshxdex98/OpenMCM2/releases?per_page=30";
        public const string VersionFile = "reborn-version.txt";

        // Kept when a package has them too: the player's own settings.
        static readonly string[] Keep = { "MCM2Reborn.ini", "OpenMCM2-Dev.ini" };

        static Updates()
        {
            ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
        }

        static WebClient Client()
        {
            var client = new WebClient();
            client.Headers[HttpRequestHeader.UserAgent] = "MCM2Reborn-Launcher";
            return client;
        }

        // The releases, the newest first; drafts and pre-releases left out.
        public static List<Release> Fetch()
        {
            string json;
            using (WebClient client = Client())
            {
                client.Headers[HttpRequestHeader.Accept] = "application/vnd.github+json";
                json = client.DownloadString(Releases);
            }
            var releases = new List<Release>();
            var list = new JavaScriptSerializer { MaxJsonLength = int.MaxValue }.DeserializeObject(json) as object[];
            foreach (var entry in list ?? new object[0])
            {
                if (!(entry is Dictionary<string, object> r) || r.Get<bool>("draft") || r.Get<bool>("prerelease"))
                    continue;
                var release = new Release {
                    Tag = r.Get<string>("tag_name") ?? "",
                    Name = r.Get<string>("name"),
                    Notes = (r.Get<string>("body") ?? "").Replace("\r\n", "\n").Trim(),
                };
                if (string.IsNullOrEmpty(release.Name))
                    release.Name = release.Tag;
                DateTime.TryParse(r.Get<string>("published_at"), CultureInfo.InvariantCulture,
                                  DateTimeStyles.AdjustToUniversal, out release.Published);
                release.Version = ParseVersion(release.Tag);
                if (r.TryGetValue("assets", out object assets) && assets is object[] files)
                    foreach (var file in files.OfType<Dictionary<string, object>>())
                    {
                        string name = file.Get<string>("name") ?? "";
                        if (name.StartsWith("MCM2Reborn-", StringComparison.OrdinalIgnoreCase)
                            && name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
                            release.PackageUrl = file.Get<string>("browser_download_url");
                    }
                releases.Add(release);
            }
            return releases.OrderByDescending(r => r.Version).ThenByDescending(r => r.Published).ToList();
        }

        static T Get<T>(this Dictionary<string, object> map, string key) =>
            map.TryGetValue(key, out object value) && value is T typed ? typed : default(T);

        // "v0.1", "0.1.2": a version; anything else 0.
        public static Version ParseVersion(string text)
        {
            text = (text ?? "").Trim().TrimStart('v', 'V');
            string digits = new string(text.TakeWhile(c => char.IsDigit(c) || c == '.').ToArray());
            if (!digits.Contains('.'))
                digits += ".0";
            return Version.TryParse(digits, out Version version) ? version : new Version(0, 0);
        }

        public static Version InstalledVersion(string folder)
        {
            string file = Path.Combine(folder, VersionFile);
            return File.Exists(file) ? ParseVersion(File.ReadAllText(file)) : new Version(0, 0);
        }

        // The release's package fetched and put over `folder`; `progress` hears the share done (0..1).
        // TRUE when it brought a newer launcher, which takes the running one's place on the next start.
        public static bool Apply(Release release, string folder, Action<string, float> progress)
        {
            string zip = Path.Combine(Path.GetTempPath(), "MCM2Reborn-" + release.Tag + ".zip");
            using (WebClient client = Client())
            {
                var done = new System.Threading.ManualResetEvent(false);
                Exception failed = null;
                client.DownloadProgressChanged += (s, e) =>
                    progress("DOWNLOADING " + release.Tag, e.TotalBytesToReceive > 0
                             ? 0.8f * e.BytesReceived / e.TotalBytesToReceive : 0);
                client.DownloadFileCompleted += (s, e) => { failed = e.Error; done.Set(); };
                client.DownloadFileAsync(new Uri(release.PackageUrl), zip);
                done.WaitOne();
                if (failed != null)
                    throw failed;
            }
            bool launcher = Unpack(zip, folder, progress);
            File.Delete(zip);
            File.WriteAllText(Path.Combine(folder, VersionFile), release.Tag);
            return launcher;
        }

        // A package's files into `folder`, the settings kept; the launcher's own beside it as .new.
        public static bool Unpack(string zip, string folder, Action<string, float> progress)
        {
            string self = Path.GetFileName(typeof(Updates).Assembly.Location);
            bool launcher = false;
            string root = Path.GetFullPath(folder) + Path.DirectorySeparatorChar;
            using (ZipArchive archive = ZipFile.OpenRead(zip))
            {
                var entries = archive.Entries.Where(e => e.Name.Length > 0).ToList();
                for (int i = 0; i < entries.Count; i++)
                {
                    ZipArchiveEntry entry = entries[i];
                    string to = Path.GetFullPath(Path.Combine(folder, entry.FullName));
                    if (!to.StartsWith(root, StringComparison.OrdinalIgnoreCase))
                        continue;                                   // nothing outside the folder
                    progress(entry.Name, 0.8f + 0.2f * i / entries.Count);
                    if (Keep.Contains(entry.FullName, StringComparer.OrdinalIgnoreCase) && File.Exists(to))
                        continue;
                    if (entry.FullName.Equals(self, StringComparison.OrdinalIgnoreCase))
                    {
                        to += ".new";
                        launcher = true;
                    }
                    Directory.CreateDirectory(Path.GetDirectoryName(to));
                    entry.ExtractToFile(to, true);
                }
            }
            progress("", 1);
            return launcher;
        }

        // A package beside the launcher (an offline install), the newest by name: or null.
        public static string LocalPackage()
        {
            string here = Path.GetDirectoryName(typeof(Updates).Assembly.Location);
            return Directory.GetFiles(here, "MCM2Reborn-*.zip").OrderByDescending(f => f).FirstOrDefault();
        }

        // The launcher a package brought: in place of this one (Windows lets a running exe be renamed).
        public static bool SwapLauncher(string folder)
        {
            string self = typeof(Updates).Assembly.Location;
            string placed = Path.Combine(folder, Path.GetFileName(self));
            string next = placed + ".new";
            if (!File.Exists(next))
                return false;
            string old = placed + ".old";
            if (File.Exists(old))
                File.Delete(old);
            if (File.Exists(placed))
                File.Move(placed, old);
            File.Move(next, placed);
            return true;
        }

        public static void TidyOldLauncher()
        {
            try
            {
                string old = typeof(Updates).Assembly.Location + ".old";
                if (File.Exists(old))
                    File.Delete(old);
            }
            catch (Exception)
            {
            }
        }
    }
}
