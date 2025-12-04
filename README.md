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

## Building (planned)

The intended toolchain:

- Windows 10/11
- Visual Studio (Community or higher)
- Windows Driver Kit (WDK) for x64

A Visual Studio solution will be added to:

- build the kernel driver,
- build the user‑mode CLI.

## Installation (planned)

Inside the guest VM:

1. Enable test‑signing if using an unsigned test driver.
2. Run `scripts/install.ps1` as Administrator:
   - Installs the driver (`nanabox_hvfilter`),
   - Copies `nanabox-lite.exe` and profiles into `C:\Program Files\NanaBoxLite` (by default).
3. Apply a profile:
   ```powershell
   "C:\Program Files\NanaBoxLite\nanabox-lite.exe" --profile "C:\Program Files\NanaBoxLite\profiles\roblox-lite.json"
   ```

## Legal

Again: this repository exists for learning, prototyping and research.  
Do **not** use it to cheat, evade anticheat, or break agreements with software vendors.
