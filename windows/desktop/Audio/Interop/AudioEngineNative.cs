using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

namespace Auraminus.Interop;

/// <summary>
/// P/Invoke declarations for AuraminusEngine.dll.
/// Maps 1:1 to the C API in auraminus_engine.h.
///
/// IMPORTANT: ViewModels must NOT call these directly.
/// Use IAudioEngineService instead.
/// </summary>
internal static class AudioEngineNative
{
    private const string DllName = "AuraminusEngine";

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);

    private const uint LOAD_WITH_ALTERED_SEARCH_PATH = 0x00000008;

    private static readonly object LoadLock = new();
    private static IntPtr _nativeModule = IntPtr.Zero;

    static AudioEngineNative()
    {
        NativeLibrary.SetDllImportResolver(typeof(AudioEngineNative).Assembly, ResolveNativeLibrary);
        EnsureNativeLibrary();
    }

    private static IntPtr ResolveNativeLibrary(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (!libraryName.Equals(DllName, StringComparison.OrdinalIgnoreCase) &&
            !libraryName.Equals(DllName + ".dll", StringComparison.OrdinalIgnoreCase))
        {
            return IntPtr.Zero;
        }

        EnsureNativeLibrary();
        return _nativeModule;
    }

    private static void EnsureNativeLibrary()
    {
        lock (LoadLock)
        {
            if (_nativeModule != IntPtr.Zero)
            {
                return;
            }

            string? dllPath = ResolveEngineDllPath();
            if (dllPath == null || !File.Exists(dllPath))
            {
                return;
            }

            _nativeModule = LoadLibraryEx(dllPath, IntPtr.Zero, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
    }

    /// <summary>
    /// Prefer the DLL next to the app (dev builds / installer). Never overwrite a
    /// side-by-side DLL with a stale embedded copy. Extract embedded only when missing.
    /// </summary>
    private static string? ResolveEngineDllPath()
    {
        string baseDir = AppContext.BaseDirectory;
        string localDll = Path.Combine(baseDir, "AuraminusEngine.dll");

        // Side-by-side DLL wins — this is what build scripts / installer deploy.
        if (File.Exists(localDll))
        {
            return localDll;
        }

        byte[]? embedded = ReadEmbeddedDll();
        if (embedded == null)
        {
            return null;
        }

        if (TryWriteVerified(localDll, embedded))
        {
            return localDll;
        }

        string cacheDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Auraminus",
            "native");
        Directory.CreateDirectory(cacheDir);
        string cacheDll = Path.Combine(cacheDir, "AuraminusEngine.dll");
        if (TryWriteVerified(cacheDll, embedded))
        {
            return cacheDll;
        }

        return null;
    }

    private static byte[]? ReadEmbeddedDll()
    {
        try
        {
            var assembly = Assembly.GetEntryAssembly() ?? Assembly.GetExecutingAssembly();
            string? match = assembly.GetManifestResourceNames()
                .FirstOrDefault(n => n.EndsWith("AuraminusEngine.dll", StringComparison.OrdinalIgnoreCase));
            if (match == null) return null;

            using var stream = assembly.GetManifestResourceStream(match);
            if (stream == null) return null;

            using var ms = new MemoryStream();
            stream.CopyTo(ms);
            return ms.ToArray();
        }
        catch
        {
            return null;
        }
    }

    private static bool TryWriteVerified(string targetPath, byte[] payload)
    {
        try
        {
            string? dir = Path.GetDirectoryName(targetPath);
            if (!string.IsNullOrEmpty(dir))
            {
                Directory.CreateDirectory(dir);
            }

            string expectedHash = Convert.ToHexString(SHA256.HashData(payload));

            if (File.Exists(targetPath))
            {
                byte[] existing = File.ReadAllBytes(targetPath);
                string existingHash = Convert.ToHexString(SHA256.HashData(existing));
                if (existingHash.Equals(expectedHash, StringComparison.OrdinalIgnoreCase))
                {
                    return true;
                }
            }

            string tempPath = targetPath + ".tmp";
            File.WriteAllBytes(tempPath, payload);
            File.Copy(tempPath, targetPath, overwrite: true);
            File.Delete(tempPath);

            byte[] written = File.ReadAllBytes(targetPath);
            string writtenHash = Convert.ToHexString(SHA256.HashData(written));
            return writtenHash.Equals(expectedHash, StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
    }

    // ── Lifecycle ──────────────────────────────────────────

    [DllImport(DllName, EntryPoint = "auraminus_engine_create", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int EngineCreate(out nint engine);

    [DllImport(DllName, EntryPoint = "auraminus_engine_destroy", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void EngineDestroy(nint engine);

    [DllImport(DllName, EntryPoint = "auraminus_engine_start", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int EngineStart(nint engine);

    [DllImport(DllName, EntryPoint = "auraminus_engine_stop", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int EngineStop(nint engine);

    // ── Device Management ──────────────────────────────────

    [DllImport(DllName, EntryPoint = "auraminus_get_devices", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int GetDevices(
        nint engine,
        [In, Out] NativeDeviceInfo[] devices,
        int capacity,
        out int count
    );

    [DllImport(DllName, EntryPoint = "auraminus_refresh_devices", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int RefreshDevices(nint engine);

    [DllImport(DllName, EntryPoint = "auraminus_add_output", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int AddOutput(nint engine, [MarshalAs(UnmanagedType.LPUTF8Str)] string deviceId);

    [DllImport(DllName, EntryPoint = "auraminus_remove_output", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int RemoveOutput(nint engine, [MarshalAs(UnmanagedType.LPUTF8Str)] string deviceId);

    // ── Per-Device Controls ────────────────────────────────

    [DllImport(DllName, EntryPoint = "auraminus_set_volume", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int SetVolume(nint engine, [MarshalAs(UnmanagedType.LPUTF8Str)] string deviceId, float volume);

    [DllImport(DllName, EntryPoint = "auraminus_set_device_delay", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int SetDelay(nint engine, [MarshalAs(UnmanagedType.LPUTF8Str)] string deviceId, uint delayMs);

    [DllImport(DllName, EntryPoint = "auraminus_get_device_delay", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int GetDelay(nint engine, [MarshalAs(UnmanagedType.LPUTF8Str)] string deviceId, out uint delayMs);

    // ── Status ─────────────────────────────────────────────

    [DllImport(DllName, EntryPoint = "auraminus_get_status", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int GetStatus(nint engine, out NativeEngineStatus status);

    // ── Device Notifications ────────────────────────────────

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void NativeDeviceEventCallback(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string deviceId,
        int eventType
    );

    [DllImport(DllName, EntryPoint = "auraminus_set_device_event_callback", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int SetDeviceEventCallback(
        nint engine,
        NativeDeviceEventCallback? callback
    );
}
