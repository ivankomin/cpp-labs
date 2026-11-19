#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIPE_BUF_SIZE     4096

#define ID_CLIENT_SEND    8001
#define ID_CLIENT_REFRESH 8002
#define IDM_ABOUT         8003
#define IDM_EXIT          8004

HINSTANCE hInst;
const wchar_t szWindowClass[] = L"NamedPipeClient2Class";
const wchar_t szTitle[]       = L"Client 2";
const wchar_t PIPE_NAME[]     = L"\\\\.\\pipe\\LOCAL\\Lab7_NamedPipe";

HWND hwndMain = nullptr;
HWND hwndEdit = nullptr;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void GenerateMetrics(HWND hWnd);
void SendPipeData(HWND hWnd);

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

    HMENU hMenuBar    = CreateMenu();
    HMENU hMenuClient = CreatePopupMenu();
    HMENU hMenuHelp   = CreatePopupMenu();

    AppendMenuW(hMenuClient, MF_STRING, ID_CLIENT_SEND,    L"Send Data via Named Pipe");
    AppendMenuW(hMenuClient, MF_STRING, ID_CLIENT_REFRESH, L"Refresh System Metrics");
    AppendMenuW(hMenuClient, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuClient, MF_STRING, IDM_EXIT,          L"Exit");

    AppendMenuW(hMenuHelp, MF_STRING, IDM_ABOUT, L"About...");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuClient, L"Client");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp,   L"Help");

    // Position Client 2 window center-right (x=530, y=320, w=480, h=270)
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

    // 3. Number of palette colors and color depth
    HDC hdc = GetDC(hWnd);
    int numColors = GetDeviceCaps(hdc, NUMCOLORS);
    int bitsPixel = GetDeviceCaps(hdc, BITSPIXEL);
    int planes    = GetDeviceCaps(hdc, PLANES);
    ReleaseDC(hWnd, hdc);

    char msgBuf[2048];
    sprintf_s(msgBuf,
        "Client #2 Data:\r\n"
        " - Toolbar 3D border width (SM_CXEDGE) = %d px\r\n"
        " - Status bar 3D border height (SM_CYEDGE) = %d px\r\n"
        " - Palette colors (GetDeviceCaps NUMCOLORS) = %d (palette entries)\r\n"
        " - Color depth (BITSPIXEL) = %d bits/pixel (planes: %d, colors: 2^%d)",
        cxEdge, cyEdge,
        numColors,
        bitsPixel, planes, bitsPixel);

    SetWindowTextA(hwndEdit, msgBuf);
}

void SendPipeData(HWND hWnd)
{
    int textLen = GetWindowTextLengthA(hwndEdit);
    if (textLen <= 0)
    {
        MessageBoxW(hWnd, L"Text field is empty. No data to send.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    int copyLen = (textLen >= PIPE_BUF_SIZE) ? (PIPE_BUF_SIZE - 1) : textLen;
    char* buffer = (char*)malloc((size_t)copyLen + 1);
    if (!buffer) return;
    GetWindowTextA(hwndEdit, buffer, copyLen + 1);

    // 1. Open existing Named Pipe created by Server
    HANDLE hPipe = CreateFileW(
        PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (hPipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY)
    {
        if (WaitNamedPipeW(PIPE_NAME, 3000))
        {
            hPipe = CreateFileW(
                PIPE_NAME,
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);
        }
    }

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        wchar_t errMsg[256];
        swprintf_s(errMsg, L"Cannot open Named Pipe (Error %lu).\r\nPlease ensure the Server is started!", err);
        MessageBoxW(hWnd, errMsg, L"Named Pipe Error", MB_OK | MB_ICONERROR);
        free(buffer);
        return;
    }

    // 2. Switch client handle to message-read mode
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &dwMode, nullptr, nullptr);

    // 3. Send payload to server via WriteFile
    DWORD cbWritten = 0;
    BOOL fSuccess = WriteFile(hPipe, buffer, (DWORD)(strlen(buffer) + 1), &cbWritten, nullptr);
    if (!fSuccess)
    {
        DWORD err = GetLastError();
        wchar_t errMsg[256];
        swprintf_s(errMsg, L"WriteFile failed (Error %lu).", err);
        MessageBoxW(hWnd, errMsg, L"Named Pipe Error", MB_OK | MB_ICONERROR);
        CloseHandle(hPipe);
        free(buffer);
        return;
    }

    // 4. Read full-duplex confirmation reply from server
    char replyBuf[512] = { 0 };
    DWORD cbRead = 0;
    if (ReadFile(hPipe, replyBuf, sizeof(replyBuf) - 1, &cbRead, nullptr) && cbRead > 0)
    {
        replyBuf[cbRead] = '\0';
        char infoMsg[768];
        sprintf_s(infoMsg, "Data sent via Named Pipe (%lu bytes)!\r\n\r\nServer reply:\r\n%s", cbWritten, replyBuf);
        MessageBoxA(hWnd, infoMsg, "Client 2", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxW(hWnd, L"Data sent to Named Pipe successfully!", L"Client 2", MB_OK | MB_ICONINFORMATION);
    }

    // 5. Close pipe handle
    CloseHandle(hPipe);
    free(buffer);
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
            SendPipeData(hWnd);
            break;
        case ID_CLIENT_REFRESH:
            GenerateMetrics(hWnd);
            break;
        case IDM_ABOUT:
            MessageBoxW(hWnd,
                L"Laboratory Work #7 - Client 2\r\n"
                L"Variant 9: 3D Borders & Color Capacity\r\n"
                L"Uses Windows Named Pipe (Full-Duplex Message Mode)",
                L"About Client 2", MB_OK | MB_ICONINFORMATION);
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
