// Copyright (c) 2026 Skitzelles - part of Game Vault, https://github.com/skitzellez/xbox-game-vault
// PolyForm Noncommercial License 1.0.0: not for sale, and this notice stays in every copy.
//
// Game Vault · Web: a web browser for the Xbox in the console's WebView2, played with the controller. It opens
// gamevault-web:?url=<http or https address>; started on its own it opens Game Vault's WEB menu. The controller is read here and sent as real key and mouse
// events (DevTools Input); pages also see it as a standard gamepad (kInject). View switches pointer and game controls.
// View + Menu held goes back to Game Vault.

#include <windows.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Microsoft.Web.WebView2.Core.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <delayimp.h>
#include <string>

using namespace winrt;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::Gaming::Input;
using namespace Windows::Storage;
using namespace Windows::UI::Core;
using namespace Microsoft::Web::WebView2::Core;

namespace {


// In every frame before the page's own scripts: the pointer and notices (top frame), and the controller as a gamepad
// (navigator.getGamepads), passed down to frames because the app can only reach the top one.
const wchar_t kInject[] = LR"JS((function () {
  if (window.__gvWeb) return;
  // verification frames run untouched: changing the browser's built-ins inside them makes the check fail
  if (location.hostname === "challenges.cloudflare.com") return;
  window.__gvWeb = true;
  const isTop = window === window.top;
  const host = window.chrome && window.chrome.webview;
  const mk = () => ({ pressed: false, touched: false, value: 0 });
  const pad = { id: "Xbox Controller (STANDARD GAMEPAD)", index: 0, connected: false, mapping: "standard", timestamp: 0,
                axes: [0, 0, 0, 0], buttons: Array.from({ length: 17 }, mk), vibrationActuator: null };
  let told = false;
  function apply(p) {
    pad.axes = p.a;
    p.v.forEach((v, i) => { const b = pad.buttons[i]; b.value = v; b.pressed = b.touched = v > 0.3; });
    pad.timestamp = performance.now();
    if (!pad.connected) {
      pad.connected = true;
      const e = new Event("gamepadconnected");
      e.gamepad = pad;
      window.dispatchEvent(e);
    }
    for (let i = 0; i < window.frames.length; i++) {
      try { window.frames[i].postMessage({ gvPad: p }, "*"); } catch (e) { }
    }
  }
  // The console's pointer lock cannot be relied on, and a stick is not a mouse: pages see a lock of our own, and the
  // right stick arrives as ordinary mouse movement on the locked element.
  let locked = null;
  const lockEvent = () => {
    document.dispatchEvent(new Event("pointerlockchange"));
    if (isTop && host) host.postMessage(locked ? "gv-lock-on" : "gv-lock-off");
  };
  Object.defineProperty(Document.prototype, "pointerLockElement", { configurable: true, get: () => locked });
  Element.prototype.requestPointerLock = function () { locked = this; lockEvent(); return Promise.resolve(); };
  Document.prototype.exitPointerLock = function () { locked = null; lockEvent(); };
  function look(dx, dy) {
    if (!locked) {
      const c = document.querySelector("canvas");
      if (!c) return;
      c.requestPointerLock();
    }
    locked.dispatchEvent(new MouseEvent("mousemove", { bubbles: true, cancelable: true, movementX: dx, movementY: dy,
                                                       clientX: innerWidth / 2, clientY: innerHeight / 2 }));
  }
  Object.defineProperty(Navigator.prototype, "getGamepads", { configurable: true, value: function () {
    if (!told && isTop && host) { told = true; host.postMessage("page reads gamepads"); }
    return pad.connected ? [pad, null, null, null] : [null, null, null, null];
  } });
  window.addEventListener("message", e => { if (e.data && e.data.gvPad) apply(e.data.gvPad); });
  if (!isTop || !host) return;
  window.addEventListener("error", e => host.postMessage("error: " + e.message));
  window.addEventListener("unhandledrejection", e => host.postMessage("rejected: " + e.reason));
  let cursor = null, toast = null, shownAt = 0, toastTimer = 0, looks = 0, frames = 0, said = 0;
  // what the page reports goes to the log (a few), and every 10 s whether it is still drawing
  const error = console.error.bind(console);
  console.error = (...a) => { error(...a); if (said++ < 40) host.postMessage("console: " + a.join(" ").slice(0, 250)); };
  const tick = () => { frames++; requestAnimationFrame(tick); };
  requestAnimationFrame(tick);
  setInterval(() => {
    host.postMessage("alive: frames " + frames + " looks " + looks + " focus " + document.hasFocus() + " " +
                     document.visibilityState + " lock " + !!locked);
    frames = 0; looks = 0;
  }, 10000);
  function layer(css) {
    const d = document.createElement("div");
    d.style.cssText = "position:fixed;z-index:2147483647;pointer-events:none;" + css;
    document.documentElement.appendChild(d);
    return d;
  }
  host.addEventListener("message", e => {
    const m = e.data;
    if (m.p) { apply(m.p); return; }
    if (m.look) { look(m.look[0], m.look[1]); looks++; return; }
    if (typeof m.cx === "number") {
      if (!cursor || !cursor.isConnected) {
        cursor = layer("left:0;top:0;width:0;height:0;border-left:13px solid #fff;border-top:13px solid #fff;" +
                       "border-right:13px solid transparent;border-bottom:13px solid transparent;" +
                       "filter:drop-shadow(0 0 2px #000)");
      }
      cursor.style.display = "block";
      cursor.style.transform = "translate(" + m.cx + "px," + m.cy + "px)";
      shownAt = performance.now();
    } else if (m.hide && cursor) {
      cursor.style.display = "none";
    } else if (m.toast) {
      if (!toast || !toast.isConnected) {
        toast = layer("left:50%;bottom:48px;transform:translateX(-50%);padding:14px 26px;border-radius:10px;" +
                      "background:rgba(0,0,0,.85);color:#fff;font:24px sans-serif;white-space:nowrap");
      }
      toast.textContent = m.toast;
      toast.style.display = "block";
      clearTimeout(toastTimer);
      toastTimer = setTimeout(() => { toast.style.display = "none"; }, 5000);
    }
  });
  setInterval(() => {
    if (cursor && cursor.style.display === "block" && performance.now() - shownAt > 6000) cursor.style.display = "none";
  }, 1000);
})();)JS";

