using Auraminus.Models;

namespace Auraminus.Services;

/// <summary>
/// Service interface for the audio engine.
/// ViewModels interact with this — never with P/Invoke directly.
/// </summary>
public interface IAudioEngineService : IDisposable
{
    /// <summary>Start audio capture and multi-output playback.</summary>
    void Start();

    /// <summary>Stop audio capture and playback.</summary>
    void Stop();

    /// <summary>Get all available audio output devices.</summary>
    List<AudioDeviceInfo> GetDevices();

    /// <summary>Refresh the device list.</summary>
    void RefreshDevices();

    /// <summary>Add a device to the output list.</summary>
    void AddOutput(string deviceId);

    /// <summary>Remove a device from the output list.</summary>
    void RemoveOutput(string deviceId);

    /// <summary>Set volume for a specific device (0.0 to 1.0).</summary>
    void SetVolume(string deviceId, float volume);

    /// <summary>Set synchronization delay for a specific device in milliseconds (0 to 500 ms).</summary>
    void SetDelay(string deviceId, uint delayMs);

    /// <summary>Get synchronization delay for a specific device in milliseconds.</summary>
    uint GetDelay(string deviceId);

    /// <summary>Get current engine status.</summary>
    EngineStatus GetStatus();

    /// <summary>True if the engine is currently running.</summary>
    bool IsRunning { get; }

    /// <summary>Fired when an audio device is connected, disconnected, or default endpoint changes.</summary>
    event Action<string, DeviceEventType>? DeviceEventOccurred;
}

public enum DeviceEventType
{
    Added = 0,
    Removed = 1,
    StateChanged = 2,
    DefaultChanged = 3
}
