// usb_gdbstub_server.exe - cortex-debug / Arduino IDE 2 pseudo-"openocd" launcher.
//
// WHY A NATIVE EXE (not the old .cmd):
//   cortex-debug spawns the configured openocd path with Node's child_process
//   .spawn() and NO shell. Modern Node (>=18.20/20.12/25) refuses to spawn
//   .cmd/.bat files without { shell: true } and throws EINVAL (CVE-2024-27980),
//   so the old usb_gdbstub_server.cmd never ran -> the gdb server "process"
//   died instantly -> cortex-debug reported `read ECONNRESET` / "Request
//   cancelled on connection close". A real .exe is spawnable exactly like the
//   genuine openocd.exe it stands in for.
//
// WHAT IT DOES:
//   cortex-debug (servertype=openocd) OWNS the gdb port: it picks a free TCP
//   port and launches us with openocd-style flags such as
//     -c "gdb_port 50000" -c "tcl_port 50001" -s <dir> -f <cfg> -f
//   then connects gdb to localhost:50000. We parse gdb_port from those args
//   (combined "gdb_port 50000" or split "gdb_port" "50000" forms; fallback
//   3335) and launch usb_gdbstub_bridge.ps1 on that port, inheriting stdio so
//   the bridge's "Listening on port <N> for gdb connections" line reaches
//   cortex-debug's openocd serverReady regex. A kill-on-close Job object makes
//   the child PowerShell die if we are terminated, so COM never leaks.
//
// BUILD (no project needed; csc ships with .NET Framework):
//   %WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe ^
//     /nologo /optimize /target:exe /platform:anycpu ^
//     /out:usb_gdbstub_server.exe usb_gdbstub_server.cs

using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

