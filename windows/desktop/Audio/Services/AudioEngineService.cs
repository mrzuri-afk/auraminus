using Auraminus.Interop;
using Auraminus.Models;

namespace Auraminus.Services;

/// <summary>
/// Audio engine service implementation.
/// Wraps P/Invoke calls and converts native types to managed types.
/// Maps native error codes to exceptions with user-friendly messages.
/// </summary>
public class AudioEngineService : IAudioEngineService
{
    private nint _engine;
    private bool _disposed;
    private readonly AudioEngineNative.NativeDeviceEventCallback _nativeCallback;

    public bool IsRunning { get; private set; }

    public event Action<string, DeviceEventType>? DeviceEventOccurred;

    public AudioEngineService()
    {
        int result = AudioEngineNative.EngineCreate(out _engine);
        ThrowIfError(result, "Failed to create audio engine");

        _nativeCallback = (deviceId, eventType) =>
        {
            SyncRunningState();
            DeviceEventOccurred?.Invoke(deviceId, (DeviceEventType)eventType);
        };
        AudioEngineNative.SetDeviceEventCallback(_engine, _nativeCallback);
    }

    private void SyncRunningState()
    {
        if (_disposed || _engine == 0) return;
        try
        {
            if (AudioEngineNative.GetStatus(_engine, out var status) == 0)
            {
                IsRunning = status.IsRunning != 0;
            }
        }
        catch
        {
            // Non-fatal — UI will reconcile on next telemetry tick
        }
    }

    public void Start()
    {
        EnsureNotDisposed();
        int result = AudioEngineNative.EngineStart(_engine);
        ThrowIfError(result, "Failed to start audio engine");
        IsRunning = true;
    }

    public void Stop()
    {
        EnsureNotDisposed();
        int result = AudioEngineNative.EngineStop(_engine);
        ThrowIfError(result, "Failed to stop audio engine");
        IsRunning = false;
    }

    public List<AudioDeviceInfo> GetDevices()
    {
        EnsureNotDisposed();

        var nativeDevices = new NativeDeviceInfo[32]; // Max 32 devices
        int result = AudioEngineNative.GetDevices(_engine, nativeDevices, nativeDevices.Length, out int count);
        ThrowIfError(result, "Failed to enumerate audio devices");

        var devices = new List<AudioDeviceInfo>(count);
        for (int i = 0; i < count; i++)
        {
            devices.Add(new AudioDeviceInfo
            {
                Id = nativeDevices[i].Id ?? string.Empty,
                Name = nativeDevices[i].Name ?? string.Empty,
                IsDefault = nativeDevices[i].IsDefault != 0,
                IsActive = nativeDevices[i].IsActive != 0,
                SupportsRender = nativeDevices[i].SupportsRender != 0
            });
        }

        return devices;
    }

    public void RefreshDevices()
    {
        EnsureNotDisposed();
        int result = AudioEngineNative.RefreshDevices(_engine);
        ThrowIfError(result, "Failed to refresh device list");
    }

    public void AddOutput(string deviceId)
    {
        EnsureNotDisposed();
        int result = AudioEngineNative.AddOutput(_engine, deviceId);
        ThrowIfError(result, $"Failed to add output device");
    }

    public void RemoveOutput(string deviceId)
    {
        EnsureNotDisposed();
        int result = AudioEngineNative.RemoveOutput(_engine, deviceId);
        ThrowIfError(result, $"Failed to remove output device");
    }

    public void SetVolume(string deviceId, float volume)
    {
        EnsureNotDisposed();
        int result = AudioEngineNative.SetVolume(_engine, deviceId, volume);
        ThrowIfError(result, "Failed to set volume");
    }

    public void SetDelay(string deviceId, uint delayMs)
    {
        EnsureNotDisposed();
        int result = AudioEngineNative.SetDelay(_engine, deviceId, delayMs);
        ThrowIfError(result, "Failed to set synchronization delay");
    }

    public uint GetDelay(string deviceId)
    {
        EnsureNotDisposed();
        int result = AudioEngineNative.GetDelay(_engine, deviceId, out uint delayMs);
        ThrowIfError(result, "Failed to get synchronization delay");
        return delayMs;
    }

    public EngineStatus GetStatus()
    {
        EnsureNotDisposed();
        int result = AudioEngineNative.GetStatus(_engine, out var nativeStatus);
        ThrowIfError(result, "Failed to get engine status");

        IsRunning = nativeStatus.IsRunning != 0;

        return new EngineStatus
        {
            IsRunning = nativeStatus.IsRunning != 0,
            CaptureSampleRate = nativeStatus.CaptureSampleRate,
            CaptureChannels = nativeStatus.CaptureChannels,
            OutputCount = nativeStatus.OutputCount,
            FramesCaptured = nativeStatus.FramesCaptured,
            BufferUnderruns = nativeStatus.BufferUnderruns,
            BufferOverruns = nativeStatus.BufferOverruns
        };
    }

    // ── Error Handling ──────────────────────────────────────

    private static void ThrowIfError(int errorCode, string context)
    {
        if (errorCode == 0) return; // AURAMINUS_OK

        string message = errorCode switch
        {
            1 => $"{context}: Invalid argument.",
            2 => $"{context}: Device not found.",
            3 => $"{context}: Device disconnected.",
            4 => $"{context}: Unsupported audio format.",
            5 => $"{context}: Audio client error.",
            6 => $"{context}: Buffer error.",
            7 => $"{context}: Audio engine is already running.",
            8 => $"{context}: Audio engine is not running.",
            9 => $"{context}: COM initialization error.",
            _ => $"{context}: Unknown error (code {errorCode})."
        };

        throw new AudioEngineException(errorCode, message);
    }

    // ── Disposal ────────────────────────────────────────────

    private void EnsureNotDisposed()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;

        if (_engine != 0)
        {
            AudioEngineNative.SetDeviceEventCallback(_engine, null);
            AudioEngineNative.EngineDestroy(_engine);
            _engine = 0;
        }

        GC.SuppressFinalize(this);
    }

    ~AudioEngineService()
    {
        Dispose();
    }
}

/// <summary>
/// Exception type for audio engine errors.
/// </summary>
public class AudioEngineException : Exception
{
    public int NativeErrorCode { get; }

    public AudioEngineException(int errorCode, string message) : base(message)
    {
        NativeErrorCode = errorCode;
    }
}
