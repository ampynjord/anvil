# Anvil — Technical Research Specification

> **Status:** Pre-PoC / Research  
> **Revision:** 0.1 — 2026-05-26

---

## 1. Vision

**Anvil** is a research project exploring the feasibility of running UWP
(Universal Windows Platform) applications on Linux by extending the Wine
compatibility layer.  Where Wine successfully re-implements the Win32 API
surface, UWP applications sit on a higher, distinct runtime stack that Wine
does not currently address.  Anvil investigates the minimum additional shims,
stubs, and architectural changes required to bootstrap that stack on Linux.

The project is explicitly a **research instrument and PoC**, not a production
fork of Wine.  Success is measured by the ability to execute simple UWP
binaries far enough to reach their first WinRT/XAML call — not by full
application compatibility.

---

## 2. Background — How Wine Handles Win32 PE Binaries

### 2.1 The Win32 Execution Model

A classic Win32 `.exe` is a PE (Portable Executable) binary that:

1. Is loaded by `ntdll.dll` → `kernel32.dll` via the Windows PE loader.
2. Resolves imports from a well-defined set of Win32 DLLs
   (`kernel32`, `user32`, `gdi32`, `ole32`, …).
3. Calls `WinMain` / `wWinMain`, creates an HWND window, and pumps a
   `GetMessage` / `DispatchMessage` loop.
4. Uses GDI/GDI+/D3D for rendering, all of which map cleanly to
   X11/Wayland/Vulkan/OpenGL on Linux.

### 2.2 Wine's Architecture

Wine provides:

| Component | Role |
|-----------|------|
| `wine` loader | Parses PE headers, maps sections, resolves imports |
| `ntdll.dll` (Wine) | NT system-call emulation via Linux `syscall(2)` |
| `kernel32`, `user32`, etc. | Win32 API re-implementations in C |
| `wineserver` | Single-process serialiser for object handles (event, mutex, etc.) |
| `winex11.drv` / `winewayland.drv` | Graphics/input back-ends bridging HWNDs to X11/Wayland |

This pipeline is mature and covers the vast majority of Win32 surface.  It
does **not** cover the WinRT/UWP stack because:

- UWP apps use **COM activation** through the Windows Runtime (`Windows.*`
  namespaces) rather than direct DLL imports.
- They run inside an **AppContainer** sandbox with a restricted token that
  Wine's security model has no concept of.
- Their UI is built on **XAML** (a vector-retained scene graph), not HWND
  windows and GDI calls.
- They rely on the **Windows Package Manager** (`AppxManifest.xml`,
  `winsxs`-style package identity) to locate resources and extensions.

---

## 3. The UWP Execution Model — What is Missing

### 3.1 The WinRT Activation Pipeline

A UWP binary does not simply call `LoadLibrary`.  Instead:

```
App.exe
  └─ calls RoInitialize() / RoActivateInstance("Windows.UI.Xaml.Application")
       └─ COM broker looks up ProgID → CLSID in the package's
          activation catalog (registered in HKCU\...\ActivatableClasses)
            └─ Loads the correct .dll from the package graph
                 └─ Returns an IInspectable* (WinRT interface)
```

Wine has no equivalent of:
- `RoInitialize` / `RoActivateInstance` (combase.dll, winrt/activation)
- The **Activation Catalog** (package-scoped COM class registry)
- **WinMD** (Windows Metadata) resolution — `.winmd` files describe WinRT
  type layouts at runtime; they are the WinRT equivalent of type libraries.

### 3.2 AppContainer Sandboxing

Windows runs UWP processes in an **AppContainer**: a restricted token with
a unique SID per package and a mandatory integrity level of "Low".  The OS
enforces this in the kernel via `NtCreateUserProcess` flags and DCOM object
isolation.

On Wine today `NtCreateUserProcess` creates a plain Linux process.  There
is no sandbox token, no capability-based file namespace isolation (the UWP
equivalent of `/proc/<pid>/fd`), and no broker process to arbitarate
brokered API calls (`Windows.Storage.Pickers`, camera, microphone).

### 3.3 The XAML Rendering Pipeline

Classic Win32 draws via GDI calls → `BitBlt` → X11 `XPutImage`.
Wine's `winex11.drv` handles this well.

XAML uses a **DirectComposition**-style retained scene graph:

```
XAML parser (Windows.UI.Xaml.dll)
  └─ builds a visual tree of UIElement objects
       └─ handed to the DWM (Desktop Window Manager) compositor
            └─ rendered via DirectX 11/12
```

Wine has partial DirectX support (via DXVK/VKD3D) but has **no
DirectComposition implementation** and no stub for
`Windows.UI.Xaml.Hosting` — the low-level compositor interface.

---

## 4. Theoretical Architectural Approach

Anvil's proposed architecture is a layered shim stack on top of Wine:

