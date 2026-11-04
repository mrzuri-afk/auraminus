using System.IO;
using System.Text.Json;

namespace Auraminus.Wpf;

public sealed record DeviceSettings(bool Selected, double Volume, double Delay);

/// <summary>Retains disconnected endpoints so their mix survives a reconnect.</summary>
public sealed class RouteSettingsStore
{
    private readonly string _path;
    public Dictionary<string, DeviceSettings> Devices { get; private set; } = new();
    public RouteSettingsStore(string? path = null)
    {
        _path = path ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Auraminus", "routing.json");
    }
    public void Load()
    {
        if (!File.Exists(_path)) return;
        try
        {
            Devices = JsonSerializer.Deserialize<Dictionary<string, DeviceSettings>>(File.ReadAllText(_path)) ?? new();
            foreach (var id in Devices.Keys.ToArray())
            {
                var value = Devices[id];
                if (value is null || !double.IsFinite(value.Volume) || !double.IsFinite(value.Delay))
                    Devices.Remove(id);
                else Devices[id] = value with { Volume = Math.Clamp(value.Volume, 0, 100), Delay = Math.Clamp(value.Delay, 0, 500) };
            }
        }
        catch (JsonException) { Devices = new(); }
    }
    public void Remember(IEnumerable<DeviceItem> devices)
    {
        foreach (var device in devices)
            Devices[device.Id] = new(device.IsSelected, device.VolumePercent, device.DelayMs);
    }
    public void Save()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(_path))!);
        var temporary = _path + ".tmp";
        File.WriteAllText(temporary, JsonSerializer.Serialize(Devices, new JsonSerializerOptions { WriteIndented = true }));
        File.Move(temporary, _path, overwrite: true);
    }
}
