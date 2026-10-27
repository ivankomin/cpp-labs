// serv.cpp : Defines the entry point for the server application.
//

#include <windows.h>
#include <winsock.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "WS2_32.lib")

#define MAX_LOADSTRING 100
#define SERV_PORT 5000
//tell WndProc whether an incoming message is a new connection request or an I/O event on an existing connection.
#define WSA_ACCEPT   (WM_USER + 0)
#define WSA_NETEVENT (WM_USER + 1)
#define MAX_CLIENTS 3

#define ID_SERVER_START    9001
#define ID_SERVER_STOP     9002
#define ID_SENDTOCLIENT_1  9003
#define ID_SENDTOCLIENT_2  9004
#define ID_SENDTOCLIENT_3  9005
#define ID_CLIENTOFF_1     9006
#define ID_CLIENTOFF_2     9007
#define ID_CLIENTOFF_3     9008
#define IDM_EXIT           9009

HINSTANCE hInst;
WCHAR szWindowClass[MAX_LOADSTRING] = L"ServerClass";
WCHAR szTitle[MAX_LOADSTRING] = L"Server - Variant 9";
HWND hWindow = nullptr;
HWND hwndEdit = nullptr;

WSADATA wsaData;
WORD wVersionRequested = MAKEWORD(1, 1);
SOCKET srv_socket = INVALID_SOCKET; //The welcoming (listening) socket descriptor.
int ClientNum = -1;
//Parallel arrays tracking active socket descriptors and their corresponding endpoint metadata (IP address and port)
SOCKET sock[MAX_CLIENTS];
SOCKADDR_IN sockaddr[MAX_CLIENTS];

char szBuf[1024];
char mess[4096] = "";

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

//The entry point of the application.
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

//Defines and registers the Win32 window class (WNDCLASSEXW)
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

//Builds the application menu dynamically, creates the parent window, and initializes socket tables.
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    HMENU hMenuBar = CreateMenu();
    HMENU hMenuServer = CreatePopupMenu();
    HMENU hMenuSend = CreatePopupMenu();
    HMENU hMenuDrop = CreatePopupMenu();

    AppendMenuW(hMenuServer, MF_STRING, ID_SERVER_START, L"Start Server");
    AppendMenuW(hMenuServer, MF_STRING, ID_SERVER_STOP, L"Stop Server");
    AppendMenuW(hMenuServer, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenuServer, MF_STRING, IDM_EXIT, L"Exit");

    AppendMenuW(hMenuSend, MF_STRING, ID_SENDTOCLIENT_1, L"Send to Client 1");
    AppendMenuW(hMenuSend, MF_STRING, ID_SENDTOCLIENT_2, L"Send to Client 2");
    AppendMenuW(hMenuSend, MF_STRING, ID_SENDTOCLIENT_3, L"Send to Client 3");

    AppendMenuW(hMenuDrop, MF_STRING, ID_CLIENTOFF_1, L"Disconnect Client 1");
    AppendMenuW(hMenuDrop, MF_STRING, ID_CLIENTOFF_2, L"Disconnect Client 2");
    AppendMenuW(hMenuDrop, MF_STRING, ID_CLIENTOFF_3, L"Disconnect Client 3");

    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuServer, L"Server");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuSend, L"SendToClient");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuDrop, L"ClientOff");

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        100, 100, 500, 400, nullptr, hMenuBar, hInstance, nullptr);

    if (!hWnd) return FALSE;
    hWindow = hWnd;

    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        sock[i] = INVALID_SOCKET;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

