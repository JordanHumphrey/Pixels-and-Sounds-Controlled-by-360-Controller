//========================================================
// Main source: win32_engine.exe
// Creator: Jordan
// S: C++
//========================================================

#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>

// TODO(jordan): put these in some sort of resource file
#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265358f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

struct win32_offscreen_buffer
{
	// NOTE(jordan): Pixels are always 32-bits wide. Memory order BB GG RR XX
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};


struct win32_window_dimension
{
	int Width;
	int Height;
};

// TODO(jordan): this is golbal for now
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// NOTE(jordan): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(jordan): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput(void)
{
	// NOTE(jordan): test this on a windows 8 machine
	HMODULE XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	if(!XInputLibrary)
	{
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}

	if(XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		if(!XInputGetState) { XInputGetState = XInputGetStateStub; }
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
		if(!XInputSetState) { XInputSetState = XInputSetStateStub; }
	}
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	// NOTE(jordan): Load up the dsound.dll
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if(DSoundLibrary)
	{
		// NOTE(jordan): create a DSound object
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)
		  GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;					
			WaveFormat.cbSize = 0;

			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// NOTE(jordan): create a primary buffer
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
					if(SUCCEEDED(Error))
					{
						// NOTE(jordan): the format has been set of direct sound
						OutputDebugStringA("Primary buffer format was set.\n");
					}
					else
					{
						// TODO(jordan): diagnostic
					}

				}
				else
				{
					// TODO(jordan): diagnostic
				}
			}
			else
			{
				// TODO(jordan): Diagnostic
			}

			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
			if(SUCCEEDED(Error))
			{
				OutputDebugStringA("Secondary Buffer created successfully. \n");
			}

			// NOTE(jordan): start playing
		}
		else
		{
			// TODO(jordan): Diagnostics
		}
	}
	else
	{
		// TODO(jordan): Diagnostics
	}

}

