#include "main_window.hpp"
#include "board_geometry.hpp"
#include "omok/match/controller.hpp"
#include "omok/match/development_positions.hpp"
#include "omok/match/forbidden.hpp"
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

namespace omok::gui {
namespace {
namespace st = state;
namespace mt = match;
constexpr wchar_t window_class[] = L"Omok.LocalMatch.MainWindow";
enum Control { Preset = 101, Owner, Start, Restart, Resign, Hints, Compact, Status, Notice, Banner, CompactLabel };
constexpr std::array<const wchar_t*, 5> preset_names{
    L"개발 확인용 · 기본 3수", L"개발 확인용 · 승리 확인 7수", L"개발 확인용 · 금수 확인 8수",
    L"개발 확인용 · 무승부 확인 199수", L"개발 확인용 · 종료 배치 9수"};
int scaled(int value, UINT dpi) { return MulDiv(value, static_cast<int>(dpi), 96); }
std::wstring wide_ascii(const std::string& s) { return {s.begin(), s.end()}; }
const wchar_t* participant(st::Participant p) { return p == st::Participant::A ? L"A" : L"B"; }
const wchar_t* stone(st::StoneColor c) { return c == st::StoneColor::Black ? L"흑" : L"백"; }
std::wstring coordinate(st::Coordinate p)
{
    return std::wstring(1, static_cast<wchar_t>(L'A' + p.column)) + std::to_wstring(p.row + 1);
}
const wchar_t* rule_reason(rules::Reason r)
{
    switch (r) {
    case rules::Reason::OutOfBounds: return L"판 밖 좌표";
    case rules::Reason::Occupied: return L"이미 놓인 자리";
    case rules::Reason::DoubleThree: return L"삼삼";
    case rules::Reason::DoubleFour: return L"사사";
    case rules::Reason::Overline: return L"장목";
    case rules::Reason::ExactFive: return L"정확한 5목";
    case rules::Reason::WhiteFiveOrMore: return L"백 5목 이상";
    case rules::Reason::MoveLimit: return L"준비 배치 포함 200수";
    case rules::Reason::BoardFull: return L"만판";
    default: return L"입력 또는 상태 확인 필요";
    }
}
std::wstring response_error(const mt::Response& r)
{
    if (r.error == mt::Error::InvalidPlacement) { return rule_reason(r.decision.reason); }
    if (r.error == mt::Error::EmptyOpening) { return L"빈 배치는 시작 배치로 제공하지 않습니다."; }
    if (r.error == mt::Error::StaleInput) { return L"이전 대국 또는 이전 보드의 입력을 거부했습니다."; }
    if (r.error == mt::Error::NotPlaying) { return L"현재 대국은 착수를 받을 수 없습니다."; }
    if (r.error == mt::Error::InvalidOpening) {
        const auto e = r.opening_status.error;
        std::wstring text = L"배치 가져오기 실패: ";
        switch (e) {
        case opening::Error::MalformedData: text += L"compact 좌표 형식 오류"; break;
        case opening::Error::WrongNumber: text += L"수순 번호 오류"; break;
        case opening::Error::WrongColor: text += L"흑·백 순서 오류"; break;
        case opening::Error::TooLong: text += L"최대 200수 초과"; break;
        case opening::Error::AfterTerminal: text += L"종료된 수 뒤에 배치가 있습니다"; break;
        case opening::Error::ForbiddenPlacement: text += L"준비 배치에 흑 금수가 있습니다"; break;
        default: text += rule_reason(r.opening_status.reason); break;
        }
        if (r.opening_status.move_number) { text += L" (" + std::to_wstring(r.opening_status.move_number) + L"수)"; }
        return text;
    }
    return L"요청을 거부했습니다. 참가자·오프닝·현재 상태를 확인하세요.";
}

struct View {
    HWND window{};
    HFONT font{};
    mt::Controller controller;
    mt::HintWorker worker;
    mt::HintCache hints;
    bool show_hints{true};
    std::wstring hint_text{L"대국 시작 후 흑 차례에 계산합니다."};
    ~View() { if (font) { DeleteObject(font); } }
    HWND control(int id) const { return GetDlgItem(window, id); }
    void notice(const std::wstring& text) const { SetWindowTextW(control(Notice), text.c_str()); }
    void make_control(const wchar_t* type, const wchar_t* text, DWORD style, int id)
    {
        CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    }
    void fonts()
    {
        const auto new_font = CreateFontW(-scaled(15, GetDpiForWindow(window)), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"맑은 고딕");
        for (int id = Preset; id <= CompactLabel; ++id) { SendMessageW(control(id), WM_SETFONT, reinterpret_cast<WPARAM>(new_font), TRUE); }
        if (font) { DeleteObject(font); }
        font = new_font;
    }
    void select_preset()
    {
        const auto index = static_cast<int>(SendMessageW(control(Preset), CB_GETCURSEL, 0, 0));
        const auto data = mt::development_data(static_cast<mt::DevelopmentPosition>(std::clamp(index, 0, 4)));
        SetWindowTextW(control(Compact), wide_ascii(data.compact).c_str());
    }
    void create()
    {
        make_control(L"STATIC", L"로컬 사람 대 사람 · 렌주 | 개발 확인용 배치 · 대회 시간/자원 제한 미구현", 0, Banner);
        make_control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, Preset);
        for (const auto name : preset_names) { SendMessageW(control(Preset), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name)); }
        SendMessageW(control(Preset), CB_SETCURSEL, 0, 0);
        make_control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, Owner);
        SendMessageW(control(Owner), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"흑 A / 백 B"));
        SendMessageW(control(Owner), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"흑 B / 백 A"));
        SendMessageW(control(Owner), CB_SETCURSEL, 0, 0);
        make_control(L"BUTTON", L"시작", BS_PUSHBUTTON | WS_TABSTOP, Start);
        make_control(L"BUTTON", L"재시작", BS_PUSHBUTTON | WS_TABSTOP, Restart);
        make_control(L"BUTTON", L"현재 차례 기권", BS_PUSHBUTTON | WS_TABSTOP, Resign);
        make_control(L"BUTTON", L"흑 금수 표시", BS_AUTOCHECKBOX | WS_TABSTOP, Hints);
        SendMessageW(control(Hints), BM_SETCHECK, BST_CHECKED, 0);
        make_control(L"STATIC", L"compact 배치 입력", 0, CompactLabel);
        make_control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, Compact);
        SendMessageW(control(Compact), EM_SETLIMITTEXT, 600, 0);
        make_control(L"STATIC", L"배치를 선택하고 시작하세요. 공식 대회 오프닝 데이터는 포함되어 있지 않습니다.", 0, Notice);
        make_control(L"STATIC", L"", 0, Status);
        select_preset();
        fonts();
        refresh_status();
        SetTimer(window, 1, 50, nullptr);
    }
    void layout()
    {
        RECT r{}; GetClientRect(window, &r);
        const auto dpi = GetDpiForWindow(window);
        const auto move = [&](int id, int x, int y, int w, int h) {
            MoveWindow(control(id), scaled(x, dpi), scaled(y, dpi), scaled(w, dpi), scaled(h, dpi), TRUE);
        };
        const int width = MulDiv(r.right, 96, static_cast<int>(dpi));
        const int height = MulDiv(r.bottom, 96, static_cast<int>(dpi));
        move(Banner, 20, 12, width - 40, 28);
        move(Preset, 20, 48, 230, 240); move(Owner, 260, 48, 140, 200);
        move(Start, 410, 48, 80, 30); move(Restart, 500, 48, 90, 30);
        move(Resign, 600, 48, 130, 30); move(Hints, 750, 48, 185, 30);
        move(CompactLabel, 20, 92, 145, 26); move(Compact, 170, 88, width - 190, 28);
        move(Notice, 20, 123, width - 40, 30);
        move(Status, width - 275, 170, 250, height - 190);
        InvalidateRect(window, nullptr, FALSE);
    }
    BoardGeometry geometry() const
    {
        RECT r{}; GetClientRect(window, &r);
        return board_geometry(r.right, r.bottom, GetDpiForWindow(window));
    }
    void refresh_status()
    {
        std::wostringstream s;
        const auto& game = controller.game();
        const auto black = game.colors().owner_of(st::StoneColor::Black);
        s << L"대국 상태\n\n";
        if (black) {
            s << L"참가자 A: " << stone(*game.colors().color_of(st::Participant::A))
              << L" / 참가자 B: " << stone(*game.colors().color_of(st::Participant::B)) << L"\n";
        } else { s << L"참가자 색 미배정\n"; }
        if (controller.phase() == mt::Phase::Playing) {
            s << L"차례: " << participant(*game.next_participant()) << L" (" << stone(*game.next_color()) << L")\n";
        } else if (controller.phase() == mt::Phase::NotStarted) { s << L"시작 대기\n"; }
        else {
            if (const auto won = std::get_if<st::Won>(&game.result())) {
                s << L"결과: " << participant(won->winner) << L" (" << stone(*game.colors().color_of(won->winner)) << L") 승리\n";
            } else if (std::holds_alternative<st::Draw>(game.result())) { s << L"결과: 무승부\n"; }
            else { s << L"결과: 중단\n"; }
            s << L"종료 사유: ";
            switch (controller.end_reason()) {
            case mt::EndReason::Five: s << rule_reason(controller.rule_reason()); break;
            case mt::EndReason::Forbidden: s << L"흑 금수 반칙패 (" << rule_reason(controller.rule_reason()) << L")"; break;
            case mt::EndReason::Resigned: s << L"기권"; break;
            case mt::EndReason::MoveLimit: s << L"200수 자동 무승부"; break;
            case mt::EndReason::BoardFull: s << L"만판"; break;
            case mt::EndReason::OpeningResult: s << L"준비 배치에서 이미 종료"; break;
            default: s << L"대국 종료"; break;
            }
            s << L"\n";
        }
        s << L"\n총 배치: " << game.history().size() << L"수 (준비 " << controller.source_opening().game().history().size() << L"수 포함)\n";
        if (!game.history().empty()) {
            const auto last = game.history().back();
            s << L"마지막: " << coordinate(last.position) << L" " << stone(last.color)
              << (last.placed_by ? L" / 참가자 " : L" / 준비 배치")
              << (last.placed_by ? participant(*last.placed_by) : L"") << L"\n";
        }
        s << L"\n" << hint_text << L"\n\n흑 금수 클릭은 반칙패입니다.\n\n개발 확인용 배치\n공식 대회 오프닝 아님\n시계·자원 제한 미구현\n\nID: " << wide_ascii(controller.source_opening().data_id());
        SetWindowTextW(control(Status), s.str().c_str());
        EnableWindow(control(Resign), controller.phase() == mt::Phase::Playing);
        EnableWindow(control(Restart), controller.phase() != mt::Phase::NotStarted);
    }
    void schedule_hints()
    {
        hints.clear();
        worker.cancel();
        if (!show_hints) { hint_text = L"흑 금수 표시 꺼짐"; }
        else if (controller.phase() == mt::Phase::Playing && controller.game().next_color() == st::StoneColor::Black) {
            hint_text = L"흑 금수 계산 중… 착수 가능";
            worker.submit({controller.token(), controller.game()});
        } else { hint_text = L"흑 차례에만 금수를 표시합니다."; }
        refresh_status();
        InvalidateRect(window, nullptr, FALSE);
    }
    void poll_hints()
    {
        const auto result = worker.take_result();
        if (!result) { return; }
        if (result->failed && result->token == controller.token()) { hint_text = L"금수 표시 계산 실패 (착수 판정은 유지)"; }
        else if (hints.accept(*result, controller.token(), show_hints && controller.phase() == mt::Phase::Playing)) {
            std::wostringstream s;
            s << L"흑 금수 " << std::count(result->forbidden.begin(), result->forbidden.end(), true)
              << L"곳 · 계산 " << std::fixed << std::setprecision(2) << result->milliseconds << L" ms";
            hint_text = s.str();
        } else { return; }
        refresh_status();
        InvalidateRect(window, nullptr, FALSE);
    }
    void start()
    {
        const int length = GetWindowTextLengthW(control(Compact));
        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
        GetWindowTextW(control(Compact), text.data(), length + 1);
        text.resize(static_cast<std::size_t>(length));
        if (std::any_of(text.begin(), text.end(), [](wchar_t c) { return c > 127; })) {
            notice(L"배치 가져오기 실패: ASCII compact 좌표만 입력하세요. 진행 중 대국은 보존됩니다."); return;
        }
        const auto index = static_cast<int>(SendMessageW(control(Preset), CB_GETCURSEL, 0, 0));
        const auto preset = mt::development_data(static_cast<mt::DevelopmentPosition>(std::clamp(index, 0, 4)));
        const std::string compact(text.begin(), text.end());
        const auto owner = SendMessageW(control(Owner), CB_GETCURSEL, 0, 0) == 1 ? st::Participant::B : st::Participant::A;
        const auto result = controller.start_compact(compact == preset.compact ? preset.id : "dev-user-input-v1", compact, owner);
        if (!result.ok()) { notice(response_error(result) + L" · 진행 중 대국은 보존됩니다."); return; }
        notice(L"개발 확인용 배치로 시작했습니다. 교차점을 클릭하면 즉시 확정됩니다.");
        schedule_hints();
    }
    void click(Pixel pixel)
    {
        if (controller.phase() != mt::Phase::Playing) { notice(L"착수할 수 없는 상태입니다. 시작 또는 재시작하세요."); return; }
        const auto position = geometry().hit(pixel);
        if (!position) { notice(L"교차점 가까이를 클릭하세요. 판 밖과 교차점 사이 클릭은 반영하지 않습니다."); return; }
        const auto result = controller.place(controller.token(), *controller.game().next_participant(), *position);
        if (!result.ok()) { notice(response_error(result)); return; }
        notice(coordinate(*position) + (result.decision.verdict == rules::Verdict::Forbidden
            ? L" 금수 · 백 승리로 종료했습니다." : L" 착수를 확정했습니다."));
        schedule_hints();
    }
    void command(int id, int notification)
    {
        if (id == Preset && notification == CBN_SELCHANGE) { select_preset(); }
        else if (id == Start && notification == BN_CLICKED) { start(); }
        else if (id == Restart && notification == BN_CLICKED) {
            const auto result = controller.restart(controller.token());
            if (!result.ok()) { notice(response_error(result)); return; }
            notice(L"동일한 준비 배치·색 배정으로 재시작했습니다."); schedule_hints();
        } else if (id == Resign && notification == BN_CLICKED && controller.game().next_participant()) {
            const auto result = controller.resign(controller.token(), *controller.game().next_participant());
            if (!result.ok()) { notice(response_error(result)); return; }
            notice(L"현재 차례 참가자가 기권했습니다."); schedule_hints();
        } else if (id == Hints && notification == BN_CLICKED) {
            show_hints = SendMessageW(control(Hints), BM_GETCHECK, 0, 0) == BST_CHECKED;
            schedule_hints();
        }
    }
};

