#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LOADSTRING 100
#define IDT_TIMER 1001

#define ID_MAILSLOT_CREATE 32771
#define ID_MAILSLOT_CLOSE  32772
#define ID_LOG_CLEAR       32773
#define IDM_ABOUT          32774
#define IDM_EXIT           32775

HINSTANCE hInst;
const wchar_t szWindowClass[] = L"MailslotServerClass";
const wchar_t szTitle[] = L"Server - Mailslot";
const wchar_t MailslotName[] = L"\\\\.\\mailslot\\lab5_mailslot";

HWND hwndMain = nullptr;
HWND hwndEdit = nullptr;
HANDLE hMailslot = INVALID_HANDLE_VALUE;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AppendLogA(HWND hEdit, const char* text);
void AppendLogW(HWND hEdit, const wchar_t* text);

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
    HMENU hMenuMailslot = CreatePopupMenu();
    HMENU hMenuHelp = CreatePopupMenu();

    AppendMenuW(hMenuMailslot, MF_STRING, ID_MAILSLOT_CREATE, L"Start Server (Create)");
    AppendMenuW(hMenuMailslot, MF_STRING, ID_MAILSLOT_CLOSE, L"Stop Server (Close)");
    AppendMenuW(hMenuMailslot, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuMailslot, MF_STRING, ID_LOG_CLEAR, L"Clear Log");
    AppendMenuW(hMenuMailslot, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuMailslot, MF_STRING, IDM_EXIT, L"Exit");

    AppendMenuW(hMenuHelp, MF_STRING, IDM_ABOUT, L"About...");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuMailslot, L"MailSlot");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp, L"Help");

    // Position server window on the left side
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
    SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)"\r\n--------------------------------------------\r\n");
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        hwndEdit = CreateWindowW(L"EDIT",
            L"Laboratory Work #5 - Mailslot Server.\r\n"
            L"Status: Ready to start.\r\n"
            L"Select menu 'MailSlot' -> 'Start Server (Create)' to begin.\r\n\r\n",
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
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_MAILSLOT_CREATE:
        {
            if (hMailslot != INVALID_HANDLE_VALUE)
            {
                MessageBoxW(hWnd, L"Server is already running and waiting for messages.", L"Info", MB_OK | MB_ICONINFORMATION);
                break;
            }

            // Create mailslot
            hMailslot = CreateMailslotW(MailslotName, 0, MAILSLOT_WAIT_FOREVER, nullptr);
            if (hMailslot == INVALID_HANDLE_VALUE)
            {
                DWORD err = GetLastError();
                wchar_t errBuf[256];
                swprintf_s(errBuf, L"[ERROR] Failed to create mailslot. Error code: %lu", err);
                AppendLogW(hwndEdit, errBuf);
                MessageBoxW(hWnd, errBuf, L"Mailslot Creation Error", MB_OK | MB_ICONERROR);
                break;
            }

            AppendLogW(hwndEdit, L"[Server Started]");
            AppendLogW(hwndEdit, L"Mailslot successfully created: \\\\.\\mailslot\\lab5_mailslot");
            AppendLogW(hwndEdit, L"Waiting for incoming client messages...\r\n");

            // Start polling timer (every 500 ms)
            SetTimer(hWnd, IDT_TIMER, 500, nullptr);
            break;
        }
        case ID_MAILSLOT_CLOSE:
        {
            if (hMailslot == INVALID_HANDLE_VALUE)
            {
                MessageBoxW(hWnd, L"Server is not currently running.", L"Info", MB_OK | MB_ICONINFORMATION);
                break;
            }

            KillTimer(hWnd, IDT_TIMER);
            CloseHandle(hMailslot);
            hMailslot = INVALID_HANDLE_VALUE;

            AppendLogW(hwndEdit, L"[Server Stopped]");
            AppendLogW(hwndEdit, L"Mailslot closed. Communication terminated.\r\n");
            break;
        }
        case ID_LOG_CLEAR:
        {
            SetWindowTextW(hwndEdit, L"Server log cleared.\r\n--------------------------------------------\r\n");
            break;
        }
        case IDM_ABOUT:
        {
            MessageBoxW(hWnd,
                L"Laboratory Work #5\r\n"
                L"Topic: Client-Server Architecture. Mailslot Communication\r\n"
                L"Variant 9 (+ Task with Asterisk *)\r\n\r\n"
                L"Developed in Microsoft Visual Studio",
                L"About", MB_OK | MB_ICONINFORMATION);
            break;
        }
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }
    case WM_TIMER:
    {
        if (wParam == IDT_TIMER && hMailslot != INVALID_HANDLE_VALUE)
        {
            DWORD cbMessage = 0;
            DWORD cMessage = 0;
            DWORD cbRead = 0;

            // Query mailslot status
            BOOL fResult = GetMailslotInfo(hMailslot, nullptr, &cbMessage, &cMessage, nullptr);
            if (fResult && cMessage != 0 && cMessage != (DWORD)MAILSLOT_NO_MESSAGE)
            {
                // Read all pending messages in queue
                while (cMessage > 0)
                {
                    char szBuf[4096] = { 0 };
                    if (ReadFile(hMailslot, szBuf, sizeof(szBuf) - 1, &cbRead, nullptr) && cbRead > 0)
                    {
                        szBuf[cbRead] = '\0';
                        AppendLogA(hwndEdit, szBuf);
                    }

                    // Check if more messages remain
                    fResult = GetMailslotInfo(hMailslot, nullptr, &cbMessage, &cMessage, nullptr);
                    if (!fResult || cbMessage == MAILSLOT_NO_MESSAGE)
                        break;
                }
            }
        }
        break;
    }
    case WM_DESTROY:
    {
        if (hMailslot != INVALID_HANDLE_VALUE)
        {
            KillTimer(hWnd, IDT_TIMER);
            CloseHandle(hMailslot);
            hMailslot = INVALID_HANDLE_VALUE;
        }
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
