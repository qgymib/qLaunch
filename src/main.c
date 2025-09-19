#include <windows.h>
#include <commctrl.h>
#include "config.h"
#include "list.h"
#include "ini.h"

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

/**
 * Formats a wide string according to the specified format and arguments.
 * Similar to snprintf but for wide strings, returns dynamically allocated memory.
 *
 * @param[in] format Wide string format specifier
 * @param[in] ... Variable arguments to be formatted
 * @return Newly allocated wide string containing the formatted result.
 *         The caller is responsible for freeing the memory using free().
 *         Returns NULL if allocation fails or format is invalid.
 */
static wchar_t* wcsformat(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);

    // First, determine required buffer size
    va_list args_copy;
    va_copy(args_copy, args);
    int len = _vsnwprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (len < 0)
    {
        va_end(args);
        return NULL;
    }

    // Allocate buffer (including space for null terminator)
    wchar_t* buffer = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!buffer)
    {
        va_end(args);
        return NULL;
    }

    // Format the string
    _vsnwprintf(buffer, len + 1, format, args);
    va_end(args);

    return buffer;
}

/**
 * @brief Get the absolute path where this executable file located.
 * @return Unicode path. Use free() when need to release it.
 */
static wchar_t* GetExeDirectory(void)
{
    wchar_t* path = NULL;
    DWORD    len = MAX_PATH;
    DWORD    size;

    do
    {
        path = (wchar_t*)realloc(path, len * sizeof(wchar_t));
        if (!path)
        {
            return NULL;
        }
        size = GetModuleFileNameW(NULL, path, len);
        if (size == 0)
        {
            free(path);
            return NULL;
        }
        if (size < len)
        {
            wchar_t* p = wcsrchr(path, L'\\');
            if (p)
            {
                *p = L'\0';
            }
            return path;
        }
        len *= 2;
    } while (len <= 32768); // Max reasonable path length

    free(path);
    return NULL;
}

static int OnParseIni(void* user, const char* section, const char* name, const char* value)
{
    (void)user;
    (void)section;
    (void)name;
    (void)value;
    return TRUE;
}

static void ParseIniConfig()
{
    wchar_t* exe_dir = GetExeDirectory();
    wchar_t* ini_path = wcsformat(L"%s\\%s", exe_dir, WIDE(QLAUNCH_CONFIG_FILE_NAME));
    free(exe_dir);
    exe_dir = NULL;

    FILE* ini_file = _wfopen(ini_path, L"rb");
    free(ini_path);
    ini_path = NULL;
    if (ini_file == NULL)
    {
        MessageBoxW(NULL, L"Open ini file failed!", L"Error", MB_ICONERROR | MB_OK);
        exit(EXIT_FAILURE);
    }

    int ret = ini_parse_file(ini_file, OnParseIni, NULL);
    fclose(ini_file);
    ini_file = NULL;

    if (ret < 0)
    {
        MessageBoxW(NULL, L"Parse ini file failed!", L"Error", MB_ICONERROR | MB_OK);
        exit(EXIT_FAILURE);
    }
}

static void AddColumnsAndItems(HWND lv)
{
    LVCOLUMNW lvc;
    lvc.mask = LVCF_WIDTH;
    lvc.cx = 500;
    ListView_InsertColumn(lv, 0, &lvc);

    ParseIniConfig();

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
    App = NULL;
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
