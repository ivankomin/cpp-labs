#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <atomic>

#define PIPE_BUF_SIZE     4096
#define WM_APP_PIPE_LOG   (WM_USER + 101)

#define ID_PIPE_START     32771
#define ID_PIPE_STOP      32772
#define ID_LOG_CLEAR      32773
#define IDM_ABOUT         32774
#define IDM_EXIT          32775

HINSTANCE hInst;
const wchar_t szWindowClass[] = L"NamedPipeServerClass";
const wchar_t szTitle[]       = L"Server";
const wchar_t PIPE_NAME[]     = L"\\\\.\\pipe\\LOCAL\\Lab7_NamedPipe";

HWND hwndMain = nullptr;
HWND hwndEdit = nullptr;

HANDLE hStopEvent      = nullptr;
HANDLE hListenerThread = nullptr;
std::atomic<bool> g_bServerRunning{ false };
std::atomic<LONG> g_activeClients{ 0 };
std::atomic<LONG> g_clientCounter{ 0 };

struct ClientContext
{
    HANDLE hPipe;
    HWND   hWnd;
    HANDLE hStopEventDup;
    LONG   clientId;
};

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AppendLogA(HWND hEdit, const char* text);
void AppendLogW(HWND hEdit, const wchar_t* text);
void PostLogToGui(HWND hWnd, const std::string& text);
void StartServer(HWND hWnd);
void StopServer(HWND hWnd, bool silent);
DWORD WINAPI PipeListenerThreadProc(LPVOID lpParam);
DWORD WINAPI ClientHandlerThreadProc(LPVOID lpParam);

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

    HMENU hMenuBar  = CreateMenu();
    HMENU hMenuPipe = CreatePopupMenu();
    HMENU hMenuHelp = CreatePopupMenu();

    AppendMenuW(hMenuPipe, MF_STRING, ID_PIPE_START, L"Start Server (Create Named Pipe)");
    AppendMenuW(hMenuPipe, MF_STRING, ID_PIPE_STOP,  L"Stop Server (Close Named Pipe)");
    AppendMenuW(hMenuPipe, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuPipe, MF_STRING, ID_LOG_CLEAR,  L"Clear Log");
    AppendMenuW(hMenuPipe, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuPipe, MF_STRING, IDM_EXIT,      L"Exit");

    AppendMenuW(hMenuHelp, MF_STRING, IDM_ABOUT, L"About...");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuPipe, L"NamedPipe");
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
    SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
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

void PostLogToGui(HWND hWnd, const std::string& text)
{
    std::string* payload = new std::string(text);
    if (!PostMessageW(hWnd, WM_APP_PIPE_LOG, 0, (LPARAM)payload))
    {
        delete payload;
    }
}

static BOOL OverlappedReadPipe(HANDLE hPipe, HANDLE hStop, char* buf, DWORD bufSize, DWORD* bytesRead)
{
    HANDLE hIoEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hIoEvent) return FALSE;

    OVERLAPPED ov = {};
    ov.hEvent = hIoEvent;

    *bytesRead = 0;
    BOOL ok = ReadFile(hPipe, buf, bufSize, bytesRead, &ov);
    if (!ok)
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            HANDLE waitHandles[2] = { hStop, hIoEvent };
            DWORD dwWait = WaitForMultipleObjects(2, waitHandles, FALSE, 5000);
            if (dwWait == WAIT_OBJECT_0 + 1)
            {
                ok = GetOverlappedResult(hPipe, &ov, bytesRead, FALSE);
            }
            else
            {
                CancelIo(hPipe);
                ok = FALSE;
            }
        }
    }

    CloseHandle(hIoEvent);
    return ok;
}

static BOOL OverlappedWritePipe(HANDLE hPipe, HANDLE hStop, const char* buf, DWORD bufSize, DWORD* bytesWritten)
{
    HANDLE hIoEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hIoEvent) return FALSE;

    OVERLAPPED ov = {};
    ov.hEvent = hIoEvent;

    *bytesWritten = 0;
    BOOL ok = WriteFile(hPipe, buf, bufSize, bytesWritten, &ov);
    if (!ok)
    {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            HANDLE waitHandles[2] = { hStop, hIoEvent };
            DWORD dwWait = WaitForMultipleObjects(2, waitHandles, FALSE, 5000);
            if (dwWait == WAIT_OBJECT_0 + 1)
            {
                ok = GetOverlappedResult(hPipe, &ov, bytesWritten, FALSE);
            }
            else
            {
                CancelIo(hPipe);
                ok = FALSE;
            }
        }
    }

    CloseHandle(hIoEvent);
    return ok;
}

