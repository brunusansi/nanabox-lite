/*
 * NanaBox Lite - User-mode CLI
 *
 * A console application that loads a JSON profile and applies it
 * to the kernel driver via IOCTL.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/nanabox_profile.h"
#include "json.hpp"

using json = nlohmann::json;

/* Function prototypes */
static void PrintUsage(const wchar_t* programName);
static bool LoadProfile(const wchar_t* profilePath, NANABOX_PROFILE* profile);
static bool ApplyProfile(const NANABOX_PROFILE* profile);
static void SafeCopyStringA(char* dest, size_t destSize, const std::string& src);
static void SafeCopyStringW(wchar_t* dest, size_t destSize, const std::wstring& src);
static std::wstring Utf8ToWide(const std::string& str);
static std::string ReadFileContents(const wchar_t* path);

int wmain(int argc, wchar_t* argv[])
{
    const wchar_t* profilePath = nullptr;
    wchar_t defaultProfilePath[MAX_PATH] = {0};
    NANABOX_PROFILE profile = {0};

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--profile") == 0 && i + 1 < argc) {
            profilePath = argv[i + 1];
            i++;
        } else if (wcscmp(argv[i], L"--help") == 0 || wcscmp(argv[i], L"-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else {
            wprintf(L"Unknown argument: %s\n", argv[i]);
            PrintUsage(argv[0]);
            return 1;
        }
    }

    /* Use default profile path if not specified */
    if (profilePath == nullptr) {
        /* Get executable directory */
        wchar_t exePath[MAX_PATH];
        DWORD len = GetModuleFileNameW(NULL, exePath, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) {
            wprintf(L"Error: Failed to get executable path.\n");
            return 1;
        }

        /* Find last backslash and replace filename with profiles\roblox-lite.json */
        wchar_t* lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash != nullptr) {
            *lastSlash = L'\0';
            _snwprintf_s(defaultProfilePath, MAX_PATH, _TRUNCATE,
                L"%s\\profiles\\roblox-lite.json", exePath);
            profilePath = defaultProfilePath;
        } else {
            profilePath = L"profiles\\roblox-lite.json";
        }
        wprintf(L"Using default profile: %s\n", profilePath);
    }

    wprintf(L"NanaBox Lite - Profile Loader\n");
    wprintf(L"=============================\n\n");

    /* Load the profile */
    wprintf(L"Loading profile: %s\n", profilePath);
    if (!LoadProfile(profilePath, &profile)) {
        return 1;
    }

    /* Display loaded profile info */
    wprintf(L"\nProfile loaded:\n");
    wprintf(L"  Version: %u\n", profile.Version);
    wprintf(L"  CPU Vendor: %hs\n", profile.Cpu.Vendor);
    wprintf(L"  CPU Brand: %hs\n", profile.Cpu.BrandString);
    wprintf(L"  Hypervisor bit: %s\n", profile.Cpu.HypervisorBit ? L"true" : L"false");
    wprintf(L"  System Manufacturer: %s\n", profile.Smbios.SystemManufacturer);
    wprintf(L"  System Product: %s\n", profile.Smbios.SystemProductName);
    wprintf(L"  Baseboard Manufacturer: %s\n", profile.Smbios.BaseBoardManufacturer);
    wprintf(L"  Baseboard Product: %s\n", profile.Smbios.BaseBoardProduct);
    wprintf(L"  System Serial: %s\n", profile.Smbios.SystemSerial);
    wprintf(L"  System UUID: %s\n", profile.Smbios.SystemUUID);

    /* Apply the profile to the driver */
    wprintf(L"\nApplying profile to driver...\n");
    if (!ApplyProfile(&profile)) {
        return 1;
    }

    wprintf(L"Profile applied successfully.\n");
    return 0;
}

static void PrintUsage(const wchar_t* programName)
{
    wprintf(L"Usage: %s [options]\n", programName);
    wprintf(L"\nOptions:\n");
    wprintf(L"  --profile <path>  Path to JSON profile file\n");
    wprintf(L"                    Default: profiles\\roblox-lite.json (relative to exe)\n");
    wprintf(L"  --help, -h        Show this help message\n");
    wprintf(L"\nExample:\n");
    wprintf(L"  %s --profile C:\\profiles\\custom.json\n", programName);
}

static std::string ReadFileContents(const wchar_t* path)
{
    HANDLE hFile = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return "";
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return "";
    }

    std::string content(static_cast<size_t>(fileSize.QuadPart), '\0');

    DWORD bytesRead = 0;
    BOOL result = ReadFile(hFile, content.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!result || bytesRead != static_cast<DWORD>(fileSize.QuadPart)) {
        return "";
    }

    return content;
}

