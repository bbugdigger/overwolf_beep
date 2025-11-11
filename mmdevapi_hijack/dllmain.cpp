#include "pch.h"

#include <Windows.h>
#include <mmdeviceapi.h>
#include <stdio.h>

// Export MMDevAPI functions with correct ordinals
#pragma comment(linker,"/export:ActivateAudioInterfaceAsync,@17")
#pragma comment(linker,"/export:DllCanUnloadNow,@35,PRIVATE")
#pragma comment(linker,"/export:DllGetClassObject,@36,PRIVATE")
#pragma comment(linker,"/export:DllRegisterServer,@37,PRIVATE")
#pragma comment(linker,"/export:DllUnregisterServer,@38,PRIVATE")

HMODULE hRealMMDevAPI = nullptr;

// Function pointer types
typedef HRESULT(WINAPI* ActivateAudioInterfaceAsync_t)(LPCWSTR deviceInterfacePath, REFIID riid, PROPVARIANT* activationParams, IActivateAudioInterfaceCompletionHandler* completionHandler, IActivateAudioInterfaceAsyncOperation** activationOperation);
typedef HRESULT(WINAPI* DllCanUnloadNow_t)(void);
typedef HRESULT(WINAPI* DllGetClassObject_t)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);
typedef HRESULT(WINAPI* DllRegisterServer_t)(void);
typedef HRESULT(WINAPI* DllUnregisterServer_t)(void);

// Function pointers
ActivateAudioInterfaceAsync_t pActivateAudioInterfaceAsync = nullptr;
DllCanUnloadNow_t pDllCanUnloadNow = nullptr;
DllGetClassObject_t pDllGetClassObject = nullptr;
DllRegisterServer_t pDllRegisterServer = nullptr;
DllUnregisterServer_t pDllUnregisterServer = nullptr;

BOOL LoadRealMMDevAPI()
{
    char systemPath[MAX_PATH] = "C:\\Windows\\System32\\MMDevAPI.dll";

    hRealMMDevAPI = LoadLibraryA(systemPath);

    if (!hRealMMDevAPI) {
        GetSystemDirectoryA(systemPath, MAX_PATH);
        strcat_s(systemPath, "\\MMDevAPI_orig.dll");
        hRealMMDevAPI = LoadLibraryA(systemPath);
    }

    if (!hRealMMDevAPI) {
        printf("Failed to load real MMDevAPI.dll\n");
        return FALSE;
    }

    pActivateAudioInterfaceAsync = (ActivateAudioInterfaceAsync_t)GetProcAddress(hRealMMDevAPI, "ActivateAudioInterfaceAsync");
    pDllCanUnloadNow = (DllCanUnloadNow_t)GetProcAddress(hRealMMDevAPI, "DllCanUnloadNow");
    pDllGetClassObject = (DllGetClassObject_t)GetProcAddress(hRealMMDevAPI, "DllGetClassObject");
    pDllRegisterServer = (DllRegisterServer_t)GetProcAddress(hRealMMDevAPI, "DllRegisterServer");
    pDllUnregisterServer = (DllUnregisterServer_t)GetProcAddress(hRealMMDevAPI, "DllUnregisterServer");

    return TRUE;
}

// Forwarding functions with logging
extern "C" HRESULT WINAPI ActivateAudioInterfaceAsync(LPCWSTR deviceInterfacePath, REFIID riid, PROPVARIANT* activationParams, IActivateAudioInterfaceCompletionHandler* completionHandler, IActivateAudioInterfaceAsyncOperation** activationOperation)
{
    printf("ActivateAudioInterfaceAsync: device=%ws\n", deviceInterfacePath ? deviceInterfacePath : L"NULL");
    if (!pActivateAudioInterfaceAsync) return E_FAIL;
    HRESULT hr = pActivateAudioInterfaceAsync(deviceInterfacePath, riid, activationParams, completionHandler, activationOperation);

    if (SUCCEEDED(hr) && activationOperation && *activationOperation)
        printf("  IActivateAudioInterfaceAsyncOperation*=%p\n", *activationOperation);

    return hr;
}

extern "C" HRESULT WINAPI DllCanUnloadNow()
{
    return pDllCanUnloadNow ? pDllCanUnloadNow() : S_FALSE;
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (!pDllGetClassObject) return E_FAIL;

    HRESULT hr = pDllGetClassObject(rclsid, riid, ppv);

    if (SUCCEEDED(hr) && ppv && *ppv) {
        printf("DllGetClassObject: object=%p\n", *ppv);
    }

    return hr;
}

extern "C" HRESULT WINAPI DllRegisterServer()
{
    return pDllRegisterServer ? pDllRegisterServer() : E_FAIL;
}

extern "C" HRESULT WINAPI DllUnregisterServer()
{
    return pDllUnregisterServer ? pDllUnregisterServer() : E_FAIL;
}

VOID WINAPI MainRoutine(HMODULE hModule)
{
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);

    printf("mmdevapi_hijack - MMDevAPI DLL Hijacking\n");
    printf("Real MMDevAPI.dll: %p\n", hRealMMDevAPI);
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if (!LoadRealMMDevAPI())
            return FALSE;
        CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(MainRoutine), hModule, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        if (hRealMMDevAPI)
            FreeLibrary(hRealMMDevAPI);
        break;
    }
    return TRUE;
}
