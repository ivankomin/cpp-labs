#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_LOADSTRING 100
#define SHARED_MEM_SIZE 4096

#define ID_CALC_SEND    9001
#define ID_SEND_CURRENT 9002
#define IDM_ABOUT       9003
#define IDM_EXIT        9004

#define IDC_INPUT_A     101
#define IDC_INPUT_B     102
#define IDC_BTN_CALC    103
#define IDC_BTN_SEND    104

HINSTANCE hInst;
const wchar_t szWindowClass[] = L"FileMappingClient3Class";
const wchar_t szTitle[]       = L"Client 3";
const wchar_t MAPPING_NAME[]  = L"Local\\Lab6_FileMapping";
const wchar_t MUTEX_NAME[]    = L"Local\\Lab6_Mutex";
const wchar_t EVENT_NAME[]    = L"Local\\Lab6_DataReadyEvent";

HWND hwndMain = nullptr;
HWND hwndInputA = nullptr;
HWND hwndInputB = nullptr;
HWND hwndEditResult = nullptr;

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void PerformCalculation(HWND hWnd, bool andSend);
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

    AppendMenuW(hMenuClient, MF_STRING, ID_CALC_SEND,    L"Calculate & Send");
    AppendMenuW(hMenuClient, MF_STRING, ID_SEND_CURRENT, L"Send Current Content");
    AppendMenuW(hMenuClient, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuClient, MF_STRING, IDM_EXIT,        L"Exit");

    AppendMenuW(hMenuHelp, MF_STRING, IDM_ABOUT, L"About...");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuClient, L"Client");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuHelp,   L"Help");

    // Position Client 3 window bottom-right (x=530, y=610, w=480, h=360)
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

    if (a <= 0.0) a = 12.0;
    if (b <= 0.0) b = 6.0;

    // Geometric computation of rectangle parameters:
    double area = a * b;                        // Area
    double perimeter = 2.0 * (a + b);           // Perimeter
    double diagonal = sqrt(a * a + b * b);      // Diagonal
    double circumRadius = diagonal / 2.0;       // Circumscribed circle radius

    char msgBuf[2048];
    sprintf_s(msgBuf,
        "Client #3 Data:\r\n"
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
        SendSharedMemoryData(hWnd);
    }
}

void SendSharedMemoryData(HWND hWnd)
{
    int textLen = GetWindowTextLengthA(hwndEditResult);
    if (textLen <= 0)
    {
        MessageBoxW(hWnd, L"Result text is empty. Run calculation first.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    char* buffer = (char*)malloc(textLen + 1);
    if (!buffer) return;
    GetWindowTextA(hwndEditResult, buffer, textLen + 1);

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

    // 3. Acquire mutex to prevent concurrent collisions
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

        MessageBoxW(hWnd, L"Data written to shared memory successfully!\r\nServer notified automatically via Event (*).", L"Client 3", MB_OK | MB_ICONINFORMATION);
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
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        CreateWindowW(L"STATIC", L"Side a:", WS_CHILD | WS_VISIBLE, 10, 15, 50, 20, hWnd, nullptr, hInst, nullptr);
        hwndInputA = CreateWindowW(L"EDIT", L"15.0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 65, 12, 70, 24, hWnd, (HMENU)IDC_INPUT_A, hInst, nullptr);
        SendMessage(hwndInputA, WM_SETFONT, (WPARAM)hFont, TRUE);

        CreateWindowW(L"STATIC", L"Side b:", WS_CHILD | WS_VISIBLE, 150, 15, 50, 20, hWnd, nullptr, hInst, nullptr);
        hwndInputB = CreateWindowW(L"EDIT", L"8.0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 205, 12, 70, 24, hWnd, (HMENU)IDC_INPUT_B, hInst, nullptr);
        SendMessage(hwndInputB, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND btnCalc = CreateWindowW(L"BUTTON", L"Calculate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 290, 10, 80, 28, hWnd, (HMENU)IDC_BTN_CALC, hInst, nullptr);
        SendMessage(btnCalc, WM_SETFONT, (WPARAM)hFont, TRUE);

        HWND btnSend = CreateWindowW(L"BUTTON", L"Calc & Send", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 375, 10, 85, 28, hWnd, (HMENU)IDC_BTN_SEND, hInst, nullptr);
        SendMessage(btnSend, WM_SETFONT, (WPARAM)hFont, TRUE);

        RECT rc;
        GetClientRect(hWnd, &rc);
        hwndEditResult = CreateWindowW(L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            10, 48, rc.right - 20, rc.bottom - 58, hWnd, nullptr, hInst, nullptr);
        SendMessage(hwndEditResult, WM_SETFONT, (WPARAM)hFont, TRUE);

        PerformCalculation(hWnd, false);
        break;
    }
    case WM_SIZE:
    {
        if (hwndEditResult)
        {
            MoveWindow(hwndEditResult, 10, 48, LOWORD(lParam) - 20, HIWORD(lParam) - 58, TRUE);
        }
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDC_BTN_CALC:
            PerformCalculation(hWnd, false);
            break;
        case IDC_BTN_SEND:
        case ID_CALC_SEND:
            PerformCalculation(hWnd, true);
            break;
        case ID_SEND_CURRENT:
            SendSharedMemoryData(hWnd);
            break;
        case IDM_ABOUT:
            MessageBoxW(hWnd,
                L"Laboratory Work #6 - Client 3\r\n"
                L"Task with Asterisk (*): Interactive Computations & Memory-Mapped IPC\r\n"
                L"Demonstrates Mutex protection & Event notification",
                L"About Client 3", MB_OK | MB_ICONINFORMATION);
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
