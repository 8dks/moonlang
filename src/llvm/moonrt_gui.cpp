// MoonLang Runtime - GUI Support with WebView2 (Multi-Window)
// Copyright (c) 2026 greenteng.com

#include "moonrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <wrl.h>
#include <WebView2.h>
#include <map>
#include <string>
#include <unordered_map>

using namespace Microsoft::WRL;

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dwmapi.lib")

// Win11 DWM Window Corner Preference (for older SDKs)
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_COLOR_NONE
#define DWMWA_COLOR_NONE 0xFFFFFFFE
#endif
#ifndef DWMWA_COLOR_DEFAULT
#define DWMWA_COLOR_DEFAULT 0xFFFFFFFF
#endif

// ============================================================================
// MoonWindow Structure (Multi-Window Support)
// ============================================================================

struct MoonWindow {
    int id;                              // Unique window ID
    HWND hwnd;                           // Win32 handle
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
    bool webviewReady;
    bool showPending;                    // Show after content loaded
    
    // Options
    bool frameless;
    bool transparent;
    bool topmost;
    bool resizable;
    bool clickThrough;
    bool devtools;
    int alpha;
    
    // Callbacks
    MoonValue* messageCallback;
    MoonValue* closeCallback;
    
    // Exposed functions (callable from JS)
    std::unordered_map<std::string, MoonValue*> exposedFuncs;
    
    // Pending content to load
    char* pendingHtml;
    char* pendingUrl;
    
    // Parent window
    int parentId;
    bool modal;
    bool inSizeMove;  // true between WM_ENTERSIZEMOVE and WM_EXITSIZEMOVE (avoids black block on transparent resize/drag)
    
    MoonWindow() : id(0), hwnd(NULL), webviewReady(false), showPending(false),
        frameless(false), transparent(false), topmost(false),
        resizable(true), clickThrough(false), devtools(false), alpha(255),
        messageCallback(nullptr), closeCallback(nullptr),
        pendingHtml(nullptr), pendingUrl(nullptr),
        parentId(0), modal(false), inSizeMove(false) {}
    
    ~MoonWindow() {
        if (pendingHtml) free(pendingHtml);
        if (pendingUrl) free(pendingUrl);
        if (messageCallback) moon_release(messageCallback);
        if (closeCallback) moon_release(closeCallback);
        for (auto& pair : exposedFuncs) {
            if (pair.second) moon_release(pair.second);
        }
    }
};

// ============================================================================
// Global State
// ============================================================================

static bool g_gui_initialized = false;
static HINSTANCE g_hInstance = NULL;
static HICON g_appIcon = NULL;
static HICON g_appIconSmall = NULL;
static ComPtr<ICoreWebView2Environment> g_webviewEnv;

// Window Manager
static std::map<int, MoonWindow*> g_windows;
static int g_nextWindowId = 1;

// Tray (global, shared)
static NOTIFYICONDATAW g_trayIcon = {0};
static bool g_hasTray = false;
static HMENU g_trayMenu = NULL;
static MoonValue* g_trayCallback = NULL;
static HWND g_trayOwnerWindow = NULL;

// DPI state
static float g_dpiScale = 1.0f;
static int g_dpi = 96;

// Virtual file system for moon:// protocol
static std::unordered_map<std::wstring, std::string> g_virtualFiles;

#define WM_TRAYICON (WM_USER + 1)
#define IDM_TRAY_BASE 1000

// ============================================================================
// Helper Functions
// ============================================================================

static wchar_t* utf8_to_wchar(const char* str) {
    if (!str) return NULL;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    wchar_t* wstr = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, wlen);
    return wstr;
}

static std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring wstr(wlen - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], wlen);
    return wstr;
}

static char* wchar_to_utf8(const wchar_t* wstr) {
    if (!wstr) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* str = (char*)malloc(len);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    return str;
}

static MoonWindow* GetWindowById(int id) {
    auto it = g_windows.find(id);
    return (it != g_windows.end()) ? it->second : nullptr;
}

static MoonWindow* GetFirstWindow() {
    return g_windows.empty() ? nullptr : g_windows.begin()->second;
}

// Get MIME type from file extension
static const wchar_t* GetMimeType(const std::wstring& path) {
    size_t dotPos = path.rfind(L'.');
    if (dotPos == std::wstring::npos) return L"application/octet-stream";
    
    std::wstring ext = path.substr(dotPos);
    // Convert to lowercase
    for (auto& c : ext) c = towlower(c);
    
    if (ext == L".html" || ext == L".htm") return L"text/html";
    if (ext == L".css") return L"text/css";
    if (ext == L".js") return L"application/javascript";
    if (ext == L".json") return L"application/json";
    if (ext == L".png") return L"image/png";
    if (ext == L".jpg" || ext == L".jpeg") return L"image/jpeg";
    if (ext == L".gif") return L"image/gif";
    if (ext == L".svg") return L"image/svg+xml";
    if (ext == L".webp") return L"image/webp";
    if (ext == L".ico") return L"image/x-icon";
    if (ext == L".woff") return L"font/woff";
    if (ext == L".woff2") return L"font/woff2";
    if (ext == L".ttf") return L"font/ttf";
    if (ext == L".xml") return L"application/xml";
    if (ext == L".txt") return L"text/plain";
    return L"application/octet-stream";
}

// Register virtual file for moon:// protocol
static void RegisterVirtualFile(const wchar_t* path, const char* content, size_t len) {
    std::wstring wpath(path);
    // Normalize path: lowercase, forward slashes
    for (auto& c : wpath) {
        if (c == L'\\') c = L'/';
        c = towlower(c);
    }
    g_virtualFiles[wpath] = std::string(content, len);
}

// ============================================================================
// DPI Awareness
// ============================================================================

typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
typedef UINT (WINAPI *GetDpiForSystemFunc)(void);
typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND);

static void InitializeDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto setDpiContext = (SetProcessDpiAwarenessContextFunc)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (setDpiContext) {
            if (setDpiContext((DPI_AWARENESS_CONTEXT)-4)) return;
            if (setDpiContext((DPI_AWARENESS_CONTEXT)-3)) return;
            if (setDpiContext((DPI_AWARENESS_CONTEXT)-2)) return;
        }
    }
    SetProcessDPIAware();
}

static void UpdateDpiScale(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto getDpiForWindow = (GetDpiForWindowFunc)GetProcAddress(user32, "GetDpiForWindow");
        if (getDpiForWindow && hwnd) {
            g_dpi = getDpiForWindow(hwnd);
            g_dpiScale = (float)g_dpi / 96.0f;
            return;
        }
        
        auto getDpiForSystem = (GetDpiForSystemFunc)GetProcAddress(user32, "GetDpiForSystem");
        if (getDpiForSystem) {
            g_dpi = getDpiForSystem();
            g_dpiScale = (float)g_dpi / 96.0f;
            return;
        }
    }
    
    HDC hdc = GetDC(NULL);
    if (hdc) {
        g_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        g_dpiScale = (float)g_dpi / 96.0f;
        ReleaseDC(NULL, hdc);
    }
}

static int ScaleForDpi(int value) {
    return (int)(value * g_dpiScale + 0.5f);
}

static int GetResizeBorderPixels() {
    int paddedBorder = GetSystemMetrics(SM_CXPADDEDBORDER);
    int border = GetSystemMetrics(SM_CXSIZEFRAME) + paddedBorder;
    if (border < 8) border = 8;
    return border;
}

