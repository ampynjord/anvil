# Anvil

Research project: running **UWP** (Universal Windows Platform) applications on Linux by extending the Wine compatibility layer.

Wine reimplements the Win32 API, but not the WinRT/UWP stack (COM activation through the Windows Runtime, AppContainer sandbox, XAML UI, package identity). Anvil studies the minimal shims, stubs and architectural changes needed to bootstrap that stack. It is a research instrument and proof of concept, not a Wine fork: success is measured by the ability to run simple UWP binaries up to their first WinRT/XAML call.

The technical specification is in [`RESEARCH.md`](RESEARCH.md).

## Contents

| Path | Role |
| --- | --- |
| `RESEARCH.md` | Specification and research notes |
| `stubs/` | C stubs: WinRT (`combase`), composition, XAML, storage, package identity, DXGI, hook engine, tests |
| `tools/anvil_msix.py` | MSIX package reader |
| `anvil_inspector.py` | Package inspection |
| `Dockerfile.anvil`, `setup_anvil_vm.sh`, `bootstrap_standalone.sh`, `run_calculator.sh` | Test environment and launch scripts |

Third-party packages used for analysis (extracted MSIX packages, source code of Microsoft applications) are deliberately not included in this repository.

## License

Proprietary code, all rights reserved (see [`LICENSE`](LICENSE)).