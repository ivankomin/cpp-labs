#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LOADSTRING 100
#define SHARED_MEM_SIZE 4096

#define ID_CLIENT_SEND    8001
#define ID_CLIENT_REFRESH 8002
#define IDM_ABOUT         8003
#define IDM_EXIT          8004

HINSTANCE hInst;
const wchar_t szWindowClass[] = L"FileMappingClient1Class";
const wchar_t szTitle[]       = L"Client 1";
const wchar_t MAPPING_NAME[]  = L"Local\\Lab6_FileMapping";
const wchar_t MUTEX_NAME[]    = L"Local\\Lab6_Mutex";
const wchar_t EVENT_NAME[]    = L"Local\\Lab6_DataReadyEvent";

HWND hwndMain = nullptr;
HWND hwndEdit = nullptr;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void GenerateMetrics(HWND hWnd);
void SendSharedMemoryData(HWND hWnd);

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
    HMENU hMenuHelp = CreatePopupMenu();

    AppendMenuW(hMenuClient, MF_STRING, ID_CLIENT_SEND,    L"Send Data to Server");
    AppendMenuW(hMenuClient, MF_STRING, ID_CLIENT_REFRESH, L"Refresh System Metrics");
    AppendMenuW(hMenuClient, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuClient, MF_STRING, IDM_EXIT,          L"Exit");

    AppendMenuW(hMenuHelp, MF_STRING, IDM_ABOUT, L"About...");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuClient, L"Client");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp,   L"Help");

    // Position Client 1 window top-right (x=530, y=30, w=480, h=270)
    hwndMain = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        530, 30, 480, 270, nullptr, hMenuBar, hInstance, nullptr);

    if (!hwndMain) return FALSE;

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);
    return TRUE;
}

void GenerateMetrics(HWND hWnd)
{
    // 1. Network / Proxy status (SM_NETWORK in WinAPI / SM_NETDRIVE in syllabus)
    int netState = GetSystemMetrics(SM_NETWORK);
    const char* netDesc = (netState & 0x01) ? "Network active (connected)" : "Not connected / offline";

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
    ReleaseDC(hWnd, hdc);

    char msgBuf[2048];
    sprintf_s(msgBuf,
        "Client #1 Data:\r\n"
        " - Network / Proxy status (SM_NETWORK) = %s (0x%02X)\r\n"
        " - Primary Screen Width (SM_CXSCREEN) = %d px\r\n"
        " - System Uptime (GetTickCount64) = %llu h %llu m %llu s (%llu ms)\r\n"
        " - Screen Horizontal Resolution (GetDeviceCaps HORZRES) = %d px",
        netDesc, netState,
        screenWidthSM,
        hours, minutes, seconds, uptimeMs,
        horzRes);

    SetWindowTextA(hwndEdit, msgBuf);
}

void SendSharedMemoryData(HWND hWnd)
{
    int textLen = GetWindowTextLengthA(hwndEdit);
    if (textLen <= 0)
    {
        MessageBoxW(hWnd, L"Text field is empty. No data to send.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    char* buffer = (char*)malloc(textLen + 1);
    if (!buffer) return;
    GetWindowTextA(hwndEdit, buffer, textLen + 1);

    // 1. Open existing File Mapping object created by server
    HANDLE hMap = OpenFileMappingW(FILE_MAP_WRITE, FALSE, MAPPING_NAME);
    if (hMap == nullptr)
    {
        DWORD err = GetLastError();
        wchar_t errMsg[256];
        swprintf_s(errMsg, L"Cannot open file mapping object (Error %lu).\r\nPlease ensure the Server is started!", err);
        MessageBoxW(hWnd, errMsg, L"FileMapping Error", MB_OK | MB_ICONERROR);
        free(buffer);
        return;
    }

    // 2. Open synchronization Mutex and Event
    HANDLE hMutex = OpenMutexW(SYNCHRONIZE, FALSE, MUTEX_NAME);
    HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, EVENT_NAME);

    // 3. Acquire mutex to prevent concurrent write collisions
    if (hMutex != nullptr)
    {
        WaitForSingleObject(hMutex, 5000);
    }

    // 4. Map shared memory view and write payload
    LPVOID pBuf = MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, SHARED_MEM_SIZE);
    if (pBuf != nullptr)
    {
        ZeroMemory(pBuf, SHARED_MEM_SIZE);
        CopyMemory(pBuf, buffer, strlen(buffer) + 1);
        UnmapViewOfFile(pBuf);

        // 5. Signal Event for automatic detection on Server (Task with Asterisk *)
        if (hEvent != nullptr)
        {
            SetEvent(hEvent);
        }

        MessageBoxW(hWnd, L"Data written to shared memory successfully!\r\nServer notified automatically via Event (*).", L"Client 1", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxW(hWnd, L"MapViewOfFile failed in client process.", L"Error", MB_OK | MB_ICONERROR);
    }

    // 6. Release mutex and cleanup handles
    if (hMutex != nullptr)
    {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    if (hEvent != nullptr)
    {
        CloseHandle(hEvent);
    }
    CloseHandle(hMap);
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
            SendSharedMemoryData(hWnd);
            break;
        case ID_CLIENT_REFRESH:
            GenerateMetrics(hWnd);
            break;
        case IDM_ABOUT:
            MessageBoxW(hWnd,
                L"Laboratory Work #6 - Client 1\r\n"
                L"Variant 9: Network, Screen Width & Resolution\r\n"
                L"Uses Memory-Mapped File (FileMapping) with Mutex & Event (*)",
                L"About Client 1", MB_OK | MB_ICONINFORMATION);
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
