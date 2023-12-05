using SharpDX.Direct3D11;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace VirtualDisplay.Core
{
    public class ScreenTransmitter
    {
        H264Encoder _encoder;
        ScreenCapture _screenCapture;
        Image? _encoderInit;

        public ScreenTransmitter()
        {
            _encoder = new H264Encoder();
            _screenCapture = new ScreenCapture();   

        }

        public void Start(ScreenInfo screen, int port)
        {
            _screenCapture.Start(screen);

            var listener = new TcpListener(IPAddress.Any, port);
            listener.Start();

            while (true)
            {
                Console.WriteLine("Listening");

                var client = listener.AcceptTcpClient();
                var binaryWriter = new BinaryWriter(client.GetStream());

                client.NoDelay = true;

                Console.WriteLine("New Client");

                try
                {
                    while (true)
                    {
                        var frame = _screenCapture.ReadImage();
                        if (frame != null)
                        {
                            var inFrame = new FrameBuffer { ByteArray = frame.Data };
                            var outFrame = new FrameBuffer();

                            //ImageUtils.Save(frame.Data!, frame.Width, frame.Height, "d:\\frame2.png");

                            if (_encoderInit == null || _encoderInit.Width != frame.Width || _encoderInit.Height != frame.Height)
                            {
                                _encoder.Start(new VideoFormat
                                {
                                    Width = frame.Width,
                                    Height = frame.Height,
                                    BitRate = 3000000,
                                    PixelFormat = PixelFormat.RGBA32,
                                    Fps = 25,
                                    Stride = frame.Width * 4
                                });
                                _encoderInit = frame;

                                binaryWriter.Write(0x80706050);
                                binaryWriter.Write(frame.Width);
                                binaryWriter.Write(frame.Height);
                            }

                            _encoder.Convert(inFrame, outFrame);

                            binaryWriter.Write(outFrame.ByteArray!.Length);
                            binaryWriter.Write(outFrame.ByteArray);

                            binaryWriter.Flush();

                            Console.WriteLine("Send Frame: {0}", outFrame.ByteArray!.Length);

                            Thread.Sleep(40);
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine("ERR: {0}", ex);
                    client.Close();
                    binaryWriter.Close();
                }
            }
        }
    }
}
