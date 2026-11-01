# HiBy R1 Cyber Tools: Network Diagnostics

**Status:** Working; device testing has been reported for Ping and DHCP connectivity. The notes below describe the checked-in implementation; feature-by-feature test results are not recorded for every tool.  
**Device:** HiBy R1  
**Firmware:** Version not recorded here  
**Repository:** [Hiby-R1-MP3-Network-Diagnostic-Tool](https://github.com/geoimaged/Hiby-R1-MP3-Network-Diagnostic-Tool)  
**Platform reference:** [bidhata/Hiby-R1-Mod](https://github.com/bidhata/Hiby-R1-Mod)

## Goal

Add a screen-based network diagnostics menu to the HiBy R1 and connect its controls to the network tools available in the stock Linux environment. The application provides reusable command execution and connectivity code so individual tools can share input, output, and timeout handling.

## Current State

The checked-in source has menus for Ping, DNS lookup, web check, Nmap, and Wi-Fi functions. The Wi-Fi screens read connection and DHCP information, scan for nearby access points, display access-point details, and can save a Wi-Fi assessment report. Nmap has host discovery, port scan, service detection, and a combined assessment flow.

The project source and redacted device screenshots are in this repository. The Nmap executable and its data directory are expected on the device's SD card; they are not bundled here. The source depends on the pinned `Hiby-R1-Mod` submodule for the framebuffer, input, and font platform layer.

## Architecture

```text
HiBy buttons / touch
        ↓
GEOS menus and on-screen target keyboard (main.c, keyboard.c)
        ↓
Connectivity and tool wrappers (connectivity.c, tools.c)
        ↓
Executor: starts argv directly, captures output, enforces timeouts
        ↓
Stock firmware utilities and SD-card Nmap
        ↓
Results displayed on screen; selected reports saved to SD card
```

The executor uses `fork`/`execvp`; input is passed as an argument list rather than a shell command string. It combines stdout and stderr into a bounded buffer and supports per-command timeouts.

## Key Files

| File | Purpose |
| --- | --- |
| `app/src/main.c` | Menus, screens, Wi-Fi and Nmap assessment flows, result parsing and reports |
| `app/src/executor.c` | Runs utilities, captures output, and enforces timeouts |
| `app/src/tools.c` | Ping, DNS, web check, and Nmap command wrappers |
| `app/src/connectivity.c` | Wi-Fi control, status, DHCP/network context, and access-point scan |
| `app/src/keyboard.c` | Reusable on-screen target-entry keyboard |
| `app/include/` | Interfaces shared by the source files |
| `app/Makefile` | MIPS32r2 cross-build rules |
| `third_party/Hiby-R1-Mod` | Pinned upstream display, input, and font dependency |

## Technical Notes

- **Platform:** HiBy R1 Linux, MIPS little-endian, MIPS32r2 target per the Makefile.
- **Interface:** The upstream platform layer supplies framebuffer drawing, physical-key input, and touch/menu input.
- **Build:** Initialize the submodule, then run `make -C app`. The default compiler prefix is `mipsel-linux-gnu-`; set `CROSS_COMPILE` if the installed toolchain uses another prefix.
- **Dependencies:** MIPS cross compiler; `Hiby-R1-Mod` submodule; stock firmware tools such as BusyBox, `wpa_cli`, `ip`, and ping; and a compatible Nmap executable plus data files on the SD card for Nmap features.
- **Device paths in the implementation:** Nmap is invoked from `/data/mnt/sd_0/nmap` with data in `/data/mnt/sd_0/nmap-data`. Wi-Fi control uses `/usr/bin/wifi_on.sh` and `/usr/bin/wifi_off.sh`.
- **Important discovery:** The stock Wi-Fi connection and DHCP setup can be queried through the device's existing `wpa_cli`, route, resolver, and process information. The app reports that state rather than implementing a separate DHCP client.
- **Build reproducibility:** The checked-in build instructions describe the intended build. A fresh-clone build has not been independently reproduced as part of preparing this repository.

## How the Features Are Implemented

### DNS lookup

The RECON menu opens the reusable target keyboard, then calls `tool_dns_lookup`. That wrapper invokes the device's BusyBox `nslookup` with the entered host or address. The screen parses the command output into the DNS server and returned answers for display. The command runs through the shared executor, so its output is captured and its execution is bounded by a timeout.

### Wi-Fi assessment

The Wi-Fi status code queries `wpa_cli` for connection state, SSID, and device IP; reads the default gateway from `ip route`; reads DNS configuration from `/etc/resolv.conf`; and checks whether `udhcpc` is running. If a device IP is present, DHCP is reported as configured even when the short-lived DHCP process is no longer visible.

For nearby access points, the app asks `wpa_cli` to scan and read scan results. The assessment screen parses the advertised SSID, BSSID, signal, frequency, and security flags, derives band/channel and a readable security summary, and can save a report under `/data/mnt/sd_0/`. The report describes advertised scan metadata and connection context; it does not claim to perform a cryptographic audit or raw wireless packet capture.

### Nmap

The Nmap menu offers discovery and assessment paths. Discovery runs Nmap host discovery (`-sn`) against the entered target. The app parses unique IPv4 hosts from the output and presents them in a selectable list. Selecting a host starts an assessment for that selected address; it does not automatically run a separate assessment against every discovered host.

The assessment flow runs Ping, a TCP connect scan of the top 1,000 ports, and a service-version scan. The implementation supplies the R1's `wlan0` IPv4 address to the port and service scans because the cross-compiled Nmap build could not reliably determine route/interface information on this device. Results can be saved as a report on the SD card.

Nmap is an external on-device dependency in this project: the executable and data directory are not included in Git. Run scans only on devices and networks you own or are authorized to assess.

### Ping

The Ping screen uses the target keyboard and runs the device's `/bin/ping` with four probes. The combined assessment flow also runs Ping and extracts a short status/latency summary before the Nmap scans. The user reported that Ping worked on the R1 after it received a DHCP lease.

### Web check with wget

The web-check screen accepts a host/IP and adds `http://` when the user did not enter a scheme. It invokes BusyBox `wget` in spider mode with a five-second network timeout and proxy use disabled. The UI filters the captured response to useful connection, HTTP, content-type, redirect, and error lines rather than displaying the full command output.

### Host discovery list

After Nmap discovery, `main.c` extracts IPv4 addresses from Nmap's host-report lines, removes duplicates, and displays the discovered devices. The user can move through the list with the device controls or select a row by touch. Selecting a row passes that host's IP directly to the assessment flow, so a second keyboard entry is unnecessary.

### On-screen keyboard

`keyboard.c` draws and handles a reusable 12-position keypad plus action keys. It supports alphabetic multi-tap input for hostnames and a numeric mode for IP addresses and CIDR targets. The keyboard also handles movement/selection, delete, clear, run, and back actions. Nmap discovery enables slash input so a subnet such as `192.168.1.0/24` can be entered; the ping and other target screens use the mode that disallows slash input.

## Changes Made

### Shared executor and tool wrappers

**Before:** Each screen would otherwise need to launch and collect output from its own commands.  
**After:** Tool screens use a shared executor and small wrappers for Ping, DNS, web checks, and Nmap.  
**Method:** The executor passes explicit argument vectors, captures standard output and error, and enforces timeouts. This implementation avoids passing entered targets through a shell.

### Network and Wi-Fi screens

**Before:** The device's stock Wi-Fi and DHCP state was not presented in the diagnostics UI.  
**After:** The app can start/stop Wi-Fi, show connection and DHCP context, scan access points, and save a metadata report.  
**Method:** `connectivity.c` wraps the stock Wi-Fi scripts and system utilities; `main.c` renders the results and report.

### Nmap discovery and assessment

**Before:** A user would need to know and enter an individual host address to inspect it.  
**After:** Discovery results are listed on-device; selecting a discovered host begins its assessment.  
**Method:** The app parses Nmap output, deduplicates IPv4 addresses, makes the list navigable, then sends the selected address through the Ping, port-scan, and service-detection flow.

### On-screen target entry

**Before:** Entering a host or IP from the device UI needed a dedicated input path.  
**After:** The target keyboard is reused across the network tools.  
**Method:** `keyboard.c` handles character/numeric entry and screen controls; callers choose whether CIDR slash input is allowed.

## Testing

**Recorded result:** The project owner reported that Ping worked on the HiBy R1 and that the device obtained a DHCP lease. The repository includes redacted screenshots showing the application running on the device.  
**Not recorded here:** Per-feature test results for DNS lookup, Wi-Fi access-point assessment, wget, Nmap discovery, each Nmap scan mode, and saving/reopening reports.  
**Build status:** The repository's build recipe has not been independently reproduced from a clean checkout as part of preparing these notes.

## Problems / Fixes

**Problem:** The cross-compiled Nmap port and service scans did not reliably determine route/interface information on the R1.  
**Cause:** The Nmap build's route/interface discovery did not work reliably in this target environment.  
**Fix:** The implementation reads the IPv4 address assigned to `wlan0` and passes it to the port and service scan commands. Host discovery uses a separate `-sn` command path.

## Next Step

Record a short, repeatable on-device test checklist with the exact tool version/location and the observed result for each menu item. Then update the build guide with the steps used for the successful device build, if those steps are available.

## Interview Takeaway

> I built a small diagnostics interface for an embedded Linux music player by connecting a framebuffer UI to the device's existing network utilities. I separated command execution, connectivity queries, and input handling so tools could share safe argument passing, captured output, and timeouts.
