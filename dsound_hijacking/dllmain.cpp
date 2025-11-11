#include "pch.h"

#include <Windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdio.h>

// Export all DirectSound functions with correct ordinals
#pragma comment(linker,"/export:DirectSoundCreate,@1")
#pragma comment(linker,"/export:DirectSoundEnumerateA,@2")
#pragma comment(linker,"/export:DirectSoundEnumerateW,@3")
#pragma comment(linker,"/export:DllCanUnloadNow,@4,PRIVATE")
#pragma comment(linker,"/export:DllGetClassObject,@5,PRIVATE")
#pragma comment(linker,"/export:DirectSoundCaptureCreate,@6")
#pragma comment(linker,"/export:DirectSoundCaptureEnumerateA,@7")
#pragma comment(linker,"/export:DirectSoundCaptureEnumerateW,@8")
#pragma comment(linker,"/export:GetDeviceID,@9")
#pragma comment(linker,"/export:DirectSoundFullDuplexCreate,@10")
#pragma comment(linker,"/export:DirectSoundCreate8,@11")
#pragma comment(linker,"/export:DirectSoundCaptureCreate8,@12")

HMODULE hRealDSound = nullptr;

// Function pointer types
typedef HRESULT(WINAPI* DirectSoundCreate_t)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
typedef HRESULT(WINAPI* DirectSoundEnumerateA_t)(LPDSENUMCALLBACKA, LPVOID);
typedef HRESULT(WINAPI* DirectSoundEnumerateW_t)(LPDSENUMCALLBACKW, LPVOID);
typedef HRESULT(WINAPI* DllCanUnloadNow_t)(void);
typedef HRESULT(WINAPI* DllGetClassObject_t)(REFCLSID, REFIID, LPVOID*);
typedef HRESULT(WINAPI* DirectSoundCaptureCreate_t)(LPCGUID, LPDIRECTSOUNDCAPTURE*, LPUNKNOWN);
typedef HRESULT(WINAPI* DirectSoundCaptureEnumerateA_t)(LPDSENUMCALLBACKA, LPVOID);
typedef HRESULT(WINAPI* DirectSoundCaptureEnumerateW_t)(LPDSENUMCALLBACKW, LPVOID);
typedef HRESULT(WINAPI* GetDeviceID_t)(LPCGUID, LPGUID);
typedef HRESULT(WINAPI* DirectSoundFullDuplexCreate_t)(LPCGUID, LPCGUID, LPCDSCBUFFERDESC, LPCDSBUFFERDESC, HWND, DWORD, LPDIRECTSOUNDFULLDUPLEX*, LPDIRECTSOUNDCAPTUREBUFFER8*, LPDIRECTSOUNDBUFFER8*, LPUNKNOWN);
typedef HRESULT(WINAPI* DirectSoundCreate8_t)(LPCGUID, LPDIRECTSOUND8*, LPUNKNOWN);
typedef HRESULT(WINAPI* DirectSoundCaptureCreate8_t)(LPCGUID, LPDIRECTSOUNDCAPTURE8*, LPUNKNOWN);

// Function pointers
DirectSoundCreate_t pDirectSoundCreate = nullptr;
DirectSoundEnumerateA_t pDirectSoundEnumerateA = nullptr;
DirectSoundEnumerateW_t pDirectSoundEnumerateW = nullptr;
DllCanUnloadNow_t pDllCanUnloadNow = nullptr;
DllGetClassObject_t pDllGetClassObject = nullptr;
DirectSoundCaptureCreate_t pDirectSoundCaptureCreate = nullptr;
DirectSoundCaptureEnumerateA_t pDirectSoundCaptureEnumerateA = nullptr;
DirectSoundCaptureEnumerateW_t pDirectSoundCaptureEnumerateW = nullptr;
GetDeviceID_t pGetDeviceID = nullptr;
DirectSoundFullDuplexCreate_t pDirectSoundFullDuplexCreate = nullptr;
DirectSoundCreate8_t pDirectSoundCreate8 = nullptr;
DirectSoundCaptureCreate8_t pDirectSoundCaptureCreate8 = nullptr;

BOOL LoadRealDSound()
{
    char systemPath[MAX_PATH];
    GetSystemDirectoryA(systemPath, MAX_PATH);
    strcat_s(systemPath, "\\dsound.dll");

    hRealDSound = LoadLibraryA(systemPath);
    if (!hRealDSound) {
        printf("Failed to load real dsound.dll\n");
        return FALSE;
    }

    pDirectSoundCreate = (DirectSoundCreate_t)GetProcAddress(hRealDSound, "DirectSoundCreate");
    pDirectSoundEnumerateA = (DirectSoundEnumerateA_t)GetProcAddress(hRealDSound, "DirectSoundEnumerateA");
    pDirectSoundEnumerateW = (DirectSoundEnumerateW_t)GetProcAddress(hRealDSound, "DirectSoundEnumerateW");
    pDllCanUnloadNow = (DllCanUnloadNow_t)GetProcAddress(hRealDSound, "DllCanUnloadNow");
    pDllGetClassObject = (DllGetClassObject_t)GetProcAddress(hRealDSound, "DllGetClassObject");
    pDirectSoundCaptureCreate = (DirectSoundCaptureCreate_t)GetProcAddress(hRealDSound, "DirectSoundCaptureCreate");
    pDirectSoundCaptureEnumerateA = (DirectSoundCaptureEnumerateA_t)GetProcAddress(hRealDSound, "DirectSoundCaptureEnumerateA");
    pDirectSoundCaptureEnumerateW = (DirectSoundCaptureEnumerateW_t)GetProcAddress(hRealDSound, "DirectSoundCaptureEnumerateW");
    pGetDeviceID = (GetDeviceID_t)GetProcAddress(hRealDSound, "GetDeviceID");
    pDirectSoundFullDuplexCreate = (DirectSoundFullDuplexCreate_t)GetProcAddress(hRealDSound, "DirectSoundFullDuplexCreate");
    pDirectSoundCreate8 = (DirectSoundCreate8_t)GetProcAddress(hRealDSound, "DirectSoundCreate8");
    pDirectSoundCaptureCreate8 = (DirectSoundCaptureCreate8_t)GetProcAddress(hRealDSound, "DirectSoundCaptureCreate8");

    return TRUE;
}

