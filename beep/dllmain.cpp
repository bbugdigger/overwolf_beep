#include "pch.h"

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys.h>
#include <stdio.h>

#include "MinHook.h"
#pragma comment(lib, "libMinHook.x64.lib")

// --- [ Diagnostics Helpers ] ---

void PrintHex(const void* data, size_t len) {
    auto bytes = (const unsigned char*)data;
    for (size_t i = 0; i < len; ++i) printf("%02X", bytes[i]);
}

void PrintPtr(const char* label, void* ptr) {
    printf("  %s: %p\n", label, ptr);
    fflush(stdout);
}

// --- [ Known GUID Database ] ---

struct KnownGUIDEntry {
    GUID guid;
    const char* description;
    const char* next_hook_suggestion;
};

const KnownGUIDEntry known_guids[] = {
    // MMDevAPI/WASAPI
    { {0xBCDE0395,0xE52F,0x467C,{0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}}, "CLSID_MMDeviceEnumerator (WASAPI)", nullptr },
    { {0xA95664D2,0x9614,0x4F35,{0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}}, "IID_IMMDeviceEnumerator (WASAPI)", nullptr },
    { {0x1CB9AD4C,0xDBFA,0x4C32,{0xB1,0x78,0xC2,0xF5,0x68,0xA7,0x03,0xB2}}, "IID_IAudioClient (WASAPI)", "-> WILL HOOK IAudioClient vtable!" },
    { {0xF294ACFC,0x3146,0x4483,{0xA7,0xBF,0xAD,0xDC,0xA7,0xC2,0x60,0xE2}}, "IID_IAudioRenderClient (WASAPI)", nullptr },
    { {0xC8ADBD64,0xE71E,0x48A0,{0xA4,0xDE,0x18,0x5C,0x39,0x5C,0xD3,0x17}}, "IID_IAudioCaptureClient (WASAPI)", nullptr },
    { {0xD666063F,0x1587,0x4E43,{0x81,0xF1,0xB9,0x48,0xE8,0x07,0x36,0x3F}}, "IID_IMMDevice", nullptr },
    // DirectSound
    { {0x279AFA83,0x4981,0x11CE,{0xA5,0x21,0x00,0x20,0xAF,0x0B,0xE5,0x60}}, "IID_IDirectSound", "*** DIRECTSOUND - POSSIBLE BEEP! ***" },
    { {0xC50A7E93,0xF395,0x4834,{0x9E,0xF6,0x7F,0xA9,0x9D,0xE5,0x09,0x66}}, "IID_IDirectSound8", "*** DIRECTSOUND8 - POSSIBLE BEEP! ***" },
    { {0xB0210780,0x89CD,0x11D0,{0xAF,0x08,0x00,0xA0,0xC9,0x25,0xCD,0x16}}, "IID_IDirectSoundCapture", "*** DIRECTSOUND CAPTURE ***" },
    { {0x00990DF4,0x0DBB,0x4872,{0x83,0x3E,0x6D,0x30,0x3E,0x80,0xAE,0xB6}}, "IID_IDirectSoundCapture8", "*** DIRECTSOUND CAPTURE8 ***" },
    // XAudio2
    { {0x5A508685,0xA254,0x4FBA,{0x9B,0x82,0x9A,0x24,0xB0,0x03,0x06,0xAF}}, "CLSID_XAudio2", "*** XAUDIO2 - VERY LIKELY THE BEEP! ***" },
    { {0x8BCF1F58,0x9FE7,0x4583,{0x8A,0xC6,0xE2,0xAD,0xC4,0x65,0xC8,0xBB}}, "IID_IXAudio2", "*** XAUDIO2 INTERFACE ***" },
    { {0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, nullptr, nullptr }
};

const KnownGUIDEntry* FindKnownGUID(const GUID& guid) {
    for (int i = 0; known_guids[i].description; ++i) {
        if (memcmp(&guid, &known_guids[i].guid, sizeof(GUID)) == 0)
            return &known_guids[i];
    }
    return nullptr;
}

void PrintKnownGUID(const GUID& guid, const char* name) {
    const KnownGUIDEntry* info = FindKnownGUID(guid);
    if (info) {
        printf("  %s: {%08lX-%04hX-%04hX-", name, guid.Data1, guid.Data2, guid.Data3);
        PrintHex(&guid.Data4[0], sizeof(guid.Data4));
        printf("} *** %s ***\n", info->description);
        if (info->next_hook_suggestion)
            printf("        %s\n", info->next_hook_suggestion);
    }
    else {
        printf("  %s: {%08lX-%04hX-%04hX-", name, guid.Data1, guid.Data2, guid.Data3);
        PrintHex(&guid.Data4[0], sizeof(guid.Data4));
        printf("}\n");
    }
}

// --- [ Forward Declarations ] ---
void PatchIMMDeviceEnumerator(void* pObject);
void PatchIMMDevice(void* pIMMDevice);
void PatchIAudioClient(void* pIAudioClient);
void PatchIAudioRenderClient(void* pIAudioRenderClient);

// --- [ PeekMessageW Hook for Space Bar Detection ] ---
typedef BOOL(WINAPI* PeekMessageW_t)(LPMSG, HWND, UINT, UINT, UINT);
PeekMessageW_t pOriginalPeekMessageW = nullptr;

BOOL WINAPI HookedPeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
    BOOL result = pOriginalPeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);

    if (result && lpMsg) {
        if (lpMsg->message == WM_KEYDOWN && lpMsg->wParam == VK_SPACE) {
            printf("\n");
            printf("================================================================================\n");
            printf(">>> SPACEBAR PRESSED - BEEP SHOULD TRIGGER NOW! <<<\n");
            printf("================================================================================\n");
            fflush(stdout);
        }
    }

    return result;
}

