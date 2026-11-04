using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Threading;
using Auraminus.Services;
using MessageBox = System.Windows.MessageBox;

namespace Auraminus.Wpf;

public partial class MainWindow : Window
{
    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(nint window, int attribute, ref int value, int size);
    private readonly RouteSettingsStore _settings = new();
    private readonly DispatcherTimer _telemetry = new() { Interval = TimeSpan.FromMilliseconds(500) };
    private readonly DispatcherTimer _saveTimer = new() { Interval = TimeSpan.FromMilliseconds(600) };
    private readonly TrayIconManager _tray;
    private AudioEngineService? _engine;
    private bool _loading;
    private bool _exiting;
    public ObservableCollection<DeviceItem> Devices { get; } = new();

    public MainWindow()
    {
        InitializeComponent();
        DevicesList.ItemsSource = Devices;
        _tray = new TrayIconManager(this);
        _telemetry.Tick += OnTelemetry;
        _saveTimer.Tick += (_, _) => SaveMix();
    }

    private void OnWindowLoaded(object sender, RoutedEventArgs e)
    {
        int dark = 1;
        DwmSetWindowAttribute(new WindowInteropHelper(this).Handle, 20, ref dark, sizeof(int));
        try { _settings.Load(); }
        catch (Exception ex) { TelemetryText.Text = $"Could not load saved mix: {ex.Message}"; }
        try
        {
            _engine = new AudioEngineService();
            _engine.DeviceEventOccurred += OnDeviceEvent;
            LoadDevices();
        }
        catch (Exception ex)
        {
            StatusText.Text = $"Audio engine unavailable: {ex.Message}";
            ActionBtn.IsEnabled = false;
        }
    }

    private void OnDeviceEvent(string id, DeviceEventType type)
    {
        Dispatcher.BeginInvoke(() =>
        {
            if (_exiting) return;
            LoadDevices();
            UpdateSessionUi();
        });
    }

    private void LoadDevices()
    {
        if (_engine is null) return;
        try
        {
            _settings.Remember(Devices);
            _engine.RefreshDevices();
            var endpoints = _engine.GetDevices().Where(d => d.IsActive && d.SupportsRender).ToList();
            _loading = true;
            foreach (var old in Devices) old.PropertyChanged -= OnMixChanged;
            Devices.Clear();
            foreach (var endpoint in endpoints)
            {
                _settings.Devices.TryGetValue(endpoint.Id, out var saved);
                var item = new DeviceItem
                {
                    Id = endpoint.Id, Name = endpoint.Name, IsDefault = endpoint.IsDefault,
                    IsSelected = saved?.Selected ?? endpoint.IsDefault,
                    VolumePercent = saved?.Volume ?? 100, DelayMs = saved?.Delay ?? 0
                };
                item.PropertyChanged += OnMixChanged;
                Devices.Add(item);
            }
            EmptyText.Visibility = Devices.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
            ActionBtn.IsEnabled = Devices.Count > 0 || _engine.IsRunning;
            UpdateSelection();
            UpdateSessionUi();
        }
        catch (Exception ex) { Report(ex); }
        finally { _loading = false; }
    }

