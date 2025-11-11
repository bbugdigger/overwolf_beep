#include "pch.h"

#include <Windows.h>
#include <stdio.h>
#include "MinHook.h"

#pragma comment(lib, "libMinHook.x64.lib")

// Original function pointer
typedef HRESULT(WINAPI* CoCreateInstance_t)(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv);
CoCreateInstance_t pOriginalCoCreateInstance = nullptr;

// Hooked CoCreateInstance
HRESULT WINAPI HookedCoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv)
{
    printf("CoCreateInstance called:\n");
    printf("  CLSID: {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
        rclsid.Data1, rclsid.Data2, rclsid.Data3,
        rclsid.Data4[0], rclsid.Data4[1], rclsid.Data4[2], rclsid.Data4[3],
        rclsid.Data4[4], rclsid.Data4[5], rclsid.Data4[6], rclsid.Data4[7]);

    HRESULT hr = pOriginalCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

    if (SUCCEEDED(hr) && ppv && *ppv) {
        printf("  Result: SUCCESS\n");
        printf("  Object: %p\n", *ppv);
        printf("  VTable: %p\n", *(void**)*ppv);
    }
    else {
        printf("  Result: FAILED (0x%08lX)\n", hr);
    }
    printf("\n");

    return hr;
}

BOOL InitializeHooks()
{
    if (MH_Initialize() != MH_OK) {
        printf("MH_Initialize failed\n");
        return FALSE;
    }

    if (MH_CreateHookApi(L"ole32.dll", "CoCreateInstance", (LPVOID)HookedCoCreateInstance, (LPVOID*)&pOriginalCoCreateInstance) != MH_OK) {
        printf("MH_CreateHookApi failed\n");
        return FALSE;
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        printf("MH_EnableHook failed\n");
        return FALSE;
    }

    printf("CoCreateInstance hook enabled\n\n");
    return TRUE;
}

DWORD WINAPI MainRoutine(LPVOID lpParam)
{
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);

    printf("Hooking CoCreateInstance...\n");
    printf("Monitoring COM object creation...\n\n");

    if (!InitializeHooks()) {
        printf("Failed to initialize hooks\n");
        return 1;
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainRoutine, hModule, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}