//Configures the master welcoming socket and puts it into a non-blocking listening state.
void ServerStart(HWND hWnd)
{
    if (srv_socket != INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Server is already active.", "Info", MB_OK | MB_ICONINFORMATION);
        return;
    }

    srv_socket = socket(AF_INET, SOCK_STREAM, 0); //Allocates an endpoint configured for IPv4 (AF_INET) and reliable byte-stream TCP (SOCK_STREAM).
    if (srv_socket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "Failed to create socket.", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    SOCKADDR_IN srv_address;
    srv_address.sin_family = AF_INET;
    srv_address.sin_port = htons(SERV_PORT); //converts the port number from host byte order (little-endian on x86/x64) to network byte order (big-endian
    srv_address.sin_addr.s_addr = INADDR_ANY; //allows the socket to bind to all available local network interfaces.

    //Binds the socket to local IP address and port 5000
    if (bind(srv_socket, (LPSOCKADDR)&srv_address, sizeof(srv_address)) == SOCKET_ERROR)
    {
        closesocket(srv_socket);
        srv_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Binding failed on port 5000.", "Error", MB_OK | MB_ICONSTOP);
        return;
    }

    //Transitions the socket from active to passive mode
    if (listen(srv_socket, 5) == SOCKET_ERROR)
    {
        closesocket(srv_socket);
        srv_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "Listen error.", "Error", MB_OK);
        return;
    }

    //Whenever a client attempts to connect (FD_ACCEPT), post a WSA_ACCEPT message to the window procedure
    if (WSAAsyncSelect(srv_socket, hWnd, WSA_ACCEPT, FD_ACCEPT) != 0)
    {
        closesocket(srv_socket);
        srv_socket = INVALID_SOCKET;
        MessageBoxA(hWnd, "WSAAsyncSelect registration failed.", "Error", MB_OK);
        return;
    }

    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)"Server started on port 5000. Listening...\r\n");
}

void ServerStop(HWND hWnd)
{
    //cancel any pending Windows message notifications for the listening socket.
    WSAAsyncSelect(srv_socket, hWnd, 0, 0);
    if (srv_socket != INVALID_SOCKET)
    {
        closesocket(srv_socket); //Closes srv_socket via closesocket() and marks it INVALID_SOCKET
        srv_socket = INVALID_SOCKET;
    }
    //Iterates through all connected client slots in sock[], clears their async event registrations, closes their sockets, and resets their slots.
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (sock[i] != INVALID_SOCKET)
        {
            WSAAsyncSelect(sock[i], hWnd, 0, 0);
            closesocket(sock[i]);
            sock[i] = INVALID_SOCKET;
        }
    }
    ClientNum = -1;
    SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)"Server stopped.\r\n");
}

//Completes the TCP connection handshake with a requesting client and assigns it a dedicated socket.
BOOL AcceptClient(int j)
{
    int sockaddr_len = sizeof(sockaddr[j]);
    //Pulls the first completed connection off the listening queue, populates sockaddr[j] with the client's IP and port
    // and returns a new socket dedicated exclusively to communication with client j.
    sock[j] = accept(srv_socket, (LPSOCKADDR)&sockaddr[j], &sockaddr_len);
    if (sock[j] != INVALID_SOCKET)
    {
        //Configures async notifications for the new client socket
        if (WSAAsyncSelect(sock[j], hWindow, WSA_NETEVENT, FD_READ | FD_CLOSE) == 0)
        {
            return TRUE;
        }
        closesocket(sock[j]);
        sock[j] = INVALID_SOCKET;
    }
    return FALSE;
}

//Closes a single client session safely.
void DisconnectClient(int j)
{
    if (j >= 0 && j < MAX_CLIENTS && sock[j] != INVALID_SOCKET)
    {
        WSAAsyncSelect(sock[j], hWindow, 0, 0);
        closesocket(sock[j]);
        sock[j] = INVALID_SOCKET;
    }
}

//Handles the WSA_ACCEPT notification dispatched from WndProc.
void WndProc_OnWSAAccept(HWND hWnd, LPARAM lParam)
{
    if (WSAGETSELECTERROR(lParam))
    {
        MessageBoxA(hWnd, "Socket accept error.", "Error", MB_OK);
        return;
    }

    //find the first available free slot
    int freeIndex = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (sock[i] == INVALID_SOCKET)
        {
            freeIndex = i;
            break;
        }
    }

    if (freeIndex == -1)
    {
        MessageBoxA(hWnd, "Max clients reached (3 connections).", "Rejected", MB_OK);
        return;
    }

    if (!AcceptClient(freeIndex))
    {
        MessageBoxA(hWnd, "Client connection failed.", "Error", MB_OK);
        return;
    }

    if (freeIndex > ClientNum)
    {
        ClientNum = freeIndex;
    }

    sprintf_s(szBuf, "Client %d connected: IP=%s, Port=%u\r\n",
        freeIndex + 1, inet_ntoa(sockaddr[freeIndex].sin_addr), htons(sockaddr[freeIndex].sin_port));

    int len = GetWindowTextLengthA(hwndEdit);
    SendMessageA(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuf);
}