static int GetWebViewInsetPixels() {
    // Keep only a very thin visible border.
    // Resize hit-test still uses GetResizeBorderPixels().
    return 1;
}

static void UpdateWebViewBounds(MoonWindow* win) {
    if (!win || !win->controller || !win->hwnd) return;

    RECT bounds;
    GetClientRect(win->hwnd, &bounds);

    // Transparent windows should stay fully interactive to the edge.
    int inset = 0;
    if (win->frameless && !win->transparent && win->resizable) {
        inset = GetWebViewInsetPixels();
    }

    bounds.left += inset;
    bounds.top += inset;
    bounds.right -= inset;
    bounds.bottom -= inset;
    if (bounds.right < bounds.left) bounds.right = bounds.left;
    if (bounds.bottom < bounds.top) bounds.bottom = bounds.top;

    win->controller->put_Bounds(bounds);
    win->controller->NotifyParentWindowPositionChanged();
}

// ============================================================================
// WebView2 Initialization (Per-Window)
// ============================================================================

static void LoadPendingContentForWindow(MoonWindow* win) {
    if (!win || !win->webview) return;
    
    if (win->pendingHtml) {
        // Use NavigateToString for now (moon:// protocol needs more debugging)
        wchar_t* whtml = utf8_to_wchar(win->pendingHtml);
        win->webview->NavigateToString(whtml);
        free(whtml);
        free(win->pendingHtml);
        win->pendingHtml = NULL;
    } else if (win->pendingUrl) {
        wchar_t* wurl = utf8_to_wchar(win->pendingUrl);
        win->webview->Navigate(wurl);
        free(wurl);
        free(win->pendingUrl);
        win->pendingUrl = NULL;
    }
}

