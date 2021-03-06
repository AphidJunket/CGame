// Windows Entry Code

// ctrl+k is comment   ctrl+q is remove
//ctrl+F6 to compile
// shft+alt+arrow to select columns


//THINGS THAT WILL GO IN HERE!
	// SAVE GAME LOCATIONS
	// HANDLE TO EXE FILE
	// ASSET LOADING PATH
	// THREADING
	// RAW INPUT
	// SLEEP/TIMEBEGINPERIOD
	// CLIPCURSOR - FOR MULTIMONITOR SUPPORT
	// FULLSCREEN SUPPORT
	// WM_SETCURSOR - VISIBILITY OF CURSOR
	// QUERYCANCELAUTOPLAY
	// WM_ACTIVATEAPP
	// BLIT SPEED IMPROVEMENTS
	// HARDWARE ACCELERATION (OPEN gl OR DIRECT3D OR BOTH)
	// GETKEYBOARDLAYOUT (INTERNATIONL KEYBOARDS)
// - TO BE CONTINUED

#include "handmade.h"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"


global_variable bool GlobalRunning; //global for now
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable	LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;
global_variable bool32 GlobalPause;

//this takes two functions from XInput.h but allows them to be called by us
// GET STATE
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//SET STATE
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration) 
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

//DIRECT SOUND DEFINE
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	if(Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result = {};
	
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,0,0);
	if(FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if(GetFileSizeEx(FileHandle, &FileSize))
		{
			uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize.QuadPart, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			if(Result.Contents)
			{
				DWORD BytesRead;
				if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
				{
					//File read successfully
					Result.ContentsSize = FileSize32;
				}
				else
				{
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
			else
			{
				//logging
			}
		}
		else
		{
			
		}
		CloseHandle(FileHandle);
	}
	return(Result);
}


DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
	bool32 Result = false;
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,0,0);
	if(FileHandle != INVALID_HANDLE_VALUE)
	{
		
		DWORD BytesWritten;
		if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			//File Written successfully
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			//Logging
		}

		CloseHandle(FileHandle);
	}
	else
	{
		//Logingg
	}
	
	return(Result);
}

struct win32_game_code
{
	HMODULE GameCodeDLL;
	FILETIME DLLLastWriteTime;
	game_update_and_render *UpdateAndRender;
	game_get_sound_samples *GetSoundSamples;
	
	bool32 IsValid;
};

inline FILETIME
Win32GetLastWriteTime(char *Filename)
{
	FILETIME LastWriteTime = {};
	
	WIN32_FIND_DATA FindData;
	HANDLE FindHandle = FindFirstFileA(Filename, &FindData);
	if(FindHandle != INVALID_HANDLE_VALUE)
	{
		LastWriteTime = FindData.ftLastWriteTime;
		FindClose(FindHandle);
	}
	return(LastWriteTime);
}	

internal win32_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
	win32_game_code Result = {};
	
	Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
	
	CopyFile(SourceDLLName, TempDLLName, FALSE);
	Result.GameCodeDLL = LoadLibraryA(TempDLLName);
	if(Result.GameCodeDLL)
	{
		Result.UpdateAndRender = (game_update_and_render *)
			GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
		Result.GetSoundSamples = (game_get_sound_samples *)
			GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
			
		Result.IsValid = (Result.UpdateAndRender &&
						  Result.GetSoundSamples);
	}
	
	if(!Result.IsValid)
	{
	
		Result.UpdateAndRender = GameUpdateAndRenderStub;
		Result.GetSoundSamples = GameGetSoundSamplesStub;
	}
	
	return(Result);
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode)
{
	if(GameCode->GameCodeDLL)
	{
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}
	
	GameCode->IsValid = false;
	GameCode->UpdateAndRender = GameUpdateAndRenderStub;
	GameCode->GetSoundSamples = GameGetSoundSamplesStub;
}

internal void
Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if(!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}
	if(!XInputLibrary)
	{
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}	
	if(XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
	}
}

