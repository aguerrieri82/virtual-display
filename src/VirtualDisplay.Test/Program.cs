

using System.Diagnostics;
using VirtualDisplay;
using VirtualDisplay.Core;
using VirtualDisplay.Unity;

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
   // display.SetDisplayModes([new DisplayMode() { Width = 1024, Height = 768, VSync = 60 }]);
    display.CreateDisplay(0, 1600, 900);

    //display.CreateDisplay(1, 1900, 1200);

    DisplayInfo? info = null;
    while (info == null || info.Width == 0)
        info = display.GetDisplayInfo(0);
}

catch
{

}

Console.Read();

//await Task.Delay(1000);

//var trans = new ScreenTransmitter();
var rec = new ScreenReceiver();

//var monitors = ScreenCapture.EnumScreens(); 

//_ = Task.Run(() => trans.Start(monitors[0], 6666));

//Thread.Sleep(1000);

_ = Task.Run(() => rec.Start("172.20.160.1", 6666));

Console.Read();