DWORD WINAPI ClientHandlerThreadProc(LPVOID lpParam)
{
    ClientContext* ctx = (ClientContext*)lpParam;
    g_activeClients.fetch_add(1);

    char readBuf[PIPE_BUF_SIZE];
    while (g_bServerRunning.load())
    {
        ZeroMemory(readBuf, sizeof(readBuf));
        DWORD cbRead = 0;
        if (!OverlappedReadPipe(ctx->hPipe, ctx->hStopEventDup, readBuf, PIPE_BUF_SIZE - 1, &cbRead) || cbRead == 0)
        {
            break;
        }

        readBuf[cbRead] = '\0';

        SYSTEMTIME st;
        GetLocalTime(&st);

        char logEntry[PIPE_BUF_SIZE + 256];
        sprintf_s(logEntry,
            "--------------------------------------------------\r\n"
            "[PIPE MSG #%ld | %lu bytes | %02d:%02d:%02d.%03d]\r\n"
            "%s\r\n",
            ctx->clientId,
            cbRead,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            readBuf);
        PostLogToGui(ctx->hWnd, logEntry);

        // Full-duplex reply back to client
        char replyBuf[256];
        sprintf_s(replyBuf,
            "ACK from Server: received %lu bytes via Named Pipe at %02d:%02d:%02d.%03d",
            cbRead, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

        DWORD cbWritten = 0;
        OverlappedWritePipe(ctx->hPipe, ctx->hStopEventDup, replyBuf, (DWORD)(strlen(replyBuf) + 1), &cbWritten);
        FlushFileBuffers(ctx->hPipe);
    }

    DisconnectNamedPipe(ctx->hPipe);
    CloseHandle(ctx->hPipe);
    CloseHandle(ctx->hStopEventDup);
    delete ctx;

    g_activeClients.fetch_sub(1);
    return 0;
}

DWORD WINAPI PipeListenerThreadProc(LPVOID lpParam)
{
    HWND hWnd = (HWND)lpParam;

    while (g_bServerRunning.load())
    {
        // Create a new Named Pipe instance supporting multiple concurrent clients (overlapped duplex message mode)
        HANDLE hPipe = CreateNamedPipeW(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            PIPE_BUF_SIZE,
            PIPE_BUF_SIZE,
            5000,
            nullptr);

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            char errMsg[256];
            sprintf_s(errMsg, "[ERROR] CreateNamedPipeW failed (Error %lu).", err);
            PostLogToGui(hWnd, errMsg);
            Sleep(500);
            continue;
        }

        HANDLE hConnectEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!hConnectEvent)
        {
            CloseHandle(hPipe);
            break;
        }

        OVERLAPPED ov = {};
        ov.hEvent = hConnectEvent;

        BOOL fConnected = ConnectNamedPipe(hPipe, &ov);
        if (!fConnected)
        {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING)
            {
                HANDLE waitHandles[2] = { hStopEvent, hConnectEvent };
                DWORD dwWait = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                if (dwWait == WAIT_OBJECT_0)
                {
                    // Stop requested
                    CancelIo(hPipe);
                    CloseHandle(hConnectEvent);
                    CloseHandle(hPipe);
                    break;
                }
                else if (dwWait == WAIT_OBJECT_0 + 1)
                {
                    DWORD dummy = 0;
                    fConnected = GetOverlappedResult(hPipe, &ov, &dummy, FALSE);
                }
            }
            else if (err == ERROR_PIPE_CONNECTED)
            {
                // Client connected between CreateNamedPipeW and ConnectNamedPipe
                fConnected = TRUE;
            }
        }

        CloseHandle(hConnectEvent);

        if (!fConnected || !g_bServerRunning.load())
        {
            CloseHandle(hPipe);
            if (!g_bServerRunning.load()) break;
            continue;
        }

        // Duplicate stop event for the worker thread so StopServer can safely close its handle
        HANDLE hStopDup = nullptr;
        DuplicateHandle(
            GetCurrentProcess(), hStopEvent,
            GetCurrentProcess(), &hStopDup,
            0, FALSE, DUPLICATE_SAME_ACCESS);

        ClientContext* ctx = new ClientContext();
        ctx->hPipe = hPipe;
        ctx->hWnd = hWnd;
        ctx->hStopEventDup = hStopDup;
        ctx->clientId = g_clientCounter.fetch_add(1) + 1;

        HANDLE hWorker = CreateThread(nullptr, 0, ClientHandlerThreadProc, ctx, 0, nullptr);
        if (hWorker)
        {
            CloseHandle(hWorker);
        }
        else
        {
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            if (hStopDup) CloseHandle(hStopDup);
            delete ctx;
        }
    }

    return 0;
}

