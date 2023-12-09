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
        NV12
    }

    public enum H264Profile
    {
        Base = 66,
        Main = 77,
        High = 100
    }

    public class VideoFormat
    {
        public int Width { get; set; }
        
        public int Height { get; set; }
        
        public int Stride { get; set; }
        
        public int Fps { get; set; }

        public int BitRate { get; set; }

        public int KeyFrameInterval { get; set; }

        public H264Profile Profile { get; set; }    

        public int Level { get; set; }

        public PixelFormat PixelFormat { get; set; }
    }
}
