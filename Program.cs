using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;
using OmniconvertCS.Cli;

namespace OmniconvertCS.Gui
{
    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            // CLI mode when any args are provided
            if (args != null && args.Length > 0)
            {
                ConsoleBootstrap.EnsureConsole();

                int rc;
                try
                {
                    rc = CliRunner.Run(args);
                }
                catch (Exception ex)
                {
                    // If anything unexpected escapes CLI, don't crash silently.
                    Console.WriteLine("FATAL: Unhandled exception in CLI mode:");
                    Console.WriteLine(ex.ToString());
                    rc = 2;
                }

                Environment.Exit(rc);
                return;
            }

            // GUI mode
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }

        private static class ConsoleBootstrap
        {
            private const int ATTACH_PARENT_PROCESS = -1;

            [DllImport("kernel32.dll", SetLastError = true)]
            private static extern bool AttachConsole(int dwProcessId);

            [DllImport("kernel32.dll", SetLastError = true)]
            private static extern bool AllocConsole();

            public static void EnsureConsole()
            {
                // If launched from cmd/bat, attach to that console.
                // If launched from Explorer (no parent console), allocate one.
                try
                {
                    if (!AttachConsole(ATTACH_PARENT_PROCESS))
                    {
                        AllocConsole();
                    }

                    // Rebind standard streams so Console.ReadLine/WriteLine works reliably in a WinForms subsystem exe.
                    Console.OutputEncoding = Encoding.UTF8;

                    var stdout = Console.OpenStandardOutput();
                    var writer = new StreamWriter(stdout) { AutoFlush = true };
                    Console.SetOut(writer);
                    Console.SetError(writer);

                    var stdin = Console.OpenStandardInput();
                    var reader = new StreamReader(stdin);
                    Console.SetIn(reader);
                }
                catch
                {
                    // Best-effort only.
                }
            }
        }
    }
}
