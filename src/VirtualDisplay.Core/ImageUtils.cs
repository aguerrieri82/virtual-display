using SharpDX.Direct3D11;
using System;
using System.Collections.Generic;
using System.Drawing.Imaging;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace VirtualDisplay.Core
{
    public static class ImageUtils
    {
        public static void Save(byte[] data, int width, int height, string fileName)
        {

            var bmp = new Bitmap(width, height);
            var bmpData = bmp.LockBits(new Rectangle(0, 0, width, height), System.Drawing.Imaging.ImageLockMode.ReadWrite, System.Drawing.Imaging.PixelFormat.Format32bppArgb);

            var bufferData = GCHandle.Alloc(data, GCHandleType.Pinned);

            NativeMethods.CopyMemory(bmpData.Scan0, bufferData.AddrOfPinnedObject(), bmpData.Stride * bmpData.Height);

            bmp.UnlockBits(bmpData);

            bmp.Save(fileName, ImageFormat.Png);
        }
    }
}