// --- [ CoCreateInstance Hook ] ---
typedef HRESULT(WINAPI* CoCreateInstance_t)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
CoCreateInstance_t pOriginalCoCreateInstance = nullptr;

// --- [ IAudioRenderClient vtable hooks ] ---
typedef HRESULT(STDMETHODCALLTYPE* FnGetBuffer)(void*, UINT32, BYTE**);
typedef HRESULT(STDMETHODCALLTYPE* FnReleaseBuffer)(void*, UINT32, DWORD);

// Store original function pointers PER VTABLE (there can be multiple IAudioRenderClient vtables!)
static std::unordered_map<void*, FnGetBuffer> g_OriginalIAudioRenderClient_GetBuffer;
static std::unordered_map<void*, FnReleaseBuffer> g_OriginalIAudioRenderClient_ReleaseBuffer;
static std::unordered_set<void*> g_PatchedIAudioRenderClientVTables;

// Store the buffer pointer from GetBuffer so we can analyze it in ReleaseBuffer
static std::unordered_map<void*, BYTE*> g_PendingBuffers;

HRESULT STDMETHODCALLTYPE My_IAudioRenderClient_GetBuffer(void* This, UINT32 NumFramesRequested, BYTE** ppData)
{
    void** vtable = *(void***)This;
    auto it = g_OriginalIAudioRenderClient_GetBuffer.find(vtable);
    if (it == g_OriginalIAudioRenderClient_GetBuffer.end()) {
        printf("[!] ERROR: No original GetBuffer found for vtable %p\n", vtable);
        fflush(stdout);
        return E_UNEXPECTED;
    }

    FnGetBuffer originalGetBuffer = it->second;
    HRESULT hr = originalGetBuffer(This, NumFramesRequested, ppData);
    return hr;
}

// Helper function to analyze audio buffer content (detect non-silence)
// Returns the peak amplitude (0.0 = silence, 1.0 = max)
float AnalyzeAudioBuffer(const BYTE* pData, UINT32 NumFrames) {
    if (!pData || NumFrames == 0) return 0.0f;

    // Assume 32-bit float stereo (2 channels, 4 bytes per sample)
    // Each frame = 2 samples (L+R) = 8 bytes
    const float* samples = (const float*)pData;
    UINT32 totalSamples = NumFrames * 2; // stereo

    float peakAmplitude = 0.0f;
    for (UINT32 i = 0; i < totalSamples; i++) {
        float absSample = fabsf(samples[i]);
        if (absSample > peakAmplitude) {
            peakAmplitude = absSample;
        }
    }

    return peakAmplitude;
}

