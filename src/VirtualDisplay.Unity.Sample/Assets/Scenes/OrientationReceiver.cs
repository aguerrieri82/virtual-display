using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

public class OrientationReceiver : MonoBehaviour
{
    UdpClient _client;
    private Task _activeTask;

    [SerializeField]
    Vector3 _delta;

    [SerializeField]
    Vector3 _mul;

    public OrientationReceiver()
    {
        _mul = new Vector3(1, 1, 1);
    }

    void Start()
    {
        _client = new UdpClient(7000);

    }

    private async Task ReceiveAsync()
    {
        var data = await _client.ReceiveAsync();
        var stream = new MemoryStream(data.Buffer);
        var reader = new BinaryReader(stream);  
        //var head = (char)reader.ReadInt16();
        var y = reader.ReadSingle();
        var x = reader.ReadSingle();
        var z = reader.ReadSingle();

        var transform = gameObject.GetComponent<Transform>();

        transform.rotation = Quaternion.Euler(x * Mathf.Rad2Deg * _mul.x + _delta.x, y * Mathf.Rad2Deg * _mul.y + _delta.y, z * Mathf.Rad2Deg * _mul.z + _delta.x);

    }

    void Update()
    {
        if (_activeTask != null && !_activeTask.IsCompleted)
            return;

        _activeTask = ReceiveAsync();
    }
}
