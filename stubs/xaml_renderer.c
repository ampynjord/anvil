/*
 * xaml_renderer.c — Anvil: GDI software renderer for the XAML visual layer.
 *
 * Renders a Windows-Calculator-style UI into the CoreWindow HWND using pure
 * GDI (no D3D11 / Composition required).  Called from the Win32 wnd_proc
 * WM_PAINT / WM_LBUTTONDOWN handlers in core_objects.c.
 *
 * Exported symbols:
 *   anvil_renderer_paint(HWND)       — render full frame (WM_PAINT)
 *   anvil_renderer_click(HWND,x,y)   — hit-test + handle click (WM_LBUTTONDOWN)
 *   anvil_renderer_set_display(text) — override display text from WinRT layer
 */

#ifndef _WIN32
/* Linux unit-test guard */
void anvil_renderer_paint(void *hwnd)    { (void)hwnd; }
void anvil_renderer_click(void *hwnd, int x, int y) { (void)hwnd; (void)x; (void)y; }
void anvil_renderer_set_display(const unsigned short *t) { (void)t; }
#else

# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <math.h>

/* =========================================================================
 * Calculator model
 * ========================================================================= */

#define DISP_MAX 32
static WCHAR g_display[DISP_MAX] = L"0";
static WCHAR g_history[DISP_MAX] = L"";
static double g_acc   = 0.0;
static int    g_op    = 0;   /* 0=none, '+''-''*''/' */
static int    g_new   = 1;   /* next digit starts fresh */

static void model_show(double v)
{
    /* Strip unnecessary trailing zeros */
    if (v == (long long)v && v >= -1e14 && v <= 1e14)
        swprintf(g_display, DISP_MAX, L"%lld", (long long)v);
    else
        swprintf(g_display, DISP_MAX, L"%.10g", v);
}

static void model_digit(WCHAR ch)
{
    if (g_new) { g_display[0] = 0; g_new = 0; }
    int len = (int)wcslen(g_display);
    if (ch == L'.' && wcschr(g_display, L'.')) return;
    if (len >= DISP_MAX - 1) return;
    if (len == 0 && ch == L'0') { g_display[0] = L'0'; g_display[1] = 0; return; }
    if (len == 1 && g_display[0] == L'0' && ch != L'.') { g_display[0] = ch; return; }
    g_display[len] = ch; g_display[len+1] = 0;
}

static double display_val(void)
{
    double v = 0.0;
    swscanf(g_display, L"%lf", &v);
    return v;
}

static void model_op(int op)
{
    double cur = display_val();
    if (g_op && !g_new) {
        switch (g_op) {
        case '+': cur = g_acc + cur; break;
        case '-': cur = g_acc - cur; break;
        case '*': cur = g_acc * cur; break;
        case '/': cur = (cur != 0) ? g_acc / cur : 0; break;
        }
    }
    g_acc = cur;
    model_show(cur);
    if (op == '=')
        swprintf(g_history, DISP_MAX, L"%.10g =", g_acc);
    else
        swprintf(g_history, DISP_MAX, L"%.10g %lc", g_acc, (wint_t)op);
    g_op  = (op == '=') ? 0 : op;
    g_new = 1;
}

static void model_clear(void)
{
    wcscpy(g_display, L"0"); wcscpy(g_history, L""); g_acc = 0; g_op = 0; g_new = 1;
}
static void model_ce(void)    { wcscpy(g_display, L"0"); g_new = 1; }
static void model_negate(void){ model_show(-display_val()); }
static void model_pct(void)   { model_show(g_acc * display_val() / 100.0); }
static void model_recip(void) { double v = display_val(); model_show(v ? 1.0/v : 0.0); }
static void model_sqrt(void)  { double v = display_val(); model_show(v >= 0 ? sqrt(v) : 0.0); }
static void model_sq(void)    { double v = display_val(); model_show(v * v); }
static void model_back(void)
{
    int len = (int)wcslen(g_display);
    if (g_new || len <= 1) { wcscpy(g_display, L"0"); g_new = 1; return; }
    g_display[len - 1] = 0;
}

/* =========================================================================
 * Button layout (Windows 10 Calculator Standard mode)
 * ========================================================================= */

