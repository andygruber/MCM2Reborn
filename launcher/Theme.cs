// MCM2:Reborn's look, as the modern menus have it (tools/modern/reborn_kit.py): its colours, its two
// faces (the menus' headings and text, carried inside the exe), and the slanted buttons and panels.

using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.IO;
using System.Runtime.InteropServices;

namespace Mcm2Reborn.Launcher
{
    static class Theme
    {
        public static readonly Color Ink = Color.FromArgb(14, 14, 16);
        public static readonly Color Panel = Color.FromArgb(22, 23, 26);
        public static readonly Color Raised = Color.FromArgb(30, 31, 36);
        public static readonly Color Edge = Color.FromArgb(64, 66, 72);
        public static readonly Color Paper = Color.FromArgb(244, 240, 230);
        public static readonly Color Grey = Color.FromArgb(150, 150, 156);
        public static readonly Color Dim = Color.FromArgb(90, 92, 98);
        public static readonly Color Orange = Color.FromArgb(255, 138, 0);
        public static readonly Color Green = Color.FromArgb(52, 199, 89);
        public static readonly Color Red = Color.FromArgb(255, 69, 58);

        static readonly PrivateFontCollection fonts = new PrivateFontCollection();
        static FontFamily heading, text;

        // The screen's pixels to the window's design pixels (96 a inch).
        public static float Scale = 1;

        public static void Load()
        {
            var assembly = typeof(Theme).Assembly;
            foreach (string name in assembly.GetManifestResourceNames())
            {
                if (!name.StartsWith("font.", StringComparison.Ordinal))
                    continue;
                using (Stream stream = assembly.GetManifestResourceStream(name))
                using (var copy = new MemoryStream())
                {
                    stream.CopyTo(copy);
                    byte[] data = copy.ToArray();
                    IntPtr memory = Marshal.AllocCoTaskMem(data.Length);
                    Marshal.Copy(data, 0, memory, data.Length);
                    fonts.AddMemoryFont(memory, data.Length);       // kept for the process's life
                }
            }
            foreach (FontFamily family in fonts.Families)
            {
                if (family.Name.IndexOf("Aeromove", StringComparison.OrdinalIgnoreCase) >= 0)
                    heading = family;
                else if (family.Name.IndexOf("Helvetica", StringComparison.OrdinalIgnoreCase) >= 0)
                    text = family;
            }
        }

        // The menus' heading face (or a bold stand-in), at `size` design pixels.
        public static Font Heading(float size) =>
            heading != null ? new Font(heading, size * Scale, FontStyle.Regular, GraphicsUnit.Pixel)
                            : new Font("Segoe UI", size * Scale, FontStyle.Bold | FontStyle.Italic, GraphicsUnit.Pixel);

        public static Font Text(float size, bool bold = false)
        {
            if (text != null)
            {
                FontStyle style = text.IsStyleAvailable(FontStyle.Italic) ? FontStyle.Italic : FontStyle.Regular;
                if (bold && text.IsStyleAvailable(style | FontStyle.Bold))
                    style |= FontStyle.Bold;
                return new Font(text, size * Scale, style, GraphicsUnit.Pixel);
            }
            return new Font("Segoe UI", size * Scale, bold ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Pixel);
        }

        public static void Smooth(Graphics g)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        }

        public static GraphicsPath Rounded(RectangleF r, float radius)
        {
            var path = new GraphicsPath();
            float d = Math.Min(radius * 2, Math.Min(r.Width, r.Height));
            if (d <= 0)
            {
                path.AddRectangle(r);
                return path;
            }
            path.AddArc(r.X, r.Y, d, d, 180, 90);
            path.AddArc(r.Right - d, r.Y, d, d, 270, 90);
            path.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
            path.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
            path.CloseFigure();
            return path;
        }

        // The menus' buttons lean: a parallelogram, its top `slant` to the right of its foot.
        public static GraphicsPath Slanted(RectangleF r, float slant)
        {
            var path = new GraphicsPath();
            path.AddPolygon(new[] {
                new PointF(r.X + slant, r.Y), new PointF(r.Right, r.Y),
                new PointF(r.Right - slant, r.Bottom), new PointF(r.X, r.Bottom) });
            return path;
        }

        public static void PanelBox(Graphics g, RectangleF r, int alpha = 230)
        {
            using (GraphicsPath path = Rounded(r, 12 * Scale))
            using (var fill = new SolidBrush(Color.FromArgb(alpha, Panel)))
            using (var edge = new Pen(Color.FromArgb(160, Edge), Scale))
            {
                g.FillPath(fill, path);
                g.DrawPath(edge, path);
            }
        }

        public static void Words(Graphics g, string words, Font font, Color colour, RectangleF r,
                                 StringAlignment across = StringAlignment.Near,
                                 StringAlignment down = StringAlignment.Center, bool wrap = false)
        {
            using (var brush = new SolidBrush(colour))
            using (var format = new StringFormat { Alignment = across, LineAlignment = down,
                                                   Trimming = StringTrimming.EllipsisCharacter })
            {
                if (!wrap)
                    format.FormatFlags |= StringFormatFlags.NoWrap;
                g.DrawString(words, font, brush, r, format);
            }
        }
    }
}
