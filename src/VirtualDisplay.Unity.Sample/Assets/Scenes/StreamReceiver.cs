using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using VirtualDisplay.Unity;

public class StreamReceiver : MonoBehaviour
{
    TcpClient _client;
    BinaryReader _reader;
    H264Decoder _decoder;
    byte[] _buffer;
    int _width;
    int _height;
    bool _connecting;
    Texture2D _texture;
    bool _newFrame;

    [SerializeField]
    Material _material;

    [Header("Connection")]
    [SerializeField]
    string _host = "localhost";

    [SerializeField]
    int _port = 6666;

    Task _activeTask;

    public StreamReceiver()
    {

    }

    void Start()
    {
        _decoder = new H264Decoder();

        if (_material == null)
            _material = gameObject.GetComponent<MeshRenderer>().material;

        ResetClient();

#if UNITY_EDITOR
        UnityEditor.EditorApplication.playModeStateChanged += OnPlayModeChanged;
#endif
    }

#if UNITY_EDITOR
    private void OnPlayModeChanged(UnityEditor.PlayModeStateChange obj)
    {
        if (obj == UnityEditor.PlayModeStateChange.ExitingPlayMode)
        {
            if (_client != null && _client.Connected)
                ResetClient();
        }
    }

#endif

    async Task ConnectAsync()
    {
        try
        {
            await _client.ConnectAsync(_host, _port);

            _reader = new BinaryReader(_client.GetStream());

            var magic = _reader.ReadUInt32();
            if (magic != 0x80706050)
                throw new InvalidOperationException();
            _width = _reader.ReadInt32();
            _height = _reader.ReadInt32();
            _buffer = new byte[_width * _height * 4 * 2];

            _texture = new Texture2D(_width, _height, TextureFormat.RGBA32, 5, true);

            _material.mainTexture = _texture;

        }
        catch (Exception ex)
        {
            ResetClient();
            _client.Close();
            _client = new TcpClient();
        }
    }

    void ReadFrame()
    {
        try
        {
            var size = _reader.ReadInt32();

            System.Diagnostics.Debug.WriteLine(size);

            var srcBuffer = _reader.ReadBytes(size);

            lock (_buffer)
            {
                _decoder.Decode(srcBuffer, _buffer);
                _newFrame = true;
            }
        }
        catch (Exception ex)
        {
            ResetClient();
        }
    }

    void ResetClient()
    {
        _client?.Close();
        _client = new TcpClient();

        if (_reader != null)
        {
            _reader.Dispose();
            _reader = null;
        }
    }

    void Update()
    {
        if (_activeTask != null && !_activeTask.IsCompleted)
            return;


        if (_newFrame)
        {
            lock (_buffer)
                _texture.SetPixelData(_buffer, 0);

            _texture.Apply(true);

            _newFrame = false;
        }

        if (!_client.Connected)
            _activeTask = ConnectAsync();
        else
            _activeTask = Task.Run(() => ReadFrame());
    }

}