internal void
Win32InitDSound(HWND Window, int SamplesPerSecond, int32 BufferSize)
{
	// Load DSound Library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if(DSoundLibrary)
	{
		// Get DirectSound object - Get a pointer to a function
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{	
			// "Create" primary buffer
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2; //stereo
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign; 
			WaveFormat.cbSize = 0;
			
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						
					}
					else
					{
						//diagnostic
					}
				}
				else
				{
					//diagnostic
				}
			}
			else
			{
				//diagnostic
			}
			// "Create" secondary buffer
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
				
			}

	// Start playing!
		}
		else
		{
			//diagnostic
		}
	}
	else
	{
		//diagnostic
	}
	

}


internal win32_window_dimension 
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;
	
	return(Result);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if(Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}
	
	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;
	
	// When the biHeight is negative is tells windows this is a top-down bitmap,  first pixel is top-left not bottom left
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB; 
	
	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;  //bytes
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	
	Buffer->Pitch = Width*Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
						   HDC DeviceContext,
						   int WindowWidth, int WindowHeight)
{
	//ASPECT RATIO CORRECTION
	StretchDIBits(DeviceContext,
				  // X, Y, Width, Height,
				  // X, Y, Width, Height, 
				  0, 0, WindowWidth, WindowHeight,							  
				  0, 0, Buffer->Width, Buffer->Height,
				  Buffer->Memory,
				  &Buffer->Info,
				  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK 
Win32MainWindowCallback(
					HWND Window,
					UINT Message,
					WPARAM WParam,
					LPARAM LParam)
{
	LRESULT Result = 0;
	switch(Message)
	{

		case WM_CLOSE:
		{
			GlobalRunning = false;
			// - SET UP HANDLE WITH A MESSAGE TO USER
		} break;
		
		case WM_ACTIVATEAPP:   //are we the active window? ?
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;
		
		case WM_DESTROY:
		{
			GlobalRunning = false;
			// - SET UP HANDLE AS AN ERROR - RECREATE WINDOW
		} break;
		
		// case WM_SYSKEYDOWN:
		// case WM_SYSKEYUP:
		// case WM_KEYDOWN:
		// case WM_KEYUP:
		// {
			// Assert(!"Keyboard input came in through a no-dispatch message!");
		// } break;
		
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);					
			Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
									   Dimension.Width, Dimension.Height);
			
			EndPaint(Window,&Paint);
			OutputDebugStringA("WMPAINT DONE\n");
		}break;
		default:
		{
			// OutputDebugStringA("default\n");
			Result = DefWindowProcA(Window, Message, WParam, LParam);
		} break;
	}
	return(Result);
}


internal void
Win32ClearBuffer(win32_sound_output *SoundOutput)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
								&Region1, &Region1Size,
								&Region2, &Region2Size,
 								0)))
	{
			
		uint8 *DestSample = (uint8 *)Region1;
		for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
		{
			*DestSample++ = 0;
		}

		DestSample = (uint8 *)Region2;
		for(DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
		{
			*DestSample++ = 0;
		}
	
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);	
		
	}
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
					 game_sound_output_buffer *SourceBuffer)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
								&Region1, &Region1Size,
								&Region2, &Region2Size,
 								0)))
	{
			
		DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		DestSample = (int16 *)Region2;
		for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);						

	}
}
internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
	Assert(NewState->EndedDown != IsDown);
	
	NewState->EndedDown = IsDown;
	OutputDebugStringA("Holding");
	++NewState->HalfTransitionCount;
}

internal void
Win32ProcessXinputDigitalButton(DWORD XInputButtonState,
								game_button_state *OldState, DWORD ButtonBit,
								game_button_state *NewState)
{
		NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
		NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
	real32 Result = 0;
	if(Value < -DeadZoneThreshold)
	{
		Result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
	}
	else if(Value > DeadZoneThreshold)
	{
		Result = (real32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
	}
	return(Result);
}

internal void
Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
	MSG Message;
	while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch(Message.message)
		{
			case WM_QUIT:
			{
				GlobalRunning = false;
			} break;
			
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				uint32 VKCode = (uint32)Message.wParam;
				bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);  //this is for holding down keys
				bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);  //this is for holding down keys
				if(WasDown != IsDown)
				{
					if(VKCode == 'W')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
					}
					else if(VKCode == 'A')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);						
					}
					else if(VKCode == 'D')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);						
					}
					else if(VKCode == 'S')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);						
					}
					else if(VKCode == 'Q')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
					}
					else if(VKCode == 'E')
					{
						Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);								
					}
					else if(VKCode == VK_UP)
					{    
						Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
					}                 
					else if(VKCode == VK_LEFT)
					{   
						Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
					}	              
					else if(VKCode == VK_DOWN)
					{                 
						Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
					}                 
					else if(VKCode == VK_RIGHT)
					{
						Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
					}                 
					else if(VKCode == VK_ESCAPE)
					{ 
						GlobalRunning = false;
						OutputDebugStringA("Quitting\n");	
					}                 
					else if(VKCode == VK_SPACE)
					{
					}
