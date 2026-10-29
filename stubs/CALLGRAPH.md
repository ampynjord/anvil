# Calculator WinRT Startup Call Graph
## Static Analysis — Anvil Milestone 1

Source: `microsoft/calculator` @ `main` (2026-05-26 sparse clone)  
Entry point: `Calculator.App` (C#, managed UWP)

---

## Execution Path (Process Start → First Frame)

```
[OS / WinRT Host]
  RoInitialize(RO_INIT_MULTITHREADED)           ← combase.dll
  CoreApplication::Run(IFrameworkViewSource*)   ← Windows.ApplicationModel.Core
    │  The WinRT host resolves the managed App class via the
    │  activation catalog entry in AppxManifest.xml:
    │    EntryPoint = "Calculator.App"
    │  It calls into coreclr/nethost to load the managed assembly,
    │  then reflectively constructs the App class.
    │
    ▼
[Managed .NET / Wine Mono boundary]
  App::App()  ← App.xaml.cs:36
    InitializeComponent()
    │   Loads App.xaml, instantiates the XAML resource dictionaries,
    │   merges XamlControlsResources (Microsoft.UI.Xaml.Controls).
    │   WinRT calls fired here:
    │     ResourceDictionary::ctor               Windows.UI.Xaml
    │     XamlControlsResources::ctor            Microsoft.UI.Xaml.Controls
    │     Application.Resources (DependencyProperty accessor)
    │
    NarratorNotifier::RegisterDependencyProperties()
    │   DependencyProperty::Register(...)        Windows.UI.Xaml
    │
    Application::HighContrastAdjustment.set(None)
    │   ApplicationHighContrastAdjustment        Windows.UI.Xaml
    │
    Application::Suspending += OnSuspending      Windows.UI.Xaml (event)

  ── App.ctor returns ──

  Application::OnLaunched(LaunchActivatedEventArgs)  ← called by WinRT host
    │   args.User.NonRoamableId                  Windows.System.User
    │   Task.Run(...)                             System.Threading (CLR)
    │
    OnAppLaunch(args, args.Arguments, args.PrelaunchActivated)
      │
      args.SplashScreen.Dismissed +=             Windows.ApplicationModel.Activation
      │                                          SplashScreen
      │
      Resources["AppMinWindowWidth"]             Windows.UI.Xaml.ResourceDictionary
      SizeHelper::FromDimensions(w, h)           Windows.Foundation
      │
      ApplicationView::GetForCurrentView()       Windows.UI.ViewManagement  ★ PRIORITY 1
      │   → returns ApplicationView for current CoreWindow
      │
      ApplicationData::Current.LocalSettings     Windows.Storage             ★ PRIORITY 2
      │   → ApplicationDataContainer (key-value store)
      │
      appView::SetPreferredMinSize(size)         Windows.UI.ViewManagement
      appView::TryResizeView(size)               Windows.UI.ViewManagement
      ApplicationView::PreferredLaunchWindowingMode.set(Auto)
      │
      Window::Current.Content                    Windows.UI.Xaml.Window      ★ PRIORITY 1
      │   → CoreWindow is the underlying OS window
      │
      Frame::ctor()                              Windows.UI.Xaml.Controls
      LocalizationService::GetFlowDirection()    (internal, reads ResourceLoader)
      │
      Frame::Navigate(typeof(MainPage), args)    Windows.UI.Xaml.Controls    ★ PRIORITY 1
      │   → Triggers MainPage XAML parse + constructor
      │   → Triggers NavigationView construction (Microsoft.UI.Xaml.Controls)
      │
      Window::Current.Content = rootFrame        Windows.UI.Xaml.Window
      ThemeHelper::InitializeAppTheme()          (internal, reads SettingsStorage)
      │
      Window::Current.Activate()                 Windows.UI.Xaml.Window      ★ PRIORITY 1
        └─ CoreWindow::Activate()                Windows.UI.Core
             └─ First paint / compositor flush

  ── Asynchronous (post-Activate) ──

  SetupJumpListAsync()
    JumpList::LoadCurrentAsync()                 Windows.UI.StartScreen      LOW PRIORITY
    JumpList::SaveAsync()                        Windows.UI.StartScreen      LOW PRIORITY
```

---

## WinRT Namespace Summary (prioritised for Anvil stubs)

| Priority | Namespace | Classes needed | Notes |
|----------|-----------|----------------|-------|
| **P1 — must stub for process survival** | `Windows.UI.Xaml` | `Application`, `Window`, `Frame`, `ResourceDictionary`, `DependencyProperty` | Core XAML host |
| **P1** | `Windows.UI.Core` | `CoreWindow`, `CoreDispatcher` | Window/input loop |
| **P1** | `Windows.ApplicationModel.Core` | `CoreApplication`, `CoreApplicationView` | App lifecycle host |
| **P2 — stub with fake data** | `Windows.UI.ViewManagement` | `ApplicationView`, `ApplicationViewWindowingMode` | Window size/mode |
| **P2** | `Windows.Storage` | `ApplicationData`, `ApplicationDataContainer` | Settings (trivially fakeable with files) |
| **P2** | `Windows.ApplicationModel.Activation` | `LaunchActivatedEventArgs`, `SplashScreen` | Launch args |
| **P3 — no-op stubs acceptable** | `Windows.UI.StartScreen` | `JumpList`, `JumpListItem` | Optional taskbar integration |
| **P3** | `Windows.System` | `User` | NonRoamableId = static GUID is fine |
| **Microsoft.UI.Xaml.Controls** | WinUI 2 NuGet | `XamlControlsResources`, `NavigationView` | Separate DLL, shipped with package |

---

## Key Insight: Managed Activation vs Native COM

Calculator is **pure C# managed UWP**. The activation path differs from native UWP:

```
Native UWP (C++/WinRT):
  RoActivateInstance("Foo.Bar") → COM activation → native vtable

Calculator (C# managed):
  CoreApplication::Run()
    → WinRT .NET projection host (Microsoft.Windows.ApplicationModel.DynamicDependency)
    → CLR / Wine Mono loads Calculator.dll
    → Reflection: Activator.CreateInstance(typeof(Calculator.App))
    → App.ctor() → InitializeComponent()
    → XAML generated code calls WinRT COM factories for each control type
```

**Implication for Anvil:** Wine Mono will handle the managed code loading.
What Anvil must provide are WinRT COM factories for the types that
`InitializeComponent()` tries to instantiate natively:
- `Windows.UI.Xaml.Application` factory
- `Windows.UI.Xaml.ResourceDictionary` factory
- `Microsoft.UI.Xaml.Controls.XamlControlsResources` factory

The XAML BAML (Binary Application Markup Language, compiled from App.xaml)
calls `XamlReader::Load()` internally, which requires `Windows.UI.Xaml.dll`.
This is the **first native barrier** Anvil must cross.

---

## First Crash Point Prediction

Under Wine today, Calculator would crash at:

```
App.ctor()
  → InitializeComponent()        [generated from App.xaml]
    → XamlReader::Load(baml)     [Windows.UI.Xaml.dll not present / no factory]
      → RoGetActivationFactory(
            "Windows.UI.Xaml.Application",
            IID_IApplicationFactory,
            &factory)
        → E_NOTIMPL / crash       ← Anvil's first target
```

The `combase_anvil.c` stub intercepts exactly here.

---

## combase.dll Functions Required (Anvil Layer 3)

```c
// Priority 1 — process cannot start without these
HRESULT RoInitialize(RO_INIT_TYPE initType);
void    RoUninitialize(void);
HRESULT RoGetActivationFactory(HSTRING classId, REFIID iid, void **factory);
HRESULT RoActivateInstance(HSTRING classId, IInspectable **instance);

// Priority 1 — HSTRING is used everywhere in WinRT ABI
HRESULT WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *str);
HRESULT WindowsCreateStringReference(LPCWSTR src, UINT32 len,
                                     HSTRING_HEADER *hdr, HSTRING *str);
void    WindowsDeleteString(HSTRING str);
LPCWSTR WindowsGetStringRawBuffer(HSTRING str, UINT32 *len);
HRESULT WindowsConcatString(HSTRING s1, HSTRING s2, HSTRING *out);
BOOL    WindowsIsStringEmpty(HSTRING str);
INT32   WindowsCompareStringOrdinal(HSTRING s1, HSTRING s2, INT32 *result);

// Priority 2 — needed for error propagation
HRESULT RoGetErrorInfo(IRestrictedErrorInfo **errInfo);
HRESULT RoSetErrorInfo(IRestrictedErrorInfo *errInfo);
```
