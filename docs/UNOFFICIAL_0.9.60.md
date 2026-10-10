# OpenPuck 0.9.60 — unofficial redcoins modification

Modification date: **2026-10-10** (Asia/Seoul).

This is a personal modification of
[safijari/openpuck](https://github.com/safijari/openpuck), based on upstream
[0.9.59](https://github.com/safijari/openpuck/tree/0.9.59).
The `0.9.60` version used here identifies this modification and does not denote
an official release by the upstream project or Valve.

## Changes

- [OpenPuck/puck_hid.cpp](../OpenPuck/puck_hid.cpp): preserve each slot's USB
  connection state while its HID IN endpoint is busy. Reset that state when
  USB is unmounted or the slot's bond is removed.
- [tools/test-puck-connection.cpp](../tools/test-puck-connection.cpp) and
  [tools/test-puck-connection.py](../tools/test-puck-connection.py): add seven
  host regression scenarios that execute the firmware's actual
  `SteamPuckController::task()` with a simulated USB bus.
- [README.md](../README.md) and this document: identify the modification,
  its source, build procedure, and verification scope.

Previously, a temporarily busy endpoint made `hid[s].ready()` return false.
The firmware cleared its remembered connection state, then sent another
`0x79 = 0x02` connection report when the endpoint became ready again.
This could repeatedly notify Steam that a controller had connected even while
the RF link remained active and input reports continued.

The modification uses USB mount state for that reset decision. The slot's
existing RF connection checks continue to handle actual RF disconnections.

## Verification and limits

The observations below were recorded on **2026-10-10** with one Windows PC,
one nRF52840 Pro Micro / nice!nano-compatible dongle, and one Steam Controller 2
in Steam mode.

- Before the fix, a ten-second capture from the attached device contained
  **7,290 connection reports** and **2,240 input reports**, with no `0x79 = 0x01`
  disconnection reports. Steam's controller log also recorded repeated
  connection-state events. These were repeated connection announcements;
  the capture does not establish repeated physical USB or RF disconnections.
- After flashing the fix, a game was running for approximately 14 minutes
  with no additional matching connection-state events in the Steam log.
  A subsequent 30-second capture contained **7,477 input reports**,
  **zero connection reports**, and **zero new Steam connection-state events**.
- After changing the cable/adapter, the log recorded the expected reconnect
  when the device was plugged in. A later 30-second capture contained
  **7,067 input reports**, **zero connection reports**, and **zero new Steam
  connection-state events**. Input report counts are not reconnection counts.
- All seven host regression scenarios passed: endpoint busy, USB remount,
  RF drop, bond rebind, multiple slots, controller power-off, and USB suspend.
- The `0.9.60` ARM firmware build passed. Its UF2 headers, block numbering,
  nRF52840 family ID, application address range, and embedded version were
  checked.

Hardware observations used the `0.9.59-connect-fix` build containing this fix.
The `0.9.60` UF2 uses the same firmware change, with the new version label and
clean source identity; it was built and file-validated but was not flashed
during that verification session. The notification popup itself was not
directly observed during the automated checks.

These results cover repeated connection reports in the tested configuration.
They do not establish that cable faults, power problems, or all other causes
of disconnection are resolved. Other host systems, controller modes, boards,
and extended-duration use were not hardware-tested for this modification.

## Firmware source and artifact identity

The locally built `OpenPuck-0.9.60.uf2` comes from this exact firmware commit:

```text
c56533491e58e532621d525e2871babfd2e866bc
```

Its generated version metadata is `0.9.60`, git hash `c5653349`, and dirty flag
`0`. The local artifact is at
`build/openpuck-0.9.60/OpenPuck-0.9.60.uf2`.

| Property | Value |
| --- | --- |
| Size | 395,776 bytes |
| UF2 blocks | 773 |
| UF2 family | `0xADA52840` (nRF52840) |
| Application start | `0x26000` |
| Application end limit, exclusive | `0xED000` |

SHA-256 of that artifact:

```text
FAF2C3B6EDB8836984422BC353475D5D4114536FC4F535BF332F90E39F3BEB35
```

This checksum identifies the existing artifact. A new build may have a
different checksum because of toolchain or build-environment differences.

## Build from the corresponding source

Use Git, Python 3, **Arduino CLI 1.5.1**, and **Adafruit nRF52 core 1.7.0**.
The verified build used the core's bundled ARM GCC 9-2019q4 toolchain and the
`adafruit:nrf52:feather52840` profile. See the existing
[build setup guide](./BUILD_AND_DEPLOY.md) for installing Arduino CLI and the
DFU packaging tools.

Pin the board core when following that guide:

```text
arduino-cli core install adafruit:nrf52@1.7.0 --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
```

From a checkout containing the firmware commit above, create a separate clean
worktree. This keeps local edits out of the firmware source identity:

```text
git worktree add --detach build/source-0.9.60 c56533491e58e532621d525e2871babfd2e866bc
cd build/source-0.9.60
```

### macOS / Linux, with GNU Make and a POSIX shell

Ensure `arduino-cli` and Python are on `PATH`, then run in that worktree:

```bash
OPK_BUILD_VERSION=0.9.60 ./gen_version.sh
make uf2 UF2_BUILD_PATH=build/cache/openpuck-060 UF2_OUTPUT_DIR=build/openpuck-0.9.60
mv build/openpuck-0.9.60/OpenPuck.ino.uf2 build/openpuck-0.9.60/OpenPuck-0.9.60.uf2
```

The build and conversion commands follow the existing Makefile and scripts.
This shell route was not separately executed on macOS or Linux for this
modification.

### Windows PowerShell

Ensure Git Bash's `bash`, Git, `arduino-cli`, and Python 3 (`python`) are on
`PATH`. Run in the clean worktree:

```powershell
$env:OPK_BUILD_VERSION = '0.9.60'
bash ./gen_version.sh
arduino-cli compile `
  -b adafruit:nrf52:feather52840 `
  --build-path build/cache/openpuck-060 `
  --output-dir build/openpuck-0.9.60 `
  --build-property 'build.extra_flags=-DNRF52840_XXAA {build.flags.usb} -DCFG_TUD_HID=6 -DCFG_TUD_TASK_QUEUE_SZ=512 -DCFG_TUD_VENDOR_TX_BUFSIZE=256' `
  --build-property 'compiler.c.elf.extra_flags=-Wl,--wrap=tud_vendor_control_xfer_cb' `
  OpenPuck
$opkDataDir = (arduino-cli config get directories.data).Trim()
$opkConverter = Join-Path $opkDataDir 'packages/adafruit/hardware/nrf52/1.7.0/tools/uf2conv/uf2conv.py'
python $opkConverter -f 0xADA52840 -c `
  -o build/openpuck-0.9.60/OpenPuck-0.9.60.uf2 `
  build/openpuck-0.9.60/OpenPuck.ino.hex
Get-FileHash build/openpuck-0.9.60/OpenPuck-0.9.60.uf2 -Algorithm SHA256
```

These are the compile flags and conversion procedure used for the verified
Windows build, with the local tool and configuration paths replaced by
standard `PATH` and Arduino data-directory lookup. If using a custom Arduino
CLI configuration, supply the same `--config-file` to both `compile` and
`config get`.

The generated `OpenPuck/git_version.h` is ignored and must not be committed.
The version-generation output should show `version=0.9.60`, `hash=c5653349`,
and `dirty=0` for this artifact's source checkout.

### Run the regression scenarios

With Python 3 and a C++11 compiler on `PATH`, run from the checkout root:

```text
python tools/test-puck-connection.py --cxx clang++
```

Alternatively, use the runner's Zig option:

```text
python tools/test-puck-connection.py --cxx /path/to/zig --zig
```

The tests require no controller or dongle. They verify the connection logic
with mocked hardware; the hardware observations above are separate evidence.

## License and firmware distribution

This modified work is provided under the **GNU Affero General Public License,
version 3 (AGPLv3)**. Preserve the original [LICENSE](../LICENSE), copyright,
license, and warranty notices in the sources. Upstream authors retain credit
for their work; the modification notice above identifies the local changes
and their date. The license includes the applicable no-warranty terms.

When publishing the UF2, provide its machine-readable Corresponding Source
under AGPLv3 with the required notices. For a download release, offer the UF2
and its corresponding source with equivalent access from the same place,
and clearly identify the firmware commit and these build instructions.
Include the repository's build scripts and source files needed to build the
firmware; identify external toolchain prerequisites as above. A release body
for the existing UF2 should identify **`c56533491e58e532621d525e2871babfd2e866bc`**
as its firmware source and include a source archive of that revision or a
direct link to its downloadable archive alongside the firmware download.

If the firmware is rebuilt from another revision, update the release's
source identity and checksum to match that actual build.

The full license controls distribution. See its provisions for
[source copies](https://www.gnu.org/licenses/agpl-3.0.html#section4),
[modified source](https://www.gnu.org/licenses/agpl-3.0.html#section5), and
[object-code distribution](https://www.gnu.org/licenses/agpl-3.0.html#section6).
