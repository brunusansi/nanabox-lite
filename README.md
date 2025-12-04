# NanaBox Lite

NanaBox Lite is an experimental playground for **Windows guest VMs running on Hyper‑V with GPU‑P (GPU partitioning)**, focused on **researching anti‑VM / anti‑detection techniques** for games and anticheat systems.

> ⚠️ **Important disclaimer**  
> This project is strictly for educational and research purposes.  
> It is **not** intended to violate game EULAs, ToS, anticheat policies, or any applicable law.  
> You are solely responsible for how you use this code.

## High‑level idea

The core idea of NanaBox Lite is:

- **Host**: Windows with Hyper‑V enabled and GPU‑P configured.
- **Guest**: Windows VM with a partitioned GPU (GPU‑P) attached.
- **NanaBox Lite components (inside the guest)**:
  - A **kernel‑mode driver** (`nanabox_hvfilter.sys`) that:
    - Receives an “identity profile” (CPU + SMBIOS info) from user mode via IOCTL.
    - Stores this profile in kernel memory, ready for future low‑level hooks (CPUID/MSR, SMBIOS, ACPI, etc.).
  - A **user‑mode CLI tool** (`nanabox-lite.exe`) that:
    - Loads a JSON profile (e.g. `profiles/roblox-lite.json`),
    - Maps it into a C struct,
    - Sends it to the kernel driver via `DeviceIoControl`.

The first MVP does **not** try to hook CPUID, MSRs or Hyper‑V internals yet.  
It is just the **plumbing**:

- shared structs + IOCTL contracts,
- a working kernel driver,
- a CLI that can talk to the driver,
- example profiles and an install script.

Later phases can build on top of this to explore:

- CPUID/MSR filtering,
- SMBIOS / ACPI spoofing,
- timing and other VM detection vectors.

## Status

🚧 **Very early WIP / experimental**

Right now the goal is to:

1. Define the profile format (CPU + SMBIOS) in a shared header.
2. Implement the kernel driver that accepts and stores this profile.
3. Implement the CLI that loads JSON profiles and calls the IOCTL.
4. Provide a PowerShell script to install everything inside a VM.

Once this wiring is stable and testable inside a GPU‑P VM, more advanced anti‑VM logic can be layered on top.

## Rough architecture

- `driver/`
  - `nanabox_hvfilter.sys` – KMDF/WDM driver exposing `\\.\NanaboxHvFilter`.
  - `.inf` file – for manual / test‑signed installation via `pnputil`.
- `src/`
  - `nanabox-lite.exe` – user‑mode CLI that:
    - parses JSON profile → `NANABOX_PROFILE`,
    - calls `NANABOX_IOCTL_SET_PROFILE`.
- `include/`
  - `nanabox_profile.h` – shared structs and IOCTL definitions.
- `profiles/`
  - `roblox-lite.json` – example identity profile for testing.
- `scripts/`
  - `install.ps1` – installs the driver + CLI + profiles inside the guest VM.

## Prerequisites

- **Windows 10 or 11** (x64)
- **Visual Studio 2022** (Community edition or higher)
- **Windows Driver Kit (WDK)** for Visual Studio 2022
  - Download from [Microsoft WDK downloads](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- **Test-signing enabled** in the target VM (for unsigned test drivers)

## Building

1. **Open the solution**:  
   Open `NanaBoxLite.sln` in Visual Studio 2022.

2. **Select configuration**:  
   Choose **Release** or **Debug** and platform **x64**.

3. **Build the solution**:  
   Build → Build Solution (or press `Ctrl+Shift+B`).

   The build outputs will be placed in:
   - `build\Release\x64\nanabox-lite.exe` (CLI tool)
   - `build\Release\x64\nanabox_hvfilter.sys` (kernel driver)

> **Note**: The driver project requires the WDK to be installed. If you get errors about missing `wdf.h` or kernel headers, ensure the WDK is properly installed and integrated with Visual Studio.

## Installation

### Prepare the guest VM

1. **Enable test signing** (required for unsigned test drivers):
   ```powershell
   bcdedit /set testsigning on
   ```
   Then reboot the VM.

2. **Copy build output** into the VM:  
   Copy the entire repository (or at least `build\`, `driver\`, `profiles\`, and `scripts\` folders) to a location inside the VM.

### Run the installer

Inside the guest VM, run as Administrator:

```powershell
cd <path-to-nanabox-lite>
.\scripts\install.ps1
```

The script will:
- Create `C:\Program Files\NanaBoxLite` (or a custom path via `-InstallDir`).
- Copy `nanabox-lite.exe` and profiles.
- Install the driver using `pnputil`.

### Apply a profile

```powershell
"C:\Program Files\NanaBoxLite\nanabox-lite.exe" --profile "C:\Program Files\NanaBoxLite\profiles\roblox-lite.json"
```

Or simply run without arguments to use the default `roblox-lite.json` profile:

```powershell
"C:\Program Files\NanaBoxLite\nanabox-lite.exe"
```

## CLI usage

```
Usage: nanabox-lite.exe [options]

Options:
  --profile <path>  Path to JSON profile file
                    Default: profiles\roblox-lite.json (relative to exe)
  --help, -h        Show this help message
```

## Profile format

Profiles are JSON files defining the CPU and SMBIOS identity to apply. Example (`profiles/roblox-lite.json`):

```json
{
  "version": 1,
  "cpu": {
    "vendor": "GenuineIntel",
    "brandString": "Intel(R) Core(TM) i7-9700K CPU @ 3.60GHz",
    "hypervisorBit": false
  },
  "smbios": {
    "systemManufacturer": "ASUSTeK COMPUTER INC.",
    "systemProductName": "ROG STRIX B660-F GAMING WIFI",
    "baseBoardManufacturer": "ASUSTeK COMPUTER INC.",
    "baseBoardProduct": "B660-F GAMING",
    "systemSerial": "MB-123456789",
    "systemUUID": "550e8400-e29b-41d4-a716-446655440000"
  }
}
```

## Legal

Again: this repository exists for learning, prototyping and research.  
Do **not** use it to cheat, evade anticheat, or break agreements with software vendors.