static bool LoadProfile(const wchar_t* profilePath, NANABOX_PROFILE* profile)
{
    try {
        std::string content = ReadFileContents(profilePath);
        if (content.empty()) {
            wprintf(L"Error: Failed to open profile file: %s\n", profilePath);
            DWORD error = GetLastError();
            wprintf(L"  Error code: %lu\n", error);
            return false;
        }

        json j = json::parse(content);

        /* Map JSON to profile struct */
        profile->Version = j.value("version", 0);

        if (profile->Version != NANABOX_PROFILE_VERSION) {
            wprintf(L"Error: Invalid profile version %u (expected %u)\n",
                profile->Version, NANABOX_PROFILE_VERSION);
            return false;
        }

        /* CPU profile */
        if (j.contains("cpu")) {
            auto& cpu = j["cpu"];
            SafeCopyStringA(profile->Cpu.Vendor, sizeof(profile->Cpu.Vendor),
                cpu.value("vendor", ""));
            SafeCopyStringA(profile->Cpu.BrandString, sizeof(profile->Cpu.BrandString),
                cpu.value("brandString", ""));
            profile->Cpu.HypervisorBit = cpu.value("hypervisorBit", false);
        }

        /* SMBIOS profile */
        if (j.contains("smbios")) {
            auto& smbios = j["smbios"];
            SafeCopyStringW(profile->Smbios.SystemManufacturer,
                sizeof(profile->Smbios.SystemManufacturer) / sizeof(WCHAR),
                Utf8ToWide(smbios.value("systemManufacturer", "")));
            SafeCopyStringW(profile->Smbios.SystemProductName,
                sizeof(profile->Smbios.SystemProductName) / sizeof(WCHAR),
                Utf8ToWide(smbios.value("systemProductName", "")));
            SafeCopyStringW(profile->Smbios.BaseBoardManufacturer,
                sizeof(profile->Smbios.BaseBoardManufacturer) / sizeof(WCHAR),
                Utf8ToWide(smbios.value("baseBoardManufacturer", "")));
            SafeCopyStringW(profile->Smbios.BaseBoardProduct,
                sizeof(profile->Smbios.BaseBoardProduct) / sizeof(WCHAR),
                Utf8ToWide(smbios.value("baseBoardProduct", "")));
            SafeCopyStringW(profile->Smbios.SystemSerial,
                sizeof(profile->Smbios.SystemSerial) / sizeof(WCHAR),
                Utf8ToWide(smbios.value("systemSerial", "")));
            SafeCopyStringW(profile->Smbios.SystemUUID,
                sizeof(profile->Smbios.SystemUUID) / sizeof(WCHAR),
                Utf8ToWide(smbios.value("systemUUID", "")));
        }

        return true;
    }
    catch (const json::exception& e) {
        wprintf(L"Error: Failed to parse JSON profile: %hs\n", e.what());
        return false;
    }
}

static bool ApplyProfile(const NANABOX_PROFILE* profile)
{
    HANDLE hDevice = CreateFileW(
        NANABOX_DOS_SYMLINK,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        wprintf(L"Error: Failed to open driver device: %s\n", NANABOX_DOS_SYMLINK);
        wprintf(L"  Error code: %lu\n", error);

        if (error == ERROR_FILE_NOT_FOUND) {
            wprintf(L"  The driver may not be installed. Run install.ps1 first.\n");
        } else if (error == ERROR_ACCESS_DENIED) {
            wprintf(L"  Access denied. Try running as Administrator.\n");
        }

        return false;
    }

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        static_cast<DWORD>(NANABOX_IOCTL_SET_PROFILE),
        const_cast<NANABOX_PROFILE*>(profile),
        static_cast<DWORD>(sizeof(NANABOX_PROFILE)),
        nullptr,
        0U,
        &bytesReturned,
        nullptr
    );

    CloseHandle(hDevice);

    if (!result) {
        DWORD error = GetLastError();
        wprintf(L"Error: DeviceIoControl failed with error code: %lu\n", error);
        return false;
    }

    return true;
}

static void SafeCopyStringA(char* dest, size_t destSize, const std::string& src)
{
    if (destSize == 0) return;
    size_t copyLen = (src.length() < destSize - 1) ? src.length() : destSize - 1;
    memcpy(dest, src.c_str(), copyLen);
    dest[copyLen] = '\0';
}

static void SafeCopyStringW(wchar_t* dest, size_t destSize, const std::wstring& src)
{
    if (destSize == 0) return;
    size_t copyLen = (src.length() < destSize - 1) ? src.length() : destSize - 1;
    wmemcpy(dest, src.c_str(), copyLen);
    dest[copyLen] = L'\0';
}

static std::wstring Utf8ToWide(const std::string& str)
{
    if (str.empty()) return L"";

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    if (wideLen <= 0) return L"";

    std::wstring wideStr(wideLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideStr.data(), wideLen);

    return wideStr;
}