#define BTN_COLS 4
#define BTN_ROWS 6

typedef struct {
    const WCHAR *label;
    COLORREF     bg;      /* normal background */
    int          action;  /* A_ constant */
} BtnDef;

#define A_DIGIT  1
#define A_DOT    2
#define A_PLUS   3
#define A_MINUS  4
#define A_MUL    5
#define A_DIV    6
#define A_EQ     7
#define A_CLEAR  8
#define A_CE     9
#define A_BACK   10
#define A_PCT    11
#define A_RECIP  12
#define A_SQ     13
#define A_SQRT   14
#define A_NEG    15

#define COL_NUM   0x323232   /* number buttons */
#define COL_OP    0x3A3A3A   /* operator buttons */
#define COL_EQ    0x005C87   /* = button (accent) */
#define COL_FN    0x2A2A2A   /* function row */

static const BtnDef s_btns[BTN_ROWS][BTN_COLS] = {
    /* row 0 */
    { {L"%", COL_FN,  A_PCT  }, {L"CE",  COL_FN, A_CE  },
      {L"C",  COL_FN, A_CLEAR}, {L"\x232B", COL_FN, A_BACK} }, /* ⌫ */
    /* row 1 */
    { {L"\xB9\xC2x", COL_FN, A_RECIP}, {L"x\xB2", COL_FN, A_SQ  },
      {L"\x221Ax",   COL_FN, A_SQRT }, {L"\xF7",  COL_OP, A_DIV  } }, /* ÷ */
    /* row 2 */
    { {L"7", COL_NUM, A_DIGIT}, {L"8", COL_NUM, A_DIGIT},
      {L"9", COL_NUM, A_DIGIT}, {L"\xD7", COL_OP, A_MUL} }, /* × */
    /* row 3 */
    { {L"4", COL_NUM, A_DIGIT}, {L"5", COL_NUM, A_DIGIT},
      {L"6", COL_NUM, A_DIGIT}, {L"-", COL_OP,  A_MINUS} },
    /* row 4 */
    { {L"1", COL_NUM, A_DIGIT}, {L"2", COL_NUM, A_DIGIT},
      {L"3", COL_NUM, A_DIGIT}, {L"+", COL_OP,  A_PLUS } },
    /* row 5 */
    { {L"\xB1",  COL_NUM, A_NEG  }, {L"0",  COL_NUM, A_DIGIT},
      {L".",     COL_NUM, A_DOT  }, {L"=",  COL_EQ,  A_EQ   } },
};

/* =========================================================================
 * Layout geometry
 * ========================================================================= */

#define WND_W      380
#define WND_H      560
#define DISP_H     130
#define HIST_H      28
#define BTN_GAP      3
#define BTN_H       64

static void btn_rect(int row, int col, RECT *r)
{
    int total_w  = WND_W;
    int btn_w    = (total_w - BTN_GAP * (BTN_COLS + 1)) / BTN_COLS;
    int x = BTN_GAP + col * (btn_w + BTN_GAP);
    int y = DISP_H + BTN_GAP + row * (BTN_H + BTN_GAP);
    r->left   = x;
    r->right  = x + btn_w;
    r->top    = y;
    r->bottom = y + BTN_H;
}

/* =========================================================================
 * Rendering
 * ========================================================================= */

static COLORREF blend(COLORREF c, int lighter)
{
    int r = (c & 0xFF) + lighter;
    int g = ((c >> 8) & 0xFF) + lighter;
    int b = ((c >> 16) & 0xFF) + lighter;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return RGB(r, g, b);
}

static int g_hover_row = -1, g_hover_col = -1;
static int g_press_row = -1, g_press_col = -1;

