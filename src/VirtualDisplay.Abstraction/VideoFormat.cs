using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace VirtualDisplay
{
    public enum PixelFormat
    {
        RGBA32,
        NV12,
        H264
    }

    public class VideoFormat
    {
        public int Width { get; set; }
        public int Height { get; set; }
        public int Stride { get; set; }
        public int Fps { get; set; }
        public int BitRate { get; set; }
        public PixelFormat PixelFormat { get; set; }
    }
}
