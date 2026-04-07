# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (first time or after CMakeLists.txt changes)
/Users/mikegarcia/Qt/6.10.1/macos/bin/qt-cmake -S . -B build -G Ninja

# Build
ninja -C build

# Clean rebuild
rm -rf build && /Users/mikegarcia/Qt/6.10.1/macos/bin/qt-cmake -S . -B build -G Ninja && ninja -C build

# Launch
open build/QK-LP100A.app
```

The build automatically ad-hoc code signs the app bundle (POST_BUILD step) for macOS network access.

## Dependencies

- **Qt 6.10.1** installed at `~/Qt/6.10.1/macos/` (Widgets, SerialPort, Network, Charts)
- **Hamlib** via Homebrew at `/opt/homebrew/` (headers in `include/hamlib/`, lib `libhamlib.dylib`)
- **Ninja** build system (`brew install ninja`)
- **macOS 26.0+** deployment target (Apple Silicon M1+)
- C++17 standard

## Architecture

Two-mode application for the TelePost LP-100A Digital Vector RF Wattmeter:

**VCP mode** (main window) — real-time power/SWR/impedance monitoring via serial or TCP to the LP-100A.

**Plot mode** (separate window, File > Open Plot Window) — automated frequency sweeps using Hamlib to control any supported rig. Steps through frequencies, keys PTT, reads LP-100A measurements, charts results.

### Module Layout

- `src/core/` — Transport abstraction (`ITransport` → `SerialTransport`, `TcpTransport`), LP-100A protocol parser (`LP100AProtocol`), Hamlib rig wrapper (`HamlibRig`)
- `src/vcp/` — Main VCP window, custom gauge widgets (`PowerGauge`, `SWRGauge`), connection dialog
- `src/plot/` — Plot window (`PlotWidget`), sweep state machine (`SweepEngine`), data container (`SweepData`)
- `src/Style.h` — **All** UI constants (colors, fonts, layout dimensions, protocol defaults). No magic numbers elsewhere.

### LP-100A Serial Protocol

115200 baud, 8N1. Single-character commands: `P` (poll), `A` (cycle alarm), `F` (toggle Peak/Avg/Tune). Poll response format:
```
;power,Z,phase,alarmPt,callsign,powerRange,peakMode,dBm,SWR
```
After sending A or F, wait 50ms (`Style::Protocol::CommandDelayMs`) before polling — the device needs time to process.

### Key Design Patterns

- **ITransport interface** decouples serial/TCP from protocol logic. Both transports emit `dataReceived()`, `connectionChanged()`, `errorOccurred()`.
- **Style.h** is the single source of truth for every visual value. Button stylesheets are built via `Style::detail::gradient()` helper. Meter colors via `Style::meterGradient()`. All layout dimensions are named constants.
- **SweepEngine** uses `QTimer::singleShot()` chaining (not blocking loops) to keep UI responsive during multi-minute sweeps. State machine: SetFreq → Settle → PTTOn → Sample → ReadData → PTTOff → next.
- **Integer Hz math** in SweepEngine avoids floating-point frequency drift. Convert to MHz only at display time.
- **HamlibRig** wraps the Hamlib C API directly (no external rigctld). Suppresses Hamlib debug output via `rig_set_debug_file(/dev/null)`. Pre-warms macOS network stack at app startup for TCP rig connections.

### Stylesheet Approach

Button stylesheets use `static const QString` (built once per process) and `Style::detail::gradient()` for QK4-style 4-stop vertical gradients. Never call `setStyleSheet()` in a poll loop — only on state changes (tracked via `m_lastAlarmSetPoint`, `m_lastAlarmTripped`).

### Settings Persistence

`QSettings("AI5QK", "QK-LP100A")` stores connection params, view style, rig config, sweep parameters. VCP auto-reconnects on launch if previously connected. Plot window saves/restores all controls on close/open.

## Known Issues

- macOS 26 icon border: dark icons get a system-applied light stroke — needs Apple HIG research
- Hamlib debug output: `rig_set_debug(RIG_DEBUG_NONE)` alone doesn't suppress all output in Hamlib 4.6.5; must also redirect via `rig_set_debug_file()`
- First TCP rig connect may fail if macOS firewall hasn't cleared the process yet; startup pre-warm mitigates this
