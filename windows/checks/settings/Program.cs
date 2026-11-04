using Auraminus.Wpf;

var directory = Path.Combine(AppContext.BaseDirectory, "auraminus-settings-" + Guid.NewGuid());
var path = Path.Combine(directory, "routing.json");
void Check(bool condition, string message) { if (!condition) throw new Exception(message); }
try
{
    var store = new RouteSettingsStore(path);
    store.Load();
    Check(store.Devices.Count == 0, "Missing settings should start empty.");
    var device = new DeviceItem { Id = "speaker-a", Name = "Speaker", IsSelected = true, VolumePercent = 63, DelayMs = 81 };
    store.Remember(new[] { device });
    store.Save();
    var restored = new RouteSettingsStore(path);
    restored.Load();
    Check(restored.Devices[device.Id] == new DeviceSettings(true, 63, 81), "Mix did not round trip.");
    restored.Remember(Array.Empty<DeviceItem>());
    restored.Save();
    Check(restored.Devices.ContainsKey(device.Id), "Disconnected device settings were lost.");
    File.WriteAllText(path, "{broken");
    restored.Load();
    Check(restored.Devices.Count == 0, "Malformed settings should recover.");
    File.WriteAllText(path, "{\"speaker-a\":{\"Selected\":true,\"Volume\":900,\"Delay\":-10},\"bad\":null}");
    restored.Load();
    Check(restored.Devices[device.Id] == new DeviceSettings(true, 100, 0), "Settings limits were not enforced.");
    Check(!restored.Devices.ContainsKey("bad"), "Null settings entry was retained.");
    device.VolumePercent = -20;
    device.DelayMs = 900;
    Check(device.VolumePercent == 0 && device.DelayMs == 500, "Device limits were not enforced.");
    Console.WriteLine("PASS: persistence, disconnected devices, malformed data, and control limits.");
}
finally { if (Directory.Exists(directory)) Directory.Delete(directory, recursive: true); }
