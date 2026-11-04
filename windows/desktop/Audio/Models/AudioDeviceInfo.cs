namespace Auraminus.Models;

/// <summary>
/// Managed representation of an audio output device.
/// </summary>
public class AudioDeviceInfo
{
    /// <summary>Windows endpoint device ID (unique identifier).</summary>
    public string Id { get; init; } = string.Empty;

    /// <summary>Friendly display name.</summary>
    public string Name { get; init; } = string.Empty;

    /// <summary>True if this is the Windows default audio device.</summary>
    public bool IsDefault { get; init; }

    /// <summary>True if the device is currently active/connected.</summary>
    public bool IsActive { get; init; }

    /// <summary>True if the device supports audio rendering (output).</summary>
    public bool SupportsRender { get; init; }
}

/// <summary>
/// Engine status snapshot.
/// </summary>
public class EngineStatus
{
    public bool IsRunning { get; init; }
    public int CaptureSampleRate { get; init; }
    public int CaptureChannels { get; init; }
    public int OutputCount { get; init; }
    public ulong FramesCaptured { get; init; }
    public ulong BufferUnderruns { get; init; }
    public ulong BufferOverruns { get; init; }
}