void anvil_renderer_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT cr; GetClientRect(hwnd, &cr);
    int cw = cr.right, ch = cr.bottom;

    /* Double-buffer */
    HDC mdc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, cw, ch);
    HGDIOBJ old_bmp = SelectObject(mdc, bmp);

    /* Background */
    HBRUSH bg_brush = CreateSolidBrush(RGB(0x1C, 0x1C, 0x1C));
    RECT full = {0, 0, cw, ch};
    FillRect(mdc, &full, bg_brush);
    DeleteObject(bg_brush);

    /* History line */
    HFONT hist_font = CreateFontW(18, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ old_font = SelectObject(mdc, hist_font);
    SetBkMode(mdc, TRANSPARENT);
    SetTextColor(mdc, RGB(0x99, 0x99, 0x99));
    RECT hr = {10, DISP_H - HIST_H - 36, cw - 10, DISP_H - 10};
    DrawTextW(mdc, g_history[0] ? g_history : L"", -1, &hr,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(mdc, old_font);
    DeleteObject(hist_font);

    /* Main display number */
    int disp_len = (int)wcslen(g_display);
    int font_sz  = disp_len > 12 ? 36 : disp_len > 8 ? 46 : 56;
    HFONT disp_font = CreateFontW(font_sz, 0, 0, 0, FW_LIGHT, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    old_font = SelectObject(mdc, disp_font);
    SetTextColor(mdc, RGB(0xFF, 0xFF, 0xFF));
    RECT dr = {10, 40, cw - 10, DISP_H - 10};
    DrawTextW(mdc, g_display, -1, &dr,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(mdc, old_font);
    DeleteObject(disp_font);

    /* Buttons */
    HFONT btn_font = CreateFontW(22, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    old_font = SelectObject(mdc, btn_font);

    for (int row = 0; row < BTN_ROWS; row++) {
        for (int col = 0; col < BTN_COLS; col++) {
            RECT br; btn_rect(row, col, &br);
            const BtnDef *bd = &s_btns[row][col];

            COLORREF bg = bd->bg;
            if (row == g_press_row && col == g_press_col)
                bg = blend(bg, 40);
            else if (row == g_hover_row && col == g_hover_col)
                bg = blend(bg, 20);

            HBRUSH bbr = CreateSolidBrush(bg);
            FillRect(mdc, &br, bbr);
            DeleteObject(bbr);

            /* subtle border */
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x40, 0x40, 0x40));
            HGDIOBJ old_pen = SelectObject(mdc, pen);
            HGDIOBJ old_br2 = SelectObject(mdc, GetStockObject(NULL_BRUSH));
            Rectangle(mdc, br.left, br.top, br.right, br.bottom);
            SelectObject(mdc, old_pen);
            SelectObject(mdc, old_br2);
            DeleteObject(pen);

            SetBkMode(mdc, TRANSPARENT);
            SetTextColor(mdc, RGB(0xFF, 0xFF, 0xFF));
            DrawTextW(mdc, bd->label, -1, &br,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    SelectObject(mdc, old_font);
    DeleteObject(btn_font);

    /* Blit */
    BitBlt(hdc, 0, 0, cw, ch, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(mdc);
    EndPaint(hwnd, &ps);
}

void anvil_renderer_click(HWND hwnd, int mx, int my)
{
    for (int row = 0; row < BTN_ROWS; row++) {
        for (int col = 0; col < BTN_COLS; col++) {
            RECT br; btn_rect(row, col, &br);
            if (mx >= br.left && mx < br.right &&
                my >= br.top  && my < br.bottom) {
                const BtnDef *bd = &s_btns[row][col];
                switch (bd->action) {
                case A_DIGIT:  model_digit(bd->label[0]); break;
                case A_DOT:    model_digit(L'.'); break;
                case A_PLUS:   model_op('+'); break;
                case A_MINUS:  model_op('-'); break;
                case A_MUL:    model_op('*'); break;
                case A_DIV:    model_op('/'); break;
                case A_EQ:     model_op('='); break;
                case A_CLEAR:  model_clear(); break;
                case A_CE:     model_ce(); break;
                case A_BACK:   model_back(); break;
                case A_PCT:    model_pct(); break;
                case A_RECIP:  model_recip(); break;
                case A_SQ:     model_sq(); break;
                case A_SQRT:   model_sqrt(); break;
                case A_NEG:    model_negate(); break;
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return;
            }
        }
    }
}

void anvil_renderer_set_display(const WCHAR *text)
{
    if (!text) return;
    wcsncpy(g_display, text, DISP_MAX - 1);
    g_display[DISP_MAX - 1] = 0;
}

#endif /* _WIN32 */
