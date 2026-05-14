#include "menu.h"

KonataMenu::KonataMenu()
    : m_hwnd(nullptr), m_parentWnd(nullptr), m_isVisible(false),
    m_hSubMenu(nullptr), m_hFpsSubMenu(nullptr), m_hQualitySubMenu(nullptr), m_hMainMenu(nullptr),
    m_callbackTarget(nullptr),
    m_onfscalechange(nullptr), m_onfpschange(nullptr), m_onqualitychange(nullptr), m_onClose(nullptr),
    m_currentScale(1.0f), m_currentFps(10), m_qualityMode(1) {

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"KonataMenuWindow";
    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, L"KonataMenuWindow", L"", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), this);

    BuildMenu();
}
KonataMenu::~KonataMenu() {
    CleanupMenu();
    if (m_hwnd && IsWindow(m_hwnd)) DestroyWindow(m_hwnd);
}
void KonataMenu::CleanupMenu() {
    if (m_hMainMenu) {
        DestroyMenu(m_hMainMenu);
        m_hMainMenu = nullptr;
    }
    m_hSubMenu = nullptr;
    m_hFpsSubMenu = nullptr;
    m_hQualitySubMenu = nullptr;
}
void KonataMenu::SetCallbacks(
    void* target,
    void(__cdecl* onfscalechange)(void*, float),
    void(__cdecl* onfpschange)(void*, int),
    void(__cdecl* onqualitychange)(void*, int),
    void(__cdecl* onClose)(void*)
) {
    m_callbackTarget = target;
    m_onfscalechange = onfscalechange;
    m_onfpschange = onfpschange;
    m_onqualitychange = onqualitychange;
    m_onClose = onClose;
}

void KonataMenu::BuildMenu() {
    CleanupMenu();

    m_hMainMenu = CreatePopupMenu();
    m_hSubMenu = CreatePopupMenu();
    AppendMenuW(m_hSubMenu, MF_STRING, IDM_SCALE_50, L"50%");
    AppendMenuW(m_hSubMenu, MF_STRING, IDM_SCALE_75, L"75%");
    AppendMenuW(m_hSubMenu, MF_STRING, IDM_SCALE_100, L"100%");
    AppendMenuW(m_hSubMenu, MF_STRING, IDM_SCALE_125, L"125%");
    AppendMenuW(m_hSubMenu, MF_STRING, IDM_SCALE_150, L"150%");
    AppendMenuW(m_hSubMenu, MF_STRING, IDM_SCALE_200, L"200%");

    int scaleCheckId = IDM_SCALE_100;
    if (m_currentScale <= 0.5f) scaleCheckId = IDM_SCALE_50;
    else if (m_currentScale <= 0.75f) scaleCheckId = IDM_SCALE_75;
    else if (m_currentScale <= 1.0f) scaleCheckId = IDM_SCALE_100;
    else if (m_currentScale <= 1.25f) scaleCheckId = IDM_SCALE_125;
    else if (m_currentScale <= 1.5f) scaleCheckId = IDM_SCALE_150;
    else scaleCheckId = IDM_SCALE_200;

    CheckMenuRadioItem(m_hSubMenu, IDM_SCALE_50, IDM_SCALE_200, scaleCheckId, MF_BYCOMMAND);
    m_hFpsSubMenu = CreatePopupMenu();
    AppendMenuW(m_hFpsSubMenu, MF_STRING, IDM_FPS_15, L"15 FPS");
    AppendMenuW(m_hFpsSubMenu, MF_STRING, IDM_FPS_24, L"24 FPS");
    AppendMenuW(m_hFpsSubMenu, MF_STRING, IDM_FPS_30, L"30 FPS");
    AppendMenuW(m_hFpsSubMenu, MF_STRING, IDM_FPS_60, L"60 FPS");
    int fpsCheckId = IDM_FPS_60;
    if (m_currentFps <= 15) fpsCheckId = IDM_FPS_15;
    else if (m_currentFps <= 24) fpsCheckId = IDM_FPS_24;
    else if (m_currentFps <= 30) fpsCheckId = IDM_FPS_30;

    CheckMenuRadioItem(m_hFpsSubMenu, IDM_FPS_15, IDM_FPS_60, fpsCheckId, MF_BYCOMMAND);

    m_hQualitySubMenu = CreatePopupMenu();
    AppendMenuW(m_hQualitySubMenu, MF_STRING, IDM_QUALITY_POINT, L"Point (pixelated)");//well i don't see much difference maybe because i have a shitty monitor
    AppendMenuW(m_hQualitySubMenu, MF_STRING, IDM_QUALITY_LINEAR, L"Linear (smooth)");//here too
    CheckMenuRadioItem(m_hQualitySubMenu, IDM_QUALITY_POINT, IDM_QUALITY_LINEAR,
        m_qualityMode == 0 ? IDM_QUALITY_POINT : IDM_QUALITY_LINEAR, MF_BYCOMMAND);

    AppendMenuW(m_hMainMenu, MF_STRING | MF_POPUP, (UINT_PTR)m_hSubMenu, L"scale");
    AppendMenuW(m_hMainMenu, MF_STRING | MF_POPUP, (UINT_PTR)m_hFpsSubMenu, L"FPS");
    AppendMenuW(m_hMainMenu, MF_STRING | MF_POPUP, (UINT_PTR)m_hQualitySubMenu, L"filter Mode");
    AppendMenuW(m_hMainMenu, MF_SEPARATOR, 0, nullptr);

    wchar_t infoText[256];
    swprintf_s(infoText, L"current: %.0f%% %d FPS %s",
        m_currentScale * 100,
        m_currentFps,
        m_qualityMode == 0 ? L"point" : L"linear");
    AppendMenuW(m_hMainMenu, MF_STRING | MF_GRAYED | MF_DISABLED, 0, infoText);

    AppendMenuW(m_hMainMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_hMainMenu, MF_STRING, IDM_EXIT, L"exit menu");
}