//Handles incoming data and disconnection notifications
void WndProc_OnWSANetEvent(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    //identify the client
    int number = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (sock[i] == (SOCKET)wParam)
        {
            number = i;
            break;
        }
    }
    if (number == -1) return;


    if (WSAGETSELECTEVENT(lParam) == FD_READ) //Extracts the low-order word of lParam, which contains the network event flag.
    {
        char recvBuf[512];
        int rc = recv((SOCKET)wParam, recvBuf, sizeof(recvBuf) - 1, 0); //Pulls up to 511 bytes from the socket into recvBuf. The return value rc is the actual count of bytes received.
        if (rc > 0)
        {
            recvBuf[rc] = '\0';
            //Formats a message attributing the payload to the corresponding client
            sprintf_s(szBuf, "\r\n[Data from Client %d]:\r\n%s\r\n", number + 1, recvBuf);
            int len = GetWindowTextLengthA(hwndEdit);
            SendMessageA(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
            SendMessageA(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuf);
        }
    }

    if (WSAGETSELECTEVENT(lParam) == FD_CLOSE)
    {
        DisconnectClient(number);
        sprintf_s(szBuf, "\r\nClient %d has disconnected.\r\n", number + 1);
        int len = GetWindowTextLengthA(hwndEdit);
        SendMessageA(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuf);
    }
}

//Sends an application-level acknowledgment back to a specific client.
void SendToClient(int j)
{
    if (j < 0 || j >= MAX_CLIENTS || sock[j] == INVALID_SOCKET)
    {
        MessageBoxA(hWindow, "Client is not connected.", "Info", MB_OK);
        return;
    }

    const char* ackMsg = "SERVER_ACK: Data acknowledged.\r\n";
    if (send(sock[j], ackMsg, (int)strlen(ackMsg), 0) != SOCKET_ERROR)
    {
        sprintf_s(szBuf, "Sent acknowledgment to Client %d.\r\n", j + 1);
        int len = GetWindowTextLengthA(hwndEdit);
        SendMessageA(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuf);
    }
}

//Provides a manual UI mechanism to terminate a client connection on demand
void ClientOff(HWND hWnd, int j)
{
    if (j < 0 || j >= MAX_CLIENTS || sock[j] == INVALID_SOCKET) return;

    sprintf_s(szBuf, "Disconnect Client %d?", j + 1);
    //If confirmed, calls DisconnectClient(j) to shut down the socket and logs the disconnection to hwndEdit
    if (IDYES == MessageBoxA(hWnd, szBuf, "Confirm", MB_YESNO | MB_ICONQUESTION))
    {
        DisconnectClient(j);
        sprintf_s(szBuf, "Client %d was disconnected.\r\n", j + 1);
        int len = GetWindowTextLengthA(hwndEdit);
        SendMessageA(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        SendMessageA(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuf);
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
            10, 10, 460, 320, hWnd, nullptr, hInst, nullptr);

        int err = WSAStartup(wVersionRequested, &wsaData);
        if (err)
        {
            MessageBoxA(hWnd, "WSAStartup failed.", "Error", MB_OK | MB_ICONSTOP);
            return FALSE;
        }

        sprintf_s(szBuf, "Using: %s\r\nStatus: %s\r\n", wsaData.szDescription, wsaData.szSystemStatus);
        SendMessageA(hwndEdit, WM_SETTEXT, 0, (LPARAM)szBuf);
        break;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_SERVER_START:   ServerStart(hWnd); break;
        case ID_SERVER_STOP:    ServerStop(hWnd); break;
        case ID_SENDTOCLIENT_1: SendToClient(0); break;
        case ID_SENDTOCLIENT_2: SendToClient(1); break;
        case ID_SENDTOCLIENT_3: SendToClient(2); break;
        case ID_CLIENTOFF_1:    ClientOff(hWnd, 0); break;
        case ID_CLIENTOFF_2:    ClientOff(hWnd, 1); break;
        case ID_CLIENTOFF_3:    ClientOff(hWnd, 2); break;
        case IDM_EXIT:          DestroyWindow(hWnd); break;
        default:                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }
    case WSA_ACCEPT:
        WndProc_OnWSAAccept(hWnd, lParam);
        break;
    case WSA_NETEVENT:
        WndProc_OnWSANetEvent(hWnd, wParam, lParam);
        break;
    case WM_DESTROY:
        ServerStop(hWnd);
        WSACleanup();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}