// client1.cpp : Mailslot Client 1 (Laboratory Work #5)
// Variant 9: proxy status (SM_NETDRIVE); system uptime (SM_CXSCREEN); screen width (GetDeviceCaps(hdc, HORZRES))

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LOADSTRING 100
#define ID_CLIENT_SEND    8001
#define ID_CLIENT_REFRESH 8002
#define IDM_EXIT          8003

HINSTANCE hInst;
const wchar_t szWindowClass[] = L"MailslotClient1Class";
const wchar_t szTitle[] = L"Client 1 - Variant 9";
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

    // Position Client 1 window top-right
    hwndMain = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        530, 30, 480, 270, nullptr, hMenuBar, hInstance, nullptr);

    if (!hwndMain) return FALSE;

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);
    return TRUE;
}

void GenerateMetrics(HWND hWnd)
{
    // 1. Network / Proxy status (SM_NETDRIVE in syllabus maps to SM_NETWORK in WinAPI)
    int netState = GetSystemMetrics(SM_NETWORK);
    const char* netDesc = (netState & 0x01) ? "Network active (connected)" : "Not connected";

    // 2. SM_CXSCREEN and System Uptime
    int screenWidthSM = GetSystemMetrics(SM_CXSCREEN);
    ULONGLONG uptimeMs = GetTickCount64();
    ULONGLONG totalSec = uptimeMs / 1000;
    ULONGLONG hours = totalSec / 3600;
    ULONGLONG minutes = (totalSec % 3600) / 60;
    ULONGLONG seconds = totalSec % 60;

    // 3. Screen horizontal resolution via GetDeviceCaps(hdc, HORZRES)
    HDC hdc = GetDC(hWnd);
    int horzRes = GetDeviceCaps(hdc, HORZRES);
    int vertRes = GetDeviceCaps(hdc, VERTRES);
    ReleaseDC(hWnd, hdc);

    char msgBuf[2048];
    sprintf_s(msgBuf,
        "Client #1 Data (Variant 9):\r\n"
        " - Proxy / Network status (SM_NETDRIVE / SM_NETWORK) = %s (0x%X)\r\n"
        " - System Uptime = %02llu:%02llu:%02llu (%llu sec)\r\n"
        " - Screen Width (SM_CXSCREEN) = %d px\r\n"
        " - Screen Resolution (GetDeviceCaps HORZRES) = %d px (VERTRES: %d px)",
        netDesc, netState, hours, minutes, seconds, totalSec, screenWidthSM, horzRes, vertRes);

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
        MessageBoxW(hWnd, L"Data sent to mailslot server successfully!", L"Client 1", MB_OK | MB_ICONINFORMATION);
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
