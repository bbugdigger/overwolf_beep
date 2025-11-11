#include "pch.h"

#include <Windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include "MinHook.h"

#pragma comment(lib, "libMinHook.x64.lib")
#pragma comment(lib, "winmm.lib")

// Original function pointers
typedef BOOL(WINAPI* PlaySoundA_t)(LPCSTR pszSound, HMODULE hmod, DWORD fdwSound);
typedef BOOL(WINAPI* PlaySoundW_t)(LPCWSTR pszSound, HMODULE hmod, DWORD fdwSound);
typedef BOOL(WINAPI* sndPlaySoundA_t)(LPCSTR pszSound, UINT fuSound);
typedef BOOL(WINAPI* sndPlaySoundW_t)(LPCWSTR pszSound, UINT fuSound);
typedef MMRESULT(WINAPI* waveOutOpen_t)(LPHWAVEOUT phwo, UINT uDeviceID, LPCWAVEFORMATEX pwfx, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen);
typedef MMRESULT(WINAPI* waveOutWrite_t)(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh);

PlaySoundA_t pOriginalPlaySoundA = nullptr;
PlaySoundW_t pOriginalPlaySoundW = nullptr;
sndPlaySoundA_t pOriginalSndPlaySoundA = nullptr;
sndPlaySoundW_t pOriginalSndPlaySoundW = nullptr;
waveOutOpen_t pOriginalWaveOutOpen = nullptr;
waveOutWrite_t pOriginalWaveOutWrite = nullptr;

// Hooked Functions
BOOL WINAPI HookedPlaySoundA(LPCSTR pszSound, HMODULE hmod, DWORD fdwSound)
{
    printf("PlaySoundA: pszSound=%s, flags=0x%X\n", pszSound ? pszSound : "NULL", fdwSound);
    return pOriginalPlaySoundA(pszSound, hmod, fdwSound);
}

BOOL WINAPI HookedPlaySoundW(LPCWSTR pszSound, HMODULE hmod, DWORD fdwSound)
{
    printf("PlaySoundW: pszSound=%ws, flags=0x%X\n", pszSound ? pszSound : L"NULL", fdwSound);
    return pOriginalPlaySoundW(pszSound, hmod, fdwSound);
}

BOOL WINAPI HookedSndPlaySoundA(LPCSTR pszSound, UINT fuSound)
{
    printf("sndPlaySoundA: pszSound=%s\n", pszSound ? pszSound : "NULL");
    return pOriginalSndPlaySoundA(pszSound, fuSound);
}

BOOL WINAPI HookedSndPlaySoundW(LPCWSTR pszSound, UINT fuSound)
{
    printf("sndPlaySoundW: pszSound=%ws\n", pszSound ? pszSound : L"NULL");
    return pOriginalSndPlaySoundW(pszSound, fuSound);
}

MMRESULT WINAPI HookedWaveOutOpen(LPHWAVEOUT phwo, UINT uDeviceID, LPCWAVEFORMATEX pwfx,
    DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen)
{
    if (pwfx)
        printf("waveOutOpen: channels=%u, samplerate=%u, bits=%u\n",
            pwfx->nChannels, pwfx->nSamplesPerSec, pwfx->wBitsPerSample);
    return pOriginalWaveOutOpen(phwo, uDeviceID, pwfx, dwCallback, dwInstance, fdwOpen);
}

MMRESULT WINAPI HookedWaveOutWrite(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh)
{
    if (pwh)
        printf("waveOutWrite: buffer=%u bytes\n", pwh->dwBufferLength);
    return pOriginalWaveOutWrite(hwo, pwh, cbwh);
}

BOOL InitializeHooks()
{
    if (MH_Initialize() != MH_OK) {
        printf("MH_Initialize failed\n");
        return FALSE;
    }

    
    if (MH_CreateHookApi(L"winmm.dll", "PlaySoundA", (LPVOID)HookedPlaySoundA, (LPVOID*)&pOriginalPlaySoundA) == MH_OK)
        return FALSE;
    if (MH_CreateHookApi(L"winmm.dll", "PlaySoundW", (LPVOID)HookedPlaySoundW, (LPVOID*)&pOriginalPlaySoundW) == MH_OK)
        return FALSE;
    if (MH_CreateHookApi(L"winmm.dll", "sndPlaySoundA", (LPVOID)HookedSndPlaySoundA, (LPVOID*)&pOriginalSndPlaySoundA) == MH_OK)
        return FALSE;
    if (MH_CreateHookApi(L"winmm.dll", "sndPlaySoundW", (LPVOID)HookedSndPlaySoundW, (LPVOID*)&pOriginalSndPlaySoundW) == MH_OK)
        return FALSE;
    if (MH_CreateHookApi(L"winmm.dll", "waveOutOpen", (LPVOID)HookedWaveOutOpen, (LPVOID*)&pOriginalWaveOutOpen) == MH_OK)
        return FALSE;
    if (MH_CreateHookApi(L"winmm.dll", "waveOutWrite", (LPVOID)HookedWaveOutWrite, (LPVOID*)&pOriginalWaveOutWrite) == MH_OK)
        return FALSE;

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        printf("MH_EnableHook failed\n");
        return FALSE;
    }

    printf("Hooks enabled...\n");
    return TRUE;
}

DWORD WINAPI MainRoutine(LPVOID lpParam)
{
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);

    printf("WinMM API Hooking\n");

    if (InitializeHooks())
        printf("Ready\n");
    else
        printf("Hook initialization failed\n");

    return 0;
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
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
