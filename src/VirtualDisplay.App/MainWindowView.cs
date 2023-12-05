using SharpDX.Direct3D;
using SharpDX.Direct3D11;
using SharpDX.DXGI;
using SharpDX.Mathematics.Interop;
using SharpDX.Windows;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Reflection.Metadata;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Media.Media3D;
using System.Windows.Media.TextFormatting;

namespace VirtualDisplay.App
{
    public class MainWindowView : INotifyPropertyChanged
    {
        VirtualDisplayManager? _display;
        H264Decoder? _decoder;
        Texture2D? _mainTexture;
        Texture2D? _stageTexture;
        WriteableBitmap? _bitmap;
        Thread? _logThread;
        Thread? _encodeThread;
        Thread? _listenThread;
        Output1? _mainOut;
        OutputDuplication? _outDup;
        H264Encoder? _encoder;
        SharpDX.Direct3D11.Device? _device;
        MemoryStream avcStream;
        TcpListener? _listner;
        TcpClient? _activeClient;
        BinaryWriter? _clientWriter;

        public MainWindowView()
        {
            avcStream = new MemoryStream();

            Init();
      
        }

        public event PropertyChangedEventHandler? PropertyChanged;

        public void Init()
        {


            var curAdapters = new Factory1().Adapters;

            _display = new VirtualDisplayManager();
            _display.Create();


            Size size = new();
            while (size.Width == 0)
            {
                size = _display.GetSize(0);
                Thread.Sleep(100);
            }

            var adapter = new Factory1().Adapters.Single(a => a.Description.Luid == size.Luid);

            _mainOut = adapter.Outputs.Last().QueryInterface<Output1>();

            DeviceCreationFlags flags = DeviceCreationFlags.BgraSupport | DeviceCreationFlags.Debug;

            _device = new SharpDX.Direct3D11.Device(adapter, flags, [FeatureLevel.Level_11_1]);

            _outDup = _mainOut.QueryInterface<Output6>().DuplicateOutput(_device);

            var textureDesc = new Texture2DDescription
            {
                Width = _mainOut.Description.DesktopBounds.Right - _mainOut.Description.DesktopBounds.Left,
                Height = _mainOut.Description.DesktopBounds.Bottom - _mainOut.Description.DesktopBounds.Top,
                MipLevels = 1,
                ArraySize = 1,
                Format = Format.B8G8R8A8_UNorm,
                SampleDescription = new SampleDescription(1, 0),
                Usage = ResourceUsage.Default,
                BindFlags = BindFlags.ShaderResource,
                CpuAccessFlags = CpuAccessFlags.None,
                OptionFlags = ResourceOptionFlags.Shared
            };

            _mainTexture = new Texture2D(_device, textureDesc);

            textureDesc.Usage = ResourceUsage.Staging;
            textureDesc.BindFlags = BindFlags.None;
            textureDesc.CpuAccessFlags = CpuAccessFlags.Write | CpuAccessFlags.Read;
            textureDesc.OptionFlags = ResourceOptionFlags.None;

            _stageTexture = new Texture2D(_device, textureDesc);

            //_display.SetTargetSurface(0, _mainTexture.QueryInterface<SharpDX.DXGI.Resource>().SharedHandle);

            _bitmap = new WriteableBitmap(_mainTexture.Description.Width, _mainTexture.Description.Height, 96, 96, PixelFormats.Bgra32, null);

            _logThread = new Thread(LogLoop);
            _logThread.Start();

            _encoder = new H264Encoder();
            _encoder.Start(new VideoFormat
            {
                BitRate = 30000000,
                PixelFormat = PixelFormat.RGBA32,
                Fps = 25,
                Width = size.Width,
                Height = size.Height,
                Stride = size.Width * 4
            });

            _encodeThread = new Thread(EncodeLoop);
            _encodeThread.Start();

            _listenThread = new Thread(ListenLoop);
            _listenThread.Start();
        }
        public unsafe VideoFrame? UpdateFrame()
        {
            try
            {
                var result = _outDup!.TryAcquireNextFrame(0, out var frame, out var res);
                
                if (!result.Success)
                    return null;

                try
                {
                    _device!.ImmediateContext.CopyResource(res.QueryInterface<SharpDX.Direct3D11.Resource>(), _stageTexture!.QueryInterface<SharpDX.Direct3D11.Resource>());
                    _device!.ImmediateContext.Flush();
                }
                finally
                {
                    _outDup.ReleaseFrame();
                }

                var dataBox = _device!.ImmediateContext.MapSubresource(_stageTexture, 0, MapMode.Read, SharpDX.Direct3D11.MapFlags.None);

                //_bitmap!.Lock();
                //_bitmap!.AddDirtyRect(new System.Windows.Int32Rect(0, 0, (int)_bitmap.Width, (int)_bitmap.Height));

                var size = _stageTexture!.Description.Height * dataBox.RowPitch;

                try
                {
                    //var dst = new Span<byte>(_bitmap.BackBuffer.ToPointer(), size);
                    //var src = new Span<byte>((void*)dataBox.DataPointer, size);

                    //src.CopyTo(dst);

                    if (avcStream.Position == 0)
                    {
                        var header = _encoder!.GetHeader();
                        //avcStream.Write(header, 0, header.Length);
                    }

                    var encFrame = _encoder!.Encode(new VideoFrame { BufferPtr = dataBox.DataPointer });

                    return encFrame;
                }
                finally
                {
                    //_bitmap.Unlock();
                    _device!.ImmediateContext.UnmapSubresource(_stageTexture, 0);
                }
            }
            catch
            {

            }

            return null;
        }

        protected static void LogLoop()
        {
            var reader = new StreamReader("\\\\.\\pipe\\VirtualMonitor");

            while (true)
            {
                var line = reader.ReadLine();
                Debug.WriteLine(line);
            }
        }

        protected void ListenLoop()
        {
            _listner = new TcpListener(IPAddress.Any, 6666);
            _listner.Start();   
            while (true)
            {
                var client = _listner.AcceptTcpClient();
                if (_activeClient != null)
                {
                    _activeClient.Close();
                    _clientWriter!.Close();
                }
                _activeClient = client;
                _clientWriter = new BinaryWriter(_activeClient.GetStream());

                _clientWriter.Write(0x80706050);
                _clientWriter.Write(_stageTexture!.Description.Width);
                _clientWriter.Write(_stageTexture.Description.Height);

            }
        }

        protected void EncodeLoop()
        {
            while (true)
            {
                var frame = UpdateFrame();

                if (frame != null && _clientWriter != null && _activeClient!.Connected)
                {
                    _clientWriter!.Write(frame!.Value.Buffer.Length);
                    _clientWriter!.Write(frame!.Value.Buffer);
                    _clientWriter.Flush();  
                }

                Thread.Sleep(40);
            }
        }


        protected void OnPropertyChanged(string name)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }

        public WriteableBitmap? Bitmap => _bitmap;
    }
}
