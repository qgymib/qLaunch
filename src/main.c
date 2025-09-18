#include <windows.h>
#include <commctrl.h>
#include "config.h"

#define WIDE2(x) L##x
#define WIDE(x) WIDE2(x)

#define QLAUNCH_MSGBOX_ERROR(msg)                                                                  \
    do                                                                                             \
    {                                                                                              \
        std::wstring _prefix =                                                                     \
            std::format(L"[%s:%d %s] ", Utf8ToWide(__FILE__), __LINE__, Utf8ToWide(__FUNCTION__)); \
        std::wstring _msg = _prefix + msg;                                                         \
        MessageBoxW(nullptr, _msg.c_str(), WIDE(QLAUNCH_PROGRAM_NAME), MB_ICONERROR | MB_OK);      \
    } while (0)

typedef struct LaunchApp
{
    HINSTANCE hInst; /* Current application instance. */
    HWND      w;     /* MainWindow */
    HWND      hList; /* ListView */
} LaunchApp;

static LaunchApp* App = NULL;

/**
 * Converts a UTF-8 encoded string to a wide-character string.
 *
 * The function uses Windows API functions to perform the conversion.
 * If the input string is empty, an empty wide-character string is returned.
 *
 * @param[in] utf8 UTF-8 encoded input string to be converted.
 * @return A wide-character (UTF-16) representation of the input string. Use free() to release it.
 */
wchar_t* utf8_to_wide(const char* utf8)
{
    if (!utf8)
    {
        return NULL;
    }

    // Determine required buffer size (in wchar_t units), including null terminator.
    int needed = MultiByteToWideChar(CP_UTF8,              // source is UTF-8
                                     MB_ERR_INVALID_CHARS, // fail on invalid sequences
                                     utf8,                 // source string
                                     -1,                   // -1 means input is null-terminated
                                     NULL,                 // no output buffer yet
                                     0                     // request size only
    );

    if (needed == 0)
    {
        // Conversion size query failed (GetLastError() for details)
        return NULL;
    }

    // Allocate buffer for the wide string
    wchar_t* wstr = (wchar_t*)malloc((size_t)needed * sizeof(wchar_t));
    if (!wstr)
    {
        return NULL;
    }

    // Perform the conversion
    int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wstr, needed);

    if (written == 0)
    {
        free(wstr);
        return NULL;
    }

    return wstr; // null-terminated
}

static void AddColumnsAndItems(HWND lv)
{
    LVCOLUMNW lvc;
    lvc.mask = LVCF_WIDTH;
    lvc.cx = 500;
    ListView_InsertColumn(lv, 0, &lvc);

    LVITEMW item;
    item.mask = LVIF_TEXT;
    item.iItem = 0;
    item.pszText = (LPWSTR)(L"");
    ListView_InsertItem(lv, &item);
}

static void CreateListView(HWND hwnd)
{
    /* Init ListView. */
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    /* Create ListView. */
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
                  LVS_NOCOLUMNHEADER | LVS_SHOWSELALWAYS | LVS_OWNERDRAWFIXED;
    App->hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"", style, 0, 0, 0, 0, hwnd,
                                 (HMENU)100, App->hInst, NULL);
    ListView_SetExtendedListViewStyle(App->hList, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT |
                                                      LVS_EX_BORDERSELECT);

    AddColumnsAndItems(App->hList);
}

static void ReleaseLaunchApp()
{
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        CreateListView(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        ReleaseLaunchApp();
        return 0;
    case WM_SIZE:
        if (App->hList)
        {
            MoveWindow(App->hList, 0, 0, LOWORD(1), HIWORD(1), TRUE);
            ListView_SetColumnWidth(App->hList, 0, LVSCW_AUTOSIZE_USEHEADER);
        }
        return 0;
    case WM_MEASUREITEM: {
        LPMEASUREITEMSTRUCT mis = (LPMEASUREITEMSTRUCT)lParam;
        if (mis->CtlID == 100)
        {
            mis->itemHeight = 120;
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID == 100 && dis->itemID != (UINT)-1)
        {
            return TRUE;
        }
        break;
    }
    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void AtLauncherExit()
{
    free(App);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    atexit(AtLauncherExit);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = WIDE(QLAUNCH_WND_CLASS_NAME);
    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(NULL, L"RegisterClassExW failed!", L"Error", MB_ICONERROR);
        exit(EXIT_FAILURE);
    }

    App = calloc(1, sizeof(*App));
    App->hInst = hInstance;
    App->w = CreateWindowExW(0, WIDE(QLAUNCH_WND_CLASS_NAME), WIDE(QLAUNCH_PROGRAM_NAME),
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 420, NULL,
                             NULL, hInstance, NULL);
    if (!App->w)
    {
        MessageBoxW(NULL, L"CreateWindowExW failed!", L"Error", MB_ICONERROR);
        exit(EXIT_FAILURE);
    }

    ShowWindow(App->w, nCmdShow);
    UpdateWindow(App->w);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)(msg.wParam);
}
