using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace Auraminus.Wpf;

public sealed class DeviceItem : INotifyPropertyChanged
{
    private bool _selected;
    private double _volume = 100;
    private double _delay;
    public required string Id { get; init; }
    public required string Name { get; init; }
    public bool IsDefault { get; init; }
    public string Role => IsDefault ? "WINDOWS SOURCE · DIRECT PLAYBACK" : "SECONDARY OUTPUT";
    public bool CanAdjust => !IsDefault;
    public bool IsSelected { get => _selected; set => Set(ref _selected, value); }
    public double VolumePercent { get => _volume; set => Set(ref _volume, Math.Clamp(value, 0, 100)); }
    public double DelayMs { get => _delay; set => Set(ref _delay, Math.Round(Math.Clamp(value, 0, 500))); }
    public event PropertyChangedEventHandler? PropertyChanged;
    private void Set<T>(ref T field, T value, [CallerMemberName] string? property = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return;
        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(property));
    }
}
