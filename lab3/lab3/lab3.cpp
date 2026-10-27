#include "framework.h"
#include "lab3.h"
#include <windows.h>
#include <stdio.h>
#include <cmath>
#include <vector>
#include <chrono>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND ghWnd = nullptr;

// Synchronization and threads
HANDLE ghDataMutex = NULL;
HANDLE ghStopEvent = NULL;
HANDLE ghWorkerThreads[2] = { NULL, NULL };

WCHAR szFactorialText[256] = L"Factorial: waiting to start...";
WCHAR szTaylorSinText[256] = L"Taylor Sin: waiting to start...";

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void LogToConsole(const char* action)
{
    printf("[Thread %lu] %s\n", GetCurrentThreadId(), action);
}

// 1. Thread computing factorial
DWORD WINAPI FactorialThreadProc(LPVOID lpParam)
{
    LogToConsole("Factorial thread started.");
    unsigned long long currentFact = 1;
    int n = 1;

    while (WaitForSingleObject(ghStopEvent, 0) == WAIT_TIMEOUT && n <= 20)
    {
        currentFact *= n;

        // Acquire mutex before updating shared display buffer
        WaitForSingleObject(ghDataMutex, INFINITE);
        LogToConsole("Factorial thread acquired display buffer (Mutex).");
        swprintf_s(szFactorialText, 256, L"Factorial: %d! = %llu", n, currentFact);
        ReleaseMutex(ghDataMutex);

        InvalidateRect(ghWnd, NULL, TRUE);
        Sleep(500); // Delay for observing intermediate results
        n++;
    }

    LogToConsole("Factorial thread exiting via ExitThread.");
    ExitThread(0);
}

// 2. Thread computing Taylor series for sin(x)
DWORD WINAPI TaylorSinThreadProc(LPVOID lpParam)
{
    LogToConsole("Taylor series sin(x) thread started.");
    double x = 1.04719755; // Pi / 3 (sin(Pi/3) ≈ 0.866025)
    double term = x;
    double currentSum = x;
    int k = 1;

    while (WaitForSingleObject(ghStopEvent, 0) == WAIT_TIMEOUT && k <= 15)
    {
        // Acquire mutex
        WaitForSingleObject(ghDataMutex, INFINITE);
        LogToConsole("Taylor thread acquired display buffer (Mutex).");
        swprintf_s(szTaylorSinText, 256, L"Taylor sin(pi/3): step k=%d | Value = %.8f", k, currentSum);
        ReleaseMutex(ghDataMutex);

        InvalidateRect(ghWnd, NULL, TRUE);
        Sleep(600);

        // Next series term: (-1)^k * x^(2k+1) / (2k+1)!
        k++;
        term = -term * x * x / ((2 * k - 2) * (2 * k - 1));
        currentSum += term;
    }

    LogToConsole("Taylor thread exiting via ExitThread.");
    ExitThread(0);
}

// 3. Star task: multi-threaded array checksum benchmark
struct BenchmarkWorkerParam
{
    const int* data;
    size_t start;
    size_t end;
    long long partialSum;
};

DWORD WINAPI BenchmarkWorkerProc(LPVOID lpParam)
{
    BenchmarkWorkerParam* p = (BenchmarkWorkerParam*)lpParam;
    long long sum = 0;
    for (size_t i = p->start; i < p->end; ++i)
    {
        sum += p->data[i];
    }
    p->partialSum = sum;
    ExitThread(0);
}

void RunBenchmarkExperiment()
{
    const size_t ARRAY_SIZE = 50000000; // 50 million elements
    std::vector<int> data(ARRAY_SIZE, 1);

    int threadCounts[] = { 2, 4, 8, 16 };

    for (int tc : threadCounts)
    {
        std::vector<HANDLE> threads(tc);
        std::vector<BenchmarkWorkerParam> params(tc);
        size_t chunkSize = ARRAY_SIZE / tc;

        auto tStart = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < tc; ++i)
        {
            params[i].data = data.data();
            params[i].start = i * chunkSize;
            params[i].end = (i == tc - 1) ? ARRAY_SIZE : (i + 1) * chunkSize;
            params[i].partialSum = 0;

            threads[i] = CreateThread(NULL, 0, BenchmarkWorkerProc, &params[i], 0, NULL);
        }

        // Wait for all worker threads using WaitForMultipleObjects
        WaitForMultipleObjects(tc, threads.data(), TRUE, INFINITE);

        auto tEnd = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

        long long totalSum = 0;
        for (int i = 0; i < tc; ++i)
        {
            totalSum += params[i].partialSum;
            CloseHandle(threads[i]);
        }

        printf("Threads: %2d | Time: %8.2f ms | Checksum: %lld\n", tc, elapsedMs, totalSum);
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Allocate console for thread logging
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);

    // Synchronization primitives
    ghDataMutex = CreateMutex(NULL, FALSE, NULL);
    ghStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Execute performance benchmark experiment
    RunBenchmarkExperiment();

    // Initialize strings and register class
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LAB3, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    // Spawn compute threads for Variant 9
    ghWorkerThreads[0] = CreateThread(NULL, 0, FactorialThreadProc, NULL, 0, NULL);
    ghWorkerThreads[1] = CreateThread(NULL, 0, TaylorSinThreadProc, NULL, 0, NULL);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LAB3));
    MSG msg;

    // Main message loop
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Signal stop event and wait for thread cleanup
    SetEvent(ghStopEvent);
    if (ghWorkerThreads[0] && ghWorkerThreads[1])
    {
        WaitForMultipleObjects(2, ghWorkerThreads, TRUE, 5000);
        CloseHandle(ghWorkerThreads[0]);
        CloseHandle(ghWorkerThreads[1]);
    }

    CloseHandle(ghDataMutex);
    CloseHandle(ghStopEvent);

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LAB3));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_LAB3);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    ghWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 500, nullptr, nullptr, hInstance, nullptr);

    if (!ghWnd)
    {
        return FALSE;
    }

    ShowWindow(ghWnd, nCmdShow);
    UpdateWindow(ghWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        int halfHeight = (clientRect.bottom - clientRect.top) / 2;

        // Region 1: Top half (Factorial)
        RECT topArea = clientRect;
        topArea.bottom = halfHeight;

        // Region 2: Bottom half (Taylor Series)
        RECT bottomArea = clientRect;
        bottomArea.top = halfHeight;

        // Split line between the two regions
        MoveToEx(hdc, 0, halfHeight, NULL);
        LineTo(hdc, clientRect.right, halfHeight);

        // Access shared text buffers under mutex synchronization
        WaitForSingleObject(ghDataMutex, INFINITE);
        DrawText(hdc, szFactorialText, -1, &topArea, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawText(hdc, szTaylorSinText, -1, &bottomArea, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ReleaseMutex(ghDataMutex);

        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}