#if HANDMADE_INTERNAL					
					else if(VKCode == 'P')
					{
						if(IsDown)
						{
							GlobalPause = !GlobalPause;
						}
					}
#endif				
				}
				
				bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
				if((VKCode == VK_F4) && AltKeyWasDown)
				{
					GlobalRunning = false;
				}
			}break;
			default:
			{
				TranslateMessage(&Message);
				DispatchMessageA(&Message);	
			}break;
		}
	}	
}


inline LARGE_INTEGER
Win32GetWallClock(void)
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return(Result);
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	real32 Result = ((real32)(End.QuadPart - Start.QuadPart) /
					 (real32)GlobalPerfCountFrequency);
	return(Result);
}
internal void
Win32DebugDrawVertical(win32_offscreen_buffer *Backbuffer,
					   int X, int Top, int Bottom, uint32 Color)
{
	if(Top <= 0)
	{
		Top = 0;
	}
	
	if(Bottom > Backbuffer->Height)
	{
		Bottom = Backbuffer->Height;
	}
	if((X >= 0) && (X < Backbuffer->Width))
	{
		uint8 *Pixel = ((uint8 *)Backbuffer->Memory + X*Backbuffer->BytesPerPixel + Top*Backbuffer->Pitch);
		for(int Y = Top; Y < Bottom; ++Y)
		{
			*(uint32 *)Pixel = Color;
			Pixel += Backbuffer->Pitch;
		}
	}
}	

inline void
Win32DrawSoundBufferMarker(win32_offscreen_buffer *Backbuffer,
						   win32_sound_output *SoundOutput,
						   real32 C, int PadX, int Top, int Bottom,
						   DWORD Value, uint32 Color)
{
	real32 XReal32 = (C * (real32)Value);
	int X = PadX + (int)XReal32;
	Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}						   

internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *Backbuffer,
					  int MarkerCount, win32_debug_time_marker *Markers,
					  int CurrentMarkerIndex,
					  win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame)
{
	int PadX = 16;
	int PadY = 16;
	int LineHeight = 64;

	real32 C = (real32)(Backbuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
	for(int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
	{
		win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];

		DWORD PlayColor = 0xFFFFFFFF;
		DWORD WriteColor = 0x00FF00FF;
		DWORD ExpectedFlipColor = 0xFFFFFF00; //yellow
		DWORD PlayWindowColor = 0xFFFF00FF;  //purple
		
		int Top = PadY;
		int Bottom = PadY + LineHeight;
		if(MarkerIndex == CurrentMarkerIndex)
		{
			int FirstTop = Top;
			Top += LineHeight+PadY ;
			Bottom += LineHeight+PadY;
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor,PlayColor);
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor,WriteColor);
			
			Top += LineHeight+PadY ;
			Bottom += LineHeight+PadY;
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation,PlayColor);
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount,WriteColor);
			
			Top += LineHeight+PadY ;
			Bottom += LineHeight+PadY;
			Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor,ExpectedFlipColor);

		}
		Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor,PlayColor);
		Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480*SoundOutput->BytesPerSample,PlayWindowColor);
		Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor,WriteColor);

	}
						  
}

void
CatStrings(size_t SourceACount, char *SourceA,
		   size_t SourceBCount, char *SourceB,
		   size_t DestCount , char *Dest)
{
	for(int Index = 0; Index < SourceACount; ++Index)
	{
		*Dest++ = *SourceA++;
	}
	for(int Index = 0; Index < SourceBCount; ++Index)
	{
		*Dest++ = *SourceB++;
	}
	
	*Dest++ = 0; //this is to add a null at the end as all C Strings end in null pointers...
}