```
┌──────────────────────────────────────────────────┐
│  UWP Application (.exe + AppxManifest.xml)        │
├──────────────────────────────────────────────────┤
│  [LAYER 4] Anvil XAML Bridge                      │
│  Maps XAML visual tree → Win32 GDI / Cairo calls  │
│  or Wayland surface via xdg-shell                 │
├──────────────────────────────────────────────────┤
│  [LAYER 3] Anvil WinRT Stubs                      │
│  RoActivateInstance, WinMD loader, IInspectable   │
│  COM activation catalog from AppxManifest.xml     │
├──────────────────────────────────────────────────┤
│  [LAYER 2] Anvil AppContainer Shim                │
│  Fake AppContainer token / SID                    │
│  Brokered API stubs (Storage, Camera, …)          │
├──────────────────────────────────────────────────┤
│  [LAYER 1] Wine (existing)                        │
│  PE loader, ntdll, kernel32, user32, D3D, …       │
├──────────────────────────────────────────────────┤
│  Linux kernel + Wayland / X11 / Vulkan            │
└──────────────────────────────────────────────────┘
```

### 4.1 Layer 2 — AppContainer Shim

**Goal:** prevent the application from crashing during token/capability
checks without implementing real sandboxing.

- Implement `NtCreateUserProcess` override in `ntdll` to stamp a fake
  AppContainer SID onto the process token.
- Stub `CheckTokenCapability`, `RtlGetTokenCapabilities`,
  `LdrQueryPackageIdentity` to return plausible values derived from
  `AppxManifest.xml`.
