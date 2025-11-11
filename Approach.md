# Progress Update — Audio Tracing Home Task

## Summary
Inspected the IAT and cross-references to locate where the beep audio originates (external file, embedded resource, or runtime audio subsystem).  
Traced the function invoked by the space-bar handler and determined the binary uses Windows Core Audio (COM/WASAPI) rather than a simple file-based playback.  
Moving from generic API hooking toward vtable / COM-object hooking to capture the audio pipeline state.

---

## What I Did (Static / IDA Reconnaissance)
- Enumerated the IAT and checked cross-references for file, resource, input, and audio-related APIs.  
- Formulated hypotheses: beep could be  
  a) read from a file,  
  b) embedded as a resource, or  
  c) produced by an audio engine at runtime.  
- Traced the code path triggered by the space bar; audio is invoked via an indirect (function-pointer) call.

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
SetWindowsHookExW (WH_KEYBOARD_LL)
SetWindowLongPtrW (to replace WndProc)

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

## Dynamic Experimentation
- Hooked common audio APIs (e.g., `waveOut*`) and attempted DLL injection / proxying for `dsound`, `winmm`, and `xaudio` families to observe calls and capture COM pointers.  
- Instrumented `CoCreateInstance` at startup and logged CLSIDs / created COM objects.  
  This revealed `IMMDeviceEnumerator` being instantiated, indicating use of MMDevice / WASAPI.

---

## Key Findings
- The space-bar handler calls audio playback via an indirect function-pointer; there is no obvious direct call like `PlaySound`.  
- Only one small resource is loaded via resource APIs — unlikely to be the audio asset.  
- The binary constructs `IMMDeviceEnumerator` (MMDevice API), so the audio path is likely using Windows Core Audio / WASAPI rather than simple file APIs or legacy `waveOut` only.

---

## Planned Next Steps (Technical Plan)
1. **Vtable-hook** `IMMDeviceEnumerator` methods  
   (candidates: `GetDevice`, `GetDefaultAudioEndpoint`, `EnumAudioEndpoints`)  
   to obtain an `IMMDevice` pointer.  
2. **Vtable-hook** `IMMDevice::Activate` to retrieve an `IAudioClient` pointer.  
3. **Vtable-hook** `IAudioClient` methods (likely `Initialize`) to capture audio format and buffer parameters  
   (sample rate, buffer size, channels).  
4. Analyze captured buffers/parameters at the `IAudioClient` hook point to determine whether beeps come from  
   in-memory buffers, a streamed file, or are synthesized at runtime.  
5. If necessary, instrument higher-level XAudio2 client interfaces to trace where audio buffers are supplied.

---

## Deliverables Planned for Final Submission
- Short write-up of the tracing process and findings (markdown).  
- Minimal proof-of-concept hooking code (source) with brief build/run instructions.  
- Console/log output demonstrating captured CLSIDs, COM interfaces, and audio-parameter data from the vtable hooks.  
- Optional: annotated IDA screenshot(s) showing the traced space-bar handler and the function-pointer call site.

---

## Notes / Rationale
- Moving to vtable/COM hooking is driven by the presence of `IMMDeviceEnumerator` and the indirect call pattern in the space-bar path.  
- Vtable hooks will expose the Core Audio pipeline state, which is not visible through standard file or waveOut API hooks.  
- The approach minimizes intrusive instrumentation while still extracting key metadata (format, buffer, playback events)  
  needed to identify the beep source.
