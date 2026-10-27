// client3.cpp : Client 3 (Star Task) - Send arbitrary text
//

#include <windows.h>
#include <winsock.h>
#include <stdio.h>

#pragma comment(lib, "WS2_32.lib")

#define MAX_LOADSTRING 100
#define SERV_PORT 5000
#define WSA_NETEVENT (WM_USER + 1)

#define ID_CLIENT_CONNECT 8001
#define ID_CLIENT_SEND    8002
#define IDM_EXIT          8003
#define IDC_TEXT_INPUT    8004

HINSTANCE hInst;
WCHAR szWindowClass[MAX_LOADSTRING] = L"Client3Class";
WCHAR szTitle[MAX_LOADSTRING] = L"Client 3 (*) - Arbitrary Text";
HWND hwndInput = nullptr;
HWND hwndLog = nullptr;

WSADATA wsaData;
WORD wVersionRequested = MAKEWORD(1, 1);
SOCKET cln_socket = INVALID_SOCKET;
SOCKADDR_IN dest_sin;
char szHostName[128] = "127.0.0.1";
char szBuf[1024];

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

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

    AppendMenuW(hMenuClient, MF_STRING, ID_CLIENT_CONNECT, L"Connect");
    AppendMenuW(hMenuClient, MF_STRING, ID_CLIENT_SEND, L"Send Custom Message");
    AppendMenuW(hMenuClient, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuClient, MF_STRING, IDM_EXIT, L"Exit");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuClient, L"Client");

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        1060, 100, 440, 360, nullptr, hMenuBar, hInstance, nullptr);

    if (!hWnd) return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

BOOL SetConnection(HWND hWnd)
{
    if (cln_socket != INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Already connected.", "Info", MB_OK);
        return TRUE;
    }

    cln_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cln_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Socket error.", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    dest_sin.sin_family = AF_INET;
    dest_sin.sin_port = htons(SERV_PORT);
    dest_sin.sin_addr.s_addr = inet_addr(szHostName);

    if (connect(cln_socket, (PSOCKADDR)&dest_sin, sizeof(dest_sin)) == SOCKET_ERROR)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Connection failed.", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    if (WSAAsyncSelect(cln_socket, hWnd, WSA_NETEVENT, FD_READ | FD_CLOSE) != 0)
    {
        MessageBoxA(hWnd, "WSAAsyncSelect failed.", "Error", MB_OK);
        return FALSE;
    }

    int len = GetWindowTextLengthA(hwndLog);
    SendMessageA(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(hwndLog, EM_REPLACESEL, FALSE, (LPARAM)"Connected to server.\r\n");
    return TRUE;
}

void SendMsg(HWND hWnd)
{
    if (cln_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Connect to server first.", "Warning", MB_OK);
        return;
    }

    //Send arbitrary user input   
    char userText[512];
    GetWindowTextA(hwndInput, userText, sizeof(userText));

    if (strlen(userText) == 0)
    {
        strcpy_s(userText, sizeof(userText), "Default text payload from Client 3.");
    }

    sprintf_s(szBuf, "Custom User Message: %s\r\n", userText);

    if (send(cln_socket, szBuf, (int)strlen(szBuf), 0) != SOCKET_ERROR)
    {
        char displayMsg[1024];
        sprintf_s(displayMsg, "Sent to server: %s", szBuf);
        int len = GetWindowTextLengthA(hwndLog);
        SendMessageA(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(hwndLog, EM_REPLACESEL, FALSE, (LPARAM)displayMsg);
    }
    else
    {
        int len = GetWindowTextLengthA(hwndLog);
        SendMessageA(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(hwndLog, EM_REPLACESEL, FALSE, (LPARAM)"Send failed.\r\n");
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        CreateWindowW(L"STATIC", L"Enter custom text to send to server:",
            WS_CHILD | WS_VISIBLE, 10, 10, 400, 20, hWnd, nullptr, hInst, nullptr);

        hwndInput = CreateWindowW(L"EDIT", L"Type custom test message here...",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            10, 35, 400, 25, hWnd, (HMENU)IDC_TEXT_INPUT, hInst, nullptr);

        hwndLog = CreateWindowW(L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 70, 400, 220, hWnd, nullptr, hInst, nullptr);

        if (WSAStartup(wVersionRequested, &wsaData) != 0)
        {
            MessageBoxA(hWnd, "WSAStartup failed.", "Error", MB_OK | MB_ICONSTOP);
            return FALSE;
        }
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_CLIENT_CONNECT: SetConnection(hWnd); break;
        case ID_CLIENT_SEND:    SendMsg(hWnd); break;
        case IDM_EXIT:          DestroyWindow(hWnd); break;
        default:                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }
    case WSA_NETEVENT:
    {
        if (WSAGETSELECTEVENT(lParam) == FD_READ)
        {
            char tempBuf[512];
            int rc = recv(cln_socket, tempBuf, sizeof(tempBuf) - 1, 0);
            if (rc > 0)
            {
                tempBuf[rc] = '\0';
                sprintf_s(szBuf, "\r\nServer: %s\r\n", tempBuf);
                int len = GetWindowTextLengthA(hwndLog);
                SendMessageA(hwndLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
                SendMessageA(hwndLog, EM_REPLACESEL, FALSE, (LPARAM)szBuf);
            }
        }
        if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
        {
            closesocket(cln_socket);
            cln_socket = INVALID_SOCKET;
            MessageBoxA(hWnd, "Server closed the connection.", "Disconnected", MB_OK);
        }
        break;
    }
    case WM_DESTROY:
        if (cln_socket != INVALID_SOCKET)
        {
            closesocket(cln_socket);
        }
        WSACleanup();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}