static void InitializeWebView2ForWindow(MoonWindow* win) {
    if (!win || !g_webviewEnv) return;
    
    g_webviewEnv->CreateCoreWebView2Controller(
        win->hwnd,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [win](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                if (FAILED(result) || !controller) {
                    return result;
                }
                
                win->controller = controller;
                controller->get_CoreWebView2(&win->webview);
                
                // Configure settings
                ComPtr<ICoreWebView2Settings> settings;
                win->webview->get_Settings(&settings);
                settings->put_IsScriptEnabled(TRUE);
                settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                settings->put_IsWebMessageEnabled(TRUE);
                settings->put_AreDevToolsEnabled(win->devtools ? TRUE : FALSE);
                
                // Set WebView2 background color
                ComPtr<ICoreWebView2Controller2> controller2;
                if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&controller2)))) {
                    if (win->transparent) {
                        // True transparent background for transparent window mode.
                        COREWEBVIEW2_COLOR transparent = { 0, 0, 0, 0 };
                        controller2->put_DefaultBackgroundColor(transparent);
                    } else if (win->frameless) {
                        // Frameless: use dark neutral background to avoid bright resize border.
                        // Dark slate (#1e1e2e)
                        COREWEBVIEW2_COLOR bgColor = { 255, 30, 30, 46 };
                        controller2->put_DefaultBackgroundColor(bgColor);
                    }
                }
                
                // Setup moon:// custom protocol for virtual file system
                win->webview->AddWebResourceRequestedFilter(L"moon://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
                win->webview->add_WebResourceRequested(
                    Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                        [](ICoreWebView2* webview, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                            LPWSTR uri = nullptr;
                            ComPtr<ICoreWebView2WebResourceRequest> request;
                            args->get_Request(&request);
                            request->get_Uri(&uri);
                            
                            if (uri && wcsncmp(uri, L"moon://", 7) == 0) {
                                // Extract path from moon://localhost/path or moon://app/path
                                std::wstring wuri(uri);
                                size_t pathStart = wuri.find(L'/', 7);  // Find / after moon://
                                if (pathStart != std::wstring::npos) {
                                    std::wstring path = wuri.substr(pathStart);
                                    // Normalize path
                                    for (auto& c : path) {
                                        if (c == L'\\') c = L'/';
                                        c = towlower(c);
                                    }
                                    
                                    // Look up in virtual file system
                                    auto it = g_virtualFiles.find(path);
                                    if (it != g_virtualFiles.end()) {
                                        // Use global environment
                                        ComPtr<ICoreWebView2Environment> env = g_webviewEnv;
                                        
                                        // Create IStream from content
                                        const std::string& content = it->second;
                                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, content.size());
                                        if (hMem) {
                                            void* pMem = GlobalLock(hMem);
                                            memcpy(pMem, content.data(), content.size());
                                            GlobalUnlock(hMem);
                                            
                                            IStream* stream = nullptr;
                                            CreateStreamOnHGlobal(hMem, TRUE, &stream);
                                            
                                            if (stream) {
                                                const wchar_t* mimeType = GetMimeType(path);
                                                wchar_t headers[512];
                                                swprintf_s(headers, 512, 
                                                    L"Content-Type: %s\r\nAccess-Control-Allow-Origin: *", 
                                                    mimeType);
                                                
                                                ComPtr<ICoreWebView2WebResourceResponse> response;
                                                env->CreateWebResourceResponse(
                                                    stream, 200, L"OK", headers, &response);
                                                
                                                if (response) {
                                                    args->put_Response(response.Get());
                                                }
                                                stream->Release();
                                            }
                                        }
                                    }
                                }
                            }
                            
                            if (uri) CoTaskMemFree(uri);
                            return S_OK;
                        }).Get(), nullptr);
                
                // Set bounds
                UpdateWebViewBounds(win);
                
                // Handle messages from web page
                win->webview->add_WebMessageReceived(
                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [win](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR message = nullptr;
                            args->TryGetWebMessageAsString(&message);
                            if (message) {
                                std::wstring msg(message);
                                if (msg.size() >= 2 && msg.front() == L'"' && msg.back() == L'"') {
                                    msg = msg.substr(1, msg.size() - 2);
                                }

                                if (msg == L"__drag__") {
                                    ReleaseCapture();
                                    SendMessageW(win->hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
                                } else if (msg.rfind(L"__resize__:", 0) == 0) {
                                    std::wstring dir = msg.substr(11);
                                    WPARAM hit = HTNOWHERE;
                                    if (dir == L"left") hit = HTLEFT;
                                    else if (dir == L"right") hit = HTRIGHT;
                                    else if (dir == L"top") hit = HTTOP;
                                    else if (dir == L"bottom") hit = HTBOTTOM;
                                    else if (dir == L"top-left") hit = HTTOPLEFT;
                                    else if (dir == L"top-right") hit = HTTOPRIGHT;
                                    else if (dir == L"bottom-left") hit = HTBOTTOMLEFT;
                                    else if (dir == L"bottom-right") hit = HTBOTTOMRIGHT;
                                    if (hit != HTNOWHERE) {
                                        ReleaseCapture();
                                        SendMessageW(win->hwnd, WM_NCLBUTTONDOWN, hit, 0);
                                    }
                                } else if (msg == L"__close__") {
                                    PostMessageW(win->hwnd, WM_CLOSE, 0, 0);
                                } else if (msg == L"__minimize__") {
                                    ShowWindow(win->hwnd, SW_MINIMIZE);
                                } else if (msg == L"__maximize__") {
                                    if (IsZoomed(win->hwnd)) {
                                        ShowWindow(win->hwnd, SW_RESTORE);
                                    } else {
                                        ShowWindow(win->hwnd, SW_MAXIMIZE);
                                    }
                                } else if (msg.rfind(L"__call__:", 0) == 0) {
                                    // Handle function call: __call__:funcName:callId:argsJson
                                    char* utf8Msg = wchar_to_utf8(msg.c_str() + 9);
                                    std::string msgStr = utf8Msg;
                                    free(utf8Msg);
                                    
                                    // Parse: funcName:callId:argsJson
                                    size_t pos1 = msgStr.find(':');
                                    size_t pos2 = msgStr.find(':', pos1 + 1);
                                    if (pos1 != std::string::npos && pos2 != std::string::npos) {
                                        std::string funcName = msgStr.substr(0, pos1);
                                        std::string callId = msgStr.substr(pos1 + 1, pos2 - pos1 - 1);
                                        std::string argsJson = msgStr.substr(pos2 + 1);
                                        
                                        // Find exposed function
                                        auto it = win->exposedFuncs.find(funcName);
                                        std::string resultJson;
                                        
                                        if (it != win->exposedFuncs.end() && it->second) {
                                            // Call the function with args
                                            MoonValue* argsVal = moon_string(argsJson.c_str());
                                            MoonValue* funcArgs[2] = { argsVal, NULL };
                                            MoonValue* result = moon_call_func(it->second, funcArgs, 1);
                                            
                                            // Convert result to string
                                            if (result && moon_is_string(result)) {
                                                resultJson = result->data.strVal;
                                            } else if (result) {
                                                resultJson = "null";
                                            } else {
                                                resultJson = "null";
                                            }
                                            moon_release(argsVal);
                                            if (result) moon_release(result);
                                        } else {
                                            resultJson = "{\"error\":\"Function not found: " + funcName + "\"}";
                                        }
                                        
                                        // Post result back to JS
                                        std::wstring js = L"window.__moonCallResults && window.__moonCallResults['";
                                        js += utf8_to_wstring(callId);
                                        js += L"'] && window.__moonCallResults['";
                                        js += utf8_to_wstring(callId);
                                        js += L"'](";
                                        js += utf8_to_wstring(resultJson);
                                        js += L");";
                                        win->webview->ExecuteScript(js.c_str(), nullptr);
                                    }
                                } else if (win->messageCallback) {
                                    char* utf8Msg = wchar_to_utf8(message);
                                    MoonValue* msgArgs[2];
                                    msgArgs[0] = moon_string(utf8Msg);
                                    msgArgs[1] = NULL;
                                    moon_call_func(win->messageCallback, msgArgs, 1);
                                    moon_release(msgArgs[0]);
                                    free(utf8Msg);
                                }
                            }
                            CoTaskMemFree(message);
                            return S_OK;
                        }).Get(), nullptr);
                
                // Handle navigation completed - fade in for frameless windows
                win->webview->add_NavigationCompleted(
                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                        [win](ICoreWebView2* webview, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                            if (win->showPending) {
                                win->showPending = false;
                                // Fade in animation: 0 -> 255 over ~150ms
                                for (int alpha = 0; alpha <= 255; alpha += 15) {
                                    SetLayeredWindowAttributes(win->hwnd, 0, (BYTE)alpha, LWA_ALPHA);
                                    UpdateWindow(win->hwnd);
                                    Sleep(6);  // ~150ms total
                                }
                                // Ensure final alpha is correct
                                SetLayeredWindowAttributes(win->hwnd, 0, (BYTE)win->alpha, LWA_ALPHA);
                            }
                            return S_OK;
                        }).Get(), nullptr);
                
                // Inject MoonGUI API with window ID
                wchar_t script[8192];
                swprintf_s(script, 8192,
                    L"window.__moonCallId = 0;"
                    L"window.__moonCallResults = {};"
                    L"window.MoonGUI = {"
                    L"  windowId: %d,"
                    L"  send: function(msg) { window.chrome.webview.postMessage(msg); },"
                    L"  close: function() { window.chrome.webview.postMessage('__close__'); },"
                    L"  minimize: function() { window.chrome.webview.postMessage('__minimize__'); },"
                    L"  maximize: function() { window.chrome.webview.postMessage('__maximize__'); },"
                    L"  drag: function() { window.chrome.webview.postMessage('__drag__'); },"
                    L"  call: function(funcName) {"
                    L"    var args = Array.prototype.slice.call(arguments, 1);"
                    L"    return new Promise(function(resolve) {"
                    L"      var callId = 'c' + (++window.__moonCallId);"
                    L"      window.__moonCallResults[callId] = function(result) {"
                    L"        delete window.__moonCallResults[callId];"
                    L"        resolve(result);"
                    L"      };"
                    L"      window.chrome.webview.postMessage('__call__:' + funcName + ':' + callId + ':' + JSON.stringify(args));"
                    L"    });"
                    L"  },"
                    L"  callSync: function(funcName) {"
                    L"    console.warn('callSync is deprecated, use call() with await instead');"
                    L"    return this.call.apply(this, arguments);"
                    L"  }"
                    L"};"
                    L"if (%d) {"
                    L"  var __moonEdgeSize = 8;"
                    L"  function __moonEdgeHit(e) {"
                    L"    var w = window.innerWidth || document.documentElement.clientWidth || 0;"
                    L"    var h = window.innerHeight || document.documentElement.clientHeight || 0;"
                    L"    var x = e.clientX, y = e.clientY;"
                    L"    var onL = x <= __moonEdgeSize, onR = x >= (w - __moonEdgeSize);"
                    L"    var onT = y <= __moonEdgeSize, onB = y >= (h - __moonEdgeSize);"
                    L"    if (onT && onL) return 'top-left';"
                    L"    if (onT && onR) return 'top-right';"
                    L"    if (onB && onL) return 'bottom-left';"
                    L"    if (onB && onR) return 'bottom-right';"
                    L"    if (onL) return 'left';"
                    L"    if (onR) return 'right';"
                    L"    if (onT) return 'top';"
                    L"    if (onB) return 'bottom';"
                    L"    return '';"
                    L"  }"
                    L"  function __moonEdgeCursor(edge) {"
                    L"    if (edge === 'left' || edge === 'right') return 'ew-resize';"
                    L"    if (edge === 'top' || edge === 'bottom') return 'ns-resize';"
                    L"    if (edge === 'top-left' || edge === 'bottom-right') return 'nwse-resize';"
                    L"    if (edge === 'top-right' || edge === 'bottom-left') return 'nesw-resize';"
                    L"    return '';"
                    L"  }"
                    L"  document.addEventListener('mousemove', function(e) {"
                    L"    var edge = __moonEdgeHit(e);"
                    L"    document.documentElement.style.cursor = __moonEdgeCursor(edge);"
                    L"  }, true);"
                    L"  document.addEventListener('mouseleave', function() {"
                    L"    document.documentElement.style.cursor = '';"
                    L"  }, true);"
                    L"  document.addEventListener('mousedown', function(e) {"
                    L"    if (e.button !== 0) return;"
                    L"    var edge = __moonEdgeHit(e);"
                    L"    if (!edge) return;"
                    L"    e.preventDefault();"
                    L"    e.stopImmediatePropagation();"
                    L"    window.chrome.webview.postMessage('__resize__:' + edge);"
                    L"  }, true);"
                    L"}"
                    L"document.addEventListener('mousedown', function(e) {"
                    L"  var el = e.target;"
                    L"  while (el) {"
                    L"    var style = window.getComputedStyle(el);"
                    L"    var region = style.getPropertyValue('-webkit-app-region') || style.getPropertyValue('app-region');"
                    L"    if (region === 'drag') {"
                    L"      e.preventDefault();"
                    L"      window.chrome.webview.postMessage('__drag__');"
                    L"      return;"
                    L"    }"
                    L"    if (region === 'no-drag') return;"
                    L"    el = el.parentElement;"
                    L"  }"
                    L"});", win->id,
                    (win->transparent && win->frameless && win->resizable) ? 1 : 0);
                
                win->webview->AddScriptToExecuteOnDocumentCreated(script, nullptr);
                
                win->webviewReady = true;
                LoadPendingContentForWindow(win);
                
                return S_OK;
            }).Get());
}