std::wstring g_logPath;

void Log(const std::wstring& line)
{
    static bool fresh = true;
    static size_t total = 0;
    if (total > 256 * 1024) return;   // pages can say a lot: the log stays small
    total += line.size();
    if (g_logPath.empty()) g_logPath = std::wstring(ApplicationData::Current().LocalFolder().Path()) + L"\\web.log";
    FILE* f = _wfopen(g_logPath.c_str(), fresh ? L"wb" : L"ab");
    fresh = false;
    if (!f) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::string utf8 = to_string(line);
    fprintf(f, "%02d:%02d:%02d.%03d %s\r\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, utf8.c_str());
    fclose(f);
}

// the site part of a web address: game.example.com and example.com count as one site (last two names of the host)
std::wstring SiteOf(const std::wstring& url)
{
    size_t start = url.find(L"://");
    if (start == std::wstring::npos) return L"";
    start += 3;
    size_t end = url.find_first_of(L"/?#:", start);
    std::wstring host = url.substr(start, end == std::wstring::npos ? end : end - start);
    std::transform(host.begin(), host.end(), host.begin(), ::towlower);
    size_t last = host.rfind(L'.');
    if (last == std::wstring::npos || last == 0) return host;
    size_t prev = host.rfind(L'.', last - 1);
    return prev == std::wstring::npos ? host : host.substr(prev + 1);
}

// LocalState\web_test.txt, words: noinject nocdp softgpu (browser tests, no file = normal)
bool TestFlag(const char* name)
{
    static std::string flags = [] {
        std::string s;
        std::wstring p = std::wstring(ApplicationData::Current().LocalFolder().Path()) + L"\\web_test.txt";
        if (FILE* f = _wfopen(p.c_str(), L"rb")) {
            char b[256];
            size_t n = fread(b, 1, sizeof(b), f);
            s.assign(b, n);
            fclose(f);
        }
        return s;
    }();
    return flags.find(name) != std::string::npos;
}

// only web addresses: a page must not start other apps through their link types (ms-*:, gamevault:, file:)
bool IsWeb(const std::wstring& u)
{
    return _wcsnicmp(u.c_str(), L"https://", 8) == 0 || _wcsnicmp(u.c_str(), L"http://", 7) == 0 ||
           _wcsicmp(u.c_str(), L"about:blank") == 0;
}

struct Key { uint32_t bit; const wchar_t* key; const wchar_t* code; int vk; const wchar_t* text; };

// the controller's own bits: Menu 1, View 2, A 4, B 8, X 16, Y 32, D-pad 64/128/256/512, LB 1024, RB 2048
// pointer controls: D-pad = arrows, Menu = Enter
const Key kBrowse[] = {
    {64, L"ArrowUp", L"ArrowUp", 38, nullptr},      {128, L"ArrowDown", L"ArrowDown", 40, nullptr},
    {256, L"ArrowLeft", L"ArrowLeft", 37, nullptr}, {512, L"ArrowRight", L"ArrowRight", 39, nullptr},
    {1, L"Enter", L"Enter", 13, L"\\r"},
};
// game controls on any site, as the Flash app: stick/D-pad = arrows, A Space, B X, X Z, Y C, Menu Enter, LB Shift, RB Ctrl
const Key kGame[] = {
    {64, L"ArrowUp", L"ArrowUp", 38, nullptr},      {128, L"ArrowDown", L"ArrowDown", 40, nullptr},
    {256, L"ArrowLeft", L"ArrowLeft", 37, nullptr}, {512, L"ArrowRight", L"ArrowRight", 39, nullptr},
    {4, L" ", L"Space", 32, L" "},                  {8, L"x", L"KeyX", 88, L"x"},
    {16, L"z", L"KeyZ", 90, L"z"},                  {32, L"c", L"KeyC", 67, L"c"},
    {1, L"Enter", L"Enter", 13, L"\\r"},            {1024, L"Shift", L"ShiftLeft", 16, nullptr},
    {2048, L"Control", L"ControlLeft", 17, nullptr},
};
// Duke (this build's default keys, source/duke3d/src/_functio.h keydefaults): bits made in DukeBits()
const Key kDuke[] = {
    {1u << 0, L"w", L"KeyW", 87, L"w"},                  {1u << 1, L"s", L"KeyS", 83, L"s"},
    {1u << 2, L"a", L"KeyA", 65, L"a"},                  {1u << 3, L"d", L"KeyD", 68, L"d"},
    {1u << 4, L"ArrowUp", L"ArrowUp", 38, nullptr},      {1u << 5, L"ArrowDown", L"ArrowDown", 40, nullptr},
    {1u << 6, L"ArrowLeft", L"ArrowLeft", 37, nullptr},  {1u << 7, L"ArrowRight", L"ArrowRight", 39, nullptr},
    {1u << 8, L"Control", L"ControlRight", 17, nullptr}, {1u << 9, L"Shift", L"ShiftLeft", 16, nullptr},
    {1u << 10, L" ", L"Space", 32, L" "},                {1u << 11, L"Control", L"ControlLeft", 17, nullptr},
    {1u << 12, L"e", L"KeyE", 69, L"e"},                 {1u << 13, L"Enter", L"Enter", 13, L"\\r"},
    {1u << 14, L"Escape", L"Escape", 27, nullptr},       {1u << 15, L";", L"Semicolon", 186, L";"},
    {1u << 16, L"'", L"Quote", 222, L"'"},
};

template <size_t N> constexpr std::pair<const Key*, size_t> Table(const Key (&t)[N]) { return { t, N }; }

struct App : implements<App, IFrameworkViewSource, IFrameworkView> {
    CoreWindow m_window{ nullptr };
    CoreWebView2Environment m_env{ nullptr };
    CoreWebView2Controller m_ctl{ nullptr };
    // composition hosting: the windowed kind makes a child window, which the Xbox never shows (Flash 0.1.0.4)
    Windows::UI::Composition::Compositor m_comp{ nullptr };
    Windows::UI::Composition::CompositionTarget m_target{ nullptr };
    Windows::UI::Composition::ContainerVisual m_root{ nullptr };
    CoreWebView2 m_web{ nullptr };
    std::wstring m_url;   // empty: started from the Home screen, not from Game Vault
    bool m_browse = false;   // opened from Game Vault's WEB menu (&browse=1): links the player presses may go to any site
    bool m_started = false, m_quitting = false;
    bool m_gameMode = false, m_duke = false, m_pageLocked = false;
    bool m_viewWas = false, m_viewUsed = false, m_click = false, m_firstKey = true;
    ULONGLONG m_comboSince = 0, m_scrollAt = 0;
    uint32_t m_down = 0, m_btnWas = 0;
    std::pair<const Key*, size_t> m_keys = Table(kBrowse);
    float m_cx = -1, m_cy = -1;
    std::wstring m_lastPad;

    IFrameworkView CreateView() { return *this; }
    void Initialize(CoreApplicationView const& view) { view.Activated({ this, &App::OnActivated }); }
    void Load(hstring const&) {}
    void Uninitialize() {}

    void SetWindow(CoreWindow const& window)
    {
        m_window = window;
        Windows::UI::ViewManagement::ApplicationViewScaling::TrySetDisableLayoutScaling(true);
        Windows::UI::ViewManagement::ApplicationView::GetForCurrentView().SetDesiredBoundsMode(
            Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);
        window.SizeChanged([this](auto&&, auto&&) { Resize(); });
        window.Activated([this](auto&&, WindowActivatedEventArgs const& a) {
            bool on = a.WindowActivationState() != CoreWindowActivationState::Deactivated;
            Log(on ? L"window active" : L"window NOT active");
            if (on && m_ctl) m_ctl.MoveFocus(CoreWebView2MoveFocusReason::Programmatic);
        });
        window.VisibilityChanged([this](auto&&, VisibilityChangedEventArgs const& a) {
            Log(a.Visible() ? L"window visible" : L"window hidden");
            if (m_ctl) m_ctl.IsVisible(a.Visible());
        });
        // B is ours, not the system's "back"
        SystemNavigationManager::GetForCurrentView().BackRequested(
            [](auto&&, BackRequestedEventArgs const& a) { a.Handled(true); });
    }

    void OnActivated(CoreApplicationView const&, IActivatedEventArgs const& args)
    {
        if (args.Kind() == ActivationKind::Protocol) {
            Uri uri = args.as<ProtocolActivatedEventArgs>().Uri();
            Log(L"started by " + std::wstring(uri.AbsoluteUri()));
            try {
                std::wstring asked(uri.QueryParsed().GetFirstValueByName(L"url"));
                if (IsWeb(asked)) m_url = asked;
                try {
                    m_browse = std::wstring(uri.QueryParsed().GetFirstValueByName(L"browse")) == L"1";
                } catch (...) {
                    m_browse = false;
                }
                Log(m_browse ? L"browse mode: links may leave the site" : L"game mode: pages stay on their site");
            } catch (...) {
            }
        }
        if (m_url.empty()) {   // no site asked for: Game Vault's WEB menu is where sites are picked
            Log(L"started without a site: opening Game Vault's WEB menu");
            Windows::System::Launcher::LaunchUriAsync(Uri(L"gamevault:?from=webmenu"));
            CoreApplication::Exit();
            return;
        }
        m_window.Activate();
        if (m_started) {
            if (m_web) m_web.Navigate(m_url);
            return;
        }
        m_started = true;
        StartWebView();
    }

    void Resize()
    {
        if (!m_ctl) return;
        Rect b = m_window.Bounds();
        m_ctl.Bounds(Rect{ 0, 0, b.Width, b.Height });
    }

    fire_and_forget StartWebView()
    {
        auto lifetime = get_strong();
        try {
            CoreWebView2EnvironmentOptions opts;
            // web games start their sound without a click (the controller never counts as one for some pages)
            opts.AdditionalBrowserArguments(TestFlag("softgpu") ? L"--autoplay-policy=no-user-gesture-required --disable-gpu"
                                                                : L"--autoplay-policy=no-user-gesture-required");
            Log(std::wstring(L"test flags: ") + (TestFlag("noinject") ? L"noinject " : L"") + (TestFlag("nocdp") ? L"nocdp " : L"") +
                (TestFlag("softgpu") ? L"softgpu" : L""));
            m_env = co_await CoreWebView2Environment::CreateWithOptionsAsync(L"", L"", opts);
            Log(L"WebView2 " + std::wstring(m_env.BrowserVersionString()));
            auto ref = CoreWebView2ControllerWindowReference::CreateFromCoreWindow(m_window);
            m_comp = Windows::UI::Composition::Compositor();
            m_target = m_comp.CreateTargetForCurrentView();
            m_root = m_comp.CreateContainerVisual();
            m_root.RelativeSizeAdjustment({ 1.0f, 1.0f });
            m_target.Root(m_root);
            auto cc = co_await m_env.CreateCoreWebView2CompositionControllerAsync(ref);
            cc.RootVisualTarget(m_root);
            m_ctl = cc;
            Resize();
            m_ctl.IsVisible(true);
            m_web = m_ctl.CoreWebView2();
            auto settings = m_web.Settings();
            settings.AreDefaultContextMenusEnabled(false);
            settings.IsStatusBarEnabled(false);
            settings.IsZoomControlEnabled(false);
            settings.AreBrowserAcceleratorKeysEnabled(false);
            settings.AreDefaultScriptDialogsEnabled(false);   // the Xbox shows no browser dialogs: they would hang pages
            m_web.ScriptDialogOpening([](auto&&, CoreWebView2ScriptDialogOpeningEventArgs const& a) {
                Log(L"dialog answered yes: " + std::wstring(a.Message()).substr(0, 200));
                a.Accept();
            });
            m_web.PermissionRequested([](auto&&, CoreWebView2PermissionRequestedEventArgs const& a) {
                Log(L"permission refused: " + std::to_wstring((int)a.PermissionKind()));
                a.State(CoreWebView2PermissionState::Deny);
            });
            m_web.DownloadStarting([this](auto&&, CoreWebView2DownloadStartingEventArgs const& a) {
                Log(L"download refused: " + std::wstring(a.DownloadOperation().Uri()).substr(0, 300));
                a.Cancel(true);
                Toast(L"Downloads are off in this browser");
            });
            m_web.NavigationStarting([this](auto&&, CoreWebView2NavigationStartingEventArgs const& a) { OnNavigate(a); });
            m_web.FrameNavigationStarting([](auto&&, CoreWebView2NavigationStartingEventArgs const& a) {
                if (!IsWeb(std::wstring(a.Uri())) && _wcsnicmp(a.Uri().c_str(), L"blob:", 5) != 0 &&
                    _wcsnicmp(a.Uri().c_str(), L"data:", 5) != 0)
                    a.Cancel(true);
            });
            m_web.NavigationCompleted([this](auto&&, CoreWebView2NavigationCompletedEventArgs const& a) {
                Log(std::wstring(L"page loaded: ") + (a.IsSuccess() ? L"ok " : L"failed ") +
                    std::to_wstring((int)a.WebErrorStatus()) + L" " + std::wstring(m_web.Source()));
                m_ctl.MoveFocus(CoreWebView2MoveFocusReason::Programmatic);
                AnnounceMode();
            });
            m_web.NewWindowRequested([this](auto&&, CoreWebView2NewWindowRequestedEventArgs const& a) {
                // one window on a console: a link the player pressed on the same site opens here. Anything else is
                // refused: ad networks open their pages on any press, and that would replace the game.
                a.Handled(true);
                std::wstring u(a.Uri());
                bool ok = a.IsUserInitiated() && IsWeb(u) && SiteOf(u) == SiteOf(std::wstring(m_web.Source()));
                Log(std::wstring(ok ? L"new window here: " : L"pop-up refused: ") + u.substr(0, 300));
                if (ok) m_web.Navigate(u);
            });
            m_web.ProcessFailed([](auto&&, CoreWebView2ProcessFailedEventArgs const& a) {
                Log(L"browser process failed: " + std::to_wstring((int)a.ProcessFailedKind()));
            });
            m_web.WebMessageReceived([this](auto&&, CoreWebView2WebMessageReceivedEventArgs const& a) {
                try {
                    std::wstring text(a.TryGetWebMessageAsString());
                    if (text == L"gv-lock-on" || text == L"gv-lock-off") {
                        m_pageLocked = text == L"gv-lock-on";
                        return;
                    }
                    Log(L"page: " + text.substr(0, 300));
                } catch (...) {
                }
            });
            // ad networks and the ad files of game sites never load: they replace the game with their own pages
            for (const wchar_t* pattern : { L"*://*.czernik.org/*", L"*://czernik.org/*", L"*://*.disticabing.co.in/*",
                                            L"*://disticabing.co.in/*", L"*://*.navallostiolly.co.in/*", L"*://*.nn125.com/*",
                                            L"*://nn125.com/*", L"*://*.cexisaje.org/*", L"*://cexisaje.org/*",
                                            L"*://*.highperformanceformat.com/*", L"*://*.effectivegatecpm.com/*",
                                            L"*://*.profitableratecpm.com/*", L"*://*.adsterra.com/*", L"*://*.popads.net/*",
                                            L"*/ads.js*", L"*/ads-config.js*", L"*/ad-frame.html*" })
                m_web.AddWebResourceRequestedFilter(pattern, CoreWebView2WebResourceContext::All);
            m_web.WebResourceRequested([this](auto&&, CoreWebView2WebResourceRequestedEventArgs const& a) {
                Log(L"ad blocked: " + std::wstring(a.Request().Uri()).substr(0, 110));
                a.Response(m_env.CreateWebResourceResponse(nullptr, 403, L"Blocked", L""));
            });
            // what the verification service answers goes to the log
            m_web.WebResourceResponseReceived([](auto&&, CoreWebView2WebResourceResponseReceivedEventArgs const& a) {
                std::wstring u(a.Request().Uri());
                if (u.find(L"cloudflare") != std::wstring::npos || u.find(L"cdn-cgi") != std::wstring::npos ||
                    u.find(L"verify") != std::wstring::npos)
                    Log(L"reply " + std::to_wstring(a.Response().StatusCode()) + L" " + u.substr(0, 110));
            });
            Log(L"user agent: " + std::wstring(settings.UserAgent()));
            if (!TestFlag("noinject")) co_await m_web.AddScriptToExecuteOnDocumentCreatedAsync(kInject);
            // browser tests: LocalState\web_script.js also runs on every page (no file = normal)
            if (FILE* f = _wfopen((std::wstring(ApplicationData::Current().LocalFolder().Path()) + L"\\web_script.js").c_str(), L"rb")) {
                std::string js;
                char b[4096];
                size_t n;
                while ((n = fread(b, 1, sizeof(b), f)) > 0) js.append(b, n);
                fclose(f);
                co_await m_web.AddScriptToExecuteOnDocumentCreatedAsync(winrt::to_hstring(js));
            }
            Log(L"opening " + m_url);
            m_web.Navigate(m_url);
        } catch (hresult_error const& e) {
            Log(L"WebView2 could not start: " + std::wstring(e.message()));
        }
    }

    void OnNavigate(CoreWebView2NavigationStartingEventArgs const& a)
    {
        std::wstring u(a.Uri());
        if (!IsWeb(u)) {
            Log(L"refused: " + u.substr(0, 300));
            a.Cancel(true);
            return;
        }
        // a page sending the player to another site by itself is an ad redirect (links the player pressed still work)
        std::wstring from(m_web.Source());
        // Pressing an ad counts as pressing a link, so game pages may not leave their site at all; only game lists may.
        const std::wstring fromSite = SiteOf(from);
        // (from the WEB menu, a link the player pressed may go anywhere: Google results, a channel site's own links)
        if (m_browse && a.IsUserInitiated()) {
        } else if (!a.IsRedirected() && IsWeb(from) && !fromSite.empty() && fromSite != SiteOf(u) && fromSite != L"ported2browser.com" &&
            from.find(L"/juegos.html") == std::wstring::npos &&
            fromSite != L"wasmarcade.com") {
            Log(L"left-site refused: " + u.substr(0, 300));
            a.Cancel(true);
            Toast(L"Ad blocked");
            return;
        }
        // Duke opens with its own controls ready; every other page opens with the pointer
        bool duke = u.find(L"eduke32-wasm") != std::wstring::npos;
        // the page always counts as in focus: Duke pauses itself when it thinks the window lost focus
        if (duke && !m_duke) m_web.CallDevToolsProtocolMethodAsync(L"Emulation.setFocusEmulationEnabled", L"{\"enabled\":true}");
        m_pageLocked = false;
        if (duke != m_duke || duke != m_gameMode) {
            ReleaseAll();
            m_duke = duke;
            m_gameMode = duke;
            m_keys = duke ? Table(kDuke) : Table(kBrowse);
        }
    }

    void Post(const std::wstring& json)
    {
        try {
            m_web.PostWebMessageAsJson(json);
        } catch (...) {
        }
    }

    void Toast(const std::wstring& text) { Post(L"{\"toast\":\"" + text + L"\"}"); }

    void AnnounceMode()
    {
        if (m_duke && m_gameMode) Toast(L"Game controls. View = pointer. Hold View + Menu = Game Vault");
        else if (m_gameMode) Toast(L"Game controls. View = pointer");
        else Toast(L"Pointer: left stick, A click, B back. View = game controls");
    }

    void SendKey(const Key& k, bool down)
    {
        // location tells the left and right Ctrl / Shift apart (Duke: right Ctrl fires, left Ctrl crouches)
        const int location = wcsstr(k.code, L"Arrow") ? 0 : wcsstr(k.code, L"Left") ? 1 : wcsstr(k.code, L"Right") ? 2 : 0;
        wchar_t j[256];
        swprintf(j, 256, L"{\"type\":\"%s\",\"key\":\"%s\",\"code\":\"%s\",\"windowsVirtualKeyCode\":%d,"
                         L"\"nativeVirtualKeyCode\":%d,\"location\":%d%s%s%s}",
                 down ? (k.text ? L"keyDown" : L"rawKeyDown") : L"keyUp", k.key, k.code, k.vk, k.vk, location,
                 down && k.text ? L",\"text\":\"" : L"", down && k.text ? k.text : L"", down && k.text ? L"\"" : L"");
        if (m_firstKey) {
            m_firstKey = false;
            Log(L"controller: first key " + std::wstring(k.code));
        }
        if (TestFlag("nocdp")) return;
        try {
            m_web.CallDevToolsProtocolMethodAsync(L"Input.dispatchKeyEvent", j);
        } catch (...) {
        }
    }

    void SendMouse(const wchar_t* type, int wheel = 0)
    {
        bool button = wcscmp(type, L"mousePressed") == 0 || wcscmp(type, L"mouseReleased") == 0;
        wchar_t j[256];
        if (wheel) {
            swprintf(j, 256, L"{\"type\":\"mouseWheel\",\"x\":%.0f,\"y\":%.0f,\"deltaX\":0,\"deltaY\":%d}", m_cx, m_cy, wheel);
        } else {
            swprintf(j, 256, L"{\"type\":\"%s\",\"x\":%.0f,\"y\":%.0f,\"button\":\"%s\",\"buttons\":%d,\"clickCount\":%d}",
                     type, m_cx, m_cy, button || m_click ? L"left" : L"none", m_click ? 1 : 0, button ? 1 : 0);
        }
        if (TestFlag("nocdp")) return;
        try {
            m_web.CallDevToolsProtocolMethodAsync(L"Input.dispatchMouseEvent", j);
        } catch (...) {
        }
    }

    void SetKeys(uint32_t bits)
    {
        for (size_t i = 0; i < m_keys.second; i++) {
            const Key& k = m_keys.first[i];
            bool on = (bits & k.bit) != 0, was = (m_down & k.bit) != 0;
            if (on != was) SendKey(k, on);
        }
        m_down = bits;
    }

    void ReleaseAll()
    {
        if (!m_web) return;
        SetKeys(0);
        if (m_click) {
            m_click = false;
            SendMouse(L"mouseReleased");
        }
    }

    uint32_t DukeBits(GamepadReading const& r, uint32_t b)
    {
        const double t = 0.35;
        const auto on = [](bool c, int n) { return c ? 1u << n : 0u; };
        return on(r.LeftThumbstickY > t, 0) | on(r.LeftThumbstickY < -t, 1) |
               on(r.LeftThumbstickX < -t, 2) | on(r.LeftThumbstickX > t, 3) |
               on(b & 64, 4) | on(b & 128, 5) | on(b & 256, 6) | on(b & 512, 7) |
               on(r.RightTrigger > 0.4, 8) | on(r.LeftTrigger > 0.4, 9) |
               on(b & 4, 10) | on(b & 8, 11) | on(b & 16, 12) | on(b & 32, 13) | on(b & 1, 14) |
               on(b & 1024, 15) | on(b & 2048, 16);
    }

    // right stick = mouse look: small movements for aiming, quick at full tilt
    void SendLook(double sx, double sy)
    {
        const double dead = 0.12;
        double x = fabs(sx) > dead ? sx : 0, y = fabs(sy) > dead ? sy : 0;
        if (!x && !y) return;
        int dx = (int)(x * fabs(x) * 26), dy = (int)(-y * fabs(y) * 16);
        if (!dx && !dy) return;
        wchar_t msg[64];
        swprintf(msg, 64, L"{\"look\":[%d,%d]}", dx, dy);
        Post(msg);
    }

    void MovePointer(double sx, double sy)
    {
        const double dead = 0.15;
        double mx = fabs(sx) > dead ? sx : 0, my = fabs(sy) > dead ? sy : 0;
        if (!mx && !my) return;
        Rect w = m_window.Bounds();
        if (m_cx < 0) m_cx = w.Width / 2, m_cy = w.Height / 2;
        double big = (std::max)(fabs(mx), fabs(my)), len = sqrt(mx * mx + my * my);
        float speed = (float)(2.0 + 18.0 * big * big);   // slow enough near the centre to hit small links
        m_cx = (std::min)(w.Width - 1, (std::max)(0.f, m_cx + (float)(mx / len) * speed));
        m_cy = (std::min)(w.Height - 1, (std::max)(0.f, m_cy - (float)(my / len) * speed));
        SendMouse(L"mouseMoved");
        wchar_t msg[64];
        swprintf(msg, 64, L"{\"cx\":%.0f,\"cy\":%.0f}", m_cx, m_cy);
        Post(msg);
    }

    void SetClick(bool click)
    {
        if (click == m_click) return;
        Rect w = m_window.Bounds();
        if (m_cx < 0) m_cx = w.Width / 2, m_cy = w.Height / 2;
        m_click = click;
        SendMouse(click ? L"mousePressed" : L"mouseReleased");
    }

    // the controller as the page's gamepad, in the standard layout's order
    void SendPadToPage(GamepadReading const& r, uint32_t b)
    {
        std::wstring j = L"{\"p\":{\"a\":[";
        wchar_t n[32];
        swprintf(n, 32, L"%.2f,%.2f,%.2f,%.2f", r.LeftThumbstickX, -r.LeftThumbstickY, r.RightThumbstickX, -r.RightThumbstickY);
        j += n;
        j += L"],\"v\":[";
        const auto bit = [&](uint32_t m) { return (b & m) ? 1 : 0; };
        swprintf(n, 32, L"%d,%d,%d,%d,%d,%d,", bit(4), bit(8), bit(16), bit(32), bit(1024), bit(2048));
        j += n;
        swprintf(n, 32, L"%.2f,%.2f,", r.LeftTrigger, r.RightTrigger);
        j += n;
        swprintf(n, 32, L"0,%d,%d,%d,", bit(1), bit(4096), bit(8192));
        j += n;
        swprintf(n, 32, L"%d,%d,%d,%d,0]}}", bit(64), bit(128), bit(256), bit(512));
        j += n;
        if (j != m_lastPad) {
            m_lastPad = j;
            Post(j);
        }
    }

    void Pad()
    {
        auto pads = Gamepad::Gamepads();
        if (pads.Size() == 0) return;
        // Every controller counts, whichever one is in the player's hands: buttons together, the stick pushed furthest.
        GamepadReading r = pads.GetAt(0).GetCurrentReading();
        for (uint32_t i = 1; i < pads.Size(); i++) {
            GamepadReading o = pads.GetAt(i).GetCurrentReading();
            r.Buttons = r.Buttons | o.Buttons;
            if (fabs(o.LeftThumbstickX) > fabs(r.LeftThumbstickX)) r.LeftThumbstickX = o.LeftThumbstickX;
            if (fabs(o.LeftThumbstickY) > fabs(r.LeftThumbstickY)) r.LeftThumbstickY = o.LeftThumbstickY;
            if (fabs(o.RightThumbstickX) > fabs(r.RightThumbstickX)) r.RightThumbstickX = o.RightThumbstickX;
            if (fabs(o.RightThumbstickY) > fabs(r.RightThumbstickY)) r.RightThumbstickY = o.RightThumbstickY;
            if (o.LeftTrigger > r.LeftTrigger) r.LeftTrigger = o.LeftTrigger;
            if (o.RightTrigger > r.RightTrigger) r.RightTrigger = o.RightTrigger;
        }
        uint32_t b = (uint32_t)r.Buttons;
        const uint32_t combo = (uint32_t)(GamepadButtons::View | GamepadButtons::Menu);
        if ((b & combo) == combo) {
            m_viewUsed = true;
            if (!m_comboSince) m_comboSince = GetTickCount64();
            if (!m_quitting && GetTickCount64() - m_comboSince > 1500) {
                m_quitting = true;
                Log(L"View + Menu: back to Game Vault");
                Windows::System::Launcher::LaunchUriAsync(Uri(L"gamevault:?from=web"));
                CoreApplication::Exit();
            }
            return;
        }
        m_comboSince = 0;
        if (!m_web) return;
        // View alone, on letting go: pointer <-> game controls
        bool view = (b & 2) != 0;
        if (m_viewWas && !view && !m_viewUsed) {
            ReleaseAll();
            m_gameMode = !m_gameMode;
            m_keys = !m_gameMode ? Table(kBrowse) : m_duke ? Table(kDuke) : Table(kGame);
            if (m_gameMode) Post(L"{\"hide\":1}");
            AnnounceMode();
            m_viewWas = false;
            m_btnWas = b;
            return;
        }
        if (!view) m_viewUsed = false;
        m_viewWas = view;

        if (m_gameMode && m_duke) {
            SetKeys(DukeBits(r, b));
            SendLook(r.RightThumbstickX, r.RightThumbstickY);
        } else if (m_gameMode) {
            const double t = 0.5;
            SetKeys((b & ~2u) | (r.LeftThumbstickY > t ? 64u : 0u) | (r.LeftThumbstickY < -t ? 128u : 0u) |
                    (r.LeftThumbstickX < -t ? 256u : 0u) | (r.LeftThumbstickX > t ? 512u : 0u));
            // a game holding the mouse gets the right stick as mouse look; otherwise it moves the pointer
            if (m_pageLocked) SendLook(r.RightThumbstickX, r.RightThumbstickY);
            else MovePointer(r.RightThumbstickX, r.RightThumbstickY);
            SetClick(r.RightTrigger > 0.5);
            SendPadToPage(r, b & ~2u);
        } else {
            SetKeys(b & ~2u);
            MovePointer(r.LeftThumbstickX, r.LeftThumbstickY);
            SetClick((b & 4) != 0 || r.RightTrigger > 0.5);
            // right stick scrolls
            double sy = r.RightThumbstickY;
            ULONGLONG now = GetTickCount64();
            if (fabs(sy) > 0.2 && now - m_scrollAt > 30) {
                m_scrollAt = now;
                Rect w = m_window.Bounds();
                if (m_cx < 0) m_cx = w.Width / 2, m_cy = w.Height / 2;
                SendMouse(L"mouseWheel", (int)(-sy * 90));
            }
            uint32_t pressed = b & ~m_btnWas;
            if (pressed & (8 | 1024)) {   // B, LB: the page before; on a site's first page from the WEB menu, back to it
                if (m_web.CanGoBack()) {
                    m_web.GoBack();
                } else if (m_browse && (pressed & 8) && !m_quitting) {
                    m_quitting = true;
                    Log(L"B on the first page: back to Game Vault's WEB menu");
                    Windows::System::Launcher::LaunchUriAsync(Uri(L"gamevault:?from=webmenu"));
                    CoreApplication::Exit();
                    return;
                }
            }
            if (pressed & 2048) { if (m_web.CanGoForward()) m_web.GoForward(); }   // RB
            if (pressed & 32) m_web.Reload();                                      // Y
        }
        m_btnWas = b;
    }

    void Run()
    {
        m_window.Activate();
        CoreDispatcher d = m_window.Dispatcher();
        while (!m_quitting) {
            d.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
            Pad();
            Sleep(16);
        }
    }
};

} // namespace

