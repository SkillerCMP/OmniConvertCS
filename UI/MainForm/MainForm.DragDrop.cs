using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace OmniconvertCS.Gui
{
    public partial class MainForm
    {
        // Which extensions we treat as “loadable” into Input
        private static bool IsSupportedLoadExtension(string? ext)
        {
            if (string.IsNullOrEmpty(ext))
                return false;

            switch (ext.ToLowerInvariant())
            {
                case ".cbc":  // CodeBreaker
                case ".bin":  // AR MAX .bin
                case ".p2m":  // XP/GS p2m
                case ".txt":  // plain text codes
                    return true;
                default:
                    return false;
            }
        }

        private void txtInput_DragEnter(object sender, DragEventArgs e)
        {
            if (!e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                e.Effect = DragDropEffects.None;
                return;
            }

            var files = (string[]?)e.Data.GetData(DataFormats.FileDrop);
            if (files == null || files.Length == 0)
            {
                e.Effect = DragDropEffects.None;
                return;
            }

            // Only show “Copy” if at least one file is something we know how to load
            bool anySupported = files.Any(f => IsSupportedLoadExtension(Path.GetExtension(f)));
            e.Effect = anySupported ? DragDropEffects.Copy : DragDropEffects.None;
        }

        private void txtInput_DragDrop(object sender, DragEventArgs e)
        {
            if (!e.Data.GetDataPresent(DataFormats.FileDrop))
                return;

            var files = (string[]?)e.Data.GetData(DataFormats.FileDrop);
            if (files == null || files.Length == 0)
                return;

            // Pick the first supported file; if none, bail
            string? selected =
                files.FirstOrDefault(f => IsSupportedLoadExtension(Path.GetExtension(f)));

            if (string.IsNullOrEmpty(selected))
                return;

            try
            {
                LoadFileIntoInput(selected);
            }
            catch (System.Exception ex)
            {
                MessageBox.Show(
                    this,
                    "Error loading file:\r\n" + ex.Message,
                    "Drag & Drop Load",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        /// <summary>
        /// Central “load this file into txtInput / game name” helper used by drag & drop
        /// and the existing File→Load handlers.
        /// </summary>
        private void LoadFileIntoInput(string path)
        {
            string ext = Path.GetExtension(path).ToLowerInvariant();

            switch (ext)
            {
                case ".cbc":
                    LoadCbcFromPath(path);
                    break;

                case ".bin":
                    LoadArmaxBinFromPath(path);
                    break;

                case ".p2m":
                    LoadP2mFromPath(path);
                    break;

                case ".txt":
                default:
                    LoadTextFromPath(path);
                    break;
            }
        }
    }
}
