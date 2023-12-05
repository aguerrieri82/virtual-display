using System;

namespace VirtualDisplay
{

    public interface IVideoConverter
    {
        void Start(VideoFormat format);

        void Convert(FrameBuffer src, FrameBuffer dst);

        void Stop();
    }
}
