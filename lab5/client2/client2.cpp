// client2.cpp : Mailslot Client 2 (Laboratory Work #5)
// Variant 9: toolbar width (SM_CXEDGE); status bar height (SM_CYEDGE); number of colors (GetDeviceCaps(hdc, NUMCOLORS))

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LOADSTRING 100
#define ID_CLIENT_SEND    8001
#define ID_CLIENT_REFRESH 8002
#define IDM_EXIT          8003

HINSTANCE hInst;
const wchar_t szWindowClass[] = L"MailslotClient2Class";
const wchar_t szTitle[] = L"Client 2";
const wchar_t MailslotName[] = L"\\\\.\\mailslot\\lab5_mailslot";

HWND hwndMain = nullptr;
HWND hwndEdit = nullptr;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void GenerateMetrics(HWND hWnd);
void SendMailslotData(HWND hWnd);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MyRegisterClass(hInstance);
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = szWindowClass;
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    HMENU hMenuBar = CreateMenu();
    HMENU hMenuClient = CreatePopupMenu();

    AppendMenuW(hMenuClient, MF_STRING, ID_CLIENT_SEND, L"Send Data to Server");
    AppendMenuW(hMenuClient, MF_STRING, ID_CLIENT_REFRESH, L"Refresh System Metrics");
    AppendMenuW(hMenuClient, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuClient, MF_STRING, IDM_EXIT, L"Exit");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuClient, L"Client");

    // Position Client 2 window middle-right
    hwndMain = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        530, 320, 480, 270, nullptr, hMenuBar, hInstance, nullptr);

    if (!hwndMain) return FALSE;

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);
    return TRUE;
}

void GenerateMetrics(HWND hWnd)
{
    // 1. Toolbar 3D border width (SM_CXEDGE)
    int cxEdge = GetSystemMetrics(SM_CXEDGE);

    // 2. Status bar 3D border height (SM_CYEDGE)
    int cyEdge = GetSystemMetrics(SM_CYEDGE);

    // 3. Number of palette colors (GetDeviceCaps(hdc, NUMCOLORS))
    HDC hdc = GetDC(hWnd);
    int numColors = GetDeviceCaps(hdc, NUMCOLORS);
    int bitsPixel = GetDeviceCaps(hdc, BITSPIXEL);
    int planes = GetDeviceCaps(hdc, PLANES);
    ReleaseDC(hWnd, hdc);

    char msgBuf[2048];
    sprintf_s(msgBuf,
        "Client #2 Data:\r\n"
        " - Toolbar 3D border width (SM_CXEDGE) = %d px\r\n"
        " - Status bar 3D border height (SM_CYEDGE) = %d px\r\n"
        " - Palette colors (GetDeviceCaps NUMCOLORS) = %d (palette)\r\n"
        " - Color depth (BITSPIXEL) = %d bits/pixel (planes: %d, colors: 2^%d)",
        cxEdge, cyEdge, numColors, bitsPixel, planes, bitsPixel);

    SetWindowTextA(hwndEdit, msgBuf);
}

void SendMailslotData(HWND hWnd)
{
    int textLen = GetWindowTextLengthA(hwndEdit);
    if (textLen <= 0)
    {
        MessageBoxW(hWnd, L"Text field is empty. No data to send.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    char* buffer = (char*)malloc(textLen + 2);
    if (!buffer) return;

    GetWindowTextA(hwndEdit, buffer, textLen + 1);

    // Attempt to open server mailslot
    HANDLE hMailslot = CreateFileW(
        MailslotName,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hMailslot == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        wchar_t errMsg[256];
        swprintf_s(errMsg, L"Mailslot server error: %lu\r\n(Make sure the server is running!)", err);
        MessageBoxW(hWnd, errMsg, L"Send Error", MB_OK | MB_ICONERROR);
        free(buffer);
        return;
    }

    DWORD cbWritten = 0;
    BOOL bSuccess = WriteFile(hMailslot, buffer, (DWORD)strlen(buffer) + 1, &cbWritten, nullptr);
    CloseHandle(hMailslot);
    free(buffer);

    if (bSuccess)
    {
        MessageBoxW(hWnd, L"Data sent to mailslot server successfully!", L"Client 2", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxW(hWnd, L"Failed to write to mailslot.", L"Write Error", MB_OK | MB_ICONERROR);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        hwndEdit = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            10, 10, rc.right - 20, rc.bottom - 20, hWnd, nullptr, hInst, nullptr);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

        GenerateMetrics(hWnd);
        break;
    }
    case WM_SIZE:
    {
        if (hwndEdit)
        {
            MoveWindow(hwndEdit, 10, 10, LOWORD(lParam) - 20, HIWORD(lParam) - 20, TRUE);
        }
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_CLIENT_SEND:
            SendMailslotData(hWnd);
            break;
        case ID_CLIENT_REFRESH:
            GenerateMetrics(hWnd);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
