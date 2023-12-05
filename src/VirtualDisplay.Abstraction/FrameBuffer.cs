namespace VirtualDisplay
{
    public class FrameBuffer
    {
       public IntPtr Pointer { get; set; }
       public byte[]? ByteArray { get; set; }
       public int Size { get; set; }
    };
}
