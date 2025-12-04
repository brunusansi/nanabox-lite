/*
 * NanaBox Lite - HV Filter Driver
 *
 * A KMDF driver that accepts profile data from user mode via IOCTL
 * and stores it in kernel memory for future use.
 */

#include <ntddk.h>
#include <wdf.h>

#define NANABOX_KERNEL_MODE
#include "../include/nanabox_profile.h"

/* Driver globals */
static NANABOX_PROFILE g_Profile;
static KSPIN_LOCK g_ProfileLock;
static BOOLEAN g_ProfileLoaded = FALSE;

/* Forward declarations */
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD NanaboxEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL NanaboxEvtIoDeviceControl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, NanaboxEvtDeviceAdd)
#endif

/*
 * DriverEntry - Driver initialization routine
 */
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    WDF_DRIVER_CONFIG config;

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "NanaboxHvFilter: DriverEntry started\n"));

    /* Initialize the spin lock for profile access */
    KeInitializeSpinLock(&g_ProfileLock);

    /* Initialize driver config */
    WDF_DRIVER_CONFIG_INIT(&config, NanaboxEvtDeviceAdd);

    /* Create WDF driver object */
    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "NanaboxHvFilter: WdfDriverCreate failed with status 0x%08X\n",
            status));
        return status;
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "NanaboxHvFilter: DriverEntry completed successfully\n"));

    return status;
}

/*
 * NanaboxEvtDeviceAdd - Create device and symbolic link
 */
NTSTATUS
NanaboxEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    NTSTATUS status;
    WDFDEVICE device;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE queue;
    DECLARE_CONST_UNICODE_STRING(deviceName, L"\\Device\\NanaboxHvFilter");
    DECLARE_CONST_UNICODE_STRING(dosDeviceName, L"\\DosDevices\\NanaboxHvFilter");

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "NanaboxHvFilter: EvtDeviceAdd started\n"));

    /* Assign device name */
    status = WdfDeviceInitAssignName(DeviceInit, &deviceName);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "NanaboxHvFilter: WdfDeviceInitAssignName failed with status 0x%08X\n",
            status));
        return status;
    }

    /* Set device type and characteristics */
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_NANABOX);
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

    /* Create the device */
    status = WdfDeviceCreate(&DeviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "NanaboxHvFilter: WdfDeviceCreate failed with status 0x%08X\n",
            status));
        return status;
    }

    /* Create symbolic link */
    status = WdfDeviceCreateSymbolicLink(device, &dosDeviceName);
    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "NanaboxHvFilter: WdfDeviceCreateSymbolicLink failed with status 0x%08X\n",
            status));
        return status;
    }

    /* Initialize the I/O queue */
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = NanaboxEvtIoDeviceControl;

    status = WdfIoQueueCreate(
        device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
    );

    if (!NT_SUCCESS(status)) {
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
            "NanaboxHvFilter: WdfIoQueueCreate failed with status 0x%08X\n",
            status));
        return status;
    }

    KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "NanaboxHvFilter: EvtDeviceAdd completed successfully\n"));

    return STATUS_SUCCESS;
}

/*
 * NanaboxEvtIoDeviceControl - Handle IOCTL requests
 */
VOID
NanaboxEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PNANABOX_PROFILE inputBuffer = NULL;
    size_t bufferSize;
    KIRQL oldIrql;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);

    switch (IoControlCode) {
    case NANABOX_IOCTL_SET_PROFILE:
        /* Validate input buffer size */
        if (InputBufferLength < sizeof(NANABOX_PROFILE)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                "NanaboxHvFilter: Input buffer too small (got %llu, need %llu)\n",
                (unsigned long long)InputBufferLength,
                (unsigned long long)sizeof(NANABOX_PROFILE)));
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        /* Get the input buffer */
        status = WdfRequestRetrieveInputBuffer(
            Request,
            sizeof(NANABOX_PROFILE),
            (PVOID*)&inputBuffer,
            &bufferSize
        );

        if (!NT_SUCCESS(status)) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                "NanaboxHvFilter: WdfRequestRetrieveInputBuffer failed with status 0x%08X\n",
                status));
            break;
        }

        /* Validate profile version */
        if (inputBuffer->Version != NANABOX_PROFILE_VERSION) {
            KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
                "NanaboxHvFilter: Invalid profile version (got %u, expected %u)\n",
                inputBuffer->Version, NANABOX_PROFILE_VERSION));
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        /* Copy the profile to global storage with spinlock protection */
        KeAcquireSpinLock(&g_ProfileLock, &oldIrql);
        RtlCopyMemory(&g_Profile, inputBuffer, sizeof(NANABOX_PROFILE));
        g_ProfileLoaded = TRUE;
        KeReleaseSpinLock(&g_ProfileLock, oldIrql);

        /* Log key fields */
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "NanaboxHvFilter: Profile loaded successfully\n"));
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "NanaboxHvFilter:   CPU Vendor: %s\n", g_Profile.Cpu.Vendor));
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "NanaboxHvFilter:   CPU Brand: %s\n", g_Profile.Cpu.BrandString));
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "NanaboxHvFilter:   Hypervisor bit: %s\n",
            g_Profile.Cpu.HypervisorBit ? "true" : "false"));
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "NanaboxHvFilter:   System Manufacturer: %ws\n",
            g_Profile.Smbios.SystemManufacturer));
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "NanaboxHvFilter:   System Product: %ws\n",
            g_Profile.Smbios.SystemProductName));

        status = STATUS_SUCCESS;
        break;

    default:
        KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL,
            "NanaboxHvFilter: Unknown IOCTL 0x%08X\n", IoControlCode));
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestComplete(Request, status);
}
