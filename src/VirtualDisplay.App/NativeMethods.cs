using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace VirtualDisplay.App
{
    internal static class NativeMethods
    {
        [DllImport("kernel32")]
        public static extern void CopyMemory(IntPtr dest, IntPtr src, uint count);
    }
}
