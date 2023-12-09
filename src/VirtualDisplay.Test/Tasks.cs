using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using VirtualDisplay.Core;

namespace VirtualDisplay.Test
{
    public static class Tasks
    {
        public static void Transmit()
        {

            var trans = new ScreenTransmitter();

            var monitors = ScreenCapture.EnumScreens();

            while (true)
            {
                _ = Task.Run(() => trans.Start(monitors[0], 6666, new EncodingSettings
                {
                    BitRate = 2000000,
                    Fps = 30,
                    KeyFrameInterval = 25,
                    Level = 50,
                    Profile = H264Profile.High
                }));

                Console.WriteLine("Press E to exit");
                var key = Console.ReadKey();

                trans.Stop();

                if (key.Key == ConsoleKey.E)
                    break;
            }



        }

        public static void CreateDisplay()
        {
            static void LogLoop()
            {
                StreamReader reader;
                while (true)
                {
                    try
                    {
                        reader = new StreamReader("\\\\.\\pipe\\VirtualMonitor");
                        break;
                    }
                    catch
                    {
                        Thread.Sleep(100);
                    }
                }

                while (true)
                {
                    var line = reader.ReadLine();
                    if (line == null)
                        return;
                    Console.WriteLine(line);
                }
            }

            var logThread = new Thread(LogLoop);
            logThread.Start();


            var display = new VirtualDisplayManager();

            try
            {
                display.Create();
                //display.SetDisplayModes([new DisplayMode() { Width = 1024, Height = 768, VSync = 60 }]);
                display.CreateDisplay(0, 1600, 900);

                //display.CreateDisplay(1, 1900, 1200);

                DisplayInfo? info = null;
                while (info == null || info.Width == 0)
                {
                    info = display.GetDisplayInfo(0);
                    Thread.Sleep(500);
                    Console.WriteLine("GetDisplayInfo...");
                }

            }

            catch
            {

            }

            Console.WriteLine("Done");

        }
    }
}