int CALLBACK 
WinMain(
		HINSTANCE Instance,
		HINSTANCE PrevInstance,
		LPSTR     CommandLine,
		int       ShowCode)
{
	
	//This finds the exe filename and truncates the path so we can call it something else
	char EXEFilename[MAX_PATH];
	DWORD SizeOfFilename = GetModuleFileNameA(0, EXEFilename, sizeof(EXEFilename));
	char *OnePastLastSlash = EXEFilename;
	for(char *Scan = EXEFilename; *Scan; ++Scan)
	{
		if(*Scan == '\\')
		{
			OnePastLastSlash = Scan + 1;
		}
	}
	
	char SourceGameCodeDLLFilename[] = "handmade.dll";
	char SourceGameCodeDLLFullPath[MAX_PATH];
	CatStrings(OnePastLastSlash - EXEFilename, EXEFilename,
			   sizeof(SourceGameCodeDLLFilename) - 1, SourceGameCodeDLLFilename,
			   sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);
	
	char TempGameCodeDLLFilename[] = "handmade_temp.dll";
	char TempGameCodeDLLFullPath[MAX_PATH];
	CatStrings(OnePastLastSlash - EXEFilename, EXEFilename,
			   sizeof(TempGameCodeDLLFilename) - 1, TempGameCodeDLLFilename,
			   sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);
			   
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;
	
	UINT DesiredSchedularMS = 1;
	bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedularMS) == TIMERR_NOERROR);
	
	Win32LoadXInput();
	
	WNDCLASSA WindowClass = {};
	
	// win32_window_dimension Dimension = Win32GetWindowDimension(Window);
	Win32ResizeDIBSection(&GlobalBackbuffer,1280, 720);

	WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	// HICON     hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

