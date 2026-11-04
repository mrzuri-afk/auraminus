using System;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Windows.Forms;
using Application = System.Windows.Application;

namespace Auraminus.Wpf;

public class TrayIconManager : IDisposable
{
    private readonly NotifyIcon _notifyIcon;
    private readonly MainWindow _mainWindow;
    private readonly ToolStripMenuItem _toggleSyncItem;
    private bool _firstMinimizeShown = false;

    public TrayIconManager(MainWindow mainWindow)
    {
        _mainWindow = mainWindow;

        _notifyIcon = new NotifyIcon
        {
            Text = "Auraminus — Sound, in good company.",
            Visible = true
        };

        // Load application icon
        try
        {
            string icoPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "app.ico");
            if (File.Exists(icoPath))
            {
                _notifyIcon.Icon = new Icon(icoPath);
            }
            else
            {
                string exePath = Path.Combine(AppContext.BaseDirectory, "Auraminus.exe");
                if (File.Exists(exePath))
                {
                    _notifyIcon.Icon = Icon.ExtractAssociatedIcon(exePath) ?? SystemIcons.Application;
                }
                else
                {
                    _notifyIcon.Icon = SystemIcons.Application;
                }
            }
        }
        catch
        {
            _notifyIcon.Icon = SystemIcons.Application;
        }

        // Context Menu
        var contextMenu = new ContextMenuStrip();

        var openItem = new ToolStripMenuItem("Open Auraminus")
        {
            Font = new System.Drawing.Font(contextMenu.Font, System.Drawing.FontStyle.Bold)
        };
        openItem.Click += (s, e) => ShowWindow();

        _toggleSyncItem = new ToolStripMenuItem("Start session");
        _toggleSyncItem.Click += (s, e) => _mainWindow.ToggleSync();

        var aboutItem = new ToolStripMenuItem("About");
        aboutItem.Click += (s, e) =>
        {
            System.Windows.Forms.MessageBox.Show(
                "Auraminus — Sound, in good company.\n\n" +
                "Indal Singh\n" +
                "Real-time multi-speaker sync for Windows.\n\n" +
                "Version 2.0.0\nBased on the WaveSync audio engine (MIT).",
                "About Auraminus",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information
            );
        };

        var exitItem = new ToolStripMenuItem("Exit");
        exitItem.Click += (s, e) =>
        {
            _notifyIcon.Visible = false;
            _mainWindow.ExitApplication();
        };

        contextMenu.Items.Add(openItem);
        contextMenu.Items.Add(_toggleSyncItem);
        contextMenu.Items.Add(new ToolStripSeparator());
        contextMenu.Items.Add(aboutItem);
        contextMenu.Items.Add(exitItem);

        _notifyIcon.ContextMenuStrip = contextMenu;

        // Left-click / double-click toggles the window
        _notifyIcon.MouseClick += (s, e) =>
        {
            if (e.Button == MouseButtons.Left)
            {
                ToggleWindow();
            }
        };
    }

    public void UpdateSyncState(bool isRunning)
    {
        if (_toggleSyncItem != null)
        {
            _toggleSyncItem.Text = isRunning ? "Stop session" : "Start session";
        }
        _notifyIcon.Text = isRunning
            ? "Auraminus — Streaming"
            : "Auraminus — Sound, in good company.";
    }

    public void NotifyMinimizedToTray()
    {
        if (!_firstMinimizeShown)
        {
            _firstMinimizeShown = true;
            _notifyIcon.ShowBalloonTip(
                2000,
                "Auraminus",
                "Running in the background. Click the tray icon to open.",
                ToolTipIcon.Info
            );
        }
    }

    public void ShowWindow()
    {
        _mainWindow.Show();
        _mainWindow.WindowState = WindowState.Normal;
        _mainWindow.Activate();
    }

    public void ToggleWindow()
    {
        if (_mainWindow.IsVisible && _mainWindow.WindowState != WindowState.Minimized)
        {
            _mainWindow.Hide();
        }
        else
        {
            ShowWindow();
        }
    }

    public void Dispose()
    {
        _notifyIcon.Visible = false;
        _notifyIcon.Dispose();
    }
}
