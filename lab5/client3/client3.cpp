// client3.cpp : Mailslot Client 3 (Laboratory Work #5)
// Task with Asterisk (*): Sending mathematical computation results
// (e.g. rectangle area) with arbitrary input parameters to the server.

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_LOADSTRING 100
#define ID_CALC_SEND    9001
#define ID_SEND_CURRENT 9002
#define IDM_EXIT        9003

#define IDC_INPUT_A     101
#define IDC_INPUT_B     102
#define IDC_BTN_CALC    103
#define IDC_BTN_SEND    104

HINSTANCE hInst;
const wchar_t szWindowClass[] = L"MailslotClient3Class";
const wchar_t szTitle[] = L"Client 3 - Task with Asterisk (*)";
const wchar_t MailslotName[] = L"\\\\.\\mailslot\\lab5_mailslot";

HWND hwndMain = nullptr;
HWND hwndInputA = nullptr;
HWND hwndInputB = nullptr;
HWND hwndEditResult = nullptr;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void PerformCalculation(HWND hWnd, bool andSend);
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

    AppendMenuW(hMenuClient, MF_STRING, ID_CALC_SEND, L"Calculate & Send");
    AppendMenuW(hMenuClient, MF_STRING, ID_SEND_CURRENT, L"Send Current Content");
    AppendMenuW(hMenuClient, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuClient, MF_STRING, IDM_EXIT, L"Exit");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuClient, L"Client");

    // Position Client 3 window bottom-right
    hwndMain = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        530, 610, 480, 360, nullptr, hMenuBar, hInstance, nullptr);

    if (!hwndMain) return FALSE;

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);
    return TRUE;
}

void PerformCalculation(HWND hWnd, bool andSend)
{
    char strA[64] = { 0 };
    char strB[64] = { 0 };

    GetWindowTextA(hwndInputA, strA, sizeof(strA));
    GetWindowTextA(hwndInputB, strB, sizeof(strB));

    double a = atof(strA);
    double b = atof(strB);

    if (a <= 0.0) a = 10.0;
    if (b <= 0.0) b = 5.0;

    // Mathematical computations for rectangle:
    double area = a * b;                        // Area
    double perimeter = 2.0 * (a + b);           // Perimeter
    double diagonal = sqrt(a * a + b * b);      // Diagonal
    double circumRadius = diagonal / 2.0;       // Circumscribed circle radius

    char msgBuf[2048];
    sprintf_s(msgBuf,
        "Client #3 Data (Task with Asterisk *):\r\n"
        " - Geometric calculation: Rectangle\r\n"
        " - Input parameters: Side a = %.2f units, Side b = %.2f units\r\n"
        " - Calculated Area (S = a * b) = %.2f sq. units\r\n"
        " - Calculated Perimeter (P = 2*(a + b)) = %.2f units\r\n"
        " - Diagonal length (d = sqrt(a^2 + b^2)) = %.2f units\r\n"
        " - Circumscribed circle radius (R = d / 2) = %.2f units",
        a, b, area, perimeter, diagonal, circumRadius);

    SetWindowTextA(hwndEditResult, msgBuf);

    if (andSend)
    {
        SendMailslotData(hWnd);
    }
}

void SendMailslotData(HWND hWnd)
{
    int textLen = GetWindowTextLengthA(hwndEditResult);
    if (textLen <= 0)
    {
        MessageBoxW(hWnd, L"Text field is empty. Please calculate first.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    char* buffer = (char*)malloc(textLen + 2);
    if (!buffer) return;

    GetWindowTextA(hwndEditResult, buffer, textLen + 1);

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
        MessageBoxW(hWnd, L"Calculation results sent to server successfully!", L"Client 3 (*)", MB_OK | MB_ICONINFORMATION);
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
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        // Labels and input fields for math parameters
        HWND hLblA = CreateWindowW(L"STATIC", L"Side A:", WS_CHILD | WS_VISIBLE,
            15, 12, 70, 20, hWnd, nullptr, hInst, nullptr);
        SendMessage(hLblA, WM_SETFONT, (WPARAM)hFont, TRUE);

        hwndInputA = CreateWindowW(L"EDIT", L"15.5", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            85, 10, 85, 22, hWnd, (HMENU)IDC_INPUT_A, hInst, nullptr);
        SendMessage(hwndInputA, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hLblB = CreateWindowW(L"STATIC", L"Side B:", WS_CHILD | WS_VISIBLE,
            195, 12, 70, 20, hWnd, nullptr, hInst, nullptr);
        SendMessage(hLblB, WM_SETFONT, (WPARAM)hFont, TRUE);

        hwndInputB = CreateWindowW(L"EDIT", L"8.2", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            265, 10, 85, 22, hWnd, (HMENU)IDC_INPUT_B, hInst, nullptr);
        SendMessage(hwndInputB, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Action buttons
        HWND hBtnCalc = CreateWindowW(L"BUTTON", L"Calculate & Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            15, 42, 210, 28, hWnd, (HMENU)IDC_BTN_CALC, hInst, nullptr);
        SendMessage(hBtnCalc, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND hBtnSend = CreateWindowW(L"BUTTON", L"Send Current Content", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            235, 42, 210, 28, hWnd, (HMENU)IDC_BTN_SEND, hInst, nullptr);
        SendMessage(hBtnSend, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Result preview box
        RECT rc;
        GetClientRect(hWnd, &rc);
        hwndEditResult = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            15, 80, rc.right - 30, rc.bottom - 90, hWnd, nullptr, hInst, nullptr);
        SendMessage(hwndEditResult, WM_SETFONT, (WPARAM)hFont, TRUE);

        PerformCalculation(hWnd, false);
        break;
    }
    case WM_SIZE:
    {
        if (hwndEditResult)
        {
            MoveWindow(hwndEditResult, 15, 80, LOWORD(lParam) - 30, HIWORD(lParam) - 90, TRUE);
        }
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDC_BTN_CALC:
        case ID_CALC_SEND:
            PerformCalculation(hWnd, true);
            break;
        case IDC_BTN_SEND:
        case ID_SEND_CURRENT:
            SendMailslotData(hWnd);
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
