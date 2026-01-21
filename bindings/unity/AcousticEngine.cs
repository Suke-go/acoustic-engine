using System;
using System.Runtime.InteropServices;

public sealed class AcousticEngine : IDisposable
{
    private IntPtr _handle;
    private bool _disposed;

    public AcousticEngine()
    {
        var config = AcousticEngineNative.ae_get_default_config();
        _handle = AcousticEngineNative.ae_create_engine(ref config);
        if (_handle == IntPtr.Zero)
        {
            throw new InvalidOperationException("Failed to create Acoustic Engine.");
        }
    }

    public void SetScenario(string name, float intensity = 1.0f)
    {
        ThrowIfDisposed();
        AcousticEngineNative.ae_apply_scenario(_handle, name, intensity);
    }

    public float Distance
    {
        set
        {
            ThrowIfDisposed();
            AcousticEngineNative.ae_set_distance(_handle, value);
        }
    }

    public float RoomSize
    {
        set
        {
            ThrowIfDisposed();
            AcousticEngineNative.ae_set_room_size(_handle, value);
        }
    }

    public float Brightness
    {
        set
        {
            ThrowIfDisposed();
            AcousticEngineNative.ae_set_brightness(_handle, value);
        }
    }

    public float Width
    {
        set
        {
            ThrowIfDisposed();
            AcousticEngineNative.ae_set_width(_handle, value);
        }
    }

    public float DryWet
    {
        set
        {
            ThrowIfDisposed();
            AcousticEngineNative.ae_set_dry_wet(_handle, value);
        }
    }

    public float Intensity
    {
        set
        {
            ThrowIfDisposed();
            AcousticEngineNative.ae_set_intensity(_handle, value);
        }
    }

    public void Process(float[] data, int channels)
    {
        ThrowIfDisposed();
        if (data == null)
            throw new ArgumentNullException(nameof(data));
        if (channels <= 0)
            throw new ArgumentOutOfRangeException(nameof(channels));

        var handle = GCHandle.Alloc(data, GCHandleType.Pinned);
        try
        {
            var buf = new AeBuffer
            {
                samples = handle.AddrOfPinnedObject(),
                frameCount = (UIntPtr)(data.Length / channels),
                channels = (byte)channels,
                interleaved = true
            };
            AcousticEngineNative.ae_process(_handle, ref buf, ref buf);
        }
        finally
        {
            handle.Free();
        }
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    ~AcousticEngine()
    {
        Dispose(false);
    }

    private void Dispose(bool disposing)
    {
        if (_disposed)
            return;

        if (_handle != IntPtr.Zero)
        {
            AcousticEngineNative.ae_destroy_engine(_handle);
            _handle = IntPtr.Zero;
        }

        _disposed = true;
    }

    private void ThrowIfDisposed()
    {
        if (_disposed)
            throw new ObjectDisposedException(nameof(AcousticEngine));
    }
}
