#include "pch.h"

#include <Windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdio.h>
#include "MinHook.h"

#pragma comment (lib, "libMinHook.x64.lib")
#pragma comment(lib, "dsound.lib")

// Function pointer typedefs
typedef HRESULT(WINAPI* fnDirectSoundCreate)(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
typedef HRESULT(WINAPI* fnDirectSoundCreate8)(LPCGUID pcGuidDevice, LPDIRECTSOUND8* ppDS8, LPUNKNOWN pUnkOuter);

// Original function pointers
fnDirectSoundCreate g_pOriginalDirectSoundCreate = NULL;
fnDirectSoundCreate8 g_pOriginalDirectSoundCreate8 = NULL;

// Console for logging
FILE* g_pConsole = NULL;

// Helper function to create console
void CreateDebugConsole() {
    AllocConsole();
    freopen_s(&g_pConsole, "CONOUT$", "w", stdout);
    freopen_s(&g_pConsole, "CONOUT$", "w", stderr);
    printf("Console initialized\n");
}

// Hooked DirectSoundCreate
HRESULT WINAPI HookedDirectSoundCreate(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter) {
    printf("\n[HOOK HIT] DirectSoundCreate called!\n");
    printf("    - pcGuidDevice: %p\n", pcGuidDevice);
    printf("    - ppDS: %p\n", ppDS);
    printf("    - pUnkOuter: %p\n", pUnkOuter);

    HRESULT hr = g_pOriginalDirectSoundCreate(pcGuidDevice, ppDS, pUnkOuter);

    if (SUCCEEDED(hr)) {
        printf("[+] DirectSoundCreate succeeded! IDirectSound instance: %p\n", ppDS ? *ppDS : NULL);
    }
    else {
        printf("[-] DirectSoundCreate failed with HRESULT: 0x%08X\n", hr);
    }

    return hr;
}

// Hooked DirectSoundCreate8
HRESULT WINAPI HookedDirectSoundCreate8(LPCGUID pcGuidDevice, LPDIRECTSOUND8* ppDS8, LPUNKNOWN pUnkOuter) {
    printf("\n[HOOK HIT] DirectSoundCreate8 called!\n");
    printf("    - pcGuidDevice: %p\n", pcGuidDevice);
    printf("    - ppDS8: %p\n", ppDS8);
    printf("    - pUnkOuter: %p\n", pUnkOuter);

    HRESULT hr = g_pOriginalDirectSoundCreate8(pcGuidDevice, ppDS8, pUnkOuter);

    if (SUCCEEDED(hr)) {
        printf("[+] DirectSoundCreate8 succeeded! IDirectSound8 instance: %p\n", ppDS8 ? *ppDS8 : NULL);
        // TODO: Hook the vtable of the created IDirectSound8 interface here
    }
    else {
        printf("[-] DirectSoundCreate8 failed with HRESULT: 0x%08X\n", hr);
    }

    return hr;
}

// Install all hooks
BOOL InstallHooks() {
    DWORD dwMinHookErr = NULL;
    HMODULE hDSound = NULL;

    printf("[i] Initializing MinHook...\n");
    if ((dwMinHookErr = MH_Initialize()) != MH_OK) {
        printf("[!] MH_Initialize failed with error: %d\n", dwMinHookErr);
        return FALSE;
    }
    printf("[+] MinHook initialized successfully\n");

    // Get dsound.dll module handle
    printf("[i] Getting dsound.dll module handle...\n");
    hDSound = GetModuleHandleA("dsound.dll");
    if (!hDSound) {
        printf("[!] dsound.dll not loaded yet, attempting to load it...\n");
        hDSound = LoadLibraryA("dsound.dll");
        if (!hDSound) {
            printf("[!] Failed to load dsound.dll\n");
            return FALSE;
        }
    }
    printf("[+] dsound.dll handle: %p\n", hDSound);

    // Get DirectSoundCreate address
    printf("[i] Getting DirectSoundCreate address...\n");
    FARPROC pDirectSoundCreate = GetProcAddress(hDSound, "DirectSoundCreate");
    if (pDirectSoundCreate) {
        printf("[+] DirectSoundCreate address: %p\n", pDirectSoundCreate);

        if ((dwMinHookErr = MH_CreateHook(pDirectSoundCreate, &HookedDirectSoundCreate, (LPVOID*)&g_pOriginalDirectSoundCreate)) != MH_OK) {
            printf("[!] MH_CreateHook(DirectSoundCreate) failed with error: %d\n", dwMinHookErr);
        }
        else {
            printf("[+] Hook created for DirectSoundCreate\n");

            if ((dwMinHookErr = MH_EnableHook(pDirectSoundCreate)) != MH_OK) {
                printf("[!] MH_EnableHook(DirectSoundCreate) failed with error: %d\n", dwMinHookErr);
            }
            else {
                printf("[+] DirectSoundCreate hook enabled!\n");
            }
        }
    }
    else {
        printf("[-] DirectSoundCreate not found in dsound.dll\n");
    }

    // Get DirectSoundCreate8 address
    printf("[i] Getting DirectSoundCreate8 address...\n");
    FARPROC pDirectSoundCreate8 = GetProcAddress(hDSound, "DirectSoundCreate8");
    if (pDirectSoundCreate8) {
        printf("[+] DirectSoundCreate8 address: %p\n", pDirectSoundCreate8);

        if ((dwMinHookErr = MH_CreateHook(pDirectSoundCreate8, &HookedDirectSoundCreate8, (LPVOID*)&g_pOriginalDirectSoundCreate8)) != MH_OK) {
            printf("[!] MH_CreateHook(DirectSoundCreate8) failed with error: %d\n", dwMinHookErr);
        }
        else {
            printf("[+] Hook created for DirectSoundCreate8\n");

            if ((dwMinHookErr = MH_EnableHook(pDirectSoundCreate8)) != MH_OK) {
                printf("[!] MH_EnableHook(DirectSoundCreate8) failed with error: %d\n", dwMinHookErr);
            }
            else {
                printf("[+] DirectSoundCreate8 hook enabled!\n");
            }
        }
    }
    else {
        printf("[-] DirectSoundCreate8 not found in dsound.dll\n");
    }

    printf("[+] All hooks installed successfully!\n");
    return TRUE;
}

// Remove all hooks
BOOL RemoveHooks() {
    DWORD dwMinHookErr = NULL;

    printf("[i] Disabling hooks...\n");
    if ((dwMinHookErr = MH_DisableHook(MH_ALL_HOOKS)) != MH_OK) {
        printf("[!] MH_DisableHook failed with error: %d\n", dwMinHookErr);
        return FALSE;
    }

    printf("[i] Uninitializing MinHook...\n");
    if ((dwMinHookErr = MH_Uninitialize()) != MH_OK) {
        printf("[!] MH_Uninitialize failed with error: %d\n", dwMinHookErr);
        return FALSE;
    }

    printf("[+] All hooks removed successfully!\n");
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);

        CreateDebugConsole();
        printf("beep5 - DirectSound MinHook injection\n");
        printf("DLL Base: %p, PID: %d\n", hModule, GetCurrentProcessId());

        if (!InstallHooks()) {
            printf("Failed to install hooks\n");
            return FALSE;
        }
        break;
    case DLL_PROCESS_DETACH:
        printf("\n[i] DLL unloading...\n");
        RemoveHooks();

        if (g_pConsole) {
            fclose(g_pConsole);
        }
        FreeConsole();
        break;
    }
    return TRUE;
}
