using System;
using System.Runtime.InteropServices;

public static class AcousticEngineNative
{
#if UNITY_EDITOR_WIN || UNITY_STANDALONE_WIN
    private const string DLL = "acoustic_engine";
#elif UNITY_EDITOR_OSX || UNITY_STANDALONE_OSX
    private const string DLL = "acoustic_engine";
#elif UNITY_STANDALONE_LINUX
    private const string DLL = "acoustic_engine";
#else
    private const string DLL = "__Internal";
#endif

    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ae_create_engine(ref AeConfig config);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern void ae_destroy_engine(IntPtr engine);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ae_process(IntPtr engine, ref AeBuffer input, ref AeBuffer output);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ae_set_distance(IntPtr engine, float value);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ae_set_room_size(IntPtr engine, float value);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ae_set_brightness(IntPtr engine, float value);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ae_set_width(IntPtr engine, float value);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ae_set_dry_wet(IntPtr engine, float value);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ae_set_intensity(IntPtr engine, float value);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern int ae_apply_scenario(IntPtr engine, string name, float intensity);
    [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
    public static extern AeConfig ae_get_default_config();
}

[StructLayout(LayoutKind.Sequential)]
public struct AeConfig
{
    public uint sampleRate;
    public uint maxBufferSize;
    public IntPtr dataPath;
    public IntPtr hrtfPath;
    [MarshalAs(UnmanagedType.I1)] public bool preloadHrtf;
    [MarshalAs(UnmanagedType.I1)] public bool preloadAllPresets;
    public UIntPtr maxReverbTimeSec;
}

[StructLayout(LayoutKind.Sequential)]
public struct AeBuffer
{
    public IntPtr samples;
    public UIntPtr frameCount;
    public byte channels;
    [MarshalAs(UnmanagedType.I1)] public bool interleaved;
}
