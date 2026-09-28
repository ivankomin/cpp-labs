// lab2.cpp : Defines the entry point for the application.
#include "framework.h"
#include <stack>
#include "lab2.h"

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
int width = 800;
int height = 600;

const COLORREF BOUNDARY_COLOR = RGB(128, 128, 128);
const COLORREF FILL_COLOR = RGB(0, 150, 255);  

// Координати вершин замкненого контуру
POINT borderPoints[] = {
    { 150, 100 },
    { 250, 120 },
    { 320, 80 },
    { 400, 160 },
    { 360, 260 },
    { 260, 320 },
    { 140, 280 },
    { 180, 190 }
};
const int pointsCount = sizeof(borderPoints) / sizeof(borderPoints[0]);

// Попередні оголошення функцій:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

// Перевірка приналежності точки внутрішній області контуру
bool IsPointInsidePolygon(POINT pt) {
    HRGN hRgn = CreatePolygonRgn(borderPoints, pointsCount, WINDING); //create polygon region based on coordinates
    BOOL inside = PtInRegion(hRgn, pt.x, pt.y);
    DeleteObject(hRgn); //free descriptor to avoid memory leaks
    return inside == TRUE;
}

// Рядковий алгоритм заливки за кольором межі (Scanline Boundary Fill)
void BoundaryFill(HDC hdc, HWND hWnd, int startX, int startY, COLORREF fillColor, COLORREF boundaryColor) {
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);

    //get clicked pixel color and if it's same as boundary or already painted, return
    COLORREF startCol = GetPixel(hdc, startX, startY);
    if (startCol == boundaryColor || startCol == fillColor || startCol == CLR_INVALID) {
        return;
    }

    //У стек кладуться не всі пікселі підряд, а лише стартові точки непокритих горизонтальних інтервалів (затравки)
    std::stack<POINT> pts;
    pts.push({ startX, startY });

    while (!pts.empty()) {
        //1. create a valid interval
        
        //get current point from stack
        POINT p = pts.top();
        pts.pop();

        int x = p.x;
        int y = p.y;

        //якщо точка вийшла за межі вікна (clientRect), вона пропускається через continue
        if (x < clientRect.left || x >= clientRect.right ||
            y < clientRect.top || y >= clientRect.bottom) {
            continue;
        }

        //get clicked pixel color and if it's same as boundary or already painted, return
        COLORREF col = GetPixel(hdc, x, y);
        if (col == boundaryColor || col == fillColor || col == CLR_INVALID) {
            continue;
        }

        // Рухаємося ліворуч до межі
        int leftX = x;
        // Змінна leftX крокує вліво від початкового x.Рух зупиняється, як тільки зустрічається колір контуру, уже зафарбований піксель або лівий край вікна.
        while (leftX >= clientRect.left) {
            COLORREF c = GetPixel(hdc, leftX, y);
            if (c == boundaryColor || c == fillColor || c == CLR_INVALID) {
                break;
            }
            leftX--;
        }
        //повертає координату на один піксель праворуч, оскільки цикл зупинився безпосередньо на самій перешкоді, яку фарбувати не можна.
        leftX++;

        // Рухаємося праворуч до межі
        //same logic as left paint
        int rightX = x;
        while (rightX < clientRect.right) {
            COLORREF c = GetPixel(hdc, rightX, y);
            if (c == boundaryColor || c == fillColor || c == CLR_INVALID) {
                break;
            }
            rightX++;
        }
        rightX--;

        //2. paint said interval
        
        // Зафарбовуємо знайдений горизонтальний відрізок
        for (int i = leftX; i <= rightX; ++i) {
            SetPixel(hdc, i, y, fillColor);
        }

        // Скануємо сусідні рядки (зверху та знизу) для пошуку нових затравок
        for (int nextY : { y - 1, y + 1 }) {
            //Якщо рядок виходить за межі клієнтської області, він пропускається.
            if (nextY < clientRect.top || nextY >= clientRect.bottom) {
                continue;
            }

            bool inSegment = false; //are we inside the unpainted interval
            for (int i = leftX; i <= rightX; ++i) {
                COLORREF c = GetPixel(hdc, i, nextY);
                if (c != boundaryColor && c != fillColor && c != CLR_INVALID) {
                    if (!inSegment) { //найперший незафарбований піксель цього сегмента
                        //Точка { i, nextY } додається в стек
                        pts.push({ i, nextY });
                        inSegment = true;
                    }
                }
                else {
                    inSegment = false;
                }
            }
        }
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_LAB2, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_LAB2));
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_LAB2));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_LAB2);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;

    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);

    HWND hWnd = CreateWindowW(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        cx / 2 - width / 2,
        cy / 2 - height / 2,
        width,
        height,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd) {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }

    case WM_SIZE:
        width = LOWORD(lParam);
        height = HIWORD(lParam);
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Малювання замкненого контуру
        HPEN hPen = CreatePen(PS_SOLID, 2, BOUNDARY_COLOR);
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

        Polygon(hdc, borderPoints, pointsCount);

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hPen);

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_LBUTTONDOWN: {
        POINT clickPt;
        clickPt.x = LOWORD(lParam);
        clickPt.y = HIWORD(lParam);

        // Запуск заливки лише за умови кліку строго всередині контуру
        if (IsPointInsidePolygon(clickPt)) {
            HDC hdc = GetDC(hWnd);
            BoundaryFill(hdc, hWnd, clickPt.x, clickPt.y, FILL_COLOR, BOUNDARY_COLOR);
            ReleaseDC(hWnd, hdc);
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

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}