#define	MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)
	real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;
	
	if(RegisterClass(&WindowClass))
	{
		HWND Window = 
			CreateWindowEx(
						   0,
						   WindowClass.lpszClassName,
						   "Handmade Hero",
						   WS_OVERLAPPEDWINDOW|WS_VISIBLE,
						   CW_USEDEFAULT,
						   CW_USEDEFAULT,
						   CW_USEDEFAULT,
						   CW_USEDEFAULT,
						   0,
						   0,
						   Instance,
						   0);
		if(Window)
		{
			HDC DeviceContext = GetDC(Window);
			
			//Sound Test
			win32_sound_output SoundOutput = {};
			
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = 3*(SoundOutput.SamplesPerSecond / GameUpdateHz);
			SoundOutput.SafetyBytes = (SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample / GameUpdateHz)/2;
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
			

			GlobalRunning = true;
#if 0
			//THIS TESTS THE PLAYCURSOR/WRITECURSOE AUDIO UPDATE FREQUENCY
			//it was 480BYTES
			while(GlobalRunning)
			{
				DWORD PlayCursor;
				DWORD WriteCursor;
				GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
				
				char TextBuffer[256];
				_snprintf_s(TextBuffer, sizeof(TextBuffer),
							"PC:%u WC%u\n", PlayCursor, WriteCursor);
				OutputDebugStringA(TextBuffer);
			}
#endif
			int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
												   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

						
#if HANDMADE_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID BaseAddress = 0;
#endif

						
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes((uint64)4);
			GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
			GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
			GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
			
			uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalSize,
													   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage +
										  GameMemory.PermanentStorageSize);
			
			if(Samples && GameMemory.PermanentStorage &&GameMemory.TransientStorage)
			{
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];
				
				LARGE_INTEGER LastCounter = Win32GetWallClock();
				LARGE_INTEGER FlipWallClock = Win32GetWallClock();
				
				int DebugTimeMarkerIndex = 0;
				win32_debug_time_marker DebugTimeMarkers[GameUpdateHz / 2] = {0};
				
				DWORD AudioLatencyBytes = 0;
				real32 AudioLatencySeconds = 0;
				bool32 SoundIsValid = false;

				win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
				uint32 LoadCounter = 0;
				
				uint64 LastCycleCount = __rdtsc();
				while(GlobalRunning)
				{
					FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
					if(CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
					{
						Win32UnloadGameCode(&Game);
						Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);						
						LoadCounter = 0;
					}

					
					game_controller_input *OldKeyboardController = GetController(OldInput, 0);
					game_controller_input *NewKeyboardController = GetController(NewInput, 0);
					*NewKeyboardController = {};
					NewKeyboardController->IsConnected = true;
					for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
					{
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = 
						OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}
					Win32ProcessPendingMessages(NewKeyboardController);
										
					if(!GlobalPause)
					{					
						// gameuser input
						//for however many controllers there are connected
						DWORD MaxControllerCount = XUSER_MAX_COUNT;
						if(MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
						{
							MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
						}
						
						for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount;++ControllerIndex)
						{
							DWORD OurControllerIndex = ControllerIndex + 1;
							game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
							game_controller_input *NewController = GetController(NewInput, OurControllerIndex);
						
							XINPUT_STATE ControllerState;
							if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
							{
								NewController->IsConnected = true;
								
								//Controller plugged in
								XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
								
								NewController->IsAnalog = true;
								NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
								{
									NewController->StickAverageY = 1.0f;
								}
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
								{
									NewController->StickAverageY = -1.0f;
								}
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
								{
									NewController->StickAverageX = -1.0f;
								}
								if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
								{
									NewController->StickAverageX = 1.0f;
								}
								
								real32 Threshold = 0.5f;
								Win32ProcessXinputDigitalButton(
									(NewController->StickAverageX < -Threshold) ? 1 : 0,
									&OldController->MoveLeft, 1,
									&NewController->MoveLeft);
								Win32ProcessXinputDigitalButton(
									(NewController->StickAverageX > Threshold) ? 1 : 0,
									&OldController->MoveRight, 1,
									&NewController->MoveRight);
								Win32ProcessXinputDigitalButton(
									(NewController->StickAverageY < -Threshold) ? 1 : 0,
									&OldController->MoveUp, 1,
									&NewController->MoveUp);
								Win32ProcessXinputDigitalButton(
									(NewController->StickAverageY > Threshold) ? 1 : 0,
									&OldController->MoveDown, 1,
									&NewController->MoveDown);
																
								Win32ProcessXinputDigitalButton(Pad->wButtons,
																&OldController->ActionDown, XINPUT_GAMEPAD_A,
																&NewController->ActionDown);
								Win32ProcessXinputDigitalButton(Pad->wButtons,
																&OldController->ActionRight, XINPUT_GAMEPAD_B,
																&NewController->ActionRight);																		
								Win32ProcessXinputDigitalButton(Pad->wButtons,
																&OldController->ActionLeft, XINPUT_GAMEPAD_X,
																&NewController->ActionLeft);
								Win32ProcessXinputDigitalButton(Pad->wButtons,
																&OldController->ActionUp, XINPUT_GAMEPAD_Y,
																&NewController->ActionUp);						
								Win32ProcessXinputDigitalButton(Pad->wButtons,
																&OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
																&NewController->LeftShoulder);						
								Win32ProcessXinputDigitalButton(Pad->wButtons,
																&OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
																&NewController->RightShoulder);						
								Win32ProcessXinputDigitalButton(Pad->wButtons,
																&OldController->Start, XINPUT_GAMEPAD_START,
																&NewController->Start);							
								Win32ProcessXinputDigitalButton(Pad->wButtons,
																&OldController->Back, XINPUT_GAMEPAD_BACK,
																&NewController->Back);
													
							}	
							else
							{
								NewController->IsConnected = false;
								// Controller not available
							}
						}
						
						
				
							

						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackbuffer.Memory;
						Buffer.Width = GlobalBackbuffer.Width;
						Buffer.Height = GlobalBackbuffer.Height;
						Buffer.Pitch = GlobalBackbuffer.Pitch;
						Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);
						
						LARGE_INTEGER AudioWallClock = Win32GetWallClock();
						real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);
						
						DWORD PlayCursor;
						DWORD WriteCursor;
						if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
						{	
							if(!SoundIsValid)
							{
								SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
								SoundIsValid = true;
							}
							
							DWORD ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample)
													% SoundOutput.SecondaryBufferSize);
							
							DWORD ExpectedSoundBytesPerFrame = 
								(SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample)/ GameUpdateHz;
							real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
							DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip/TargetSecondsPerFrame)*(real32)ExpectedSoundBytesPerFrame);
							
							DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedSoundBytesPerFrame;
							
							DWORD SafeWriteCursor = WriteCursor;
							if(SafeWriteCursor < PlayCursor)
							{
								 SafeWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							Assert(SafeWriteCursor >= PlayCursor);
							SafeWriteCursor += SoundOutput.SafetyBytes;
							
							bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);
							
							DWORD TargetCursor=0;
							if(AudioCardIsLowLatency)
							{
								TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);							
							}
							else
							{
								TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + 
												SoundOutput.SafetyBytes);
							}
							TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);
							
							DWORD BytesToWrite=0;												
							if(ByteToLock > TargetCursor)
							{
								BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
								BytesToWrite += TargetCursor;
							}
							else
							{
								BytesToWrite = TargetCursor - ByteToLock;
							}	
						
							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples = Samples;					
							Game.GetSoundSamples(&GameMemory, &SoundBuffer);
						
	// DirectSound output test