internal static class NiusGdbStubServer
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr CreateJobObject(IntPtr a, string name);

    [DllImport("kernel32.dll")]
    private static extern bool SetInformationJobObject(IntPtr hJob, int infoClass, IntPtr lpInfo, uint cb);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool AssignProcessToJobObject(IntPtr hJob, IntPtr hProcess);

    [StructLayout(LayoutKind.Sequential)]
    private struct JOBOBJECT_BASIC_LIMIT_INFORMATION
    {
        public long PerProcessUserTimeLimit;
        public long PerJobUserTimeLimit;
        public uint LimitFlags;
        public UIntPtr MinimumWorkingSetSize;
        public UIntPtr MaximumWorkingSetSize;
        public uint ActiveProcessLimit;
        public UIntPtr Affinity;
        public uint PriorityClass;
        public uint SchedulingClass;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IO_COUNTERS
    {
        public ulong ReadOperationCount;
        public ulong WriteOperationCount;
        public ulong OtherOperationCount;
        public ulong ReadTransferCount;
        public ulong WriteTransferCount;
        public ulong OtherTransferCount;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
    {
        public JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
        public IO_COUNTERS IoInfo;
        public UIntPtr ProcessMemoryLimit;
        public UIntPtr JobMemoryLimit;
        public UIntPtr PeakProcessMemoryUsed;
        public UIntPtr PeakJobMemoryUsed;
    }

    private const int JobObjectExtendedLimitInformation = 9;
    private const uint JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x2000;

    private static IntPtr CreateKillOnCloseJob()
    {
        IntPtr job = CreateJobObject(IntPtr.Zero, null);
        if (job == IntPtr.Zero)
        {
            return IntPtr.Zero;
        }

        var ext = new JOBOBJECT_EXTENDED_LIMIT_INFORMATION();
        ext.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        int len = Marshal.SizeOf(typeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));
        IntPtr buf = Marshal.AllocHGlobal(len);
        try
        {
            Marshal.StructureToPtr(ext, buf, false);
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, buf, (uint)len);
        }
        finally
        {
            Marshal.FreeHGlobal(buf);
        }

        return job;
    }

    private static void Log(string message)
    {
        try
        {
            string dir = Environment.GetEnvironmentVariable("TEMP");
            if (string.IsNullOrEmpty(dir))
            {
                dir = Path.GetTempPath();
            }

            string path = Path.Combine(dir, "nius_gdbstub_server.log");
            File.AppendAllText(path, string.Format("[{0:yyyy-MM-dd HH:mm:ss.fff}] {1}{2}", DateTime.Now, message, Environment.NewLine));
        }
        catch
        {
        }
    }

    private static int ParseGdbPort(string[] args)
    {
        for (int i = 0; i < args.Length; i++)
        {
            string a = args[i] ?? string.Empty;
            if (!a.StartsWith("gdb_port", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            string rest = a.Substring("gdb_port".Length).Trim();
            int p;
            if (int.TryParse(rest, out p) && p > 0 && p < 65536)
            {
                return p;
            }

            if (rest.Length == 0 && i + 1 < args.Length && int.TryParse((args[i + 1] ?? string.Empty).Trim(), out p) && p > 0 && p < 65536)
            {
                return p;
            }
        }

        return 3335;
    }

    private static int Main(string[] args)
    {
        Log("server.exe invoked args=[" + string.Join(" | ", args) + "]");
        int port = ParseGdbPort(args);
        Log("resolved gdb_port=" + port);

        string exeDir = AppDomain.CurrentDomain.BaseDirectory;
        string bridge = Path.Combine(exeDir, "usb_gdbstub_bridge.ps1");
        if (!File.Exists(bridge))
        {
            Log("ERROR: bridge script not found: " + bridge);
            Console.Error.WriteLine("usb_gdbstub_bridge.ps1 not found next to launcher: " + bridge);
            return 2;
        }

        IntPtr job = CreateKillOnCloseJob();

        // Redirect + relay the bridge's stdout/stderr and flush each line.
        // cortex-debug reads OUR stdout to match its openocd serverReady regex
        // ("Listening on port N for gdb connections"). PowerShell block-buffers
        // a piped stdout, and the bridge blocks on accept() right after the
        // banner, so without an explicit relay+flush that line would never
        // reach cortex-debug and it would time out waiting for the GDB server.
        var psi = new ProcessStartInfo
        {
            FileName = "powershell.exe",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            Arguments = string.Format(
                "-NoProfile -ExecutionPolicy Bypass -File \"{0}\" -Board promicro_nrf52840 -TcpPort {1} -BaudRate 115200 -PreferServiceCdc",
                bridge, port)
        };
        Log("launching: powershell.exe " + psi.Arguments);

        Process proc;
        try
        {
            proc = new Process { StartInfo = psi };
            proc.OutputDataReceived += delegate(object s, DataReceivedEventArgs e)
            {
                if (e.Data != null)
                {
                    Console.Out.WriteLine(e.Data);
                    Console.Out.Flush();
                }
            };
            proc.ErrorDataReceived += delegate(object s, DataReceivedEventArgs e)
            {
                if (e.Data != null)
                {
                    Console.Error.WriteLine(e.Data);
                    Console.Error.Flush();
                }
            };
            proc.Start();
            proc.BeginOutputReadLine();
            proc.BeginErrorReadLine();
        }
        catch (Exception e)
        {
            Log("ERROR: failed to start powershell: " + e.Message);
            Console.Error.WriteLine(e.Message);
            return 3;
        }

        try
        {
            if (job != IntPtr.Zero)
            {
                AssignProcessToJobObject(job, proc.Handle);
            }
        }
        catch
        {
        }

        AppDomain.CurrentDomain.ProcessExit += delegate
        {
            try
            {
                if (!proc.HasExited)
                {
                    proc.Kill();
                }
            }
            catch
            {
            }
        };
        Console.CancelKeyPress += delegate
        {
            try
            {
                if (!proc.HasExited)
                {
                    proc.Kill();
                }
            }
            catch
            {
            }
        };

        proc.WaitForExit();
        int code = proc.ExitCode;
        Log("bridge exited code=" + code);
        return code;
    }
}