void paint(View& view)
{
    PAINTSTRUCT ps{};
    const auto output = BeginPaint(view.window, &ps);
    RECT area{}; GetClientRect(view.window, &area);
    const auto dc = CreateCompatibleDC(output);
    const auto bitmap = CreateCompatibleBitmap(output, std::max<LONG>(1, area.right), std::max<LONG>(1, area.bottom));
    const auto old_bitmap = SelectObject(dc, bitmap);
    FillRect(dc, &area, GetSysColorBrush(COLOR_WINDOW));
    const auto g = view.geometry();
    const auto old_font = SelectObject(dc, view.font ? view.font : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);
    if (g.valid()) {
        const auto dpi = GetDpiForWindow(view.window);
        const int margin = scaled(20, dpi);
        RECT board{g.left - margin, g.top - margin, g.left + g.step * 14 + margin, g.top + g.step * 14 + margin};
        const auto wood = CreateSolidBrush(RGB(239, 211, 163)); FillRect(dc, &board, wood); DeleteObject(wood);
        const auto line = CreatePen(PS_SOLID, std::max(1, scaled(1, dpi)), RGB(98, 79, 52));
        const auto old_pen = SelectObject(dc, line);
        for (int i = 0; i < 15; ++i) {
            MoveToEx(dc, g.left, g.top + i * g.step, nullptr); LineTo(dc, g.left + 14 * g.step, g.top + i * g.step);
            MoveToEx(dc, g.left + i * g.step, g.top, nullptr); LineTo(dc, g.left + i * g.step, g.top + 14 * g.step);
            wchar_t label[8]{};
            label[0] = static_cast<wchar_t>(L'A' + i);
            TextOutW(dc, g.left + i * g.step - scaled(5, dpi), g.top - scaled(39, dpi), label, 1);
            const auto number = std::to_wstring(i + 1);
            TextOutW(dc, g.left - scaled(38, dpi), g.top + i * g.step - scaled(10, dpi), number.c_str(), static_cast<int>(number.size()));
        }
        auto old_brush = SelectObject(dc, GetStockObject(BLACK_BRUSH));
        for (const st::Coordinate p : {st::Coordinate{3, 3}, {3, 11}, {7, 7}, {11, 3}, {11, 11}}) {
            const auto c = g.point(p); const int r = std::max(2, g.step / 12); Ellipse(dc, c.x - r, c.y - r, c.x + r + 1, c.y + r + 1);
        }
        for (std::size_t i = 0; i < st::board_area; ++i) {
            const auto cell = view.controller.game().board()[i];
            if (cell == st::Cell::Empty) { continue; }
            const auto c = g.point(*st::Coordinate::from_index(i));
            const int r = std::max(3, g.step * 42 / 100);
            SelectObject(dc, GetStockObject(cell == st::Cell::Black ? BLACK_BRUSH : WHITE_BRUSH));
            Ellipse(dc, c.x - r, c.y - r, c.x + r + 1, c.y + r + 1);
        }
        const auto red = CreatePen(PS_SOLID, std::max(1, scaled(2, dpi)), RGB(190, 36, 49)); SelectObject(dc, red);
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        if (!view.controller.game().history().empty()) {
            const auto c = g.point(view.controller.game().history().back().position);
            const int r = std::max(3, g.step / 6); Ellipse(dc, c.x - r, c.y - r, c.x + r + 1, c.y + r + 1);
        }
        const auto& cached = view.hints.value();
        if (view.show_hints && cached && cached->token == view.controller.token()) {
            for (std::size_t i = 0; i < st::board_area; ++i) {
                if (!cached->forbidden[i]) { continue; }
                const auto c = g.point(*st::Coordinate::from_index(i)); const int r = std::max(3, g.step / 5);
                MoveToEx(dc, c.x - r, c.y - r, nullptr); LineTo(dc, c.x + r, c.y + r);
                MoveToEx(dc, c.x - r, c.y + r, nullptr); LineTo(dc, c.x + r, c.y - r);
            }
        }
        SelectObject(dc, old_brush); SelectObject(dc, old_pen); DeleteObject(red); DeleteObject(line);
    }
    SelectObject(dc, old_font);
    BitBlt(output, 0, 0, area.right, area.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old_bitmap); DeleteObject(bitmap); DeleteDC(dc);
    EndPaint(view.window, &ps);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* view = reinterpret_cast<View*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        view = static_cast<View*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        view->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
    }
    if (!view) { return DefWindowProcW(window, message, wparam, lparam); }
    try {
        switch (message) {
        case WM_CREATE: view->create(); view->layout(); return 0;
        case WM_COMMAND: view->command(LOWORD(wparam), HIWORD(wparam)); return 0;
        case WM_LBUTTONUP: view->click({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}); return 0;
        case WM_PAINT: paint(*view); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: view->layout(); return 0;
        case WM_TIMER: view->poll_hints(); return 0;
        case WM_GETMINMAXINFO: {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
            limits->ptMinTrackSize = {scaled(980, GetDpiForWindow(window)), scaled(700, GetDpiForWindow(window))}; return 0;
        }
        case WM_DPICHANGED: {
            const auto* r = reinterpret_cast<const RECT*>(lparam);
            SetWindowPos(window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            view->fonts(); view->layout(); return 0;
        }
        case WM_CLOSE:
            static_cast<void>(view->controller.close(view->controller.token()));
            view->worker.cancel(); DestroyWindow(window); return 0;
        case WM_DESTROY: KillTimer(window, 1); PostQuitMessage(0); return 0;
        default: break;
        }
    } catch (...) { view->notice(L"요청 처리 중 오류가 발생했습니다. 현재 대국 상태를 확인하세요."); }
    return DefWindowProcW(window, message, wparam, lparam);
}
int report_error(const wchar_t* step)
{
    wchar_t details[512]{};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(), 0, details, 512, nullptr);
    MessageBoxW(nullptr, details[0] ? details : L"Windows 오류가 발생했습니다.", step, MB_OK | MB_ICONERROR);
    return 1;
}
}
int run(HINSTANCE instance, int show_command)
{
    try {
        View view;
        WNDCLASSEXW descriptor{};
        descriptor.cbSize = sizeof(descriptor); descriptor.hInstance = instance;
        descriptor.lpfnWndProc = window_proc; descriptor.lpszClassName = window_class;
        descriptor.hCursor = LoadCursorW(nullptr, IDC_ARROW); descriptor.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        descriptor.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        if (!RegisterClassExW(&descriptor)) { return report_error(L"창 등록 실패"); }
        const auto dpi = GetDpiForSystem();
        RECT frame{0, 0, scaled(1100, dpi), scaled(850, dpi)};
        if (!AdjustWindowRectExForDpi(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi)) { return report_error(L"창 크기 계산 실패"); }
        const auto window = CreateWindowExW(0, window_class, L"오목 프로젝트 · 로컬 대국", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left, frame.bottom - frame.top,
            nullptr, nullptr, instance, &view);
        if (!window) { return report_error(L"창 생성 실패"); }
        ShowWindow(window, show_command); UpdateWindow(window);
        MSG message{};
        for (;;) {
            const auto result = GetMessageW(&message, nullptr, 0, 0);
            if (result == -1) { return report_error(L"메시지 처리 실패"); }
            if (result == 0) { return static_cast<int>(message.wParam); }
            if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
        }
    } catch (...) { MessageBoxW(nullptr, L"대국 창을 초기화하지 못했습니다.", L"오목 프로젝트", MB_OK | MB_ICONERROR); return 1; }
}
} // namespace omok::gui