#if HANDMADE_INTERNAL

							
							win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
							Marker->OutputPlayCursor = PlayCursor;
							Marker->OutputWriteCursor = WriteCursor;
							Marker->OutputLocation = ByteToLock;
							Marker->OutputByteCount = BytesToWrite;
							Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
							
							DWORD UnwrappedWriteCursor = WriteCursor;
							if(UnwrappedWriteCursor < PlayCursor)
							{
								UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
							}
							AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
							AudioLatencySeconds = 
								(((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample)
								 / (real32)SoundOutput.SamplesPerSecond);
							char TextBuffer[256];
							_snprintf_s(TextBuffer, sizeof(TextBuffer),
										"BTL:%u TargetCursor:%u BTW:%u  -  PlayC:%u WriteC:%u DELTA:%u (%fs)\n",
										ByteToLock, TargetCursor, BytesToWrite,
										PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
							OutputDebugStringA(TextBuffer);							
#endif
							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);						
						}
						else
						{
							SoundIsValid = false;
						}
						
					
						LARGE_INTEGER WorkCounter = Win32GetWallClock();
						real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
						
						real32 SecondsElapsedForFrame = WorkSecondsElapsed;
						if(SecondsElapsedForFrame < TargetSecondsPerFrame)
						{
							if(SleepIsGranular)
							{
								DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
								if(SleepMS > 0)
								{
									Sleep(SleepMS);								
								}
							}
							
							real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							if(TestSecondsElapsedForFrame < TargetSecondsPerFrame)
							{
								//LOG MISSED SLEEP TIME
							}
							while(SecondsElapsedForFrame < TargetSecondsPerFrame)
							{
								SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
							}
						}
						else
						{
								//Logging
						}
						
						LARGE_INTEGER EndCounter = Win32GetWallClock();
						real32 MSPerFrame = 1000.0f*Win32GetSecondsElapsed(LastCounter, EndCounter);
						LastCounter = EndCounter;
						
						//this displays the frame
						win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
						Win32DebugSyncDisplay(&GlobalBackbuffer, ArrayCount(DebugTimeMarkers), DebugTimeMarkers,
											  DebugTimeMarkerIndex-1,&SoundOutput, TargetSecondsPerFrame);
#endif
						Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, 
												   Dimension.Width, Dimension.Height);
												   
						FlipWallClock = Win32GetWallClock();

#if HANDMADE_INTERNAL
		//DEBUG CODE
						{
							DWORD PlayCursor;
							DWORD WriteCursor;
							if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
							{
								win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];

								Marker->FlipPlayCursor = PlayCursor;
								Marker->FlipWriteCursor = WriteCursor;
							}
							
						}
#endif

				
						game_input *Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp;
				
						
						uint64 EndCycleCount = __rdtsc();
						uint64 CyclesElapsed = EndCycleCount - LastCycleCount;				
						LastCycleCount = EndCycleCount;
						
						real64 FPS = 0.0f;
						real64 MCPF = ((real64)CyclesElapsed / (1000.0f* 1000.0f));
					
						char FPSBuffer[256];
						_snprintf_s(FPSBuffer, sizeof(FPSBuffer),
									"%.02fms/f, %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);
						OutputDebugStringA(FPSBuffer);					
#if HANDMADE_INTERNAL
						++DebugTimeMarkerIndex;
						if(DebugTimeMarkerIndex == ArrayCount(DebugTimeMarkers))
						{
							DebugTimeMarkerIndex = 0;
						}					
#endif
					}
				}
			}
			else
			{
				//Logging
			}
		}
		else
		{
			//  LOGGING
		}
	}
	
	return(0);
}
