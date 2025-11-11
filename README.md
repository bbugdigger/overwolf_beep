## How to Build Projects

I used v143 MSVC from Visual Studio 2022 but it can be built with older versions also (v142 and v141).

## How to inject the Dll's

### For projects:
- beep (CoCreateInitialize hooking)
- dsound_hooking (Dsound.dll hooking)
- winmm_hooking (winmm.dll hooking)

I have used GuidedHacking injector and i have checked checkbox `Auto Inject` which means that dll will be injected into process at exe startup, so before any dll is loaded. I dont know if ExtremeInjector has this feature.

### For projects:
- dsound_hijacking
- mmdevapi_hijacking
- xaudio_hijacking (XAudio2.dll)
- xaudio_hijack (XAudio2_9redist.dll - comes with a game)

I have performed dll proxying/hijacking with these dll's, meaning they will need to be put in the same directory as `CaravanSandWitch-Win64-Shipping.exe`, except XAudio2_9redist.dll which will need to be put in `C:\Program Files (x86)\Steam\steamapps\common\Caravan Sandwitch Demo\Engine\Binaries\ThirdParty\Windows\XAudio2_9\x64` folder.

The goal with dll hijacking was to try to hook some functions that game imports from these dlls to try to get COM objects. I wasnt sure that `Auto Inject` was working as intended and this was before i tried hooking `CoCreateInitialize`.

## How is the beep sound detected?

While the project is not yet finished I have managed to catch the COM object from which I will start vtable hooking to finally detect beep sound. Full details about my reverse engineering can be found in `Approach.md` file.
