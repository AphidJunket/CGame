#if !defined(WIN32_HANDMADE_H)
struct win32_offscreen_buffer
{
	BITMAPINFO Info; //type of BITMAPINFO, so has own .methods
	void *Memory;
	int Width;
	int Height;
	int Pitch; // how many bytes the pointer needs to go to the next row
	int BytesPerPixel;
};

struct win32_window_dimension
{
	int Width;
	int Height;
};

struct win32_sound_output
{
	int SamplesPerSecond;
	uint32 RunningSampleIndex;
	int BytesPerSample;
	DWORD SecondaryBufferSize;
	DWORD SafetyBytes;
	real32 tSine;
	int LatencySampleCount;
};

struct win32_debug_time_marker
{
	DWORD OutputPlayCursor;
	DWORD OutputWriteCursor;
	DWORD OutputLocation;
	DWORD OutputByteCount;
	
	DWORD ExpectedFlipPlayCursor;
	DWORD FlipPlayCursor;
	DWORD FlipWriteCursor;
};

#define WIN32_HANDMADE_H
#endif