// Forwarding functions with logging
extern "C" HRESULT WINAPI DirectSoundCreate(LPCGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
{
    printf("DirectSoundCreate: lpGuid=%p, ppDS=%p\n", lpGuid, ppDS);
    HRESULT hr = pDirectSoundCreate(lpGuid, ppDS, pUnkOuter);
    if (SUCCEEDED(hr) && ppDS && *ppDS)
        printf("  IDirectSound*=%p, VTable=%p\n", *ppDS, *(void**)*ppDS);
    return hr;
}

extern "C" HRESULT WINAPI DirectSoundCreate8(LPCGUID lpGuid, LPDIRECTSOUND8* ppDS8, LPUNKNOWN pUnkOuter)
{
    printf("DirectSoundCreate8: lpGuid=%p, ppDS8=%p\n", lpGuid, ppDS8);
    HRESULT hr = pDirectSoundCreate8(lpGuid, ppDS8, pUnkOuter);
    if (SUCCEEDED(hr) && ppDS8 && *ppDS8)
        printf("  IDirectSound8*=%p, VTable=%p\n", *ppDS8, *(void**)*ppDS8);
    return hr;
}

extern "C" HRESULT WINAPI DirectSoundEnumerateA(LPDSENUMCALLBACKA lpDSEnumCallback, LPVOID lpContext)
{
    return pDirectSoundEnumerateA ? pDirectSoundEnumerateA(lpDSEnumCallback, lpContext) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundEnumerateW(LPDSENUMCALLBACKW lpDSEnumCallback, LPVOID lpContext)
{
    return pDirectSoundEnumerateW ? pDirectSoundEnumerateW(lpDSEnumCallback, lpContext) : E_FAIL;
}

extern "C" HRESULT WINAPI DllCanUnloadNow()
{
    return pDllCanUnloadNow ? pDllCanUnloadNow() : E_FAIL;
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    return pDllGetClassObject ? pDllGetClassObject(rclsid, riid, ppv) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundCaptureCreate(LPCGUID lpGuid, LPDIRECTSOUNDCAPTURE* ppDSC, LPUNKNOWN pUnkOuter)
{
    return pDirectSoundCaptureCreate ? pDirectSoundCaptureCreate(lpGuid, ppDSC, pUnkOuter) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundCaptureEnumerateA(LPDSENUMCALLBACKA lpDSEnumCallback, LPVOID lpContext)
{
    return pDirectSoundCaptureEnumerateA ? pDirectSoundCaptureEnumerateA(lpDSEnumCallback, lpContext) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundCaptureEnumerateW(LPDSENUMCALLBACKW lpDSEnumCallback, LPVOID lpContext)
{
    return pDirectSoundCaptureEnumerateW ? pDirectSoundCaptureEnumerateW(lpDSEnumCallback, lpContext) : E_FAIL;
}

extern "C" HRESULT WINAPI GetDeviceID(LPCGUID lpGuidSrc, LPGUID lpGuidDest)
{
    return pGetDeviceID ? pGetDeviceID(lpGuidSrc, lpGuidDest) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundFullDuplexCreate(LPCGUID pcGuidCaptureDevice, LPCGUID pcGuidRenderDevice,
    LPCDSCBUFFERDESC pcDSCBufferDesc, LPCDSBUFFERDESC pcDSBufferDesc, HWND hWnd,
    DWORD dwLevel, LPDIRECTSOUNDFULLDUPLEX* ppDSFD, LPDIRECTSOUNDCAPTUREBUFFER8* ppDSCBuffer8,
    LPDIRECTSOUNDBUFFER8* ppDSBuffer8, LPUNKNOWN pUnkOuter)
{
    return pDirectSoundFullDuplexCreate ? pDirectSoundFullDuplexCreate(pcGuidCaptureDevice, pcGuidRenderDevice, pcDSCBufferDesc,
        pcDSBufferDesc, hWnd, dwLevel, ppDSFD, ppDSCBuffer8, ppDSBuffer8, pUnkOuter) : E_FAIL;
}

extern "C" HRESULT WINAPI DirectSoundCaptureCreate8(LPCGUID lpGuid, LPDIRECTSOUNDCAPTURE8* ppDSC8, LPUNKNOWN pUnkOuter)
{
    return pDirectSoundCaptureCreate8 ? pDirectSoundCaptureCreate8(lpGuid, ppDSC8, pUnkOuter) : E_FAIL;
}

VOID WINAPI MainRoutine(HMODULE hModule)
{
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);

    printf("DirectSound DLL Hijacking\n");
    printf("Real dsound.dll: %p\n", hRealDSound);
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if (!LoadRealDSound())
            return FALSE;
        CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(MainRoutine), hModule, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        if (hRealDSound)
            FreeLibrary(hRealDSound);
        break;
    }
    return TRUE;
}
