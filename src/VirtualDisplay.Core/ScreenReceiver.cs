using SharpDX.Direct3D11;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Linq;
using System.Net.Sockets;
using System.Reflection.PortableExecutable;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using VirtualDisplay.Unity;

namespace VirtualDisplay.Core
{
    public class ScreenReceiver
    {


        public void Start(string host, int port)
        {
            using var decoder = new H264Decoder();

            using var client = new TcpClient();

            client.Connect(host, port);

            using var reader = new BinaryReader(client.GetStream());

            var magic = reader.ReadUInt32();
            if (magic != 0x80706050)
                throw new InvalidOperationException();

            var width = reader.ReadInt32();
            var height = reader.ReadInt32();
            var buffer = new byte[width * height * 4 * 1];

            while (client.Connected)
            {
                var size = reader.ReadInt32();
                var srcBuffer = reader.ReadBytes(size);
                decoder.Decode(srcBuffer, buffer);
                //ImageUtils.Save(buffer, width, height, "d:\\frame.png");
            }

        }
    }
}