// ============================================================================
// Window Procedure (Multi-Window)
// ============================================================================

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MoonWindow* win = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        win = (MoonWindow*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)win);
        if (win) win->hwnd = hwnd;
    } else {
        win = (MoonWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    // Handle tray messages regardless of window
    if (msg == WM_TRAYICON) {
        if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
            if (g_trayCallback) {
                MoonValue* args[2];
                if (lParam == WM_LBUTTONUP) {
                    args[0] = moon_string("click");
                } else {
                    if (g_trayMenu) {
                        POINT pt;
                        GetCursorPos(&pt);
                        SetForegroundWindow(hwnd);
                        int cmd = TrackPopupMenu(g_trayMenu, TPM_RETURNCMD | TPM_NONOTIFY,
                                                 pt.x, pt.y, 0, hwnd, NULL);
                        if (cmd >= IDM_TRAY_BASE) {
                            wchar_t buf[256];
                            GetMenuStringW(g_trayMenu, cmd, buf, 256, MF_BYCOMMAND);
                            char* action = wchar_to_utf8(buf);
                            args[0] = moon_string(action);
                            free(action);
                        } else {
                            args[0] = moon_string("menu");
                        }
                    } else {
                        args[0] = moon_string("rightclick");
                    }
                }
                args[1] = NULL;
                moon_call_func(g_trayCallback, args, 1);
                moon_release(args[0]);
            }
        }
        return 0;
    }
    
    if (msg == WM_COMMAND && LOWORD(wParam) >= IDM_TRAY_BASE && g_trayCallback) {
        wchar_t buf[256];
        GetMenuStringW(g_trayMenu, LOWORD(wParam), buf, 256, MF_BYCOMMAND);
        char* action = wchar_to_utf8(buf);
        MoonValue* args[2];
        args[0] = moon_string(action);
        args[1] = NULL;
        moon_call_func(g_trayCallback, args, 1);
        moon_release(args[0]);
        free(action);
        return 0;
    }
    
    if (!win) return DefWindowProcW(hwnd, msg, wParam, lParam);
    
    switch (msg) {
        case WM_CREATE:
            InitializeWebView2ForWindow(win);
            return 0;

        case WM_ERASEBKGND:
            if (win->transparent) {
                // Transparent windows must not paint class background, otherwise
                // a solid rectangle appears behind transparent web content.
                return 1;
            }
            break;
        
        case WM_NCCALCSIZE:
            // For non-transparent frameless windows: extend client area to full window.
            // Transparent windows use WS_POPUP and do not need this override.
            if (win->frameless && !win->transparent) {
                // Return 0 without modifying params = client area fills entire window
                return 0;
            }
            break;
        
        case WM_NCHITTEST: {
            if (win->frameless) {
                // For frameless windows: enable resize from edges.
                if (win->resizable) {
                    if (win->transparent) {
                        RECT windowRect;
                        if (!GetWindowRect(hwnd, &windowRect)) break;
                        POINT screenPt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                        int border = GetResizeBorderPixels();

                        bool onLeft = screenPt.x >= windowRect.left && screenPt.x < (windowRect.left + border);
                        bool onRight = screenPt.x < windowRect.right && screenPt.x >= (windowRect.right - border);
                        bool onTop = screenPt.y >= windowRect.top && screenPt.y < (windowRect.top + border);
                        bool onBottom = screenPt.y < windowRect.bottom && screenPt.y >= (windowRect.bottom - border);

                        if (onTop) {
                            if (onLeft) return HTTOPLEFT;
                            if (onRight) return HTTOPRIGHT;
                            return HTTOP;
                        }
                        if (onBottom) {
                            if (onLeft) return HTBOTTOMLEFT;
                            if (onRight) return HTBOTTOMRIGHT;
                            return HTBOTTOM;
                        }
                        if (onLeft) return HTLEFT;
                        if (onRight) return HTRIGHT;
                    } else {
                        RECT windowRect;
                        HRESULT hr = DwmGetWindowAttribute(
                            hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &windowRect, sizeof(windowRect));
                        if (FAILED(hr)) {
                            if (!GetWindowRect(hwnd, &windowRect)) {
                                break;
                            }
                        }
                        POINT screenPt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                        // Match OS frame thickness as much as possible, with a safe minimum.
                        int borderX = GetResizeBorderPixels();
                        int borderY = borderX;

                        bool onLeft = screenPt.x >= windowRect.left && screenPt.x < (windowRect.left + borderX);
                        bool onRight = screenPt.x < windowRect.right && screenPt.x >= (windowRect.right - borderX);
                        bool onTop = screenPt.y >= windowRect.top && screenPt.y < (windowRect.top + borderY);
                        bool onBottom = screenPt.y < windowRect.bottom && screenPt.y >= (windowRect.bottom - borderY);

                        if (onTop) {
                            if (onLeft) return HTTOPLEFT;
                            if (onRight) return HTTOPRIGHT;
                            return HTTOP;
                        }
                        if (onBottom) {
                            if (onLeft) return HTBOTTOMLEFT;
                            if (onRight) return HTBOTTOMRIGHT;
                            return HTBOTTOM;
                        }
                        if (onLeft) return HTLEFT;
                        if (onRight) return HTRIGHT;
                    }
                }

                // Keep frameless interior interactive for WebView input/drag regions.
                return HTCLIENT;
            }
            break;
        }

        case WM_SIZE:
            if (win->controller) {
                UpdateWebViewBounds(win);
            }
            return 0;

        case WM_SIZING:
            if (win->controller) {
                UpdateWebViewBounds(win);
            }
            return TRUE;

        case WM_WINDOWPOSCHANGED:
            if (win->controller) {
                UpdateWebViewBounds(win);
            }
            break;

        case WM_SETCURSOR:
            if (win->frameless && win->resizable) {
                switch (LOWORD(lParam)) {
                    case HTLEFT:
                    case HTRIGHT:
                        SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                        return TRUE;
                    case HTTOP:
                    case HTBOTTOM:
                        SetCursor(LoadCursor(NULL, IDC_SIZENS));
                        return TRUE;
                    case HTTOPLEFT:
                    case HTBOTTOMRIGHT:
                        SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
                        return TRUE;
                    case HTTOPRIGHT:
                    case HTBOTTOMLEFT:
                        SetCursor(LoadCursor(NULL, IDC_SIZENESW));
                        return TRUE;
                    default:
                        break;
                }
            }
            break;

        case WM_MOVE:
        case WM_EXITSIZEMOVE:
            if (win->controller) {
                UpdateWebViewBounds(win);
                if (msg == WM_EXITSIZEMOVE) {
                    win->inSizeMove = false;
                    if (win->transparent && win->frameless && win->resizable) {
                        SetLayeredWindowAttributes(hwnd, 0, (BYTE)win->alpha, LWA_ALPHA);
                        COLORREF borderNone = DWMWA_COLOR_NONE;
                        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &borderNone, sizeof(borderNone));
                        if (win->webview) {
                            win->webview->ExecuteScript(
                                L"window.dispatchEvent(new Event('resize'));", nullptr);
                        }
                    }
                }
            }
            return 0;

        case WM_DPICHANGED: {
            g_dpi = HIWORD(wParam);
            g_dpiScale = (float)g_dpi / 96.0f;
            RECT* suggested = (RECT*)lParam;
            SetWindowPos(hwnd, NULL, 
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
            
        case WM_CLOSE:
            // Call close callback if set
            if (win->closeCallback) {
                MoonValue* args[2];
                args[0] = moon_int(win->id);
                args[1] = NULL;
                moon_call_func(win->closeCallback, args, 1);
                moon_release(args[0]);
            }
            
            // Clean up WebView2
            win->controller = nullptr;
            win->webview = nullptr;
            
            // Remove from registry
            g_windows.erase(win->id);
            
            // Update tray owner if needed
            if (g_trayOwnerWindow == hwnd) {
                g_trayOwnerWindow = g_windows.empty() ? NULL : g_windows.begin()->second->hwnd;
                if (g_hasTray && g_trayOwnerWindow) {
                    g_trayIcon.hWnd = g_trayOwnerWindow;
                    Shell_NotifyIconW(NIM_MODIFY, &g_trayIcon);
                }
            }
            
            DestroyWindow(hwnd);
            delete win;
            
            // Quit if no windows left (unless tray is active)
            if (g_windows.empty() && !g_hasTray) {
                PostQuitMessage(0);
            }
            return 0;
            
        case WM_DESTROY:
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// GUI Initialization
// ============================================================================

extern "C" {

MoonValue* moon_gui_init(void) {
    if (g_gui_initialized) return moon_bool(true);
    
    InitializeDpiAwareness();
    UpdateDpiScale(NULL);
    
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    g_hInstance = GetModuleHandle(NULL);
    
    // Load application icon
    g_appIcon = LoadIconW(g_hInstance, MAKEINTRESOURCEW(1));
    g_appIconSmall = (HICON)LoadImageW(g_hInstance, MAKEINTRESOURCEW(1), 
                                        IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!g_appIcon) g_appIcon = LoadIconW(NULL, IDI_APPLICATION);
    if (!g_appIconSmall) g_appIconSmall = LoadIconW(NULL, IDI_APPLICATION);
    
    // Register window class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.hIcon = g_appIcon;
    wc.hIconSm = g_appIconSmall;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    // Match frameless fallback/background with dark neutral color (#1e1e2e).
    static HBRUSH s_bgBrush = CreateSolidBrush(RGB(30, 30, 46));
    wc.hbrBackground = s_bgBrush;
    wc.lpszClassName = L"MoonLangWindow";
    
    if (!RegisterClassExW(&wc)) {
        return moon_bool(false);
    }
    
    // Transparent windows: no class background so newly exposed area on resize/drag is not filled (avoids "different background" block).
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszClassName = L"MoonLangWindowTransparent";
    if (!RegisterClassExW(&wc)) {
        return moon_bool(false);
    }
    
    // Create WebView2 environment (shared)
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    wcscat_s(tempPath, L"MoonLang_WebView2");
    
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, tempPath, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (SUCCEEDED(result) && env) {
                    g_webviewEnv = env;
                }
                return S_OK;
            }).Get());
    
    // Wait briefly for environment creation
    MSG msg;
    DWORD startTime = GetTickCount();
    while (!g_webviewEnv && GetTickCount() - startTime < 3000) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        Sleep(10);
    }
    
    g_gui_initialized = true;
    return moon_bool(true);
}

