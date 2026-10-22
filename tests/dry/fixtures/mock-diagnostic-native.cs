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
        if (scenario == "DelayedStdout")
            Thread.Sleep(500);

        if (scenario == "SplitKernelLine")
        {
            Console.Write("17\t12:34:56.789\t4 - STORM");
            Console.Out.Flush();
            Thread.Sleep(300);
            Console.WriteLine("INI: StorNVMe split record");
        }
        else if (scenario == "IncorrectMessageSource")
            Console.WriteLine("TRACE: arbitrary STORMINI: substring");
        else if (scenario == "PnpStatusPrefix")
            Console.WriteLine("18\t12:34:56.790\t4 - PNP_STATUS: minor=START status=0");
        else if (scenario != "MissingCaptureReady" &&
            scenario != "UnexpectedCaptureExit")
            Console.WriteLine("17\t12:34:56.789\t4 - STORMINI: StorNVMe kernel record");

        if (scenario == "CollectorStderr")
            Console.Error.WriteLine("MOCK COLLECTOR ERROR");

        if (scenario == "LargeKernelBurst")
            for (int i = 0; i < 5000; ++i)
                Console.WriteLine((18 + i).ToString() +
                    "\t12:34:56.790\t4 - STORMINI: StorNVMe burst record");
        if (scenario == "DelayedFileFlush") {
            Thread.Sleep(1200);
            if (rawLog != null) File.WriteAllText(rawLog, "MOCK KERNEL RECORD\r\n");
        }
        else if (scenario != "MissingCaptureReady" && rawLog != null)
            File.WriteAllText(rawLog, "MOCK KERNEL RECORD\r\n");

        if (scenario == "UnexpectedCaptureExit")
            return 42;
        if (scenario == "ExitAfterReadiness") {
            Thread.Sleep(100);
            return 43;
        }
        if (scenario == "CaptureTimeout" || scenario == "MissingCaptureReady" ||
            scenario == "IncorrectMessageSource")
            Thread.Sleep(15000);
        else
            Thread.Sleep(800);
        return 0;
    }
}