void KonataMenu::UpdateScale(float scale) {
    m_currentScale = scale;
}
void KonataMenu::UpdateFps(int fps) {
    m_currentFps = fps;
}
void KonataMenu::UpdateQuality(int mode) {
    m_qualityMode = mode;
}
void KonataMenu::HandleMenuCommand(int commandId) {
    switch (commandId) {
    case IDM_SCALE_50:
        if (m_onfscalechange) m_onfscalechange(m_callbackTarget, 0.5f);
        break;
    case IDM_SCALE_75:
        if (m_onfscalechange) m_onfscalechange(m_callbackTarget, 0.75f);
        break;
    case IDM_SCALE_100:
        if (m_onfscalechange) m_onfscalechange(m_callbackTarget, 1.0f);
        break;
    case IDM_SCALE_125:
        if (m_onfscalechange) m_onfscalechange(m_callbackTarget, 1.25f);
        break;
    case IDM_SCALE_150:
        if (m_onfscalechange) m_onfscalechange(m_callbackTarget, 1.5f);
        break;
    case IDM_SCALE_200:
        if (m_onfscalechange) m_onfscalechange(m_callbackTarget, 2.0f);
        break;

    case IDM_FPS_15:
        if (m_onfpschange) m_onfpschange(m_callbackTarget, 15);
        break;
    case IDM_FPS_24:
        if (m_onfpschange) m_onfpschange(m_callbackTarget, 24);
        break;
    case IDM_FPS_30:
        if (m_onfpschange) m_onfpschange(m_callbackTarget, 30);
        break;
    case IDM_FPS_60:
        if (m_onfpschange) m_onfpschange(m_callbackTarget, 60);
        break;

    case IDM_QUALITY_POINT:
        if (m_onqualitychange) m_onqualitychange(m_callbackTarget, 0);
        break;
    case IDM_QUALITY_LINEAR:
        if (m_onqualitychange) m_onqualitychange(m_callbackTarget, 1);
        break;

    case IDM_EXIT:
        EndMenu();//BUG FIX, for some fucking reason if try to end this shit with Hide then some shit just doesn't works, switched to EndMenu
        return;
    }
}
void KonataMenu::Show(HWND parent, int x, int y) {
    m_parentWnd = parent;
    m_isVisible = true;
    BuildMenu();
    SetForegroundWindow(m_hwnd);
    TrackPopupMenuEx(m_hMainMenu, TPM_LEFTALIGN | TPM_TOPALIGN, x, y, m_hwnd, nullptr);
    m_isVisible = false;
    if (m_onClose && m_callbackTarget) {
        m_onClose(m_callbackTarget);
    }
}
void KonataMenu::Hide() {
    if (!m_isVisible) return;

    m_isVisible = false;
    EndMenu();

    if (m_onClose && m_callbackTarget) {
        m_onClose(m_callbackTarget);
    }
}
LRESULT CALLBACK KonataMenu::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    KonataMenu* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<KonataMenu*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else {
        pThis = reinterpret_cast<KonataMenu*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (pThis) return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
LRESULT KonataMenu::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        int commandId = LOWORD(wParam);
        HandleMenuCommand(commandId);
        return 0;
    }

    case WM_ENTERMENULOOP:
        m_isVisible = true;
        return 0;

    case WM_EXITMENULOOP:
        m_isVisible = false;
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}