struct win32_sound_output
{
	// NOTE(jordan): sound test
  	int SamplesPerSecond;
  	int ToneHz;
  	int16 ToneVolume;
  	uint32 RunningSampleIndex;
  	int WavePeriod;
  	int BytesPerSample;
  	int SecondaryBufferSize;
  	real32 tSine;
  	int LatencySampleCount;
};

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
	// 16bit 16bit 16bit ...
	// [LEFT RIGHT] LEFT RIGHT LEFT RIGHT LEFT ...
	 
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
		int16 *SampleOut = (int16 *)Region1;							
		for(DWORD SampleIndex = 0;
			SampleIndex < Region1SampleCount;
			++SampleIndex)
		{
			real32 SineValue = sinf(SoundOutput->tSine);
			int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;

			SoundOutput->tSine += 2.0f*Pi32*1.0f / (real32)SoundOutput->WavePeriod;
			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		SampleOut = (int16  *)Region2;
		for(DWORD SampleIndex = 0;
			SampleIndex < Region2SampleCount;
			++SampleIndex)
		{
			real32 SineValue = sinf(SoundOutput->tSine);
			int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;

			SoundOutput->tSine += 2.0f*Pi32*1.0f / (real32)SoundOutput->WavePeriod;
			++SoundOutput->RunningSampleIndex;
		}
		
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

win32_window_dimension
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
RenderGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	// TODO(jordan): lets see what the optimizer does for this
	
	uint8 *Row = (uint8 *)Buffer->Memory;

	for(int Y = 0; 
		Y < Buffer->Height;
		++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for(int X = 0;
			X < Buffer->Width;
			++X)
		{
			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);

			/*
				Pixel (32 - bits)
				
				Memory:		BB GG RR xx
				Register: 	XX RR GG BB

				Or the bits of blue with the bits of green.
				Move them up to where they would be, which is 8 bits up.
				So I shift them 8 bits and OR them together to my Pixel.
			*/

			*Pixel++ = ((Green << 8) | Blue);
		}

		Row += Buffer->Pitch;
	}
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	// TODO(jordan): bulletproof this
	// Maybe don't free first, free after, then free first if that fails
	
	if(Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	
	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	// TODO(jordan): probly want to clear this to black

	Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,						   
						   HDC DeviceContext, int WindowWidth, int WindowHeight)
{
	// TODO(jordan) Aspect Ratio Correction
	StretchDIBits(DeviceContext, 
					/*
					X, Y, Width, Height,
					X, Y, Width, Height,
					*/
					0, 0, WindowWidth, WindowHeight,
					0, 0, Buffer->Width, Buffer->Height,
					Buffer->Memory,
					&Buffer->Info,
					DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK 
Win32MainWindowCallback(HWND   Window,
  				   UINT   Message,
  				   WPARAM WParam,
  				   LPARAM LParam)
{
	LRESULT Result = 0;

	switch(Message)
	{
		case  WM_CLOSE:
		{
			//TODO(jordan): handle this with a message to the user?
			GlobalRunning = false;
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugString("WM_ACTIVATEAPP\n");
		} break;

		case WM_DESTROY:
		{
			//TODO(jordan): handle this as an error - recreate window?
			GlobalRunning = false;
		} break;
	
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VKCode = WParam;
			bool WasDown = ((LParam & (1 << 30)) != 0);
			bool IsDown = ((LParam & (1 << 31)) == 0);

			if(WasDown != IsDown)
			{
				if(VKCode == 'W')
				{
				}
				else if(VKCode == 'A')
				{
				}
				else if(VKCode == 'S')
				{
				}
				else if(VKCode == 'D')
				{
				}
				else if(VKCode == 'Q')
				{
				}
				else if(VKCode == 'E')
				{
				}
				else if(VKCode == VK_UP)
				{
				}
				else if(VKCode == VK_LEFT)
				{
				}
				else if(VKCode == VK_DOWN)
				{
				}
				else if(VKCode == VK_RIGHT)
				{
				}
				else if(VKCode == VK_ESCAPE)
				{
					OutputDebugStringA("ESCAPE: ");
					if(IsDown)
					{
						OutputDebugStringA("IsDown");
					}
					if(WasDown)
					{
						OutputDebugStringA("WasDown");
					}
					OutputDebugStringA("\n");
				}
				else if(VKCode == VK_SPACE)
				{
				}
			}

			bool32 AltKeyWasDown = ((LParam & (1 << 29)) != 0);
			if((VKCode == VK_F4) && AltKeyWasDown)
			{
				GlobalRunning = false;
			}
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, 
									   Dimension.Width, Dimension.Height);
			
			EndPaint(Window, &Paint);

		} break;

		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return (Result);
}

int 	CALLBACK
WinMain(HINSTANCE Instance,
		HINSTANCE PrevInstance,
		LPSTR CommandLine,
		int ShowCode)
{
	LARGE_INTEGER PerfCountFrequencyResult;
  	QueryPerformanceFrequency(&PerfCountFrequencyResult);
  	int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	Win32LoadXInput();

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
  	WindowClass.lpfnWndProc = Win32MainWindowCallback;
  	WindowClass.hInstance = Instance;
  	//HICON     hIcon;
  	WindowClass.lpszClassName = "EngineWindowClass";

  	if(RegisterClass(&WindowClass))
  	{
  		HWND Window = 
  			CreateWindowExA(
  				0,
  				WindowClass.lpszClassName,
  				"Engine v0.1",
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
  				// NOTE(jordan): since we specified CS_OWNDC, we can just
  				// get one device context and use it forever because we
  				// are not sharing it with anyone.
  				HDC DeviceContext = GetDC(Window);

  				// NOTE(jordan): graphics test
  				int XOffset = 0;
  				int YOffset = 0;

				win32_sound_output SoundOutput = {};
				SoundOutput.SamplesPerSecond = 48000;
				SoundOutput.ToneHz = 256;
				SoundOutput.ToneVolume = 3000;
				SoundOutput.RunningSampleIndex = 0;
				SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
				SoundOutput.BytesPerSample = sizeof(int16)*2;
				SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
				SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
  				Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
  				Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample);
				GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

				GlobalRunning = true;

  				LARGE_INTEGER LastCounter;
				QueryPerformanceCounter(&LastCounter);
  				while(GlobalRunning)
  				{
  					MSG Message;
  					while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
  					{
  						if(Message.message == WM_QUIT)
  						{
  							GlobalRunning = false;
  						}

  						TranslateMessage(&Message);
  						DispatchMessage(&Message);
  					}

  					// TODO(jordan): should we poll this more frequently?
  					for(DWORD ControllerIndex = 0;
  						ControllerIndex <  XUSER_MAX_COUNT;
  						++ControllerIndex)
  					{
  						XINPUT_STATE ControllerState;
  						if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
  						{
  							// NOTE(jordan): this controller is plugged in
  							// TODO(jordan): see if ControllerState.dwPacketNumber incrememnts too rapidly for us to care
  							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

  							bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
  							bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
  							bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
  							bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
  							bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
  							bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
  							bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
  							bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
  							bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
  							bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
  							bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
  							bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

  							int16 StickX = Pad->sThumbLX;
  							int16 StickY = Pad->sThumbLY;

  							// TODO(jordan): deadzone handling needed using XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
  							XOffset += StickX / 4096;
  							YOffset += StickY / 4096;

							SoundOutput.ToneHz = 512 + (int)(256.0f*((real32)StickY / 30000.0f));
							SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
  						}
  						else
  						{
  							// NOTE(jordan): this controller is not plugged in
  						}
  					}

  					
  					/*XINPUT_VIBRATION Vibration;
  					Vibration.wLeftMotorSpeed = 60000;
  					Vibration.wRightMotorSpeed = 60000;
  					XInputSetState(0, &Vibration); */

					RenderGradient(&GlobalBackBuffer, XOffset, YOffset);

					// NOTE(jordan): DirectSound output test - square wave

					DWORD PlayCursor;
					DWORD WriteCursor;
					if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
					{
						DWORD ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % 
											SoundOutput.SecondaryBufferSize);
						DWORD TargetCursor = ((PlayCursor + 
											 (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) %
											 SoundOutput.SecondaryBufferSize);
						DWORD BytesToWrite;
						if(ByteToLock > TargetCursor)
						{
							BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
							BytesToWrite += TargetCursor;
						}
						else
						{
							BytesToWrite = TargetCursor - ByteToLock;
						}

						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
						
					}

					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
					Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
											   Dimension.Width, Dimension.Height);

					LARGE_INTEGER EndCounter;
  					QueryPerformanceCounter(&EndCounter);

  					int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
  					int32 MSPerFrame = ((1000*CounterElapsed) / PerfCountFrequency);

  					char Buffer[256];
  					wsprintf(Buffer, "Milliseconds/Frame: %dms\n", MSPerFrame);
  					OutputDebugStringA(Buffer);

  					LastCounter = EndCounter;
  				}
  			}
  			else
  			{
  				//TODO(jordan): Logger system
  			}
  	}
  	else
  	{
  		// TODO(jordan): logger system
  	}

	return (0);
}