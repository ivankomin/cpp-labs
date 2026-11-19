#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#define MAX_LOADSTRING 100
#define SHARED_MEM_SIZE 4096
#define WM_APP_DATA_READY (WM_USER + 101)

#define ID_FILEMAPPING_START 32771
#define ID_FILEMAPPING_READ  32772
#define ID_FILEMAPPING_STOP  32773
#define ID_LOG_CLEAR         32774
#define IDM_ABOUT            32775
#define IDM_EXIT             32776

HINSTANCE hInst;
const wchar_t szWindowClass[] = L"FileMappingServerClass";
const wchar_t szTitle[] = L"Server";
const wchar_t MAPPING_NAME[] = L"Local\\Lab6_FileMapping";
const wchar_t MUTEX_NAME[]   = L"Local\\Lab6_Mutex";
const wchar_t EVENT_NAME[]   = L"Local\\Lab6_DataReadyEvent";

HWND hwndMain = nullptr;
HWND hwndEdit = nullptr;

HANDLE hMapFile = INVALID_HANDLE_VALUE;
HANDLE hMutex = nullptr;
HANDLE hEventDataReady = nullptr;
HANDLE hWatchdogThread = nullptr;
bool g_bServerRunning = false;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AppendLogA(HWND hEdit, const char* text);
void AppendLogW(HWND hEdit, const wchar_t* text);
void StartServer(HWND hWnd);
void StopServer(HWND hWnd);
void ReadSharedMemory(HWND hWnd, bool isAuto);
DWORD WINAPI WatchdogThreadProc(LPVOID lpParam);

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
    HMENU hMenuMapping = CreatePopupMenu();
    HMENU hMenuHelp = CreatePopupMenu();

    AppendMenuW(hMenuMapping, MF_STRING, ID_FILEMAPPING_START, L"Start Server (Create FileMapping)");
    AppendMenuW(hMenuMapping, MF_STRING, ID_FILEMAPPING_READ,  L"Read from Shared Memory (Manual)");
    AppendMenuW(hMenuMapping, MF_STRING, ID_FILEMAPPING_STOP,  L"Stop Server (Close FileMapping)");
    AppendMenuW(hMenuMapping, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuMapping, MF_STRING, ID_LOG_CLEAR, L"Clear Log");
    AppendMenuW(hMenuMapping, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuMapping, MF_STRING, IDM_EXIT, L"Exit");

    AppendMenuW(hMenuHelp, MF_STRING, IDM_ABOUT, L"About...");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuMapping, L"FileMapping");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp, L"Help");

    // Position server window on the left side (x=30, y=30, w=480, h=620)
    hwndMain = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        30, 30, 480, 620, nullptr, hMenuBar, hInstance, nullptr);

    if (!hwndMain) return FALSE;

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);
    return TRUE;
}

void AppendLogA(HWND hEdit, const char* text)
{
    int len = GetWindowTextLengthA(hEdit);
    SendMessageA(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)"\r\n\r\n");
    SendMessageA(hEdit, WM_VSCROLL, SB_BOTTOM, 0);
}