// WebView2 loads some Windows functions on first use that the Xbox does not have: a missing one raises 0xC06D007F with
// a DelayLoadInfo, and we answer with a stand-in that reports success (Flash, dump 2026-10-27).
BOOL WINAPI DoneStandIn() { return TRUE; }
extern "C" __declspec(dllimport) PVOID WINAPI AddVectoredExceptionHandler(ULONG, PVECTORED_EXCEPTION_HANDLER);

LONG WINAPI DelayLoadStandIn(EXCEPTION_POINTERS* e)
{
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if ((code != VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND) &&
         code != VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)) ||
        e->ExceptionRecord->NumberParameters < 1)
        return EXCEPTION_CONTINUE_SEARCH;
    auto* dli = reinterpret_cast<DelayLoadInfo*>(e->ExceptionRecord->ExceptionInformation[0]);
    char what[160];
    snprintf(what, sizeof(what), "%s!%s", dli->szDll ? dli->szDll : "?",
             dli->dlp.fImportByName ? dli->dlp.szProcName : "(ordinal)");
    Log(L"not on the Xbox, stand-in used: " + std::wstring(what, what + strlen(what)));
    dli->pfnCur = reinterpret_cast<FARPROC>(DoneStandIn);
    return EXCEPTION_CONTINUE_EXECUTION;
}

LONG WINAPI LastChance(EXCEPTION_POINTERS* e)
{
    wchar_t msg[128];
    swprintf(msg, 128, L"CRASH %08X at %p", (unsigned)e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress);
    Log(msg);
    return EXCEPTION_CONTINUE_SEARCH;
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    SetUnhandledExceptionFilter(LastChance);
    AddVectoredExceptionHandler(1, DelayLoadStandIn);
    init_apartment();   // CoreApplication::Run makes the window's thread itself: this one stays multithreaded
    Log(L"Game Vault · Web starting");
    try {
        CoreApplication::Run(make<App>());
    } catch (hresult_error const& e) {
        Log(L"could not start: " + std::wstring(e.message()));
    }
    return 0;
}
