# Anvil

Projet de recherche : faire tourner des applications **UWP** (Universal Windows Platform) sous Linux en étendant la couche de compatibilité Wine.

Wine réimplémente l'API Win32, mais pas la pile WinRT/UWP (activation COM via le Windows Runtime, bac à sable AppContainer, interface XAML, identité de paquet). Anvil étudie les shims, stubs et changements d'architecture minimaux pour amorcer cette pile. C'est un instrument de recherche et une preuve de concept, pas un fork de Wine : le succès se mesure à la capacité d'exécuter de simples binaires UWP jusqu'à leur premier appel WinRT/XAML.

La spécification technique est dans [`RESEARCH.md`](RESEARCH.md).

## Contenu

| Chemin | Rôle |
| --- | --- |
| `RESEARCH.md` | Spécification et notes de recherche |
| `stubs/` | Stubs C : WinRT (`combase`), composition, XAML, stockage, identité de paquet, DXGI, moteur de hooks, tests |
| `tools/anvil_msix.py` | Lecture de paquets MSIX |
| `anvil_inspector.py` | Inspection d'un paquet |
| `Dockerfile.anvil`, `setup_anvil_vm.sh`, `bootstrap_standalone.sh`, `run_calculator.sh` | Environnement de test et lancement |

Les paquets tiers analysés (extraction de paquets MSIX, code source d'applications Microsoft) ne sont volontairement pas inclus dans ce dépôt.

## Licence

Code propriétaire, tous droits réservés (voir [`LICENSE`](LICENSE)).