HRESULT STDMETHODCALLTYPE My_IAudioRenderClient_ReleaseBuffer(void* This, UINT32 NumFramesWritten, DWORD dwFlags)
{
    //This logging is to noisy. To often is ReleaseBuffer called
    //printf("[*] IAudioRenderClient::ReleaseBuffer called\n");
    //PrintPtr("this", This);
    //printf("  NumFramesWritten: %u, Flags: 0x%08lX\n", NumFramesWritten, dwFlags);

    void** vtable = *(void***)This;
    auto it = g_OriginalIAudioRenderClient_ReleaseBuffer.find(vtable);
    if (it == g_OriginalIAudioRenderClient_ReleaseBuffer.end()) {
        printf("[!] ERROR: No original ReleaseBuffer found for vtable %p\n", vtable);
        fflush(stdout);
        return E_UNEXPECTED;
    }

    FnReleaseBuffer originalReleaseBuffer = it->second;
    HRESULT hr = originalReleaseBuffer(This, NumFramesWritten, dwFlags);
    return hr;
}

void PatchIAudioRenderClient(void* pObj)
{
    if (!pObj) return;
    void** vtable = *(void***)pObj;

    // Check if we've already patched this vtable
    if (g_PatchedIAudioRenderClientVTables.find(vtable) != g_PatchedIAudioRenderClientVTables.end()) {
        printf("[*] IAudioRenderClient vtable already patched (vtable: %p), skipping\n", vtable);
        fflush(stdout);
        return;
    }

    DWORD oldProtect = 0;
    if (VirtualProtect(vtable, sizeof(void*) * 10, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        // Store the original function pointers for THIS specific vtable
        g_OriginalIAudioRenderClient_GetBuffer[vtable] = (FnGetBuffer)vtable[3];
        vtable[3] = (void*)&My_IAudioRenderClient_GetBuffer;

        g_OriginalIAudioRenderClient_ReleaseBuffer[vtable] = (FnReleaseBuffer)vtable[4];
        vtable[4] = (void*)&My_IAudioRenderClient_ReleaseBuffer;

        VirtualProtect(vtable, sizeof(void*) * 10, oldProtect, &oldProtect);
        g_PatchedIAudioRenderClientVTables.insert(vtable);
        printf("[+] IAudioRenderClient vtable patched (vtable: %p)\n", vtable);
    }
    else {
        printf("[!] Failed to patch IAudioRenderClient vtable\n");
    }
    fflush(stdout);
}

// --- [ IAudioClient vtable hooks ] ---
typedef HRESULT(STDMETHODCALLTYPE* FnInitialize)(void*, DWORD, DWORD, REFERENCE_TIME, REFERENCE_TIME, const WAVEFORMATEX*, LPCGUID);
typedef HRESULT(STDMETHODCALLTYPE* FnGetService)(void*, REFIID, void**);
typedef HRESULT(STDMETHODCALLTYPE* FnStart)(void*);
typedef HRESULT(STDMETHODCALLTYPE* FnStop)(void*);

// Store original function pointers PER VTABLE (there can be multiple IAudioClient vtables!)
static std::unordered_map<void*, FnInitialize> g_OriginalIAudioClient_Initialize;
static std::unordered_map<void*, FnGetService> g_OriginalIAudioClient_GetService;
static std::unordered_map<void*, FnStart> g_OriginalIAudioClient_Start;
static std::unordered_map<void*, FnStop> g_OriginalIAudioClient_Stop;
static std::unordered_set<void*> g_PatchedIAudioClientVTables;

HRESULT STDMETHODCALLTYPE My_IAudioClient_Initialize(
    void* This,
    DWORD ShareMode,
    DWORD StreamFlags,
    REFERENCE_TIME hnsBufferDuration,
    REFERENCE_TIME hnsPeriodicity,
    const WAVEFORMATEX* pFormat,
    LPCGUID AudioSessionGuid
) {
    printf("\n");
    printf(">>> IAudioClient::Initialize - NEW AUDIO STREAM BEING CREATED! <<<\n");
    PrintPtr("this", This);
    printf("  ShareMode: %lu, StreamFlags: 0x%08lX\n", ShareMode, StreamFlags);
    printf("  BufferDuration: %lld, Periodicity: %lld\n", hnsBufferDuration, hnsPeriodicity);
    if (pFormat) {
        printf("  WAVEFORMATEX:\n");
        printf("    wFormatTag: 0x%04X\n", pFormat->wFormatTag);
        printf("    nChannels: %u\n", pFormat->nChannels);
        printf("    nSamplesPerSec: %lu\n", pFormat->nSamplesPerSec);
        printf("    wBitsPerSample: %u\n", pFormat->wBitsPerSample);
    }

    void** vtable = *(void***)This;
    auto it = g_OriginalIAudioClient_Initialize.find(vtable);
    if (it == g_OriginalIAudioClient_Initialize.end()) {
        printf("[!] ERROR: No original Initialize found for vtable %p\n", vtable);
        fflush(stdout);
        return E_UNEXPECTED;
    }

    FnInitialize originalInitialize = it->second;
    HRESULT hr = originalInitialize(This, ShareMode, StreamFlags, hnsBufferDuration, hnsPeriodicity, pFormat, AudioSessionGuid);
    printf("  HRESULT: 0x%08lX\n", hr);
    printf("\n");
    fflush(stdout);
    return hr;
}

HRESULT STDMETHODCALLTYPE My_IAudioClient_GetService(void* This, REFIID riid, void** ppv)
{
    printf("[*] IAudioClient::GetService called\n");
    PrintPtr("this", This);
    PrintKnownGUID(riid, "Service IID");

    void** vtable = *(void***)This;
    auto it = g_OriginalIAudioClient_GetService.find(vtable);
    if (it == g_OriginalIAudioClient_GetService.end()) {
        printf("[!] ERROR: No original GetService found for vtable %p\n", vtable);
        fflush(stdout);
        return E_UNEXPECTED;
    }

    FnGetService originalGetService = it->second;
    HRESULT hr = originalGetService(This, riid, ppv);
    printf("  HRESULT: 0x%08lX\n", hr);
    if (SUCCEEDED(hr) && ppv && *ppv) {
        PrintPtr("Service Interface", *ppv);
        // Check if it's IAudioRenderClient
        if (riid.Data1 == 0xF294ACFC && riid.Data2 == 0x3146 && riid.Data3 == 0x4483) {
            PatchIAudioRenderClient(*ppv);
            printf("  >>> IAudioRenderClient hooked (will only log near spacebar press)\n");
        }
    }
    fflush(stdout);
    return hr;
}

HRESULT STDMETHODCALLTYPE My_IAudioClient_Start(void* This)
{
    printf("\n");
    printf(">>> IAudioClient::Start - AUDIO STREAM STARTING! <<<\n");
    PrintPtr("this", This);

    void** vtable = *(void***)This;
    auto it = g_OriginalIAudioClient_Start.find(vtable);
    if (it == g_OriginalIAudioClient_Start.end()) {
        printf("[!] ERROR: No original Start found for vtable %p\n", vtable);
        fflush(stdout);
        return E_UNEXPECTED;
    }

    FnStart originalStart = it->second;
    HRESULT hr = originalStart(This);
    printf("  HRESULT: 0x%08lX\n", hr);
    printf("\n");
    fflush(stdout);
    return hr;
}

HRESULT STDMETHODCALLTYPE My_IAudioClient_Stop(void* This)
{
    printf("[*] IAudioClient::Stop called\n");
    PrintPtr("this", This);

    void** vtable = *(void***)This;
    auto it = g_OriginalIAudioClient_Stop.find(vtable);
    if (it == g_OriginalIAudioClient_Stop.end()) {
        printf("[!] ERROR: No original Stop found for vtable %p\n", vtable);
        fflush(stdout);
        return E_UNEXPECTED;
    }

    FnStop originalStop = it->second;
    HRESULT hr = originalStop(This);
    printf("  HRESULT: 0x%08lX\n", hr);
    fflush(stdout);
    return hr;
}

void PatchIAudioClient(void* pObj)
{
    if (!pObj) return;
    void** vtable = *(void***)pObj;

    // Check if we've already patched this vtable
    if (g_PatchedIAudioClientVTables.find(vtable) != g_PatchedIAudioClientVTables.end()) {
        printf("[*] IAudioClient vtable already patched (vtable: %p), skipping\n", vtable);
        fflush(stdout);
        return;
    }

    DWORD oldProtect = 0;
    if (VirtualProtect(vtable, sizeof(void*) * 20, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        // Store the original function pointers for THIS specific vtable
        g_OriginalIAudioClient_Initialize[vtable] = (FnInitialize)vtable[3];
        vtable[3] = (void*)&My_IAudioClient_Initialize;

        g_OriginalIAudioClient_Start[vtable] = (FnStart)vtable[10];
        vtable[10] = (void*)&My_IAudioClient_Start;

        g_OriginalIAudioClient_Stop[vtable] = (FnStop)vtable[11];
        vtable[11] = (void*)&My_IAudioClient_Stop;

        g_OriginalIAudioClient_GetService[vtable] = (FnGetService)vtable[14];
        vtable[14] = (void*)&My_IAudioClient_GetService;

        VirtualProtect(vtable, sizeof(void*) * 20, oldProtect, &oldProtect);
        g_PatchedIAudioClientVTables.insert(vtable);
        printf("[+] IAudioClient vtable patched (vtable: %p)\n", vtable);
    }
    else {
        printf("[!] Failed to patch IAudioClient vtable\n");
    }
    fflush(stdout);
}

// --- [ IMMDevice vtable hooks ] ---
typedef HRESULT(STDMETHODCALLTYPE* FnIMMDevice_Activate)(void*, REFIID, DWORD, PROPVARIANT*, void**);

// Store original function pointer PER VTABLE (there can be multiple IMMDevice vtables!)
static std::unordered_map<void*, FnIMMDevice_Activate> g_OriginalIMMDevice_Activate;
static std::unordered_set<void*> g_PatchedIMMDeviceVTables;

HRESULT STDMETHODCALLTYPE My_IMMDevice_Activate(
    void* This,
    REFIID iid,
    DWORD dwClsCtx,
    PROPVARIANT* pActivationParams,
    void** ppInterface
) {
    // Check if this is a DirectSound, IAudioClient or XAudio2 interface
    bool isDirectSound = (iid.Data1 == 0x279AFA83 || iid.Data1 == 0xC50A7E93 ||
        iid.Data1 == 0xB0210780 || iid.Data1 == 0x00990DF4);
    bool isIAudioClient = (iid.Data1 == 0x1CB9AD4C && iid.Data2 == 0xDBFA && iid.Data3 == 0x4C32);

    if (isDirectSound || isIAudioClient) {
        printf("\n");
        printf("********************************************************************************\n");
        printf("*** NEW AUDIO INTERFACE ACTIVATION! ***\n");
        printf("********************************************************************************\n");
    }

    printf("[*] IMMDevice::Activate called\n");
    PrintPtr("IMMDevice*", This);
    PrintKnownGUID(iid, "Requested IID");
    printf("  dwClsCtx: 0x%08lX\n", dwClsCtx);

    void** vtable = *(void***)This;
    auto it = g_OriginalIMMDevice_Activate.find(vtable);
    if (it == g_OriginalIMMDevice_Activate.end()) {
        printf("[!] ERROR: No original Activate found for vtable %p\n", vtable);
        fflush(stdout);
        return E_UNEXPECTED;
    }

    FnIMMDevice_Activate originalActivate = it->second;
    HRESULT hr = originalActivate(This, iid, dwClsCtx, pActivationParams, ppInterface);
    printf("  HRESULT: 0x%08lX\n", hr);

    if (SUCCEEDED(hr) && ppInterface && *ppInterface) {
        PrintPtr("Result Interface", *ppInterface);
        printf("  VTable: %p\n", *(void**)*ppInterface);

        // Hook IAudioClient if it's a new audio client
        if (isIAudioClient) {
            printf(">>> NEW IAudioClient created (HOOKING DISABLED - was breaking audio)\n");
            PatchIAudioClient(*ppInterface);
        }

        if (isDirectSound) {
            printf(">>> DirectSound interface created - this might be the beep!\n");
        }
    }

    if (isDirectSound || isIAudioClient) {
        printf("********************************************************************************\n");
    }

    fflush(stdout);
    return hr;
}

void PatchIMMDevice(void* pIMMDevice) {
    if (!pIMMDevice) return;
    void** vtable = *(void***)pIMMDevice;

    // Check if we've already patched this vtable
    if (g_PatchedIMMDeviceVTables.find(vtable) != g_PatchedIMMDeviceVTables.end()) {
        printf("[*] IMMDevice vtable already patched (vtable: %p), skipping\n", vtable);
        fflush(stdout);
        return;
    }

    DWORD oldProtect = 0;
    if (VirtualProtect(vtable, sizeof(void*) * 8, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        // Store the original function pointer for THIS specific vtable
        FnIMMDevice_Activate originalActivate = (FnIMMDevice_Activate)vtable[3];
        g_OriginalIMMDevice_Activate[vtable] = originalActivate;

        vtable[3] = (void*)&My_IMMDevice_Activate;
        VirtualProtect(vtable, sizeof(void*) * 8, oldProtect, &oldProtect);
        g_PatchedIMMDeviceVTables.insert(vtable);
        printf("[+] IMMDevice::Activate vtable patched (vtable: %p, original: %p)\n", vtable, originalActivate);
    }
    else {
        printf("[!] Failed to patch IMMDevice vtable\n");
    }
    fflush(stdout);
}

// --- [ IMMDeviceEnumerator vtable hooks ] ---
typedef HRESULT(STDMETHODCALLTYPE* FnEnumAudioEndpoints)(void*, int, DWORD, void**);
typedef HRESULT(STDMETHODCALLTYPE* FnGetDefaultAudioEndpoint)(void*, int, int, void**);
typedef HRESULT(STDMETHODCALLTYPE* FnGetDevice)(void*, LPCWSTR, void**);

FnEnumAudioEndpoints orig_EnumAudioEndpoints = nullptr;
FnGetDefaultAudioEndpoint orig_GetDefaultAudioEndpoint = nullptr;
FnGetDevice orig_GetDevice = nullptr;

// Track if we've already unhooked GetDefaultAudioEndpoint
static bool g_GetDefaultAudioEndpoint_Unhooked = false;
static void* g_IMMDeviceEnumerator_VTable = nullptr;
static bool g_IMMDeviceEnumerator_VTable_Patched = false;

HRESULT STDMETHODCALLTYPE My_EnumAudioEndpoints(void* This, int dataFlow, DWORD stateMask, void** ppDevices)
{
    printf("[*] IMMDeviceEnumerator::EnumAudioEndpoints called\n");
    PrintPtr("this", This);
    printf("  dataFlow: %d, stateMask: 0x%08lX\n", dataFlow, stateMask);
    HRESULT hr = orig_EnumAudioEndpoints(This, dataFlow, stateMask, ppDevices);
    printf("  HRESULT: 0x%08lX\n", hr);
    fflush(stdout);
    return hr;
}

HRESULT STDMETHODCALLTYPE My_GetDefaultAudioEndpoint(void* This, int dataFlow, int role, void** ppDevice)
{
    printf("[*] IMMDeviceEnumerator::GetDefaultAudioEndpoint called\n");
    HRESULT hr = orig_GetDefaultAudioEndpoint(This, dataFlow, role, ppDevice);
    if (SUCCEEDED(hr) && ppDevice && *ppDevice) {
        PrintPtr("IMMDevice VTable", *(void**)*ppDevice);
        PatchIMMDevice(*ppDevice);  // RE-ENABLED with per-vtable fix
    }

    // Unhook GetDefaultAudioEndpoint after first successful call
    if (!g_GetDefaultAudioEndpoint_Unhooked && g_IMMDeviceEnumerator_VTable) {
        void** vtable = (void**)g_IMMDeviceEnumerator_VTable;
        DWORD oldProtect = 0;
        if (VirtualProtect(vtable, sizeof(void*) * 8, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            vtable[4] = (void*)orig_GetDefaultAudioEndpoint;  // Restore original function
            VirtualProtect(vtable, sizeof(void*) * 8, oldProtect, &oldProtect);
            g_GetDefaultAudioEndpoint_Unhooked = true;
        }
    }

    fflush(stdout);
    return hr;
}

HRESULT STDMETHODCALLTYPE My_GetDevice(void* This, LPCWSTR pwstrId, void** ppDevice)
{
    printf("[*] IMMDeviceEnumerator::GetDevice called\n");
    PrintPtr("this", This);
    if (pwstrId) wprintf(L"  DeviceId: %ls\n", pwstrId);
    HRESULT hr = orig_GetDevice(This, pwstrId, ppDevice);
    printf("  HRESULT: 0x%08lX\n", hr);
    if (SUCCEEDED(hr) && ppDevice && *ppDevice) {
        PatchIMMDevice(*ppDevice);
    }
    fflush(stdout);
    return hr;
}

void PatchIMMDeviceEnumerator(void* pObj)
{
    if (!pObj) return;
    void** vtable = *(void***)pObj;

    // Check if we've already patched this vtable (vtables are SHARED across all instances!)
    if (g_IMMDeviceEnumerator_VTable_Patched && vtable == g_IMMDeviceEnumerator_VTable) {
        printf("[*] IMMDeviceEnumerator vtable already patched (vtable: %p), skipping\n", vtable);
        fflush(stdout);
        return;
    }

    g_IMMDeviceEnumerator_VTable = vtable;  // Store vtable for unhooking later
    DWORD oldProtect = 0;
    if (VirtualProtect(vtable, sizeof(void*) * 8, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        orig_EnumAudioEndpoints = (FnEnumAudioEndpoints)vtable[3];
        vtable[3] = (void*)&My_EnumAudioEndpoints;
        orig_GetDefaultAudioEndpoint = (FnGetDefaultAudioEndpoint)vtable[4];
        vtable[4] = (void*)&My_GetDefaultAudioEndpoint;
        orig_GetDevice = (FnGetDevice)vtable[5];
        vtable[5] = (void*)&My_GetDevice;
        VirtualProtect(vtable, sizeof(void*) * 8, oldProtect, &oldProtect);
        g_IMMDeviceEnumerator_VTable_Patched = true;
        printf("[+] IMMDeviceEnumerator vtable patched at %p\n", pObj);
        printf("[+] VTable address: %p\n", g_IMMDeviceEnumerator_VTable);
        printf("[+] Original GetDefaultAudioEndpoint: %p\n", orig_GetDefaultAudioEndpoint);
    }
    else {
        printf("[!] Failed to patch IMMDeviceEnumerator vtable\n");
    }
    fflush(stdout);
}

// --- [ CoCreateInstance Hook ] ---
HRESULT WINAPI HookedCoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv)
{
    // Check for XAudio2 or DirectSound
    bool isXAudio2 = (rclsid.Data1 == 0x5A508685 || riid.Data1 == 0x8BCF1F58);
    bool isDirectSound = (riid.Data1 == 0x279AFA83 || riid.Data1 == 0xC50A7E93);

    if (isXAudio2 || isDirectSound) {
        printf("\n");
        printf("################################################################################\n");
        printf("### AUDIO ENGINE CREATION - THIS MIGHT BE FOR THE BEEP! ###\n");
        printf("################################################################################\n");
    }

    printf("CoCreateInstance called:\n");
    PrintKnownGUID(rclsid, "CLSID");
    PrintKnownGUID(riid, "IID");
    printf("  dwClsContext: 0x%lX\n", dwClsContext);

    HRESULT hr = pOriginalCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

    if (SUCCEEDED(hr) && ppv && *ppv) {
        printf("  Result: SUCCESS\n");
        PrintPtr("Object", *ppv);
        printf("  VTable: %p\n", *(void**)*ppv);

        // Check if IMMDeviceEnumerator
        if (riid.Data1 == 0xA95664D2 && riid.Data2 == 0x9614 && riid.Data3 == 0x4F35) {
            PatchIMMDeviceEnumerator(*ppv);
        }
    }
    else {
        printf("  Result: FAILED (0x%08lX)\n", hr);
    }

    if (isXAudio2 || isDirectSound) {
        printf("################################################################################\n");
    }

    printf("\n");
    fflush(stdout);

    return hr;
}

// --- [ MinHook Setup ] ---
BOOL InitializeHooks()
{
    if (MH_Initialize() != MH_OK) {
        printf("MH_Initialize failed\n");
        return FALSE;
    }

    // Hook CoCreateInstance for WASAPI detection
    if (MH_CreateHookApi(L"ole32.dll", "CoCreateInstance", (LPVOID)HookedCoCreateInstance, (LPVOID*)&pOriginalCoCreateInstance) != MH_OK) {
        printf("MH_CreateHookApi (CoCreateInstance) failed\n");
        return FALSE;
    }

    // Hook PeekMessageW for spacebar detection
    if (MH_CreateHookApi(L"user32.dll", "PeekMessageW", (LPVOID)HookedPeekMessageW, (LPVOID*)&pOriginalPeekMessageW) != MH_OK) {
        printf("MH_CreateHookApi (PeekMessageW) failed\n");
        return FALSE;
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        printf("MH_EnableHook failed\n");
        return FALSE;
    }

    printf("CoCreateInstance hook enabled\n");
    printf("PeekMessageW hook enabled\n\n");
    return TRUE;
}

DWORD WINAPI MainRoutine(LPVOID lpParam)
{
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);

    printf("Audio Hooking...\n");

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
