/*
 * NanaBox Lite - Shared Profile Definitions
 *
 * This header contains the shared data structures and IOCTL definitions
 * used by both the kernel-mode driver and user-mode CLI.
 */

#ifndef NANABOX_PROFILE_H
#define NANABOX_PROFILE_H

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif

#define NANABOX_PROFILE_VERSION 1

/*
 * CPU Profile - Contains spoofed CPU identification data
 */
typedef struct _NANABOX_CPU_PROFILE {
    CHAR    Vendor[16];       /* "GenuineIntel", "AuthenticAMD" */
    CHAR    BrandString[64];  /* CPUID brand string */
    BOOLEAN HypervisorBit;    /* desired value for CPUID(1).ECX[31] */
    UINT8   Reserved[3];      /* padding for alignment */
} NANABOX_CPU_PROFILE, *PNANABOX_CPU_PROFILE;

/*
 * SMBIOS Profile - Contains spoofed SMBIOS data
 */
typedef struct _NANABOX_SMBIOS_PROFILE {
    WCHAR SystemManufacturer[64];
    WCHAR SystemProductName[64];
    WCHAR BaseBoardManufacturer[64];
    WCHAR BaseBoardProduct[64];
    WCHAR SystemSerial[64];
    WCHAR SystemUUID[40]; /* GUID as string */
} NANABOX_SMBIOS_PROFILE, *PNANABOX_SMBIOS_PROFILE;

/*
 * Full Profile - Combines CPU and SMBIOS profiles
 */
typedef struct _NANABOX_PROFILE {
    UINT32               Version; /* NANABOX_PROFILE_VERSION */
    NANABOX_CPU_PROFILE  Cpu;
    NANABOX_SMBIOS_PROFILE Smbios;
} NANABOX_PROFILE, *PNANABOX_PROFILE;

/*
 * Device Names
 */
#define NANABOX_DEVICE_NAME     L"\\Device\\NanaboxHvFilter"
#define NANABOX_DOS_DEVICE_NAME L"\\DosDevices\\NanaboxHvFilter"
#define NANABOX_DOS_SYMLINK     L"\\\\.\\NanaboxHvFilter"

/*
 * IOCTL Definitions
 */
#define FILE_DEVICE_NANABOX 0x8000
#define NANABOX_IOCTL_SET_PROFILE CTL_CODE( \
    FILE_DEVICE_NANABOX, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif /* NANABOX_PROFILE_H */
