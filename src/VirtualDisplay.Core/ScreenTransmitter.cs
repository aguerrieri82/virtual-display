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
    public class EncodingSettings
    {

        public int Fps { get; set; }

        public int BitRate { get; set; }

        public int KeyFrameInterval { get; set; }

        public H264Profile Profile { get; set; }

        public int Level { get; set; }
    }

    public class ScreenTransmitter
    {
        H264Encoder _encoder;
        ScreenCapture _screenCapture;
        Image? _encoderInit;
        bool _isStarted;
        protected TcpListener? _listener;
        protected TcpClient? _client;
        protected BinaryWriter? _writer;

        public ScreenTransmitter()
        {
            _encoder = new H264Encoder();
            _screenCapture = new ScreenCapture();

        }

        public void Stop()
        {
            if (!_isStarted)
                return;

            _isStarted = false;

            if (_listener != null)
            {
                _listener.Stop();
                _listener = null;
            }

            if (_client != null)
            {
                _client.Close();
                _client = null;
            }

            if (_writer != null)
            {
                _writer.Close();
                _writer = null;
            }

            _screenCapture.Stop();

        }

        public void Start(ScreenInfo screen, int port, EncodingSettings settings)
        {
            if (_isStarted)
                return;

            _isStarted = true;

            _screenCapture.Start(screen);

            _listener = new TcpListener(IPAddress.Any, port);
            _listener.Start();

            while (_isStarted)
            {
                Console.WriteLine("Listening");

                try
                {
                    _client = _listener.AcceptTcpClient();
                    _writer = new BinaryWriter(_client.GetStream());
                    _client.NoDelay = true;

                    Console.WriteLine("New Client");
                }
                catch
                {
                    continue;
                }

                bool headerSent = false;

                try
                {
                    while (_client != null && _isStarted)
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
                                    BitRate = settings.BitRate,
                                    PixelFormat = PixelFormat.RGBA32,
                                    Fps = settings.Fps,
                                    KeyFrameInterval = settings.KeyFrameInterval,
                                    Level = settings.Level,
                                    Profile = settings.Profile,
                                    Stride = frame.Width * 4
                                });
                                _encoderInit = frame;


                            }

                            if (!headerSent)
                            {
                                _writer.Write(0x80706050);
                                _writer.Write(frame.Width);
                                _writer.Write(frame.Height);
                                headerSent = true;
                            }

                            _encoder.Convert(inFrame, outFrame);

                            _writer.Write(outFrame.ByteArray!.Length);
                            _writer.Write(outFrame.ByteArray);

                            _writer.Flush();

                            Console.WriteLine("Send Frame: {0}", outFrame.ByteArray!.Length);

                            Thread.Sleep(1000 / settings.Fps);
                        }
                    }

                }
                catch (Exception ex)
                {
                    Console.WriteLine("ERR: {0}", ex);
       
                }
                finally
                {
                    _client.Close();
                    _writer.Close();
                }
            }


        }
    }
}