void AppendLogW(HWND hEdit, const wchar_t* text)
{
    int len = GetWindowTextLengthW(hEdit);
    SendMessageW(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    SendMessageW(hEdit, WM_VSCROLL, SB_BOTTOM, 0);
}

DWORD WINAPI WatchdogThreadProc(LPVOID lpParam)
{
    HWND hWnd = (HWND)lpParam;
    while (g_bServerRunning)
    {
        // Wait for event signaled by any client (with 250ms periodic check)
        DWORD dwWait = WaitForSingleObject(hEventDataReady, 250);
        if (dwWait == WAIT_OBJECT_0)
        {
            if (g_bServerRunning)
            {
                // notify GUI thread of incoming data automatically!
                PostMessageW(hWnd, WM_APP_DATA_READY, 0, 0);
            }
        }
    }
    return 0;
}

void StartServer(HWND hWnd)
{
    if (g_bServerRunning)
    {
        MessageBoxW(hWnd, L"Server FileMapping is already active and listening.", L"Info", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // 1. Create File Mapping object backed by system paging file
    hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,       // Use system paging file
        nullptr,                    // Default security attributes
        PAGE_READWRITE,             // Read/Write permissions
        0,                          // Maximum size (high-order DWORD)
        SHARED_MEM_SIZE,            // Maximum size (low-order DWORD): 4096 bytes
        MAPPING_NAME                // Name of mapping object
    );

    if (hMapFile == nullptr || hMapFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        wchar_t errMsg[256];
        swprintf_s(errMsg, L"[ERROR] CreateFileMapping failed. Error code: %lu", err);
        AppendLogW(hwndEdit, errMsg);
        MessageBoxW(hWnd, errMsg, L"FileMapping Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Zero-initialize mapped memory buffer
    LPVOID pInitBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
    if (pInitBuf != nullptr)
    {
        ZeroMemory(pInitBuf, SHARED_MEM_SIZE);
        UnmapViewOfFile(pInitBuf);
    }

    // 2. Create Named Mutex for inter-process synchronization
    hMutex = CreateMutexW(nullptr, FALSE, MUTEX_NAME);

    // 3. Create Auto-Reset Named Event for Task with Asterisk (*)
    hEventDataReady = CreateEventW(nullptr, FALSE, FALSE, EVENT_NAME);

    // 4. Spawn Watchdog listener thread
    g_bServerRunning = true;
    hWatchdogThread = CreateThread(nullptr, 0, WatchdogThreadProc, hWnd, 0, nullptr);

    AppendLogW(hwndEdit, L" [Server FileMapping Successfully Started]");
    AppendLogW(hwndEdit, L"Waiting for clients to write data...\r\n");
}

void StopServer(HWND hWnd)
{
    if (!g_bServerRunning)
    {
        MessageBoxW(hWnd, L"Server is not currently running.", L"Info", MB_OK | MB_ICONINFORMATION);
        return;
    }

    g_bServerRunning = false;

    // Wake watchdog thread if it is currently waiting
    if (hEventDataReady)
    {
        SetEvent(hEventDataReady);
    }

    if (hWatchdogThread)
    {
        WaitForSingleObject(hWatchdogThread, 1000);
        CloseHandle(hWatchdogThread);
        hWatchdogThread = nullptr;
    }

    if (hEventDataReady)
    {
        CloseHandle(hEventDataReady);
        hEventDataReady = nullptr;
    }

    if (hMutex)
    {
        CloseHandle(hMutex);
        hMutex = nullptr;
    }

    if (hMapFile != INVALID_HANDLE_VALUE && hMapFile != nullptr)
    {
        CloseHandle(hMapFile);
        hMapFile = INVALID_HANDLE_VALUE;
    }

    AppendLogW(hwndEdit, L"[Server Stopped]");
    AppendLogW(hwndEdit, L"FileMapping object closed. Shared memory released.\r\n");
}

void ReadSharedMemory(HWND hWnd, bool isAuto)
{
    if (!g_bServerRunning || hMapFile == INVALID_HANDLE_VALUE || hMapFile == nullptr)
    {
        if (!isAuto)
        {
            MessageBoxW(hWnd, L"FileMapping is not active. Please start the server first.", L"Warning", MB_OK | MB_ICONWARNING);
        }
        return;
    }

    // Acquire mutex for thread/process-safe read
    if (hMutex)
    {
        WaitForSingleObject(hMutex, 2000);
    }

    // Map view of file into server process address space
    LPVOID pBuf = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, SHARED_MEM_SIZE);
    if (pBuf == nullptr)
    {
        if (hMutex) ReleaseMutex(hMutex);
        if (!isAuto)
        {
            MessageBoxW(hWnd, L"MapViewOfFile failed to map shared memory view.", L"Map Error", MB_OK | MB_ICONERROR);
        }
        return;
    }

    const char* rawText = (const char*)pBuf;
    if (rawText[0] != '\0')
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char logBuffer[SHARED_MEM_SIZE + 256];
        sprintf_s(logBuffer,
            "--------------------------------------------------\r\n"
            "[%s at %02d:%02d:%02d.%03d]\r\n"
            "%s",
            isAuto ? "AUTO-DETECTED (*)" : "MANUAL READ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            rawText);

        AppendLogA(hwndEdit, logBuffer);
    }
    else if (!isAuto)
    {
        AppendLogW(hwndEdit, L"[MANUAL READ]: Shared memory buffer is currently empty.");
    }

    UnmapViewOfFile(pBuf);

    if (hMutex)
    {
        ReleaseMutex(hMutex);
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
        hwndEdit = CreateWindowW(L"EDIT",
            L"Select menu 'FileMapping' -> 'Start Server' to allocate shared memory.\r\n\r\n",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 10, rc.right - 20, rc.bottom - 20, hWnd, nullptr, hInst, nullptr);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
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
    case WM_APP_DATA_READY:
    {
        // Automatically detected changes in shared memory via Event (Task with Asterisk *)
        ReadSharedMemory(hWnd, true);
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_FILEMAPPING_START:
            StartServer(hWnd);
            break;
        case ID_FILEMAPPING_READ:
            ReadSharedMemory(hWnd, false);
            break;
        case ID_FILEMAPPING_STOP:
            StopServer(hWnd);
            break;
        case ID_LOG_CLEAR:
            SetWindowTextW(hwndEdit, L"Server log cleared.\r\n\r\n");
            break;
        case IDM_ABOUT:
            MessageBoxW(hWnd,
                L"Laboratory Work #6\r\n"
                L"Topic: Client-Server Architecture. File Mapping (Shared Memory)\r\n"
                L"Variant 9: Network, screen metrics & 3D borders\r\n"
                L"Task with Asterisk (*): Auto-detection of changes via Named Event & Mutex\r\n"
                L"Developed in Microsoft Visual Studio",
                L"About FileMapping Server", MB_OK | MB_ICONINFORMATION);
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
    {
        StopServer(hWnd);
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
