using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace VirtualDisplay.Unity
{
    public class H264Decoder : IDisposable
    {
        [DllImport("VirtualDisplay.Decoder.Native")]
        static extern IntPtr CreateDecoder();

        [DllImport("VirtualDisplay.Decoder.Native")]
        static extern void Decode(IntPtr decoder, IntPtr srcBuffer, int srcBufferSize, IntPtr dstBuffer, int dstBufferSize);


        [DllImport("VirtualDisplay.Decoder.Native")]
        static extern void DestroyDecoder(IntPtr handle);


        IntPtr _handle;

        public H264Decoder()
        {
            _handle = CreateDecoder();  
        }

        public void Decode(byte[] srcBuffer, byte[] dstBuffer)
        {
            var src = GCHandle.Alloc(srcBuffer, GCHandleType.Pinned);
            var dst = GCHandle.Alloc(dstBuffer, GCHandleType.Pinned);

            Decode(_handle, src.AddrOfPinnedObject(), srcBuffer.Length, dst.AddrOfPinnedObject(), dstBuffer.Length);
        }

        public void Dispose()
        {
            if (_handle != IntPtr.Zero) 
            {
                DestroyDecoder(_handle);
                _handle = IntPtr.Zero;  
            }

            GC.SuppressFinalize(this);  
        }


    }
}
