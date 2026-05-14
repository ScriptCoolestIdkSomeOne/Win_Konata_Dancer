#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <map>

struct MenuItemInfo {
    int id;
    std::wstring text;
    bool isSeparator;
    bool isDisabled;

    MenuItemInfo() : id(0), isSeparator(false), isDisabled(false) {}
    MenuItemInfo(int _id, const std::wstring& _text) : id(_id), text(_text), isSeparator(false), isDisabled(false) {}
    MenuItemInfo(bool sep) : id(0), isSeparator(sep), isDisabled(false) {}
};
class KonataMenu {
private:
    HWND m_hwnd;
    HWND m_parentWnd;
    bool m_isVisible;
    HMENU m_hSubMenu;
    HMENU m_hFpsSubMenu;
    HMENU m_hQualitySubMenu;
    HMENU m_hMainMenu;
    static const int IDM_SCALE_50 = 1001;
    static const int IDM_SCALE_75 = 1002;
    static const int IDM_SCALE_100 = 1003;
    static const int IDM_SCALE_125 = 1004;
    static const int IDM_SCALE_150 = 1005;
    static const int IDM_SCALE_200 = 1006;

    static const int IDM_FPS_10 = 2000;//basic
    static const int IDM_FPS_15 = 2001;
    static const int IDM_FPS_24 = 2002;
    static const int IDM_FPS_30 = 2003;
    static const int IDM_FPS_60 = 2004;

    static const int IDM_QUALITY_POINT = 3001;
    static const int IDM_QUALITY_LINEAR = 3002;

    static const int IDM_EXIT = 9999;
    void* m_callbackTarget;
    void(__cdecl* m_onfscalechange)(void*, float);
    void(__cdecl* m_onfpschange)(void*, int);
    void(__cdecl* m_onqualitychange)(void*, int);
    void(__cdecl* m_onClose)(void*);

    float m_currentScale;
    int m_currentFps;
    int m_qualityMode;

    void BuildMenu();
    void CleanupMenu();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void HandleMenuCommand(int commandId);

public:
    KonataMenu();
    ~KonataMenu();

    void SetCallbacks(
        void* target,
        void(__cdecl* onfscalechange)(void*, float),
        void(__cdecl* onfpschange)(void*, int),
        void(__cdecl* onqualitychange)(void*, int),
        void(__cdecl* onClose)(void*)
    );

    void UpdateScale(float scale);
    void UpdateFps(int fps);
    void UpdateQuality(int mode);
    void Show(HWND parent, int x, int y);
    void Hide();
    HWND GetHWND() { return m_hwnd; }
    bool IsVisible() { return m_isVisible; }
};