using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace NexusDx1
{
    /// <summary>
    /// Update Analyzer Software Dialog - Semi-transparent draggable window for updating analyzer APK
    /// </summary>
    public class UpdateDialog : Form
    {
        private Panel pnlContent;
        private Panel pnlHeader;
        private Label lblTitle;
        private Button btnClose;
        private Label lblInstruction;
        private Panel pnlRequirements;
        private Label lblRequirements;
        private Panel pnlButtons;
        private ModernUI.ModernButton btnSelectApk;
        private ModernUI.ModernButton btnCancel;
        private Label lblSelectedFile;

        // For dragging
        private bool _isDragging = false;
        private Point _dragStartPoint;

        // ADB Manager
        private readonly AdbManager _adbManager;
        private readonly string _deviceIp;
        private readonly int _devicePort;
        private string _selectedApkPath = string.Empty;

        public UpdateDialog(AdbManager adbManager, string deviceIp, int devicePort)
        {
            _adbManager = adbManager;
            _deviceIp = deviceIp;
            _devicePort = devicePort;
            InitializeComponents();
        }

        private void InitializeComponents()
        {
            // Window settings
            this.Text = "Update Analyzer Software";
            this.Size = new Size(650, 550);
            this.FormBorderStyle = FormBorderStyle.None;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(240, 240, 240); // Border color
            this.Padding = new Padding(1); // Thin border
            this.Opacity = 0.96; // Semi-transparent

            // Non-modal window
            this.ShowInTaskbar = false;
            this.TopMost = false;

            // Rounded corners (Windows 11 style)
            this.Region = CreateRoundedRegion(this.ClientRectangle, 12);

            // Content panel
            pnlContent = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Color.White,
                Padding = new Padding(0)
            };

            // Header panel (draggable)
            pnlHeader = new Panel
            {
                Dock = DockStyle.Top,
                Height = 50,
                BackColor = Color.FromArgb(100, 150, 200),
                Padding = new Padding(20, 0, 10, 0)
            };

            // Add drag functionality
            pnlHeader.MouseDown += PnlHeader_MouseDown;
            pnlHeader.MouseMove += PnlHeader_MouseMove;
            pnlHeader.MouseUp += PnlHeader_MouseUp;

            // Title
            lblTitle = new Label
            {
                Text = "  🔄 Update Analyzer Software",
                Dock = DockStyle.Fill,
                Font = new Font("Segoe UI", 13F, FontStyle.Bold),
                ForeColor = Color.White,
                TextAlign = ContentAlignment.MiddleLeft
            };
            lblTitle.MouseDown += PnlHeader_MouseDown;
            lblTitle.MouseMove += PnlHeader_MouseMove;
            lblTitle.MouseUp += PnlHeader_MouseUp;

            // Close button
            btnClose = new Button
            {
                Text = "✕",
                Dock = DockStyle.Right,
                Width = 50,
                Height = 50,
                FlatStyle = FlatStyle.Flat,
                BackColor = Color.FromArgb(100, 150, 200),
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 16F),
                Cursor = Cursors.Hand
            };
            btnClose.FlatAppearance.BorderSize = 0;
            btnClose.FlatAppearance.MouseOverBackColor = Color.FromArgb(220, 100, 100);
            btnClose.Click += (s, e) => this.Close();

            // Instruction label
            lblInstruction = new Label
            {
                Text = $"Update Semen Analyzer Software\n({_deviceIp}:{_devicePort})\n\nPlease read the requirements below carefully:",
                Dock = DockStyle.Top,
                Height = 100,
                Font = new Font("Segoe UI", 11F),
                ForeColor = Color.FromArgb(60, 60, 60),
                TextAlign = ContentAlignment.MiddleCenter,
                Padding = new Padding(20)
            };

            // Requirements panel
            pnlRequirements = new Panel
            {
                Dock = DockStyle.Top,
                Height = 250,
                BackColor = Color.FromArgb(255, 250, 240),
                Padding = new Padding(30, 15, 30, 15)
            };

            // Requirements label
            lblRequirements = new Label
            {
                Dock = DockStyle.Fill,
                Font = new Font("Segoe UI", 9.5F),
                ForeColor = Color.FromArgb(200, 80, 40),
                Text = "⚠️ REQUIREMENTS:\n\n" +
                       "1. The APK file for software upgrade must be named iSpermUpdate.apk.\n\n" +
                       "2. Requirements for APK file storage location:\n" +
                       "   2-1 Do not place the file on your PC desktop.\n" +
                       "   2-2 The folder path must contain only English characters;\n" +
                       "         no other languages are allowed.\n\n" +
                       "3. Please save the APK file to a valid folder on your PC before starting.\n\n" +
                       "Example: D:\\ABC\\iSpermUpdate.apk"
            };

            pnlRequirements.Controls.Add(lblRequirements);

            // Selected file label
            lblSelectedFile = new Label
            {
                Text = "No file selected",
                Dock = DockStyle.Top,
                Height = 30,
                Font = new Font("Segoe UI", 9.5F, FontStyle.Italic),
                ForeColor = Color.Gray,
                TextAlign = ContentAlignment.MiddleCenter,
                Padding = new Padding(20, 5, 20, 5)
            };

            // Button panel
            pnlButtons = new Panel
            {
                Dock = DockStyle.Top,
                Height = 70,
                BackColor = Color.White,
                Padding = new Padding(40, 10, 40, 10)
            };

            // Select APK button
            btnSelectApk = new ModernUI.ModernButton
            {
                Text = "📂 Select iSpermUpdate.apk",
                Width = 260,
                Height = 50,
                BackColor = Color.FromArgb(100, 150, 200),
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 11F, FontStyle.Bold),
                Cursor = Cursors.Hand,
                Location = new Point(40, 10),
                BorderRadius = 8,
                BorderColor = Color.FromArgb(80, 130, 180),
                BorderWidth = 2,
                NormalBackColor = Color.FromArgb(100, 150, 200),
                HoverBackColor = Color.FromArgb(120, 170, 220),
                PressedBackColor = Color.FromArgb(80, 130, 180)
            };
            btnSelectApk.Click += BtnSelectApk_Click;

            // Cancel button
            btnCancel = new ModernUI.ModernButton
            {
                Text = "✕ Cancel",
                Width = 200,
                Height = 50,
                BackColor = Color.FromArgb(160, 160, 160),
                ForeColor = Color.White,
                Font = new Font("Segoe UI", 11F, FontStyle.Bold),
                Cursor = Cursors.Hand,
                Location = new Point(330, 10),
                BorderRadius = 8,
                BorderColor = Color.FromArgb(140, 140, 140),
                BorderWidth = 2,
                NormalBackColor = Color.FromArgb(160, 160, 160),
                HoverBackColor = Color.FromArgb(180, 180, 180),
                PressedBackColor = Color.FromArgb(140, 140, 140)
            };
            btnCancel.Click += (s, e) => this.Close();

            // Assembly
            pnlHeader.Controls.Add(lblTitle);
            pnlHeader.Controls.Add(btnClose);

            pnlButtons.Controls.Add(btnSelectApk);
            pnlButtons.Controls.Add(btnCancel);

            pnlContent.Controls.Add(pnlButtons);
            pnlContent.Controls.Add(lblSelectedFile);
            pnlContent.Controls.Add(pnlRequirements);
            pnlContent.Controls.Add(lblInstruction);

            this.Controls.Add(pnlContent);
            this.Controls.Add(pnlHeader);

            // Handle resize to maintain rounded corners
            this.Resize += (s, e) => this.Region = CreateRoundedRegion(this.ClientRectangle, 12);
        }

        /// <summary>
        /// Select APK file button click handler
        /// </summary>
        private async void BtnSelectApk_Click(object? sender, EventArgs e)
        {
            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.Title = "Select iSpermUpdate.apk";
                openFileDialog.Filter = "APK Files (*.apk)|*.apk";
                openFileDialog.FileName = "iSpermUpdate.apk";
                openFileDialog.CheckFileExists = true;

                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    string selectedFile = openFileDialog.FileName;
                    string fileName = Path.GetFileName(selectedFile);
                    string directoryPath = Path.GetDirectoryName(selectedFile) ?? "";

                    // Validate file name
                    if (!fileName.Equals("iSpermUpdate.apk", StringComparison.OrdinalIgnoreCase))
                    {
                        MessageBox.Show(
                            "The selected file must be named 'iSpermUpdate.apk'.\n\nPlease rename the file and try again.",
                            "Invalid File Name",
                            MessageBoxButtons.OK,
                            MessageBoxIcon.Warning
                        );
                        return;
                    }

                    // Validate path contains only English characters
                    if (!IsValidEnglishPath(directoryPath))
                    {
                        MessageBox.Show(
                            "The folder path contains non-English characters.\n\nPlease move the APK file to a folder with only English characters in the path.",
                            "Invalid Path",
                            MessageBoxButtons.OK,
                            MessageBoxIcon.Warning
                        );
                        return;
                    }

                    // Check if file is on desktop
                    string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
                    if (directoryPath.StartsWith(desktopPath, StringComparison.OrdinalIgnoreCase))
                    {
                        MessageBox.Show(
                            "The APK file should not be placed on the Desktop.\n\nPlease move it to another folder (e.g., D:\\ABC\\) and try again.",
                            "Invalid Location",
                            MessageBoxButtons.OK,
                            MessageBoxIcon.Warning
                        );
                        return;
                    }

                    // All validations passed
                    _selectedApkPath = selectedFile;
                    lblSelectedFile.Text = $"Selected: {selectedFile}";
                    lblSelectedFile.ForeColor = Color.FromArgb(34, 139, 34); // Green

                    // Confirm installation
                    var result = MessageBox.Show(
                        $"Are you sure you want to UPDATE the Analyzer Software?\n\n" +
                        $"Device: {_deviceIp}:{_devicePort}\n" +
                        $"APK File: {selectedFile}\n\n" +
                        $"The device will install and restart the application automatically.",
                        "Confirm Update",
                        MessageBoxButtons.YesNo,
                        MessageBoxIcon.Question
                    );

                    if (result == DialogResult.Yes)
                    {
                        await InstallApkAsync();
                    }
                }
            }
        }

        /// <summary>
        /// Install APK to device
        /// </summary>
        private async Task InstallApkAsync()
        {
            try
            {
                btnSelectApk.Enabled = false;
                btnCancel.Enabled = false;

                var commandResult = await _adbManager.InstallApkAsync(_deviceIp, _devicePort, _selectedApkPath);

                if (commandResult.Success)
                {
                    // Delay and close dialog
                    await Task.Delay(2000);
                    this.Close();
                }
                else
                {
                    MessageBox.Show(
                        $"Failed to install APK.\n\nError: {commandResult.ErrorMessage}",
                        "Installation Failed",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error
                    );
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error: {ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                btnSelectApk.Enabled = true;
                btnCancel.Enabled = true;
            }
        }

        /// <summary>
        /// Validate that path contains only English characters
        /// </summary>
        private bool IsValidEnglishPath(string path)
        {
            foreach (char c in path)
            {
                if (c > 127 && c != '\\' && c != '/' && c != ':')
                {
                    return false;
                }
            }
            return true;
        }

        /// <summary>
        /// Append result text
        /// </summary>
        private void AppendResult(string text)
        {
            if (txtResult.InvokeRequired)
            {
                txtResult.Invoke(new Action(() => AppendResult(text)));
                return;
            }

            txtResult.AppendText(text);
            txtResult.SelectionStart = txtResult.Text.Length;
            txtResult.ScrollToCaret();
        }

        /// <summary>
        /// Create rounded region for form
        /// </summary>
        private Region CreateRoundedRegion(Rectangle rect, int radius)
        {
            GraphicsPath path = new GraphicsPath();
            int diameter = radius * 2;

            path.AddArc(rect.X, rect.Y, diameter, diameter, 180, 90);
            path.AddArc(rect.Right - diameter, rect.Y, diameter, diameter, 270, 90);
            path.AddArc(rect.Right - diameter, rect.Bottom - diameter, diameter, diameter, 0, 90);
            path.AddArc(rect.X, rect.Bottom - diameter, diameter, diameter, 90, 90);
            path.CloseFigure();

            return new Region(path);
        }

        // Dragging handlers
        private void PnlHeader_MouseDown(object? sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                _isDragging = true;
                _dragStartPoint = e.Location;
            }
        }

        private void PnlHeader_MouseMove(object? sender, MouseEventArgs e)
        {
            if (_isDragging)
            {
                Point newLocation = this.Location;
                newLocation.X += e.X - _dragStartPoint.X;
                newLocation.Y += e.Y - _dragStartPoint.Y;
                this.Location = newLocation;
            }
        }

        private void PnlHeader_MouseUp(object? sender, MouseEventArgs e)
        {
            _isDragging = false;
        }
    }
}