    private void OnMixChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (_loading) return;
        UpdateSelection();
        _saveTimer.Stop();
        _saveTimer.Start();
    }

    private void UpdateSelection() => SelectionText.Text =
        $"{Devices.Count(d => d.IsSelected)} selected · {Devices.Count} available";

    private void SaveMix()
    {
        _saveTimer.Stop();
        try
        {
            _settings.Remember(Devices);
            _settings.Save();
            TelemetryText.Text = "Mix saved on this computer.";
        }
        catch (Exception ex) { TelemetryText.Text = $"Could not save mix: {ex.Message}"; }
    }

    private void ApplyMix(DeviceItem item)
    {
        _engine!.AddOutput(item.Id);
        if (item.IsDefault) return; // Windows owns the source's gain and timing.
        _engine.SetVolume(item.Id, (float)(item.VolumePercent / 100));
        _engine.SetDelay(item.Id, (uint)item.DelayMs);
    }

    private void OnActionClick(object sender, RoutedEventArgs e)
    {
        if (_engine is null) return;
        try
        {
            if (_engine.IsRunning) _engine.Stop();
            else
            {
                var selected = Devices.Where(d => d.IsSelected).ToList();
                if (selected.Count == 0)
                {
                    StatusText.Text = "Choose at least one output to begin.";
                    return;
                }
                // Clear any partially staged route from a previous failed start.
                _engine.Stop();
                try
                {
                    foreach (var device in selected) ApplyMix(device);
                    _engine.Start();
                }
                catch { _engine.Stop(); throw; }
            }
            UpdateSessionUi();
        }
        catch (Exception ex) { Report(ex); }
    }

    private void UpdateSessionUi()
    {
        bool running = _engine?.IsRunning == true;
        ActionBtnText.Text = running ? "Stop session" : "Start session";
        StatusText.Text = running ? "Session live · system audio is being shared." : "Ready · choose outputs and start your session.";
        _tray.UpdateSyncState(running);
        if (running) _telemetry.Start();
        else { _telemetry.Stop(); TelemetryText.Text = "No audio session running."; }
    }

    private void OnDeviceSelectionChanged(object sender, RoutedEventArgs e)
    {
        if (_loading || _engine?.IsRunning != true || sender is not FrameworkElement { DataContext: DeviceItem item }) return;
        try
        {
            if (item.IsSelected) ApplyMix(item);
            else _engine.RemoveOutput(item.Id);
        }
        catch (Exception ex)
        {
            item.IsSelected = !item.IsSelected;
            Report(ex);
        }
    }

    private void OnVolumeChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_loading) return;
        if (sender is FrameworkElement { DataContext: DeviceItem item }) item.VolumePercent = e.NewValue;
        AdjustDevice(sender);
    }
    private void OnDelayChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_loading) return;
        if (sender is FrameworkElement { DataContext: DeviceItem item }) item.DelayMs = e.NewValue;
        AdjustDevice(sender);
    }
    private void AdjustDevice(object sender)
    {
        if (_loading || _engine?.IsRunning != true || sender is not FrameworkElement { DataContext: DeviceItem item } || !item.IsSelected || item.IsDefault) return;
        try
        {
            _engine.SetVolume(item.Id, (float)(item.VolumePercent / 100));
            _engine.SetDelay(item.Id, (uint)item.DelayMs);
        }
        catch (Exception ex) { Report(ex); }
    }

    private void OnTelemetry(object? sender, EventArgs e)
    {
        try
        {
            var status = _engine!.GetStatus();
            if (!status.IsRunning) { UpdateSessionUi(); return; }
            TelemetryText.Text = $"{status.CaptureSampleRate:N0} Hz · {status.OutputCount} secondary outputs · {status.FramesCaptured:N0} frames\n{status.BufferUnderruns} underruns · {status.BufferOverruns} overruns";
        }
        catch (Exception ex) { _telemetry.Stop(); Report(ex); }
    }

    private void Report(Exception ex) => StatusText.Text = ex.Message;
    private void OnRefreshClick(object sender, RoutedEventArgs e) => LoadDevices();
    private void OnSaveClick(object sender, RoutedEventArgs e) => SaveMix();
    public void ToggleSync() => Dispatcher.Invoke(() => OnActionClick(this, new RoutedEventArgs()));
    public void ExitApplication() { _exiting = true; Close(); }
    protected override void OnClosing(CancelEventArgs e)
    {
        SaveMix();
        if (!_exiting)
        {
            e.Cancel = true;
            Hide();
            _tray.NotifyMinimizedToTray();
        }
        base.OnClosing(e);
    }
    protected override void OnClosed(EventArgs e)
    {
        _exiting = true;
        _telemetry.Stop();
        _saveTimer.Stop();
        if (_engine is not null)
        {
            _engine.DeviceEventOccurred -= OnDeviceEvent;
            _engine.Dispose();
        }
        _tray.Dispose();
        base.OnClosed(e);
        System.Windows.Application.Current.Shutdown();
    }
}
