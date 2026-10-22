using System;
using System.IO;
using System.Threading;

public static class MockDiagnosticNative
{
    public static int Main(string[] args)
    {
        string scenario = Environment.GetEnvironmentVariable(
            "LECS65_DIAGNOSTIC_MOCK_SCENARIO") ?? "Success";
        string counter = Environment.GetEnvironmentVariable(
            "LECS65_DIAGNOSTIC_MOCK_COUNTER");
        bool install = Array.IndexOf(args, "/add-driver") >= 0;
        bool restart = Array.IndexOf(args, "/restart-device") >= 0;
        Console.WriteLine("MOCK ARGS: " + String.Join("|", args));
        if (!String.IsNullOrEmpty(counter))
            File.AppendAllText(counter, install ? "install\r\n" :
                (restart ? "restart\r\n" : "capture\r\n"));

        if (install || restart)
        {
            Console.WriteLine("MOCK PNPUTIL INVOCATION");
            return scenario == "PnPUtilFailure" ? 55 : 0;
        }

        Console.WriteLine("MOCK DEBUGVIEW INVOCATION");
        if (scenario == "DebugViewStartupFailure")
            return 41;

        string rawLog = null;
        for (int i = 0; i + 1 < args.Length; ++i)
            if (args[i] == "--log") rawLog = args[i + 1];
        if (scenario != "MissingCaptureReady")
            Console.WriteLine("MOCK_KERNEL: independently attributable record");
        if (scenario == "DelayedFileFlush") {
            Thread.Sleep(1200);
            if (rawLog != null) File.WriteAllText(rawLog, "MOCK KERNEL RECORD\r\n");
        }
        else if (scenario != "MissingCaptureReady" && rawLog != null)
            File.WriteAllText(rawLog, "MOCK KERNEL RECORD\r\n");

        if (scenario == "UnexpectedCaptureExit")
            return 42;
        if (scenario == "CaptureTimeout" || scenario == "MissingCaptureReady")
            Thread.Sleep(15000);
        else
            Thread.Sleep(800);
        return 0;
    }
}
