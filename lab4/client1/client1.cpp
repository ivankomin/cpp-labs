// client1.cpp : Defines the entry point for Client 1.
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

HINSTANCE hInst;
WCHAR szWindowClass[MAX_LOADSTRING] = L"Client1Class";
WCHAR szTitle[MAX_LOADSTRING] = L"Client 1 - Variant 9";
HWND hwndEdit = nullptr;

WSADATA wsaData;
WORD wVersionRequested = MAKEWORD(1, 1);
SOCKET cln_socket = INVALID_SOCKET;
SOCKADDR_IN dest_sin;
char szHostName[128] = "127.0.0.1"; //Target IP address
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
    //Retrieves messages from the thread's message queue. 
    // Because this program relies on WSAAsyncSelect, network notifications from the OS kernel arrive through this loop as regular window messages.
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
    AppendMenuW(hMenuClient, MF_STRING, ID_CLIENT_SEND, L"Send Data");
    AppendMenuW(hMenuClient, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuClient, MF_STRING, IDM_EXIT, L"Exit");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuClient, L"Client");

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        620, 100, 420, 300, nullptr, hMenuBar, hInstance, nullptr);

    if (!hWnd) return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

//Performs the TCP handshake sequence to connect to the server:
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
        MessageBoxA(hWnd, "Socket creation failed.", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    dest_sin.sin_family = AF_INET; //Address family IPv4.
    dest_sin.sin_port = htons(SERV_PORT); //Converts port 5000 from host byte order to network byte order
    dest_sin.sin_addr.s_addr = inet_addr(szHostName); //Converts the standard dotted-decimal string into an in-memory 32-bit IPv4 binary address.

    //Initiates the TCP 3-way handshake with the server at 127.0.0.1:5000
    if (connect(cln_socket, (PSOCKADDR)&dest_sin, sizeof(dest_sin)) == SOCKET_ERROR)
    {
        closesocket(cln_socket);
        cln_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Connection failed. Ensure server is running.", "Error", MB_OK | MB_ICONSTOP);
        return FALSE;
    }

    //Switches the socket from standard blocking mode to asynchronous non-blocking mode.
    if (WSAAsyncSelect(cln_socket, hWnd, WSA_NETEVENT, FD_READ | FD_CLOSE) != 0)
    {
        MessageBoxA(hWnd, "WSAAsyncSelect failed.", "Error", MB_OK);
        return FALSE;
    }

    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)"Connected to server.\r\n");
    return TRUE;
}

void SendMsg(HWND hWnd)
{
    if (cln_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Connect to server first.", "Warning", MB_OK);
        return;
    }

    // 1. Check network / remote drive status
    int netState = GetSystemMetrics(SM_NETWORK);
    const char* netDriveStatus = (netState & 0x01) ? "Present / Active" : "Not present";

    // 2. Screen width in pixels
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);

    // 3. Screen horizontal resolution via GetDeviceCaps
    HDC hdc = GetDC(hWnd);
    int horzRes = GetDeviceCaps(hdc, HORZRES);
    ReleaseDC(hWnd, hdc);

    sprintf_s(szBuf,
        "Network drive status: %s (SM_NETWORK=0x%X)\r\n"
        "Screen Width (SM_CXSCREEN): %d px\r\n"
        "Horizontal Resolution (HORZRES): %d px\r\n",
        netDriveStatus, netState, screenWidth, horzRes);

    if (send(cln_socket, szBuf, (int)strlen(szBuf), 0) != SOCKET_ERROR)
    {
        char displayMsg[1500];
        sprintf_s(displayMsg, "Data sent to server:\r\n%s", szBuf);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)displayMsg);
    }
    else
    {
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)"Failed to send data.\r\n");
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        hwndEdit = CreateWindowW(L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, 10, 380, 220, hWnd, nullptr, hInst, nullptr);

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
        //Triggered when the server sends an acknowledgment or message.
        if (WSAGETSELECTEVENT(lParam) == FD_READ)
        {
            char tempBuf[512];
            int rc = recv(cln_socket, tempBuf, sizeof(tempBuf) - 1, 0);
            if (rc > 0)
            {
                tempBuf[rc] = '\0';
                sprintf_s(szBuf, "\r\nServer response: %s\r\n", tempBuf);
                int len = GetWindowTextLengthA(hwndEdit);
                SendMessageA(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
                SendMessageA(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuf);
            }
        }
        //Triggered if the server terminates or forcibly drops the connection. Closes the local descriptor, resets the socket flag to invalid, and alerts the user.
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