// ============================================================================
// Window Creation - Returns Window ID
// ============================================================================

MoonValue* moon_gui_create_advanced(MoonValue* title, MoonValue* width, MoonValue* height, MoonValue* options) {
    if (!g_gui_initialized) {
        moon_gui_init();
    }
    
    MoonWindow* win = new MoonWindow();
    win->id = g_nextWindowId++;
    
    const char* titleStr = moon_is_string(title) ? title->data.strVal : "MoonLang";
    int w = moon_is_int(width) ? (int)moon_to_int(width) : 800;
    int h = moon_is_int(height) ? (int)moon_to_int(height) : 600;
    
    int scaledW = ScaleForDpi(w);
    int scaledH = ScaleForDpi(h);
    
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = 0;
    
    bool centerWindow = true;
    int customX = CW_USEDEFAULT;
    int customY = CW_USEDEFAULT;
    
    if (options && moon_is_dict(options)) {
        MoonValue* framelessVal = moon_dict_get(options, moon_string("frameless"), moon_bool(false));
        MoonValue* transparentVal = moon_dict_get(options, moon_string("transparent"), moon_bool(false));
        MoonValue* topmostVal = moon_dict_get(options, moon_string("topmost"), moon_bool(false));
        MoonValue* resizableVal = moon_dict_get(options, moon_string("resizable"), moon_bool(true));
        MoonValue* centerVal = moon_dict_get(options, moon_string("center"), moon_bool(true));
        MoonValue* xVal = moon_dict_get(options, moon_string("x"), moon_null());
        MoonValue* yVal = moon_dict_get(options, moon_string("y"), moon_null());
        MoonValue* alphaVal = moon_dict_get(options, moon_string("alpha"), moon_null());
        MoonValue* clickThroughVal = moon_dict_get(options, moon_string("clickThrough"), moon_bool(false));
        MoonValue* devtoolsVal = moon_dict_get(options, moon_string("devtools"), moon_bool(false));
        MoonValue* parentVal = moon_dict_get(options, moon_string("parent"), moon_null());
        
        win->frameless = moon_to_bool(framelessVal);
        win->transparent = moon_to_bool(transparentVal);
        win->topmost = moon_to_bool(topmostVal);
        win->resizable = moon_to_bool(resizableVal);
        win->clickThrough = moon_to_bool(clickThroughVal);
        win->devtools = moon_to_bool(devtoolsVal);
        centerWindow = moon_to_bool(centerVal);
        
        if (moon_is_int(parentVal)) {
            win->parentId = (int)moon_to_int(parentVal);
        }
        
        if (moon_is_int(alphaVal)) {
            win->alpha = (int)moon_to_int(alphaVal);
            if (win->alpha < 0) win->alpha = 0;
            if (win->alpha > 255) win->alpha = 255;
        } else if (moon_is_float(alphaVal)) {
            double a = moon_to_float(alphaVal);
            win->alpha = (int)(a * 255.0);
            if (win->alpha < 0) win->alpha = 0;
            if (win->alpha > 255) win->alpha = 255;
        }
        
        if (moon_is_int(xVal)) {
            customX = ScaleForDpi((int)moon_to_int(xVal));
            centerWindow = false;
        }
        if (moon_is_int(yVal)) {
            customY = ScaleForDpi((int)moon_to_int(yVal));
            centerWindow = false;
        }
        
        moon_release(framelessVal);
        moon_release(transparentVal);
        moon_release(topmostVal);
        moon_release(resizableVal);
        moon_release(centerVal);
        moon_release(xVal);
        moon_release(yVal);
        moon_release(alphaVal);
        moon_release(clickThroughVal);
        moon_release(devtoolsVal);
        moon_release(parentVal);
        
        if (win->transparent) win->frameless = true;
        
        if (win->frameless) {
            if (win->transparent) {
                // Transparent mode keeps popup style for true alpha composition.
                // This avoids hidden frame artifacts and preserves pure transparent visuals.
                style = WS_POPUP;
            } else {
                // Frameless with Win11 animations (like Wails):
                // Keep WS_CAPTION for DWM to recognize as a normal window
                style = WS_OVERLAPPEDWINDOW;
            }
        }
        if (!win->resizable) {
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        }
        if (win->topmost) {
            exStyle |= WS_EX_TOPMOST;
        }
        if (win->transparent || win->alpha < 255 || win->frameless) {
            exStyle |= WS_EX_LAYERED;  // Also for frameless fade-in effect
        }
        if (win->clickThrough) {
            exStyle |= WS_EX_TRANSPARENT;
        }
    }
    
    int posX, posY;
    if (centerWindow) {
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        posX = (screenW - scaledW) / 2;
        posY = (screenH - scaledH) / 2;
    } else {
        posX = (customX != CW_USEDEFAULT) ? customX : 100;
        posY = (customY != CW_USEDEFAULT) ? customY : 100;
    }
    
    // Get parent window handle if specified
    HWND parentHwnd = NULL;
    if (win->parentId > 0) {
        MoonWindow* parentWin = GetWindowById(win->parentId);
        if (parentWin) parentHwnd = parentWin->hwnd;
    }
    
    wchar_t* wtitle = utf8_to_wchar(titleStr);
    
    const wchar_t* className = win->transparent ? L"MoonLangWindowTransparent" : L"MoonLangWindow";
    HWND hwnd = CreateWindowExW(
        exStyle,
        className,
        wtitle,
        style,
        posX, posY, scaledW, scaledH,
        parentHwnd, NULL, g_hInstance, win  // Pass win as lpParam
    );
    
    free(wtitle);
    
    if (!hwnd) {
        delete win;
        return moon_int(0);
    }
    
    // Update DPI scale
    UpdateDpiScale(hwnd);
    
    // Set layered window attributes.
    // For transparent windows, force explicit alpha mode to avoid resize-time
    // hit-test fallback that can behave like click-through on new pixels.
    if (win->transparent || win->alpha < 255) {
        SetLayeredWindowAttributes(hwnd, 0, (BYTE)win->alpha, LWA_ALPHA);
    }
    
    // BUG FIX (do not revert): Do NOT set DWMWA_WINDOW_CORNER_PREFERENCE for transparent
    // windows. It causes a visible rectangular transparent layer/band around the content.
    // Round look must come from HTML/CSS (border-radius) only for transparent windows.
    if (!win->transparent) {
        DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));
    }

    // Transparent frameless: hide DWM border.
    if (win->frameless && win->transparent) {
        COLORREF borderNone = DWMWA_COLOR_NONE;
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &borderNone, sizeof(borderNone));
    }

    // Frameless windows: use Windows default border rendering for native look.
    if (win->frameless && !win->transparent) {
        COLORREF borderColor = DWMWA_COLOR_DEFAULT;
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
    }
    
    // For frameless windows: extend DWM frame into client area (like Wails)
    // This enables Win11 animations while allowing us to hide the title bar
    if (win->frameless && !win->transparent) {
        MARGINS margins = { -1, -1, -1, -1 };  // Extend frame to entire window
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
    
    // Register window
    g_windows[win->id] = win;
    
    // Set as tray owner if first window
    if (!g_trayOwnerWindow) {
        g_trayOwnerWindow = hwnd;
    }
    
    // For frameless windows: force frame update BEFORE showing.
    // This applies WM_NCCALCSIZE consistently across transparent/non-transparent.
    if (win->frameless) {
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, 
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
        if (!win->transparent) {
            // Start fully transparent, fade in after content loads
            SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
            win->showPending = true;
        }
    }
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return moon_int(win->id);
}

