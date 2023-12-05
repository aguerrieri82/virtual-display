using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using SharpDX.Direct3D;
using SharpDX.Direct3D11;
using SharpDX.DXGI;

namespace VirtualDisplay.Core
{
    public class ScreenInfo
    {
        public nint MonitorHandle { get; set; }

        public long DeviceLuid { get; set; }

        public int Index { get; set; }

        public int Width { get; set; }

        public int Height { get; set; }
    }

    public class Image
    {
        public int Width { get; set; }

        public int Height { get; set; }

        public byte[]? Data { get; set; }
    }

    public class ScreenCapture
    {
        SharpDX.Direct3D11.Device? _device;
        OutputDuplication? _outDup;
        Texture2D? _stageTexture;
        byte[]? _buffer;
        int _lastFrame;



        public ScreenCapture()
        {
        }

        public static IList<ScreenInfo> EnumScreens()
        {
            var result = new List<ScreenInfo>();
            var dxgi = new Factory1();

            foreach (var adapter in dxgi.Adapters1)
            {
                int i = 0;
                foreach (var output in adapter.Outputs)
                {
                    var info = new ScreenInfo()
                    {
                        DeviceLuid = adapter.Description.Luid,
                        Index = i,
                        Width = output.Description.DesktopBounds.Right - output.Description.DesktopBounds.Left,
                        Height = output.Description.DesktopBounds.Bottom - output.Description.DesktopBounds.Top,
                        MonitorHandle = output.Description.MonitorHandle,
                    };
                    result.Add(info);
                    i++;
                }
            }
            return result;
        }

        public void Start(ScreenInfo screen)
        {
            var dxgi = new Factory1();
            var adapter = dxgi.Adapters.Single(a => a.Description.Luid == screen.DeviceLuid);
            var output = adapter.Outputs.Single(a => a.Description.MonitorHandle == screen.MonitorHandle);

            _device = new SharpDX.Direct3D11.Device(adapter, DeviceCreationFlags.BgraSupport | DeviceCreationFlags.Debug, [FeatureLevel.Level_11_1]);
            _outDup = output.QueryInterface<Output6>().DuplicateOutput(_device);

            _lastFrame = -1;
        }

        public Image? ReadImage()
        {
            var result = _outDup!.TryAcquireNextFrame(0, out var frame, out var res);
            if (!result.Success)
                return null;

            try
            {
      
                if (frame.AccumulatedFrames == _lastFrame)
                    return null;

                _lastFrame = frame.AccumulatedFrames;

                var texture = res.QueryInterface<Texture2D>();

                if (_stageTexture == null || texture.Description.Width != _stageTexture.Description.Width || texture.Description.Height != _stageTexture.Description.Height)
                {
                    var textureDesc = new Texture2DDescription
                    {
                        Width = texture.Description.Width,
                        Height = texture.Description.Height,
                        MipLevels = 1,
                        ArraySize = 1,
                        Format = Format.B8G8R8A8_UNorm,
                        SampleDescription = new SampleDescription(1, 0),
                        Usage = ResourceUsage.Staging,
                        BindFlags = BindFlags.None,
                        CpuAccessFlags = CpuAccessFlags.Write | CpuAccessFlags.Read,
                        OptionFlags = ResourceOptionFlags.None
                    };

                    _stageTexture = new Texture2D(_device, textureDesc);
                }

                _device!.ImmediateContext.CopyResource(res.QueryInterface<SharpDX.Direct3D11.Resource>(), _stageTexture!.QueryInterface<SharpDX.Direct3D11.Resource>());
                _device!.ImmediateContext.Flush();
            }
            finally
            {
                _outDup.ReleaseFrame();
            }

            var dataBox = _device!.ImmediateContext.MapSubresource(_stageTexture, 0, MapMode.Read, SharpDX.Direct3D11.MapFlags.None);

            var size = _stageTexture!.Description.Height * dataBox.RowPitch;

            if (_buffer == null || _buffer.Length != size)
                _buffer = new byte[size];

            var handle = GCHandle.Alloc(_buffer, GCHandleType.Pinned);

            NativeMethods.CopyMemory(handle.AddrOfPinnedObject(), dataBox.DataPointer, size);

            _device!.ImmediateContext.UnmapSubresource(_stageTexture, 0);

            return new Image
            {
                Data = _buffer,
                Width = _stageTexture.Description.Width,
                Height = _stageTexture.Description.Height,
            };
        }
    }
}
