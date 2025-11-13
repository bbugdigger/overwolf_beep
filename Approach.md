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
