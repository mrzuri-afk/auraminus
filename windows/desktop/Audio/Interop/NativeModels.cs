using System.Runtime.InteropServices;

namespace Auraminus.Interop;

/// <summary>
/// Native struct matching AuraminusDeviceInfo from auraminus_engine.h.
/// Used for P/Invoke marshaling.
/// </summary>
[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct NativeDeviceInfo
{
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
    public string Id;

    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
    public string Name;

    public int IsDefault;
    public int IsActive;
    public int SupportsRender;
}

/// <summary>
/// Native struct matching AuraminusEngineStatus from auraminus_engine.h.
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct NativeEngineStatus
{
    public int IsRunning;
    public int CaptureSampleRate;
    public int CaptureChannels;
    public int OutputCount;
    public ulong FramesCaptured;
    public ulong BufferUnderruns;
    public ulong BufferOverruns;
}