- Implement a **package identity** resolver: map the package family name to a
  directory on the Linux filesystem where extracted package files live
  (Anvil's own "install root").

### 4.2 Layer 3 — WinRT Stub Library (`anvil_winrt.dll`)

**Goal:** provide the minimum COM/WinRT activation surface to let the app
reach its first `Application::Start` call.

1. **WinMD resolver** — scan the package directory for `.winmd` files and
   build an in-memory type map (GUID → vtable layout).  WinMD files are
   standard ECMA-335 metadata assemblies; existing .NET metadata parsers
   (e.g. `mono/metadata`, `dnlib`) can read them.
2. **Activation factory registry** — parse `AppxManifest.xml`
   `<Extension Category="windows.activatableClass">` to populate a CLSID →
   DLL path map.  This replaces the Windows Activation Catalog registry keys.
3. **IInspectable / IUnknown base** — thin C++ COM base matching the WinRT
   ABI (reference count, QueryInterface, GetIids, GetRuntimeClassName,
   GetTrustLevel).
4. **Prioritised stubs** for the most-called system classes:
   - `Windows.UI.Xaml.Application`
   - `Windows.UI.Core.CoreWindow`
   - `Windows.UI.Core.CoreDispatcher`
   - `Windows.ApplicationModel.Core.CoreApplication`
   - `Windows.UI.ViewManagement.ApplicationView`

### 4.3 Layer 4 — XAML Bridge (`anvil_xaml.dll`)

**Goal:** intercept XAML visual tree construction and redirect rendering to
a Linux-native display path.

Two candidate strategies:

| Strategy | Pros | Cons |
|----------|------|------|
| **A — HWND fallback** | Re-uses Wine's existing `winex11.drv` / `winewayland.drv` | Limited layout fidelity; no GPU compositing |
| **B — Native Wayland surface** | Full GPU path via Vulkan; correct DPI | Requires a Wayland compositor; more complex |

For the initial PoC, **Strategy A** is preferred: map each top-level XAML
`Window` to a Wine HWND, flatten the visual tree to a GDI `HDC` via a
simple software rasteriser (e.g. Cairo or Skia), and paint the result.

The bridge intercepts:
- `Windows.UI.Xaml.Application::Start` — entry-point hook.
- `Windows.UI.Core.CoreWindow::Activate` — triggers HWND creation.
- `Windows.UI.Xaml.Controls.Panel` layout pass — measures/arranges children.
- `Windows.UI.Xaml.Media.DrawingContext` / `Compositor` — redirects draw
  calls to a Cairo surface.

---

## 5. Primary Hurdles

### 5.1 Windows.Management and Windows.ApplicationModel Namespaces

These two namespaces are tightly coupled to Windows-specific kernel and
broker infrastructure:

| Class / API | What it does | Linux obstacle |
|-------------|-------------|----------------|
| `Windows.ApplicationModel.Package` | Queries the package identity (name, version, install path) from the AppX deployment store | No deployment store; must fake responses from `AppxManifest.xml` |
| `Windows.ApplicationModel.Core.CoreApplication` | Manages the UWP app lifecycle (Suspending, Resuming, EnteredBackground events) | These events are fired by the PLM (Process Lifetime Manager) in `svchost`; Wine has no PLM |
| `Windows.Management.Deployment.PackageManager` | Queries installed packages, registers background tasks | Requires a package database (WinRT Object Store) that does not exist under Wine |
| `Windows.Storage.ApplicationData` | Per-package isolated storage (`LocalFolder`, `RoamingFolder`) | Trivially shimmed with a per-package directory under `~/.local/share/anvil/<PackageId>/` |
| `Windows.Security.Authentication.*` | Web Auth Broker, MSA, AAD tokens | Requires HTTPS brokering through a privileged process; lowest priority |

**Mitigation strategy:** implement read-only stubs for `Package` and
`CoreApplication` that return values parsed from `AppxManifest.xml`.  Defer
`PackageManager` entirely.  Stub the PLM lifecycle events as no-ops in the
first PoC.

### 5.2 WinRT Threading Model

UWP assumes a **Single-Threaded Apartment (STA) per UI thread** with
`CoreDispatcher` marshalling cross-thread calls.  Wine's COM STA support
exists but is incomplete for WinRT's `DispatcherQueue` extensions.  Anvil
must carefully sequence:

1. `RoInitialize(RO_INIT_MULTITHREADED)` on the main thread.
2. `CoreApplication::Run` which internally calls `CoreWindow::Dispatcher`.
3. Background thread activation factories must marshal back via
   `CoreDispatcher::RunAsync`.

### 5.3 XAML Telemetry and ETW

`Windows.UI.Xaml.dll` emits extensive ETW (Event Tracing for Windows)
traces.  Under Wine, `EtwRegisterTraceGuids` is partially stubbed but ETW
provider registration can crash if the internal provider table is
inconsistent.  Anvil must ensure ETW registration no-ops cleanly without
AV/access violations.

### 5.4 Resource Index (.pri files)

Package resources (strings, images, assets) are stored in a binary
**Package Resource Index** (`.pri` file).  Resolving localised strings
goes through `Windows.ApplicationModel.Resources.ResourceLoader`, which
reads `.pri` using an undocumented binary format.  Microsoft has released
the `makepri.exe` tool but not a cross-platform parser.  Anvil should use
or port the open-source `PriParser` project
(see § 7 — References) for basic string/asset resolution.

---

## 6. Sample UWP Apps for Integration Testing

The following open-source, unencrypted UWP applications are recommended as
Anvil's first integration targets, ordered by complexity:

### 6.1 Windows Calculator (microsoft/calculator)

- **Repository:** `https://github.com/microsoft/calculator`
- **Package type:** UWP XAML (WinUI 2 / Windows.UI.Xaml)
- **Entry point:** `Calculator.exe` / managed entry point `App`
- **Why it is a good target:** Ships an `AppxManifest.xml`; uses standard
  XAML controls (Grid, TextBlock, Button); has no external service
  dependencies; well-studied codebase.
- **Dependencies of interest:** `Microsoft.UI.Xaml` (WinUI 2), `calcengine`
  (pure C++ lib — no WinRT).

### 6.2 Fluent XAML Theme Editor (microsoft/fluent-xaml-theme-editor)

- **Repository:** `https://github.com/microsoft/fluent-xaml-theme-editor`
- **Package type:** UWP XAML (WinUI 2)
- **Why it is a good target:** Exercises a wider range of XAML controls
  (NavigationView, ColorPicker, DataGrid) making it a stress-test for the
  XAML bridge's control coverage.  Still no external network dependencies in
  the core UI path.

### 6.3 XAML Controls Gallery (microsoft/Xaml-Controls-Gallery)

- **Repository:** `https://github.com/microsoft/Xaml-Controls-Gallery`
- **Package type:** UWP / WinUI 3 variant available
- **Why it is a good target:** The canonical Microsoft showcase app for every
  WinUI control type.  Running even a subset of its pages would validate broad
  XAML surface coverage.  The WinUI 2 (`release/winui2`) branch targets
  `Windows.UI.Xaml` exclusively, making it the better starting point before
  tackling WinUI 3.

---

## 7. References and Further Reading

- **Wine source tree:** `dlls/combase/` — WinRT activation stubs (partial)
- **ReactOS:** `sdk/lib/rtl/` — AppContainer SID helpers
- **ECMA-335 CLI Metadata Spec** — required to parse `.winmd` files
- **`winmd` Rust crate** — cross-platform WinMD reader
- **`PriParser`** — open-source `.pri` file reader (C# / MIT)
- **`Windows.UI.Xaml` internals blog series** — Raymond Chen / MSDN Archive
- **DirectComposition DDI documentation** — for eventual GPU path in Layer 4

---

## 8. Immediate Next Steps (Milestone 1)

- [ ] Run `anvil_inspector.py` against Calculator `.msix` to produce a
      baseline manifest report.
- [ ] Identify which WinMD files ship with Calculator and list the WinRT
      interfaces it calls at startup (use API Monitor or WPA on Windows first).
- [ ] Prototype a minimal `combase.dll` override in Wine that intercepts
      `RoActivateInstance("Windows.ApplicationModel.Core.CoreApplication")`.
- [ ] Verify that the Calculator process reaches `WinMain` equivalent under
      Wine before any WinRT call is made.
