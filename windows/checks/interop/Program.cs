using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using Auraminus.Services;

Console.Title = "Auraminus 2 — Multi-Speaker Audio Controller";
Console.OutputEncoding = System.Text.Encoding.UTF8;

Console.ForegroundColor = ConsoleColor.Cyan;
Console.WriteLine("╔════════════════════════════════════════════════════════════╗");
Console.WriteLine("║        Auraminus — Sound, in good company.                ║");
Console.WriteLine("║            Developer Audio Console                 ║");
Console.WriteLine("╚════════════════════════════════════════════════════════════╝");
Console.ResetColor();
Console.WriteLine();

try
{
    using var service = new AudioEngineService();

    if (args.Contains("--smoke"))
    {
        service.RefreshDevices();
        var endpoints = service.GetDevices();
        var status = service.GetStatus();
        if (status.IsRunning) throw new Exception("Engine unexpectedly running on creation.");
        service.Stop();
        Console.WriteLine($"PASS: native create, enumerate ({endpoints.Count} devices), status, stop, and dispose.");
        return;
    }

    service.DeviceEventOccurred += (deviceId, eventType) =>
    {
        Console.ForegroundColor = ConsoleColor.Magenta;
        string eventDesc = eventType switch
        {
            DeviceEventType.Added => "Device Connected",
            DeviceEventType.Removed => "Device Disconnected",
            DeviceEventType.StateChanged => "Device State / Power Changed",
            DeviceEventType.DefaultChanged => "Windows Default Audio Device Switched",
            _ => eventType.ToString()
        };
        Console.WriteLine($"\n  ⚡ [HOTPLUG] {eventDesc}");
        Console.ResetColor();
    };

    service.RefreshDevices();
    var devices = service.GetDevices();

    if (devices.Count == 0)
    {
        Console.ForegroundColor = ConsoleColor.Yellow;
        Console.WriteLine("[WARN] No active audio render devices found.");
        Console.ResetColor();
        return;
    }

    Console.WriteLine("Available Audio Output Devices:\n");
    for (int i = 0; i < devices.Count; i++)
    {
        var d = devices[i];
        string def = d.IsDefault ? " [DEFAULT]" : "";
        Console.ForegroundColor = d.IsDefault ? ConsoleColor.Green : ConsoleColor.White;
        Console.WriteLine($"  [{i + 1}] {d.Name}{def}");
    }
    Console.ResetColor();

    Console.WriteLine();
    Console.Write("Enter device numbers to output to (e.g. 1, 2) [default: all]: ");
    string? input = Console.ReadLine();

    var selectedIndices = new List<int>();
    if (string.IsNullOrWhiteSpace(input))
    {
        selectedIndices = Enumerable.Range(0, Math.Min(2, devices.Count)).ToList();
    }
    else
    {
        foreach (var part in input.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            if (int.TryParse(part, out int num) && num >= 1 && num <= devices.Count)
            {
                selectedIndices.Add(num - 1);
            }
        }
    }

    if (selectedIndices.Count == 0)
    {
        Console.WriteLine("No valid devices selected. Exiting.");
        return;
    }

    Console.WriteLine();
    foreach (var idx in selectedIndices)
    {
        var d = devices[idx];
        service.AddOutput(d.Id);
        service.SetVolume(d.Id, 1.0f);
        Console.ForegroundColor = ConsoleColor.Green;
        if (d.IsDefault)
        {
            Console.WriteLine($"  [+] Output: {d.Name} [DEFAULT - Plays Directly from Windows]");
        }
        else
        {
            Console.WriteLine($"  [+] Output: {d.Name} [Synchronized via Auraminus - 100% Volume]");
        }
    }
    Console.ResetColor();

    Console.WriteLine();
    Console.ForegroundColor = ConsoleColor.Yellow;
    Console.WriteLine(">> Starting Auraminus real-time audio pipeline...");
    Console.ResetColor();

    service.Start();

    Console.ForegroundColor = ConsoleColor.Green;
    Console.WriteLine(">> PLAYING! Audio is now synchronized across all selected speakers.");
    Console.WriteLine(">> Press [Enter] or [Q] to stop playback.");
    Console.ResetColor();
    Console.WriteLine();

    // Monitoring loop
    if (Console.IsInputRedirected)
    {
        for (int i = 0; i < 3; i++)
        {
            Thread.Sleep(1000);
            var status = service.GetStatus();
            Console.WriteLine($"[LIVE] Captured: {status.FramesCaptured:N0} frames | Rate: {status.CaptureSampleRate}Hz | Active Outputs: {status.OutputCount} | Underruns: {status.BufferUnderruns}");
        }
    }
    else
    {
        while (!Console.KeyAvailable)
        {
            Thread.Sleep(1000);
            var status = service.GetStatus();
            Console.Write($"\r[LIVE] Captured: {status.FramesCaptured:N0} frames | Rate: {status.CaptureSampleRate}Hz | Active Outputs: {status.OutputCount} | Underruns: {status.BufferUnderruns}   ");
        }
        Console.ReadKey(true); // Consume key
    }
    Console.WriteLine();
    Console.WriteLine("\n>> Stopping audio engine...");
    service.Stop();

    Console.ForegroundColor = ConsoleColor.Cyan;
    Console.WriteLine(">> Stopped cleanly. Thank you for testing Auraminus!");
    Console.ResetColor();
}
catch (Exception ex)
{
    Environment.ExitCode = 1;
    Console.ForegroundColor = ConsoleColor.Red;
    Console.WriteLine($"\n[ERROR] {ex.Message}");
    Console.ResetColor();
}
