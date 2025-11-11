#include "pch.h"

#include <Windows.h>
#include <stdio.h>
#include <x3daudio.h>

// Forward declarations
struct IXAudio2;
struct IUnknown;

typedef UINT32 XAUDIO2_PROCESSOR;
#define XAUDIO2_DEFAULT_PROCESSOR 0x00000001
#define XAUDIO2_ANY_PROCESSOR 0xffffffff
#define XAUDIO2_DEBUG_ENGINE 0x0001
#define XAUDIO2_STOP_ENGINE_WHEN_IDLE 0x0002

// Export XAudio2 functions with correct ordinals
#pragma comment(linker,"/export:XAudio2Create,@1")
#pragma comment(linker,"/export:CreateAudioReverb,@2")
#pragma comment(linker,"/export:CreateAudioVolumeMeter,@3")
#pragma comment(linker,"/export:CreateFX,@4")
#pragma comment(linker,"/export:X3DAudioCalculate,@5")
#pragma comment(linker,"/export:X3DAudioInitialize,@6")

HMODULE hRealXAudio2 = nullptr;

// Function pointer types
typedef HRESULT(WINAPI* XAudio2Create_t)(IXAudio2** ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor);
typedef HRESULT(WINAPI* CreateAudioVolumeMeter_t)(IUnknown** ppApo);
typedef HRESULT(WINAPI* CreateAudioReverb_t)(IUnknown** ppApo);
typedef HRESULT(WINAPI* CreateFX_t)(REFCLSID clsid, IUnknown** ppEffect, const void* pInitDat, UINT32 InitDataByteSize);
typedef void(WINAPI* X3DAudioCalculate_t)(const X3DAUDIO_HANDLE Instance, const X3DAUDIO_LISTENER* pListener, const X3DAUDIO_EMITTER* pEmitter, UINT32 Flags, X3DAUDIO_DSP_SETTINGS* pDSPSettings);
typedef HRESULT(WINAPI* X3DAudioInitialize_t)(UINT32 SpeakerChannelMask, FLOAT32 SpeedOfSound, X3DAUDIO_HANDLE Instance);

// Function pointers
XAudio2Create_t pXAudio2Create = nullptr;
CreateAudioVolumeMeter_t pCreateAudioVolumeMeter = nullptr;
CreateAudioReverb_t pCreateAudioReverb = nullptr;
CreateFX_t pCreateFX = nullptr;
X3DAudioCalculate_t pX3DAudioCalculate = nullptr;
X3DAudioInitialize_t pX3DAudioInitialize = nullptr;

BOOL LoadRealXAudio2()
{
    const char* absolutePath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Caravan Sandwitch Demo\\Engine\\Binaries\\ThirdParty\\Windows\\XAudio2_9\\x64\\xaudio2_9redist_orig.dll";
    hRealXAudio2 = LoadLibraryA(absolutePath);

    pXAudio2Create = (XAudio2Create_t)GetProcAddress(hRealXAudio2, "XAudio2Create");
    pCreateAudioVolumeMeter = (CreateAudioVolumeMeter_t)GetProcAddress(hRealXAudio2, "CreateAudioVolumeMeter");
    pCreateAudioReverb = (CreateAudioReverb_t)GetProcAddress(hRealXAudio2, "CreateAudioReverb");
    pCreateFX = (CreateFX_t)GetProcAddress(hRealXAudio2, "CreateFX");
    pX3DAudioCalculate = (X3DAudioCalculate_t)GetProcAddress(hRealXAudio2, "X3DAudioCalculate");
    pX3DAudioInitialize = (X3DAudioInitialize_t)GetProcAddress(hRealXAudio2, "X3DAudioInitialize");

    if (!pXAudio2Create) {
        printf("Failed to get XAudio2Create function\n");
        return FALSE;
    }

    return TRUE;
}

// Forwarding functions with logging
extern "C" HRESULT WINAPI XAudio2Create(IXAudio2** ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor)
{
    printf("XAudio2Create: ppXAudio2=%p, Flags=0x%X, Processor=0x%X\n", ppXAudio2, Flags, XAudio2Processor);
    HRESULT hr = pXAudio2Create(ppXAudio2, Flags, XAudio2Processor);
    if (SUCCEEDED(hr) && ppXAudio2 && *ppXAudio2)
        printf("  IXAudio2*=%p, VTable=%p\n", *ppXAudio2, *(void**)*ppXAudio2);
    return hr;
}

extern "C" HRESULT WINAPI CreateAudioVolumeMeter(IUnknown** ppApo)
{
    return pCreateAudioVolumeMeter ? pCreateAudioVolumeMeter(ppApo) : E_FAIL;
}

extern "C" HRESULT WINAPI CreateAudioReverb(IUnknown** ppApo)
{
    return pCreateAudioReverb ? pCreateAudioReverb(ppApo) : E_FAIL;
}

extern "C" HRESULT WINAPI CreateFX(REFCLSID clsid, IUnknown** ppEffect, const void* pInitDat, UINT32 InitDataByteSize)
{
    return pCreateFX ? pCreateFX(clsid, ppEffect, pInitDat, InitDataByteSize) : E_FAIL;
}

extern "C" __declspec(dllexport) void WINAPI X3DAudioCalculate(const X3DAUDIO_HANDLE Instance, const X3DAUDIO_LISTENER* pListener,
    const X3DAUDIO_EMITTER* pEmitter, UINT32 Flags, X3DAUDIO_DSP_SETTINGS* pDSPSettings)
{
    if (pX3DAudioCalculate)
        pX3DAudioCalculate(Instance, pListener, pEmitter, Flags, pDSPSettings);
}

extern "C" __declspec(dllexport) HRESULT WINAPI X3DAudioInitialize(UINT32 SpeakerChannelMask, FLOAT32 SpeedOfSound, X3DAUDIO_HANDLE Instance)
{
    return pX3DAudioInitialize ? pX3DAudioInitialize(SpeakerChannelMask, SpeedOfSound, Instance) : E_FAIL;
}

VOID WINAPI MainRoutine(HMODULE hModule)
{
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);

    printf("XAudio2 DLL Hijacking\n");
    printf("Real xaudio2_9redist.dll: %p\n", hRealXAudio2);
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if (!LoadRealXAudio2())
            return FALSE;
        CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(MainRoutine), hModule, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        if (hRealXAudio2)
            FreeLibrary(hRealXAudio2);
        break;
    }
    return TRUE;
}