void StartServer(HWND hWnd)
{
    if (g_bServerRunning.load())
    {
        MessageBoxW(hWnd, L"Named Pipe Server is already running and listening.", L"Info", MB_OK | MB_ICONINFORMATION);
        return;
    }

    hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hStopEvent)
    {
        MessageBoxW(hWnd, L"Failed to create stop synchronization event.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    g_bServerRunning.store(true);
    hListenerThread = CreateThread(nullptr, 0, PipeListenerThreadProc, hWnd, 0, nullptr);
    if (!hListenerThread)
    {
        g_bServerRunning.store(false);
        CloseHandle(hStopEvent);
        hStopEvent = nullptr;
        MessageBoxW(hWnd, L"Failed to create pipe listener thread.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    AppendLogW(hwndEdit, L" [Named Pipe Server Successfully Started]");
    AppendLogW(hwndEdit, L"Waiting for clients to connect...\r\n");
}

void StopServer(HWND hWnd, bool silent)
{
    if (!g_bServerRunning.load())
    {
        if (!silent)
        {
            MessageBoxW(hWnd, L"Named Pipe Server is not currently running.", L"Info", MB_OK | MB_ICONINFORMATION);
        }
        return;
    }

    g_bServerRunning.store(false);

    if (hStopEvent)
    {
        SetEvent(hStopEvent);
    }

    if (hListenerThread)
    {
        WaitForSingleObject(hListenerThread, 3000);
        CloseHandle(hListenerThread);
        hListenerThread = nullptr;
    }

    // Briefly wait for any active client handler threads to finish cleanly
    for (int i = 0; i < 40 && g_activeClients.load() > 0; ++i)
    {
        Sleep(25);
    }

    if (hStopEvent)
    {
        CloseHandle(hStopEvent);
        hStopEvent = nullptr;
    }

    if (!silent)
    {
        AppendLogW(hwndEdit, L"[Server Stopped]");
        AppendLogW(hwndEdit, L"Named Pipe listener closed. All pipe instances released.\r\n");
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
            L"Select menu 'NamedPipe' -> 'Start Server' to create pipe.\r\n\r\n",
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
    case WM_APP_PIPE_LOG:
    {
        std::string* payload = (std::string*)lParam;
        if (payload)
        {
            AppendLogA(hwndEdit, payload->c_str());
            delete payload;
        }
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_PIPE_START:
            StartServer(hWnd);
            break;
        case ID_PIPE_STOP:
            StopServer(hWnd, false);
            break;
        case ID_LOG_CLEAR:
            SetWindowTextW(hwndEdit, L"Server log cleared.\r\n\r\n");
            break;
        case IDM_ABOUT:
            MessageBoxW(hWnd,
                L"Laboratory Work #7\r\n"
                L"Topic: Client-Server Architecture. Named Pipes (IPC)\r\n"
                L"Variant 9: Network, screen metrics, 3D borders & colors\r\n"
                L"Features: Multithreaded Overlapped Full-Duplex Pipe Server\r\n"
                L"Developed in Microsoft Visual Studio",
                L"About Named Pipe Server", MB_OK | MB_ICONINFORMATION);
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
        StopServer(hWnd, true);
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