// ============================================================================
// Window Operations (by Window ID)
// ============================================================================

void moon_gui_load_html_win(MoonValue* winId, MoonValue* html) {
    if (!moon_is_int(winId) || !moon_is_string(html)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win) return;
    
    if (win->pendingHtml) free(win->pendingHtml);
    win->pendingHtml = _strdup(html->data.strVal);
    if (win->pendingUrl) { free(win->pendingUrl); win->pendingUrl = NULL; }
    
    if (win->webviewReady) {
        LoadPendingContentForWindow(win);
    }
}

void moon_gui_load_url_win(MoonValue* winId, MoonValue* url) {
    if (!moon_is_int(winId) || !moon_is_string(url)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win) return;
    
    if (win->pendingUrl) free(win->pendingUrl);
    win->pendingUrl = _strdup(url->data.strVal);
    if (win->pendingHtml) { free(win->pendingHtml); win->pendingHtml = NULL; }
    
    if (win->webviewReady) {
        LoadPendingContentForWindow(win);
    }
}

void moon_gui_show_win(MoonValue* winId, MoonValue* show) {
    if (!moon_is_int(winId)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->hwnd) return;
    
    if (moon_to_bool(show)) {
        ShowWindow(win->hwnd, SW_SHOW);
        SetForegroundWindow(win->hwnd);
    } else {
        ShowWindow(win->hwnd, SW_HIDE);
    }
}

void moon_gui_set_title_win(MoonValue* winId, MoonValue* title) {
    if (!moon_is_int(winId) || !moon_is_string(title)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->hwnd) return;
    
    wchar_t* wtitle = utf8_to_wchar(title->data.strVal);
    SetWindowTextW(win->hwnd, wtitle);
    free(wtitle);
}

void moon_gui_set_size_win(MoonValue* winId, MoonValue* w, MoonValue* h) {
    if (!moon_is_int(winId)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->hwnd) return;
    
    SetWindowPos(win->hwnd, NULL, 0, 0, 
                 ScaleForDpi((int)moon_to_int(w)), 
                 ScaleForDpi((int)moon_to_int(h)),
                 SWP_NOMOVE | SWP_NOZORDER);
}

