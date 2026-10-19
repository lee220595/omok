#include "main_window.hpp"

namespace omok::gui {
namespace {
constexpr wchar_t window_class[] = L"Omok.Foundation.MainWindow";
constexpr wchar_t window_title[] = L"오목 프로젝트";

int scaled(int value, UINT dpi)
{
    return MulDiv(value, static_cast<int>(dpi), 96);
}

void paint(HWND window)
{
    PAINTSTRUCT paint_state{};
    const HDC dc = BeginPaint(window, &paint_state);
    RECT area{};
    GetClientRect(window, &area);
    FillRect(dc, &area, GetSysColorBrush(COLOR_WINDOW));

    const UINT dpi = GetDpiForWindow(window);
    const HFONT font = CreateFontW(-scaled(20, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
        FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"맑은 고딕");
    const HGDIOBJ old_font = SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
    InflateRect(&area, -scaled(32, dpi), -scaled(32, dpi));
    DrawTextW(dc,
        L"오목 프로젝트\n\n"
        L"기본 창이 준비되었습니다.\n"
        L"대국 기능은 다음 단계에서 추가됩니다.",
        -1, &area, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old_font);
    if (font) {
        DeleteObject(font);
    }
    EndPaint(window, &paint_state);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_PAINT:
        paint(window);
        return 0;
    case WM_SIZE:
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_GETMINMAXINFO: {
        const UINT dpi = GetDpiForWindow(window);
        auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
        limits->ptMinTrackSize = {scaled(480, dpi), scaled(320, dpi)};
        return 0;
    }
    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

int report_error(const wchar_t* step)
{
    const DWORD error = GetLastError();
    wchar_t details[512]{};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, details, 512, nullptr);
    MessageBoxW(nullptr, details[0] ? details : L"Windows 오류가 발생했습니다.",
        step, MB_OK | MB_ICONERROR);
    return 1;
}
} // namespace

int run(HINSTANCE instance, int show_command)
{
    WNDCLASSEXW descriptor{};
    descriptor.cbSize = sizeof(descriptor);
    descriptor.hInstance = instance;
    descriptor.lpfnWndProc = window_proc;
    descriptor.lpszClassName = window_class;
    descriptor.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    descriptor.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    descriptor.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    if (!RegisterClassExW(&descriptor)) {
        return report_error(L"창 등록 실패");
    }

    const UINT dpi = GetDpiForSystem();
    RECT frame{0, 0, scaled(800, dpi), scaled(520, dpi)};
    if (!AdjustWindowRectExForDpi(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi)) {
        return report_error(L"창 크기 계산 실패");
    }
    const HWND window = CreateWindowExW(0, window_class, window_title,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        frame.right - frame.left, frame.bottom - frame.top,
        nullptr, nullptr, instance, nullptr);
    if (!window) {
        return report_error(L"창 생성 실패");
    }
    ShowWindow(window, show_command);
    UpdateWindow(window);

    MSG message{};
    for (;;) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == -1) {
            return report_error(L"메시지 처리 실패");
        }
        if (result == 0) {
            return static_cast<int>(message.wParam);
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}
} // namespace omok::gui
