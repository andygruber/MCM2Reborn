// The launcher's window, drawn in the modern menus' look: MCM2:Reborn's name and how many are riding
// at the top, the play (or install) side on the left, the changelog on the right.
//
// Before MCM2:Reborn is installed the left side installs it: the copies of the game found, which
// to make it from, and where. After, it plays: and on starting it puts any newer release over
// the install by itself, the launcher with it.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace Mcm2Reborn.Launcher
{
    sealed class LauncherForm : Form
    {
        const int DesignWidth = 1120, DesignHeight = 660;
        const int CountEveryMs = 60000;

        sealed class Hit
        {
            public RectangleF Area;
            public Action Click;
            public bool Enabled = true;
        }

        // What it shows and does.
        string folder;                              // MCM2:Reborn's install, or null
        List<GameCopy> copies = new List<GameCopy>();
        int chosen;
        bool intoGameFolder;
        string destination = Install.DefaultFolder;
        List<Release> releases;
        string releasesFailed;
        int players = -2;                           // -2 counting, -1 unknown
        string status = "";
        Color statusColour = Theme.Grey;
        float progress = -1;                        // 0..1 while busy, else -1
        bool busy;
        Bitmap photo;
        float changelogScroll, changelogHeight;
        RectangleF changelogArea;
        int[] setupChosen;                          // the setup page's choices while it is open, else null
        readonly TextBox riderName;                 // the first rider's name, on the install page

        readonly List<Hit> hits = new List<Hit>();
        Hit hover;
        readonly System.Windows.Forms.Timer countTimer = new System.Windows.Forms.Timer { Interval = CountEveryMs };

        public LauncherForm()
        {
            Text = "MCM2:Reborn";
            FormBorderStyle = FormBorderStyle.None;
            StartPosition = FormStartPosition.CenterScreen;
            DoubleBuffered = true;
            BackColor = Theme.Ink;
            using (Graphics g = CreateGraphics())
                Theme.Scale = g.DpiX / 96f;
            ClientSize = new Size(S(DesignWidth), S(DesignHeight));
            try
            {
                Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
            }
            catch (Exception)
            {
            }
            countTimer.Tick += (s, e) => CountPlayers();

            // The profile's name as the game allows it: 15 of letters, digits and _-()$#@.
            riderName = new TextBox {
                MaxLength = 15, BorderStyle = BorderStyle.FixedSingle, BackColor = Theme.Panel,
                ForeColor = Theme.Paper, Font = Theme.Text(18, true), Visible = false,
            };
            riderName.KeyPress += (s, e) => e.Handled = !char.IsControl(e.KeyChar)
                && !(e.KeyChar < 128 && (char.IsLetterOrDigit(e.KeyChar) || "_-()$#@".IndexOf(e.KeyChar) >= 0));
            Controls.Add(riderName);
        }

        static int S(float design) => (int) Math.Round(design * Theme.Scale);
        static RectangleF R(float x, float y, float w, float h) =>
            new RectangleF(x * Theme.Scale, y * Theme.Scale, w * Theme.Scale, h * Theme.Scale);

        // ---- Starting ----

        protected override void OnLoad(EventArgs e)
        {
            base.OnLoad(e);
            folder = Install.Installed;
            if (folder != null && Install.GameExe(folder) == null)
                folder = null;
            if (folder == null)
                FindCopies();
            else
                ShowInstalled();
            FetchReleases();
        }

        // The releases from GitHub, then any newer one put over the install.
        void FetchReleases()
        {
            releases = null;
            releasesFailed = null;
            Background(() => {
                List<Release> found = null;
                string failed = null;
                try
                {
                    found = Updates.Fetch();
                }
                catch (Exception ex)
                {
                    failed = ex.Message;
                }
                Ui(() => {
                    releases = found;
                    releasesFailed = failed;
                    UpdateIfNewer();
                });
            });
        }

        void FindCopies()
        {
            status = "LOOKING FOR MOTOCROSS MADNESS 2...";
            Background(() => {
                List<GameCopy> found = Install.Find();
                Ui(() => {
                    copies = found;
                    copies.Add(new GameCopy { Download = true, Kind = "DOWNLOAD",
                                              Folder = "MOTOCROSS MADNESS 2, DOWNLOADED WITH MCM2:REBORN" });
                    chosen = 0;
                    status = "";
                    statusColour = Theme.Grey;
                });
            });
        }

        void ShowInstalled()
        {
            photo?.Dispose();
            photo = LoadPhoto(Path.Combine(folder, @"UI\modern\photo1.tga"));
            status = "";
            if (!Setup.Done(folder))
                OpenSetup(true);
            CountPlayers();
            countTimer.Start();
            Invalidate();
        }

        // The setup page, its choices as the settings are now; the first time, the unset ones at
        // the recommended (the first after CLASSIC for the picture, else the first).
        void OpenSetup(bool first)
        {
            setupChosen = Setup.Current(folder);
            for (int c = 0; first && c < setupChosen.Length; c++)
                if (setupChosen[c] < 0)
                    setupChosen[c] = c == 0 ? 2 : 0;
            Invalidate();
        }

        void SaveSetup()
        {
            Setup.Save(folder, setupChosen);
            setupChosen = null;
            status = "SETTINGS SAVED. EVERY ONE IS ALSO IN THE GAME'S OPTIONS";
            statusColour = Theme.Green;
            Invalidate();
        }

        // ---- Updates ----

        Release Newest => releases?.FirstOrDefault(r => r.PackageUrl != null);

        void UpdateIfNewer()
        {
            Release newest = Newest;
            if (folder == null || busy || newest == null || newest.Version <= Updates.InstalledVersion(folder))
            {
                if (folder != null && !busy && status.Length == 0)
                    status = releasesFailed != null ? "COULD NOT CHECK FOR UPDATES" : "UP TO DATE";
                Invalidate();
                return;
            }
            if (Process.GetProcessesByName(Path.GetFileNameWithoutExtension(Install.GameExe(folder))).Length > 0)
            {
                status = "UPDATE " + newest.Tag.ToUpperInvariant() + " READY: CLOSE THE GAME TO GET IT";
                statusColour = Theme.Orange;
                Invalidate();
                return;
            }
            Work("UPDATING TO " + newest.Tag.ToUpperInvariant(), report => {
                bool launcher = Updates.Apply(newest, folder, report);
                return launcher ? "restart" : "UPDATED TO " + newest.Tag.ToUpperInvariant();
            });
        }

        // Something long, off the window's thread: `job` reports its progress and says how it ended
        // ("restart" for the launcher to start again, it having been replaced).
        void Work(string what, Func<Action<string, float>, string> job)
        {
            busy = true;
            progress = 0;
            status = what;
            statusColour = Theme.Paper;
            Invalidate();
            Background(() => {
                string ended;
                try
                {
                    ended = job((file, done) => Ui(() => {
                        progress = done;
                        status = what + (file.Length > 0 ? "  ·  " + file.ToUpperInvariant() : "");
                    }));
                }
                catch (Exception ex)
                {
                    ended = "FAILED: " + ex.Message.ToUpperInvariant();
                }
                Ui(() => {
                    busy = false;
                    progress = -1;
                    if (ended == "restart" && folder != null && Updates.SwapLauncher(folder))
                    {
                        Process.Start(Path.Combine(folder, Path.GetFileName(Application.ExecutablePath)));
                        Close();
                        return;
                    }
                    status = ended;
                    statusColour = ended.StartsWith("FAILED") ? Theme.Red : Theme.Green;
                    if (folder != null && !countTimer.Enabled)
                        ShowInstalled();
                    Invalidate();
                });
            });
        }

        // ---- Installing ----

        void InstallNow()
        {
            if (copies.Count == 0 || busy)
                return;
            GameCopy copy = copies[chosen];
            string to = intoGameFolder && !copy.Download ? copy.Folder : destination;
            string rider = riderName.Text.Trim();
            if (!copy.Download && copy.DataFolder == null && !Install.IsGame(copy.Folder))
                return;
            if (copy.Download && !Updates.BaseOffered(releases))
            {
                status = releasesFailed != null ? "NO INTERNET: THE GAME COULD NOT BE DOWNLOADED"
                                                 : "THE GAME'S DOWNLOAD IS NOT UP YET: PICK A COPY YOU HAVE";
                statusColour = Theme.Red;
                Invalidate();
                return;
            }
            Release newest = Newest;
            string local = Updates.LocalPackage();
            if (local == null && newest == null)
            {
                status = releasesFailed != null ? "NO INTERNET: MCM2:REBORN COULD NOT BE DOWNLOADED"
                                                 : "NO RELEASE OF MCM2:REBORN TO INSTALL YET";
                statusColour = Theme.Red;
                Invalidate();
                return;
            }
            List<Release> known = releases;
            Work("INSTALLING", report => {
                if (copy.Download)
                    Updates.InstallBase(known, to, (file, done) => report("THE GAME  ·  " + file, done * 0.6f));
                else
                    Install.CopyGame(copy, to, (file, done) => report("COPYING THE GAME  ·  " + file, done * 0.6f));
                if (local != null)
                {
                    Updates.Unpack(local, to, (file, done) => report(file, 0.6f + 0.4f * done));
                    string tag = Path.GetFileNameWithoutExtension(local).Substring("MCM2Reborn-".Length);
                    File.WriteAllText(Path.Combine(to, Updates.VersionFile), tag);
                }
                else
                {
                    Updates.Apply(newest, to, (file, done) => report(file, 0.6f + 0.4f * done));
                }
                // The launcher in its install: a newer one the package brought is that one.
                string next = Path.Combine(to, Path.GetFileName(Application.ExecutablePath) + ".new");
                Install.PlaceLauncher(to);
                if (File.Exists(next))
                {
                    string placed = Path.Combine(to, Path.GetFileName(Application.ExecutablePath));
                    File.Copy(next, placed, true);
                    File.Delete(next);
                }
                Install.Installed = to;
                Setup.NameFirstRider(to, rider);
                Ui(() => folder = to);
                return "INSTALLED. READY TO RIDE";
            });
        }

        void Browse()
        {
            using (var dialog = new FolderBrowserDialog { Description = "Motocross Madness 2's folder (where Mcm2.exe is)" })
            {
                if (dialog.ShowDialog(this) != DialogResult.OK)
                    return;
                if (!Install.IsGame(dialog.SelectedPath))
                {
                    status = "THAT FOLDER HAS NO MCM2.EXE, RES AND UI";
                    statusColour = Theme.Red;
                }
                else
                {
                    var copy = new GameCopy { Folder = dialog.SelectedPath, Kind = "CHOSEN" };
                    if (!Directory.Exists(Path.Combine(copy.Folder, "SX")))
                        copy.DataFolder = Install.FindDisc();
                    copies.Insert(0, copy);
                    chosen = 0;
                    status = "";
                }
            }
            Invalidate();
        }

        void ChooseDestination()
        {
            using (var dialog = new FolderBrowserDialog { Description = "Where MCM2:Reborn goes" })
            {
                if (dialog.ShowDialog(this) == DialogResult.OK)
                {
                    destination = Path.Combine(dialog.SelectedPath, "MCM2 Reborn");
                    intoGameFolder = false;
                }
            }
            Invalidate();
        }

        // ---- Playing ----

        void Play()
        {
            string exe = folder != null ? Install.GameExe(folder) : null;
            if (exe == null || busy)
                return;
            Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = folder });
            WindowState = FormWindowState.Minimized;
        }

        void CountPlayers()
        {
            string at = folder;
            Background(() => {
                int count = Players.Count(at);
                Ui(() => {
                    players = count;
                    Invalidate();
                });
            });
        }

        // ---- Drawing ----

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            Theme.Smooth(g);
            hits.Clear();
            DrawBackground(g);
            DrawTop(g);
            riderName.Visible = folder == null;
            if (folder == null)
                DrawInstall(g);
            else if (setupChosen != null)
                DrawSetup(g);
            else
                DrawPlay(g);
            DrawChangelog(g);
            using (Font small = Theme.Text(12))
                Theme.Words(g, "MCM2:REBORN IS MADE BY ITS COMMUNITY FROM YOUR OWN COPY OF MOTOCROSS MADNESS 2.",
                            small, Theme.Dim, R(36, 624, 640, 20));
        }

        void DrawBackground(Graphics g)
        {
            var all = new RectangleF(0, 0, ClientSize.Width, ClientSize.Height);
            if (photo != null)
            {
                float scale = Math.Max(all.Width / photo.Width, all.Height / photo.Height);
                float w = photo.Width * scale, h = photo.Height * scale;
                g.DrawImage(photo, (all.Width - w) / 2, (all.Height - h) / 2, w, h);
            }
            using (var shade = new LinearGradientBrush(all, Color.FromArgb(235, Theme.Ink), Color.FromArgb(150, Theme.Ink),
                                                       LinearGradientMode.Horizontal))
                g.FillRectangle(shade, all);
            using (var edge = new Pen(Theme.Edge, Theme.Scale))
                g.DrawRectangle(edge, 0, 0, ClientSize.Width - 1, ClientSize.Height - 1);
        }

        void DrawTop(Graphics g)
        {
            using (Font title = Theme.Heading(30))
            using (Font small = Theme.Text(13, true))
            {
                Theme.Words(g, "MCM2:REBORN", title, Theme.Paper, R(36, 18, 400, 44));
                using (var bar = new SolidBrush(Theme.Orange))
                    g.FillRectangle(bar, R(36, 64, 220, 4));
                Theme.Words(g, "LAUNCHER  ·  VERSION " + Application.ProductVersion, small, Theme.Grey, R(36, 72, 400, 20));
            }
            // The window's own buttons.
            WindowButton(g, R(DesignWidth - 92, 14, 36, 30), "–", () => WindowState = FormWindowState.Minimized);
            WindowButton(g, R(DesignWidth - 52, 14, 36, 30), "✕", Close);

            // How many are riding.
            RectangleF pill = R(DesignWidth - 330, 56, 294, 38);
            using (GraphicsPath path = Theme.Rounded(pill, 19 * Theme.Scale))
            using (var fill = new SolidBrush(Color.FromArgb(220, 16, 17, 20)))
            using (var edge = new Pen(players > 0 ? Theme.Green : Theme.Edge, Theme.Scale))
            {
                g.FillPath(fill, path);
                g.DrawPath(edge, path);
            }
            Color dot = players > 0 ? Theme.Green : players == 0 ? Theme.Grey : Theme.Dim;
            using (var brush = new SolidBrush(dot))
                g.FillEllipse(brush, pill.X + S(18), pill.Y + pill.Height / 2 - S(5), S(10), S(10));
            string words = folder == null ? "INSTALL TO SEE WHO'S RIDING"
                         : players >= 0 ? players + (players == 1 ? " RIDER ONLINE" : " RIDERS ONLINE")
                         : players == -2 ? "COUNTING RIDERS..." : "RIDER COUNT UNAVAILABLE";
            using (Font font = Theme.Text(15, true))
                Theme.Words(g, words, font, Theme.Paper, new RectangleF(pill.X + S(38), pill.Y, pill.Width - S(48), pill.Height));
        }

        void WindowButton(Graphics g, RectangleF area, string mark, Action click)
        {
            Hit hit = Add(area, click);
            if (Hovered(hit))
                using (var fill = new SolidBrush(Theme.Raised))
                    g.FillRectangle(fill, area);
            using (Font font = Theme.Text(16, true))
                Theme.Words(g, mark, font, Theme.Paper, area, StringAlignment.Center);
        }

        void DrawPlay(Graphics g)
        {
            using (Font big = Theme.Heading(56))
                Theme.Words(g, "READY TO RIDE", big, Theme.Paper, R(36, 150, 620, 80));
            Version installed = Updates.InstalledVersion(folder);
            using (Font text = Theme.Text(15))
                Theme.Words(g, "VERSION " + (installed.Major + installed.Minor > 0 ? installed.ToString() : "?")
                               + "   ·   " + folder.ToUpperInvariant(), text, Theme.Grey, R(36, 228, 620, 24));
            BigButton(g, R(36, 280, 380, 78), "PLAY  ›", Play, !busy);
            SmallButton(g, R(36, 384, 230, 46), "CHECK FOR UPDATES", () => {
                status = "";
                FetchReleases();
            }, !busy);
            SmallButton(g, R(282, 384, 190, 46), "OPEN FOLDER", () => Process.Start("explorer.exe", "\"" + folder + "\""), true);
            SmallButton(g, R(488, 384, 150, 46), "SETUP", () => OpenSetup(false), !busy);
            DrawStatus(g, 452);
        }

        void DrawInstall(Graphics g)
        {
            using (Font big = Theme.Heading(46))
                Theme.Words(g, "INSTALL", big, Theme.Paper, R(36, 118, 620, 64));
            using (Font text = Theme.Text(14))
                Theme.Words(g, "THE GAME MCM2:REBORN IS MADE FROM. PICK ONE:", text, Theme.Grey, R(36, 180, 500, 22));
            SmallButton(g, R(540, 172, 136, 32), "BROWSE...", Browse, !busy);
            float y = 210;
            // Two copies found at most, and the download, always last.
            var shown = copies.Where(c => !c.Download).Take(2).Concat(copies.Where(c => c.Download)).ToList();
            foreach (GameCopy copy in shown)
            {
                int which = copies.IndexOf(copy);
                CopyRow(g, R(36, y, 640, 42), copy, which == chosen, () => chosen = which);
                y += 46;
            }

            bool download = copies.Count > 0 && copies[chosen].Download;
            using (Font text = Theme.Text(14))
                Theme.Words(g, "WHERE IT GOES:", text, Theme.Grey, R(36, y + 4, 640, 22));
            y += 28;
            Choice(g, R(36, y, 640, 38), "A NEW FOLDER  ·  " + destination.ToUpperInvariant(), !intoGameFolder || download,
                   () => intoGameFolder = false);
            SmallButton(g, R(560, y + 4, 110, 30), "CHANGE", ChooseDestination, !busy);
            y += 40;
            if (!download)
                Choice(g, R(36, y, 640, 38), "INTO THE GAME'S OWN FOLDER (FOR A COPY YOU KEEP FOR IT)", intoGameFolder,
                       () => intoGameFolder = true);
            y += 48;

            using (Font text = Theme.Text(14))
                Theme.Words(g, "YOUR RIDER'S NAME (YOUR FIRST PROFILE):", text, Theme.Grey, R(36, y, 640, 22));
            Rectangle box = Rectangle.Round(R(36, y + 26, 300, 36));
            if (riderName.Bounds != box)
                riderName.Bounds = box;
            BigButton(g, R(366, y + 18, 300, 54), "INSTALL  ›", InstallNow, !busy && copies.Count > 0);
            DrawStatus(g, y + 82);
        }

        void DrawSetup(Graphics g)
        {
            using (Font big = Theme.Heading(46))
                Theme.Words(g, "SETUP", big, Theme.Paper, R(36, 118, 620, 64));
            using (Font text = Theme.Text(14))
                Theme.Words(g, "A QUICK START: CHANGE ANY OF IT LATER IN THE GAME'S OPTIONS.", text, Theme.Grey,
                            R(36, 180, 640, 22));
            float y = 222;
            for (int c = 0; c < Setup.Choices.Length; c++, y += 76)
            {
                SetupChoice choice = Setup.Choices[c];
                using (Font label = Theme.Heading(15))
                    Theme.Words(g, choice.Title, label, Theme.Orange, R(36, y, 640, 22));
                float x = 36, w = (640 - 10 * (choice.Presets.Length - 1)) / (float) choice.Presets.Length;
                for (int p = 0; p < choice.Presets.Length; p++, x += w + 10)
                {
                    int which = c, preset = p;
                    Option(g, R(x, y + 26, w, 40), choice.Presets[p].Name, setupChosen[c] == p,
                           () => setupChosen[which] = preset);
                }
            }
            BigButton(g, R(36, y + 4, 260, 58), "SAVE  ›", SaveSetup, !busy);
            SmallButton(g, R(312, y + 10, 150, 46), "BACK", () => {
                setupChosen = null;
                Invalidate();
            }, true);
        }

        void Option(Graphics g, RectangleF area, string words, bool on, Action pick)
        {
            Hit hit = Add(area, pick, !busy);
            using (GraphicsPath path = Theme.Rounded(area, 8 * Theme.Scale))
            using (var fill = new SolidBrush(on ? Theme.Orange : Hovered(hit) ? Theme.Raised : Color.FromArgb(220, Theme.Panel)))
            using (var edge = new Pen(on ? Theme.Orange : Theme.Edge, Theme.Scale))
            {
                g.FillPath(fill, path);
                g.DrawPath(edge, path);
            }
            using (Font font = Theme.Heading(14))
                Theme.Words(g, words, font, on ? Theme.Ink : Theme.Paper, area, StringAlignment.Center);
        }

        void CopyRow(Graphics g, RectangleF area, GameCopy copy, bool on, Action pick)
        {
            Hit hit = Add(area, pick, !busy);
            using (GraphicsPath path = Theme.Rounded(area, 8 * Theme.Scale))
            using (var fill = new SolidBrush(Hovered(hit) ? Theme.Raised : Color.FromArgb(220, Theme.Panel)))
            using (var edge = new Pen(on ? Theme.Orange : Theme.Edge, (on ? 2 : 1) * Theme.Scale))
            {
                g.FillPath(fill, path);
                g.DrawPath(edge, path);
            }
            using (Font tag = Theme.Text(12, true))
            using (Font text = Theme.Text(14))
            {
                string kind = copy.Kind + (copy.Complete ? "" : copy.DataFolder != null ? "  ·  DATA ON THE CD" : "  ·  CD NEEDED");
                Theme.Words(g, kind, tag, on ? Theme.Orange : Theme.Grey,
                            new RectangleF(area.X + S(14), area.Y + S(2), area.Width - S(28), S(18)));
                Theme.Words(g, copy.Folder, text, Theme.Paper,
                            new RectangleF(area.X + S(14), area.Y + S(18), area.Width - S(28), S(24)));
            }
        }

        void Choice(Graphics g, RectangleF area, string words, bool on, Action pick)
        {
            Hit hit = Add(area, pick, !busy);
            float d = S(16);
            var ring = new RectangleF(area.X + S(4), area.Y + (area.Height - d) / 2, d, d);
            using (var edge = new Pen(on ? Theme.Orange : Theme.Grey, 2 * Theme.Scale))
                g.DrawEllipse(edge, ring);
            if (on)
                using (var fill = new SolidBrush(Theme.Orange))
                    g.FillEllipse(fill, RectangleF.Inflate(ring, -S(4), -S(4)));
            using (Font text = Theme.Text(14, on))
                Theme.Words(g, words, text, Hovered(hit) || on ? Theme.Paper : Theme.Grey,
                            new RectangleF(area.X + S(30), area.Y, area.Width - S(150), area.Height));
        }

        void BigButton(Graphics g, RectangleF area, string words, Action click, bool enabled)
        {
            Hit hit = Add(area, click, enabled);
            Color fill = !enabled ? Color.FromArgb(92, 64, 30) : Hovered(hit) ? Color.FromArgb(255, 160, 40) : Theme.Orange;
            using (GraphicsPath path = Theme.Slanted(area, 22 * Theme.Scale))
            using (var brush = new SolidBrush(fill))
                g.FillPath(brush, path);
            using (Font font = Theme.Heading(area.Height / Theme.Scale * 0.45f))
                Theme.Words(g, words, font, enabled ? Theme.Ink : Color.FromArgb(40, 30, 20), area, StringAlignment.Center);
        }

        void SmallButton(Graphics g, RectangleF area, string words, Action click, bool enabled)
        {
            Hit hit = Add(area, click, enabled);
            using (GraphicsPath path = Theme.Slanted(area, 12 * Theme.Scale))
            using (var fill = new SolidBrush(Hovered(hit) && enabled ? Theme.Raised : Color.FromArgb(220, 16, 17, 20)))
            using (var edge = new Pen(Hovered(hit) && enabled ? Theme.Orange : Theme.Edge, Theme.Scale))
            {
                g.FillPath(fill, path);
                g.DrawPath(edge, path);
            }
            using (Font font = Theme.Heading(15))
                Theme.Words(g, words, font, enabled ? Theme.Paper : Theme.Dim, area, StringAlignment.Center);
        }

        void DrawStatus(Graphics g, float y)
        {
            if (progress >= 0)
            {
                RectangleF track = R(36, y, 640, 10);
                using (GraphicsPath path = Theme.Rounded(track, 5 * Theme.Scale))
                using (var fill = new SolidBrush(Theme.Raised))
                    g.FillPath(fill, path);
                var done = new RectangleF(track.X, track.Y, Math.Max(track.Height, track.Width * progress), track.Height);
                using (GraphicsPath path = Theme.Rounded(done, 5 * Theme.Scale))
                using (var fill = new SolidBrush(Theme.Orange))
                    g.FillPath(fill, path);
                y += 18;
            }
            using (Font font = Theme.Text(14, true))
                Theme.Words(g, status, font, statusColour, R(36, y, 640, 24));
        }

        void DrawChangelog(Graphics g)
        {
            RectangleF panel = R(712, 118, 372, 494);
            Theme.PanelBox(g, panel);
            using (Font heading = Theme.Heading(20))
                Theme.Words(g, "CHANGELOG", heading, Theme.Orange, new RectangleF(panel.X + S(20), panel.Y + S(14), panel.Width, S(30)));
            changelogArea = new RectangleF(panel.X + S(20), panel.Y + S(54), panel.Width - S(40), panel.Height - S(70));
            GraphicsState saved = g.Save();
            g.SetClip(changelogArea);
            float y = changelogArea.Y - changelogScroll;
            using (Font name = Theme.Heading(17))
            using (Font date = Theme.Text(12, true))
            using (Font notes = Theme.Text(14))
            using (var brush = new SolidBrush(Theme.Paper))
            {
                if (releases == null)
                {
                    Theme.Words(g, releasesFailed != null ? "THE CHANGELOG COULD NOT BE READ. NO INTERNET?" : "READING THE CHANGELOG...",
                                notes, Theme.Grey, new RectangleF(changelogArea.X, y, changelogArea.Width, S(40)), wrap: true);
                }
                else if (releases.Count == 0)
                {
                    Theme.Words(g, "NO RELEASES YET.", notes, Theme.Grey, new RectangleF(changelogArea.X, y, changelogArea.Width, S(24)));
                }
                foreach (Release release in releases ?? new List<Release>())
                {
                    Theme.Words(g, release.Name.ToUpperInvariant(), name, Theme.Paper, new RectangleF(changelogArea.X, y, changelogArea.Width, S(24)));
                    y += S(24);
                    Theme.Words(g, release.Published == default(DateTime) ? release.Tag : release.Published.ToString("d MMMM yyyy").ToUpperInvariant(),
                                date, Theme.Orange, new RectangleF(changelogArea.X, y, changelogArea.Width, S(18)));
                    y += S(22);
                    string body = string.IsNullOrEmpty(release.Notes) ? "No notes." : Plain(release.Notes);
                    SizeF size = g.MeasureString(body, notes, (int) changelogArea.Width);
                    g.DrawString(body, notes, brush, new RectangleF(changelogArea.X, y, changelogArea.Width, size.Height + S(4)));
                    y += size.Height + S(20);
                }
            }
            changelogHeight = y + changelogScroll - changelogArea.Y;
            g.Restore(saved);
        }

        // A release's notes as GitHub writes them (Markdown) made plain: no heading marks, bullets as dots.
        static string Plain(string markdown)
        {
            var lines = markdown.Split('\n').Select(line => {
                string l = line.TrimEnd();
                string t = l.TrimStart();
                if (t.StartsWith("#"))
                    return t.TrimStart('#').Trim().ToUpperInvariant();
                if (t.StartsWith("- ") || t.StartsWith("* "))
                    return new string(' ', l.Length - t.Length) + "• " + t.Substring(2);
                return l.Replace("**", "").Replace("`", "");
            });
            return string.Join("\n", lines);
        }

        // ---- The mouse ----

        Hit Add(RectangleF area, Action click, bool enabled = true)
        {
            var hit = new Hit { Area = area, Click = click, Enabled = enabled };
            hits.Add(hit);
            return hit;
        }

        Hit At(Point p) => hits.LastOrDefault(h => h.Enabled && h.Area.Contains(p));

        // The hits are made again each time the window is drawn: the one under the mouse is by its place.
        bool Hovered(Hit hit) => hover != null && hit.Enabled && hover.Area == hit.Area;

        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);
            Hit now = At(e.Location);
            if ((now == null) != (hover == null) || (now != null && hover != null && now.Area != hover.Area))
            {
                hover = now;
                Invalidate();
            }
            Cursor = now != null ? Cursors.Hand : Cursors.Default;
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e);
            if (e.Button == MouseButtons.Left && At(e.Location) == null && e.Y < S(110))
            {
                ReleaseCapture();
                SendMessage(Handle, 0xA1, (IntPtr) 2, IntPtr.Zero);         // WM_NCLBUTTONDOWN, HTCAPTION: dragged
            }
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            base.OnMouseUp(e);
            Hit hit = e.Button == MouseButtons.Left ? At(e.Location) : null;
            if (hit == null)
                return;
            hit.Click();
            Invalidate();
        }

        protected override void OnMouseWheel(MouseEventArgs e)
        {
            base.OnMouseWheel(e);
            if (!changelogArea.Contains(e.Location))
                return;
            float most = Math.Max(0, changelogHeight - changelogArea.Height);
            changelogScroll = Math.Max(0, Math.Min(most, changelogScroll - e.Delta / 120f * S(48)));
            Invalidate();
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            base.OnMouseLeave(e);
            hover = null;
            Invalidate();
        }

        [DllImport("user32.dll")] static extern bool ReleaseCapture();
        [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr window, int message, IntPtr w, IntPtr l);

        // ---- Threads ----

        static void Background(Action work) =>
            new Thread(() => work()) { IsBackground = true }.Start();

        void Ui(Action work)
        {
            if (IsDisposed)
                return;
            try
            {
                BeginInvoke((Action) (() => {
                    work();
                    Invalidate();
                }));
            }
            catch (InvalidOperationException)
            {
                // the window closed meanwhile
            }
        }

        // The menus' photo (ui\modern\photo1.tga: 32-bit, uncompressed), at the window's size.
        Bitmap LoadPhoto(string path)
        {
            try
            {
                byte[] tga = File.ReadAllBytes(path);
                int idLength = tga[0], type = tga[2], width = BitConverter.ToUInt16(tga, 12),
                    height = BitConverter.ToUInt16(tga, 14), bits = tga[16];
                bool topDown = (tga[17] & 0x20) != 0;
                if (type != 2 || bits != 32)
                    return null;
                var full = new Bitmap(width, height, PixelFormat.Format32bppArgb);
                BitmapData data = full.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.WriteOnly,
                                                PixelFormat.Format32bppArgb);
                int start = 18 + idLength;
                for (int i = start + 3; i < tga.Length; i += 4)
                    tga[i] = 255;                           // the renderer's alpha means nothing here
                for (int row = 0; row < height; row++)
                {
                    int from = start + (topDown ? row : height - 1 - row) * width * 4;
                    Marshal.Copy(tga, from, data.Scan0 + row * data.Stride, width * 4);
                }
                full.UnlockBits(data);
                float scale = Math.Max((float) ClientSize.Width / width, (float) ClientSize.Height / height);
                var small = new Bitmap((int) (width * scale), (int) (height * scale));
                using (Graphics g = Graphics.FromImage(small))
                {
                    Theme.Smooth(g);
                    g.DrawImage(full, 0, 0, small.Width, small.Height);
                }
                full.Dispose();
                return small;
            }
            catch (Exception)
            {
                return null;
            }
        }
    }
}