void moon_gui_set_position_win(MoonValue* winId, MoonValue* x, MoonValue* y) {
    if (!moon_is_int(winId)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->hwnd) return;
    
    int posX = (int)moon_to_int(x);
    int posY = (int)moon_to_int(y);
    
    // Center if x or y is -1
    if (posX == -1 || posY == -1) {
        RECT rect;
        GetWindowRect(win->hwnd, &rect);
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int winW = rect.right - rect.left;
        int winH = rect.bottom - rect.top;
        posX = (screenW - winW) / 2;
        posY = (screenH - winH) / 2;
    } else {
        posX = ScaleForDpi(posX);
        posY = ScaleForDpi(posY);
    }
    
    SetWindowPos(win->hwnd, NULL, posX, posY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void moon_gui_close_win(MoonValue* winId) {
    if (!moon_is_int(winId)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->hwnd) return;
    
    PostMessageW(win->hwnd, WM_CLOSE, 0, 0);
}

void moon_gui_minimize_win(MoonValue* winId) {
    if (!moon_is_int(winId)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->hwnd) return;
    
    ShowWindow(win->hwnd, SW_MINIMIZE);
}

void moon_gui_maximize_win(MoonValue* winId) {
    if (!moon_is_int(winId)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->hwnd) return;
    
    if (IsZoomed(win->hwnd)) {
        ShowWindow(win->hwnd, SW_RESTORE);
    } else {
        ShowWindow(win->hwnd, SW_MAXIMIZE);
    }
}

void moon_gui_restore_win(MoonValue* winId) {
    if (!moon_is_int(winId)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->hwnd) return;
    
    ShowWindow(win->hwnd, SW_RESTORE);
}

void moon_gui_on_message_win(MoonValue* winId, MoonValue* callback) {
    if (!moon_is_int(winId)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win) return;
    
    if (win->messageCallback) {
        moon_release(win->messageCallback);
    }
    win->messageCallback = callback;
    if (callback) {
        moon_retain(callback);
    }
}

void moon_gui_on_close_win(MoonValue* winId, MoonValue* callback) {
    if (!moon_is_int(winId)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win) return;
    
    if (win->closeCallback) {
        moon_release(win->closeCallback);
    }
    win->closeCallback = callback;
    if (callback) {
        moon_retain(callback);
    }
}

void moon_gui_expose(MoonValue* name, MoonValue* callback) {
    if (!moon_is_string(name)) return;
    
    MoonWindow* win = GetFirstWindow();
    if (!win) return;
    
    std::string funcName = name->data.strVal;
    
    // Release old callback if exists
    auto it = win->exposedFuncs.find(funcName);
    if (it != win->exposedFuncs.end() && it->second) {
        moon_release(it->second);
    }
    
    // Store new callback
    if (callback) {
        moon_retain(callback);
    }
    win->exposedFuncs[funcName] = callback;
}

void moon_gui_expose_win(MoonValue* winId, MoonValue* name, MoonValue* callback) {
    if (!moon_is_int(winId) || !moon_is_string(name)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win) return;
    
    std::string funcName = name->data.strVal;
    
    // Release old callback if exists
    auto it = win->exposedFuncs.find(funcName);
    if (it != win->exposedFuncs.end() && it->second) {
        moon_release(it->second);
    }
    
    // Store new callback
    if (callback) {
        moon_retain(callback);
    }
    win->exposedFuncs[funcName] = callback;
}

MoonValue* moon_gui_eval_win(MoonValue* winId, MoonValue* js) {
    if (!moon_is_int(winId) || !moon_is_string(js)) return moon_null();
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->webview) return moon_null();
    
    wchar_t* wjs = utf8_to_wchar(js->data.strVal);
    win->webview->ExecuteScript(wjs, nullptr);
    free(wjs);
    
    return moon_null();
}

void moon_gui_post_message_win(MoonValue* winId, MoonValue* msg) {
    if (!moon_is_int(winId) || !moon_is_string(msg)) return;
    
    MoonWindow* win = GetWindowById((int)moon_to_int(winId));
    if (!win || !win->webview) return;
    
    // Use ExecuteScript to dispatch moonmessage event (consistent with interpreter)
    std::string msgStr = msg->data.strVal;
    std::wstring js = L"window.dispatchEvent(new CustomEvent('moonmessage', {detail: ";
    js += utf8_to_wstring(msgStr);
    js += L"}));";
    win->webview->ExecuteScript(js.c_str(), nullptr);
}

// ============================================================================
// System Tray Support
// ============================================================================

MoonValue* moon_gui_tray_create(MoonValue* tooltip, MoonValue* iconPath) {
    if (g_windows.empty()) return moon_bool(false);
    
    if (!g_trayOwnerWindow) {
        g_trayOwnerWindow = g_windows.begin()->second->hwnd;
    }
    
    memset(&g_trayIcon, 0, sizeof(g_trayIcon));
    g_trayIcon.cbSize = sizeof(NOTIFYICONDATAW);
    g_trayIcon.hWnd = g_trayOwnerWindow;
    g_trayIcon.uID = 1;
    g_trayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_trayIcon.uCallbackMessage = WM_TRAYICON;
    
    g_trayIcon.hIcon = g_appIconSmall ? g_appIconSmall : LoadIcon(NULL, IDI_APPLICATION);
    
    if (moon_is_string(iconPath) && strlen(iconPath->data.strVal) > 0) {
        wchar_t* wpath = utf8_to_wchar(iconPath->data.strVal);
        HICON customIcon = (HICON)LoadImageW(NULL, wpath, IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
        if (customIcon) {
            g_trayIcon.hIcon = customIcon;
        }
        free(wpath);
    }
    
    if (moon_is_string(tooltip)) {
        wchar_t* wtip = utf8_to_wchar(tooltip->data.strVal);
        wcsncpy_s(g_trayIcon.szTip, wtip, 127);
        free(wtip);
    }
    
    g_hasTray = Shell_NotifyIconW(NIM_ADD, &g_trayIcon) != FALSE;
    return moon_bool(g_hasTray);
}

void moon_gui_tray_remove(void) {
    if (g_hasTray) {
        Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
        g_hasTray = false;
    }
    if (g_trayMenu) {
        DestroyMenu(g_trayMenu);
        g_trayMenu = NULL;
    }
}

MoonValue* moon_gui_tray_set_menu(MoonValue* items) {
    if (!moon_is_list(items)) return moon_bool(false);
    
    if (g_trayMenu) {
        DestroyMenu(g_trayMenu);
    }
    g_trayMenu = CreatePopupMenu();
    
    MoonList* list = items->data.listVal;
    for (int i = 0; i < list->length; i++) {
        MoonValue* item = list->items[i];
        if (moon_is_list(item) && item->data.listVal->length >= 2) {
            MoonValue* labelVal = item->data.listVal->items[0];
            
            if (moon_is_string(labelVal)) {
                const char* label = labelVal->data.strVal;
                if (strcmp(label, "-") == 0) {
                    AppendMenuW(g_trayMenu, MF_SEPARATOR, 0, NULL);
                } else {
                    wchar_t* wlabel = utf8_to_wchar(label);
                    AppendMenuW(g_trayMenu, MF_STRING, IDM_TRAY_BASE + i, wlabel);
                    free(wlabel);
                }
            }
        }
    }
    
    return moon_bool(true);
}

void moon_gui_tray_on_click(MoonValue* callback) {
    if (g_trayCallback) {
        moon_release(g_trayCallback);
    }
    g_trayCallback = callback;
    if (callback) {
        moon_retain(callback);
    }
}

// ============================================================================
// Backward Compatible Single-Window API
// ============================================================================

void moon_gui_load_url(MoonValue* url) {
    MoonWindow* win = GetFirstWindow();
    if (!win || !moon_is_string(url)) return;
    
    if (win->pendingUrl) free(win->pendingUrl);
    win->pendingUrl = _strdup(url->data.strVal);
    if (win->pendingHtml) { free(win->pendingHtml); win->pendingHtml = NULL; }
    
    if (win->webviewReady) {
        LoadPendingContentForWindow(win);
    }
}

void moon_gui_load_html(MoonValue* html) {
    MoonWindow* win = GetFirstWindow();
    if (!win || !moon_is_string(html)) return;
    
    if (win->pendingHtml) free(win->pendingHtml);
    win->pendingHtml = _strdup(html->data.strVal);
    if (win->pendingUrl) { free(win->pendingUrl); win->pendingUrl = NULL; }
    
    if (win->webviewReady) {
        LoadPendingContentForWindow(win);
    }
}

void moon_gui_on_message(MoonValue* callback) {
    MoonWindow* win = GetFirstWindow();
    if (!win) return;
    
    if (win->messageCallback) {
        moon_release(win->messageCallback);
    }
    win->messageCallback = callback;
    if (callback) {
        moon_retain(callback);
    }
}

void moon_gui_show_window(MoonValue* show) {
    MoonWindow* win = GetFirstWindow();
    if (!win || !win->hwnd) return;
    
    if (moon_to_bool(show)) {
        ShowWindow(win->hwnd, SW_SHOW);
        SetForegroundWindow(win->hwnd);
    } else {
        ShowWindow(win->hwnd, SW_HIDE);
    }
}

// ============================================================================
// Message Loop
// ============================================================================

void moon_gui_run(void) {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void moon_gui_quit(void) {
    moon_gui_tray_remove();
    
    // Post WM_CLOSE to all windows instead of immediately destroying them
    // This avoids re-entrancy issues when gui_quit is called from a callback
    for (auto& pair : g_windows) {
        if (pair.second && pair.second->hwnd) {
            PostMessage(pair.second->hwnd, WM_CLOSE, 0, 0);
        }
    }
}

// ============================================================================
// Legacy API (for backward compatibility)
// ============================================================================

MoonValue* moon_gui_create(MoonValue* options) {
    return moon_gui_create_advanced(moon_string("MoonLang"), moon_int(800), moon_int(600), options);
}

void moon_gui_show(MoonValue* winVal, MoonValue* show) {
    // If winVal is an int (window ID), use window-specific version
    if (moon_is_int(winVal)) {
        moon_gui_show_win(winVal, show);
    } else {
        // Legacy: first param is ignored, show is the actual boolean
        moon_gui_show_window(show ? show : winVal);
    }
}

void moon_gui_set_title(MoonValue* winVal, MoonValue* title) {
    if (moon_is_int(winVal)) {
        moon_gui_set_title_win(winVal, title);
    } else {
        MoonWindow* win = GetFirstWindow();
        if (win && win->hwnd && moon_is_string(winVal)) {
            wchar_t* wtitle = utf8_to_wchar(winVal->data.strVal);
            SetWindowTextW(win->hwnd, wtitle);
            free(wtitle);
        }
    }
}

void moon_gui_set_size(MoonValue* winVal, MoonValue* w, MoonValue* h) {
    if (moon_is_int(winVal) && w && h) {
        moon_gui_set_size_win(winVal, w, h);
    } else {
        MoonWindow* win = GetFirstWindow();
        if (win && win->hwnd) {
            SetWindowPos(win->hwnd, NULL, 0, 0, 
                         (int)moon_to_int(winVal), (int)moon_to_int(w),
                         SWP_NOMOVE | SWP_NOZORDER);
        }
    }
}

void moon_gui_set_position(MoonValue* winVal, MoonValue* x, MoonValue* y) {
    if (moon_is_int(winVal) && x && y) {
        moon_gui_set_position_win(winVal, x, y);
    } else {
        MoonWindow* win = GetFirstWindow();
        if (win && win->hwnd) {
            SetWindowPos(win->hwnd, NULL, 
                         (int)moon_to_int(winVal), (int)moon_to_int(x),
                         0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    }
}

void moon_gui_close(MoonValue* winVal) {
    if (moon_is_int(winVal)) {
        moon_gui_close_win(winVal);
    } else {
        MoonWindow* win = GetFirstWindow();
        if (win && win->hwnd) {
            PostMessageW(win->hwnd, WM_CLOSE, 0, 0);
        }
    }
}

// ============================================================================
// Message Box
// ============================================================================

MoonValue* moon_gui_alert(MoonValue* msg) {
    if (!moon_is_string(msg)) return moon_null();
    
    HWND hwnd = GetFirstWindow() ? GetFirstWindow()->hwnd : NULL;
    wchar_t* wmsg = utf8_to_wchar(msg->data.strVal);
    MessageBoxW(hwnd, wmsg, L"MoonLang", MB_OK | MB_ICONINFORMATION);
    free(wmsg);
    
    return moon_null();
}

MoonValue* moon_gui_confirm(MoonValue* msg) {
    if (!moon_is_string(msg)) return moon_bool(false);
    
    HWND hwnd = GetFirstWindow() ? GetFirstWindow()->hwnd : NULL;
    wchar_t* wmsg = utf8_to_wchar(msg->data.strVal);
    int result = MessageBoxW(hwnd, wmsg, L"MoonLang", MB_YESNO | MB_ICONQUESTION);
    free(wmsg);
    
    return moon_bool(result == IDYES);
}

} // extern "C"

#else
// ============================================================================
// Non-Windows Stubs
// ============================================================================

extern "C" {

MoonValue* moon_gui_init(void) { return moon_bool(false); }
MoonValue* moon_gui_create(MoonValue* options) { return moon_int(0); }
MoonValue* moon_gui_create_advanced(MoonValue* title, MoonValue* width, MoonValue* height, MoonValue* options) { return moon_int(0); }

void moon_gui_load_html_win(MoonValue* winId, MoonValue* html) {}
void moon_gui_load_url_win(MoonValue* winId, MoonValue* url) {}
void moon_gui_show_win(MoonValue* winId, MoonValue* show) {}
void moon_gui_set_title_win(MoonValue* winId, MoonValue* title) {}
void moon_gui_set_size_win(MoonValue* winId, MoonValue* w, MoonValue* h) {}
void moon_gui_set_position_win(MoonValue* winId, MoonValue* x, MoonValue* y) {}
void moon_gui_close_win(MoonValue* winId) {}
void moon_gui_minimize_win(MoonValue* winId) {}
void moon_gui_maximize_win(MoonValue* winId) {}
void moon_gui_restore_win(MoonValue* winId) {}
void moon_gui_on_message_win(MoonValue* winId, MoonValue* callback) {}
void moon_gui_on_close_win(MoonValue* winId, MoonValue* callback) {}
MoonValue* moon_gui_eval_win(MoonValue* winId, MoonValue* js) { return moon_null(); }
void moon_gui_post_message_win(MoonValue* winId, MoonValue* msg) {}
void moon_gui_expose(MoonValue* name, MoonValue* callback) {}
void moon_gui_expose_win(MoonValue* winId, MoonValue* name, MoonValue* callback) {}

MoonValue* moon_gui_tray_create(MoonValue* tooltip, MoonValue* iconPath) { return moon_bool(false); }
void moon_gui_tray_remove(void) {}
MoonValue* moon_gui_tray_set_menu(MoonValue* items) { return moon_bool(false); }
void moon_gui_tray_on_click(MoonValue* callback) {}

void moon_gui_show_window(MoonValue* show) {}
void moon_gui_load_url(MoonValue* url) {}
void moon_gui_load_html(MoonValue* html) {}
void moon_gui_on_message(MoonValue* callback) {}

void moon_gui_show(MoonValue* win, MoonValue* show) {}
void moon_gui_set_title(MoonValue* win, MoonValue* title) {}
void moon_gui_set_size(MoonValue* win, MoonValue* w, MoonValue* h) {}
void moon_gui_set_position(MoonValue* win, MoonValue* x, MoonValue* y) {}
void moon_gui_close(MoonValue* win) {}

void moon_gui_run(void) {}
void moon_gui_quit(void) {}
MoonValue* moon_gui_alert(MoonValue* msg) { return moon_null(); }
MoonValue* moon_gui_confirm(MoonValue* msg) { return moon_bool(false); }

} // extern "C"

#endif
