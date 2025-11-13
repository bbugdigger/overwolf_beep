## How to Build Projects

I used Visual Studio 2022 (v143 toolset).

## How to inject the Dll's

I have used GuidedHacking injector and I have checked checkbox `Auto Inject` which means that dll will be injected into process at exe startup, so before any dll is loaded. It should work with ExtremeInjector's `Auto Inject` feature as well.

## How is the beep sound detected?

The only thing to be added to finally detect beep sound is buffer content analysis on the captured buffers/parameters at IAudioClient vtable hooks. 

1. Vtable-hooking IMMDeviceEnumerator methods
(candidates: GetDevice, GetDefaultAudioEndpoint, EnumAudioEndpoints)
to obtain an IMMDevice pointer.
2. Vtable-hook IMMDevice::Activate to retrieve an IAudioClient pointer.
3. Vtable-hook IAudioClient and IAudioRendererClient methods to capture audio format and buffer parameters
(sample rate, buffer size, channels).
4. Analyze captured buffers/parameters at the IAudioClient hook to determine the beep sound

Full details about my reverse engineering can be found in [Approach.md](https://github.com/bbugdigger/overwolf_beep/blob/main/Approach.md).
