using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Windows.Forms;
using Microsoft.Win32;

namespace IridescentFlowerSetup
{
    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            if (args.Length > 0 && args[0].Equals("/uninstall", StringComparison.OrdinalIgnoreCase))
            {
                Application.Run(new UninstallForm());
                return;
            }

            Application.Run(new SetupForm());
        }
    }

    internal sealed class SetupForm : Form
    {
        private readonly TextBox destinationBox;
        private readonly CheckBox customModulesBox;
        private readonly Button installButton;
        private readonly Button browseButton;
        private readonly Label statusLabel;
        private readonly ProgressBar progressBar;
        private bool installed;

        public SetupForm()
        {
            Text = "Iridescent Flower 1.1.0 Setup";
            ClientSize = new Size(720, 430);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(20, 16, 28);
            ForeColor = Color.FromArgb(245, 234, 255);
            Font = new Font("Segoe UI", 9.5f);

            var title = new Label
            {
                Text = "IRIDESCENT FLOWER",
                Font = new Font("Segoe UI Semibold", 24f),
                ForeColor = Color.FromArgb(245, 234, 255),
                AutoSize = true,
                Location = new Point(28, 24)
            };
            Controls.Add(title);

            var subtitle = new Label
            {
                Text = "Windows x64 VST3 · Version 1.1.0",
                ForeColor = Color.FromArgb(180, 150, 210),
                AutoSize = true,
                Location = new Point(31, 68)
            };
            Controls.Add(subtitle);

            var description = new Label
            {
                Text = "This wizard installs the plugin and, optionally, the BBD Delay custom module.\r\n" +
                       "The .vst3 bundle is copied as a folder. Re-scan VST3 plugins in your host afterwards.",
                ForeColor = Color.FromArgb(205, 195, 215),
                AutoSize = true,
                Location = new Point(31, 103)
            };
            Controls.Add(description);

            var pathLabel = new Label
            {
                Text = "VST3 install folder",
                ForeColor = Color.FromArgb(205, 195, 215),
                AutoSize = true,
                Location = new Point(31, 157)
            };
            Controls.Add(pathLabel);

            destinationBox = new TextBox
            {
                Location = new Point(31, 181),
                Width = 550,
                BackColor = Color.FromArgb(10, 7, 16),
                ForeColor = Color.FromArgb(245, 234, 255),
                BorderStyle = BorderStyle.FixedSingle,
                Text = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
                                    "Common Files", "VST3")
            };
            Controls.Add(destinationBox);

            browseButton = new Button
            {
                Text = "Browse...",
                Location = new Point(591, 180),
                Size = new Size(98, 26)
            };
            browseButton.Click += (s, e) => BrowseForFolder();
            Controls.Add(browseButton);

            customModulesBox = new CheckBox
            {
                Text = "Install example BBD Delay custom module",
                Checked = true,
                AutoSize = true,
                Location = new Point(31, 226),
                ForeColor = Color.FromArgb(205, 195, 215)
            };
            Controls.Add(customModulesBox);

            var note = new Label
            {
                Text = "Default destination: C:\\Program Files\\Common Files\\VST3 (administrator required).",
                ForeColor = Color.FromArgb(150, 125, 175),
                AutoSize = true,
                Location = new Point(31, 258)
            };
            Controls.Add(note);

            progressBar = new ProgressBar
            {
                Location = new Point(31, 300),
                Size = new Size(658, 12),
                Style = ProgressBarStyle.Marquee,
                MarqueeAnimationSpeed = 25,
                Visible = false
            };
            Controls.Add(progressBar);

            statusLabel = new Label
            {
                Text = "Ready.",
                ForeColor = Color.FromArgb(205, 195, 215),
                AutoSize = true,
                Location = new Point(31, 322)
            };
            Controls.Add(statusLabel);

            installButton = new Button
            {
                Text = "Install",
                Location = new Point(575, 366),
                Size = new Size(114, 34),
                BackColor = Color.FromArgb(70, 30, 95),
                ForeColor = Color.FromArgb(245, 234, 255),
                FlatStyle = FlatStyle.Flat
            };
            installButton.FlatAppearance.BorderColor = Color.FromArgb(200, 120, 255);
            installButton.Click += (s, e) => Install();
            Controls.Add(installButton);

            var cancelButton = new Button
            {
                Text = "Cancel",
                Location = new Point(470, 366),
                Size = new Size(94, 34),
                FlatStyle = FlatStyle.Flat
            };
            cancelButton.Click += (s, e) => Close();
            Controls.Add(cancelButton);
        }

        private void BrowseForFolder()
        {
            using (var dialog = new FolderBrowserDialog())
            {
                dialog.Description = "Select the VST3 folder used by your host.";
                dialog.SelectedPath = destinationBox.Text;

                if (dialog.ShowDialog(this) == DialogResult.OK)
                    destinationBox.Text = dialog.SelectedPath;
            }
        }

        private void Install()
        {
            if (installed)
            {
                Close();
                return;
            }

            var destination = destinationBox.Text.Trim();

            if (destination.Length == 0)
            {
                MessageBox.Show(this, "Choose a VST3 install folder.", "Iridescent Flower",
                                MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                installButton.Enabled = false;
                browseButton.Enabled = false;
                progressBar.Visible = true;
                statusLabel.Text = "Extracting package...";
                Application.DoEvents();

                var temp = Path.Combine(Path.GetTempPath(), "IridescentFlowerSetup_" + Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(temp);

                using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("IridescentPayload.zip"))
                {
                    if (stream == null)
                        throw new InvalidOperationException("Installer payload is missing.");

                    using (var archive = new ZipArchive(stream, ZipArchiveMode.Read))
                        archive.ExtractToDirectory(temp);
                }

                var source = Path.Combine(temp, "Iridescent Flower.vst3");
                if (!Directory.Exists(source))
                    throw new InvalidOperationException("The package does not contain Iridescent Flower.vst3.");

                Directory.CreateDirectory(destination);
                var target = Path.Combine(destination, "Iridescent Flower.vst3");

                if (Directory.Exists(target))
                    Directory.Delete(target, true);

                statusLabel.Text = "Installing VST3...";
                Application.DoEvents();
                CopyDirectory(source, target);

                if (customModulesBox.Checked)
                {
                    var customSource = Path.Combine(temp, "Custom Modules");
                    var customTarget = Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
                        "Iridescent Flower", "Custom Modules");

                    if (Directory.Exists(customSource))
                    {
                        statusLabel.Text = "Installing custom modules...";
                        Application.DoEvents();
                        Directory.CreateDirectory(customTarget);
                        CopyDirectory(customSource, customTarget);
                    }
                }

                RegisterUninstaller(target);

                try { Directory.Delete(temp, true); } catch { }

                progressBar.Visible = false;
                statusLabel.Text = "Installed successfully.";
                installButton.Text = "Close";
                installButton.Enabled = true;
                installed = true;

                MessageBox.Show(this,
                    "Iridescent Flower 1.1.0 has been installed.\r\n\r\n" +
                    "VST3: " + target + "\r\n\r\n" +
                    "Re-scan VST3 plugins in your host, then load:\r\n" +
                    "LeGui Studio -> Iridescent Flower",
                    "Installation complete", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                progressBar.Visible = false;
                installButton.Enabled = true;
                browseButton.Enabled = true;
                statusLabel.Text = "Installation failed.";
                MessageBox.Show(this, ex.Message, "Installation failed",
                                MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private static void CopyDirectory(string source, string destination)
        {
            Directory.CreateDirectory(destination);

            foreach (var file in Directory.GetFiles(source))
                File.Copy(file, Path.Combine(destination, Path.GetFileName(file)), true);

            foreach (var directory in Directory.GetDirectories(source))
                CopyDirectory(directory, Path.Combine(destination, Path.GetFileName(directory)));
        }

        private static void RegisterUninstaller(string vst3Target)
        {
            try
            {
                var installRoot = Path.Combine(vst3Target, "Uninstall");
                Directory.CreateDirectory(installRoot);
                var uninstaller = Path.Combine(installRoot, "Uninstall.exe");
                if (!string.Equals(Path.GetFullPath(Application.ExecutablePath),
                                   Path.GetFullPath(uninstaller), StringComparison.OrdinalIgnoreCase))
                    File.Copy(Application.ExecutablePath, uninstaller, true);

                using (var key = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64)
                           .CreateSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\IridescentFlower"))
                {
                    key.SetValue("DisplayName", "Iridescent Flower 1.1.0");
                    key.SetValue("DisplayVersion", "1.1.0");
                    key.SetValue("Publisher", "LeGui Studio");
                    key.SetValue("InstallLocation", vst3Target);
                    key.SetValue("DisplayIcon", uninstaller);
                    key.SetValue("UninstallString", "\"" + uninstaller + "\" /uninstall");
                    key.SetValue("NoModify", 1, RegistryValueKind.DWord);
                    key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
                }
            }
            catch
            {
                // Registry/uninstaller registration is optional; the plugin is already usable.
            }
        }
    }

    internal sealed class UninstallForm : Form
    {
        public UninstallForm()
        {
            Text = "Iridescent Flower 1.1.0 Uninstall";
            ClientSize = new Size(540, 220);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(20, 16, 28);
            ForeColor = Color.FromArgb(245, 234, 255);
            Font = new Font("Segoe UI", 9.5f);

            Controls.Add(new Label
            {
                Text = "Remove Iridescent Flower 1.1.0?",
                Font = new Font("Segoe UI Semibold", 16f),
                AutoSize = true,
                Location = new Point(26, 24)
            });

            Controls.Add(new Label
            {
                Text = "The VST3 plugin will be removed. User presets, avatars and custom modules are kept.",
                AutoSize = true,
                MaximumSize = new Size(485, 0),
                Location = new Point(29, 74)
            });

            var remove = new Button
            {
                Text = "Uninstall",
                Location = new Point(392, 160),
                Size = new Size(112, 32),
                FlatStyle = FlatStyle.Flat
            };
            remove.Click += (s, e) => Uninstall();
            Controls.Add(remove);

            var cancel = new Button
            {
                Text = "Cancel",
                Location = new Point(288, 160),
                Size = new Size(94, 32),
                FlatStyle = FlatStyle.Flat
            };
            cancel.Click += (s, e) => Close();
            Controls.Add(cancel);
        }

        private void Uninstall()
        {
            try
            {
                var uninstallDir = Path.GetDirectoryName(Application.ExecutablePath);
                var folder = Directory.GetParent(uninstallDir).FullName;

                RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64)
                    .DeleteSubKeyTree(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\IridescentFlower", false);

                // The uninstaller lives inside the plugin folder, so let cmd.exe
                // remove the folder after this process exits.
                var command = "/c ping 127.0.0.1 -n 3 >nul & rmdir /s /q \"" + folder + "\"";
                Process.Start(new ProcessStartInfo("cmd.exe", command)
                {
                    CreateNoWindow = true,
                    UseShellExecute = false
                });

                MessageBox.Show(this, "Iridescent Flower has been removed.", "Uninstall complete",
                                MessageBoxButtons.OK, MessageBoxIcon.Information);
                Close();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, ex.Message, "Uninstall failed",
                                MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
