# Reverse Engineering Approach: Detecting Game Audio Events via WASAPI Hooking

**Key Achievement**: Successfully identified the exact audio buffer containing the beep sound by hooking Windows Audio Session API (WASAPI) COM interfaces

**What to Improve:** Analyzing buffer contents in real-time

---

## Static Analysis using IDA Pro
- Enumerated the IAT table and checked cross-references in IDA for file, resource, input, and audio-related APIs.  
- Formulated hypotheses: beep could be  
  a) read from a file,  
  b) embedded as a resource, or  
  c) produced by an audio engine at runtime.  
- Only one small resource is loaded via resource APIs — unlikely to be the audio asset.

---

## APIs / Interfaces Monitored / Attempted Hooks

### File & Resource APIs

```
CreateFileW
ReadFile
SetFilePointer
GetFileSize
CreateFileMappingW
MapViewOfFile
UnmapViewOfFile
FindResourceW
LoadResource
LockResource
SizeofResource
```

### Audio / COM Related
```
CoCreateInstance (instrumented to log created COM objects)
CoInitialize
CoInitializeEx
```

### Keyboard / Input / Message Handling
```
GetRawInputData
RegisterRawInputDevices
GetAsyncKeyState
GetKeyState
GetMessageW
PeekMessageW
DispatchMessageW
SetWindowsHookExW
SetWindowLongPtrW

Monitored window messages:
WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_SYSKEYDOWN
```

### Observed Audio-Related DLLs

```
dsound.dll
winmm.dll
xaudio2.dll
xaudio2_redist9.dll
MMDevAPI.dll
```

---

## Dynamic Analysis
- Hooked common audio APIs (e.g., `waveOut*`) and attempted DLL injection / proxying for `dsound`, `winmm`, and `xaudio` dlls to observe calls and capture COM pointers.  
- Instrumented `CoCreateInstance` at startup and logged CLSIDs / created COM objects.  
  This revealed `IMMDeviceEnumerator` being instantiated, indicating use of MMDevice / WASAPI.

**Traced Spacebar Handler**:
1. Located window message handling code (`WM_KEYDOWN` with `wParam == VK_SPACE`)
2. Followed execution path from spacebar detection to audio invocation
3. Audio playback is invoked via an **indirect function pointer call**, not a direct API call like `PlaySound()`
4. That means that the games audio system uses abstraction layers (COM interfaces), making simple API hooking insufficient.

**COM Object Discovery**:
1. During CoCreateInstance instrumentation I discovered that IMMDeviceEnumerator COM object was created during startup
2. That confirm the hypothesis that the game audio system uses WASAPI interfaces

**COM Interface Hooking:**
COM interfaces are accessed through **vtables** (virtual function tables). Each COM object has a pointer to its vtable, which contains function pointers. So that means we need to perform vtable hooking (patching the function pointers in the vtable to redirect calls to our hooks)

**IMMDeviceEnumerator Hooking**
Since in logs it was noticed that IMMDeviceEnumerator COM object was created I performed vtable hooking on following functions:
- `GetDefaultAudioEndpoint` - Gets the default audio output device
- `EnumAudioEndpoints` - Enumerates all audio devices
- `GetDevice` - Gets a specific device by ID

The objective was to capture `IMMDevice` pointer.

**IMMDevice::Activate Hooking**

When looking at the MSDN docs for IMMDevice::Activate we see that creates a COM object with the specified interface.
By looking at the list of possible interfaces we see some interesting candidates for further hooking:
```
IID_IAudioClient
IID_IDirectSound
IID_IDirectSound8
IID_IDirectSoundCapture
IID_IDirectSoundCapture8
```

By looking at the logs to see which interfaces are created we see `IAudioClient`, so thats what we hook next!

**IAudioClient Hooking**

So the goal is not to monitor audio stream lifecycle and parameters from IAudioClient methods
- `Initialize` - Sets up audio stream (buffer size, sample rate, channels)
- `Start` - Begins audio playback
- `Stop` - Stops audio playback
- `GetService` - Retrieves related interfaces (like `IAudioRenderClient`)

So by further looking at the logs and consulting with MSDN documentation we can infer that IAudioClient and IAudioRenderClient work together:
IAudioClient::Initialize - Sets up the stream (buffer size, format, sample rate)
IAudioClient::Start - Starts the audio playback
IAudioRenderClient::GetBuffer - Gets a pointer to the buffer to WRITE audio data
IAudioRenderClient::ReleaseBuffer - Releases the buffer after writing

Also by looking at the logs we can see that there are 2 IAudioRenderClient instances continuously processing audio.

***

**Key Finiding** 

No New Streams on Spacebar Press: Critically, there are NO new IAudioClient::Initialize or IAudioClient::Start calls after the spacebar press. That means that the beep sound is played on an existing stream.

Now here lies a new problem. We're successfully hooking the buffer operations, but we can't tell which buffer contains the beep just by looking at addresses. Both streams are active all the time.

Next Steps would be inspecting buffer contents! We need to analyze the actual audio data in the buffers to detect when the beep is written.
