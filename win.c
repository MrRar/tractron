#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <tractron.h>

#include <libc/nt/windows.h>
#include <libc/nexgen32e/nt2sysv.h>
#include <libc/nt/dll.h>
#include <libc/nt/enum/idc.h>
#include <libc/nt/enum/color.h>
#include <libc/nt/enum/cw.h>
#include <libc/nt/enum/ws.h>
#include <libc/nt/enum/sw.h>
#include <libc/nt/enum/wm.h>
#include <libc/nt/enum/rdw.h>
#include <libc/nt/struct/msg.h>
#include <libc/nt/enum/bitblt.h>
#include <libc/nt/events.h>
#include <libc/nt/paint.h>
#include <libc/nt/struct/paintstruct.h>
#include <libc/nt/struct/rect.h>
#include <libc/nt/dll.h>

int char_width = 10;
int char_height = 10 * 2;

int64_t hwnd;
int64_t hfont;

bool __msabi (*TextOutW)(
	uint64_t hdc, int32_t x, int32_t y,
	uint16_t *lpString, int32_t c) = NULL;

uint32_t __msabi (*SetBkColor)(uint64_t hdc, uint32_t color) = NULL;

uint64_t __msabi (*CreateFontA)(
	int32_t cHeight, int32_t cWidth,
	int32_t cEscapement, int32_t cOrientation, int32_t cWeight,
	uint32_t bItalic, uint32_t bUnderline, uint32_t bStrikeOut,
	uint32_t iCharSet, uint32_t iOutPrecision,
	uint32_t iClipPresision, uint32_t iQuality,
	uint32_t iPitchAndFamily, char *pszFaceName ) = NULL;

bool __msabi (*PeekMessageW)(
	struct NtMsg *lpMsg, uint64_t hWnd,
	uint32_t wMsgFilterMin, uint32_t wMsgFilterMax,
	uint32_t wRemoveMsg) = NULL;

uint32_t __msabi (*ReleaseDC)(int64_t hWnd, int64_t hDC);

uint32_t __msabi (*GetDC)(int64_t hWnd);

bool __msabi (*AdjustWindowRectEx)(
	struct NtRect *lpRect,
	uint32_t dwStyle,
	bool bMenu,
	uint32_t dwExStyle) = NULL;

static int cga_color_to_colorref(unsigned char ch) {
	switch (ch) {
	case BRIGHT_BLUE: return 0xff5555;
	case BRIGHT_GREEN: return 0x55ff55;
	case BRIGHT_CYAN: return 0xffff55;
	case BRIGHT_RED: return 0x5555ff;
	case BRIGHT_MAGENTA: return 0xff55ff;
	case DARK_YELLOW: return 0x0055aa;
	case BRIGHT_YELLOW: return 0x55ffff;
	case DARK_BLACK: return 0x000000;
	case DARK_WHITE: return 0xaaaaaa;
	case BRIGHT_WHITE: return 0xffffff;
	default: return 0xaa5500;
	}
}

static short unsigned int *cp437_to_utf16(unsigned char ch) {
	switch (ch) {
	case 0xc9: return u"╔";
	case 0xbb: return u"╗";
	case 0xc8: return u"╚";
	case 0xbc: return u"╝";
	case 0xcd: return u"═";
	case 0xba: return u"║";
	case 0x1e: return u"▲";
	case 0x10: return u"►";
	case 0x1f: return u"▼";
	case 0x11: return u"◄";
	case 0xb3: return u"│";
	case 0xc4: return u"─";
	case 0xda: return u"┌";
	case 0xbf: return u"┐";
	case 0xc0: return u"└";
	case 0xd9: return u"┘";
	default: return u"?";
	}
}

void windows_draw_window() {
	int64_t window_hdc = GetDC(hwnd);
	struct NtRect window_rect = {
		0, 0,
		screen_width * char_width,
		screen_height * char_height
	};
	int64_t bmp_hdc = CreateCompatibleDC(window_hdc);
	int64_t bmp = CreateCompatibleBitmap(window_hdc, window_rect.right, window_rect.bottom);
	SelectObject(bmp_hdc, bmp);
	SelectObject(bmp_hdc, hfont);
	SetBkMode(bmp_hdc, 1);

	char buf[screen_width * 2];

	for (int y = 0; y < screen_height; y++) {
		int start = 0;
		int count = 0;
		unsigned char run_color = screen_buffer[(y * screen_width) * 2 + 1];
		for (int x = 0; x < screen_width; x++) {
			unsigned char ch = screen_buffer[(y * screen_width + x) * 2];
			unsigned char color = screen_buffer[(y * screen_width + x) * 2 + 1];

			if (color & TEXT_BLINK) {
				if (current_time % 1024 > 512) {
					ch = ' ';
				} else {
					color = color ^ TEXT_BLINK;
				}
			}

			if (color != run_color && ch != ' ') {
				SetTextColor(bmp_hdc, cga_color_to_colorref(run_color));
				TextOutW(bmp_hdc, start * char_width, y * char_height, (uint16_t *)buf, count);
				start = x;
				count = 0;
				run_color = color;
			}

			if (ch < ' ' || ch > '~') {
				unsigned char *str = (unsigned char *)cp437_to_utf16(ch);
				buf[count * 2] = str[0];
				buf[count * 2 + 1] = str[1];
			} else {
				buf[count * 2] = ch;
				buf[count * 2 + 1] = 0;
			}
			count++;
		}
		SetTextColor(bmp_hdc, cga_color_to_colorref(run_color));
		TextOutW(bmp_hdc, start * char_width, y * char_height, (uint16_t *)buf, count);
	}
	BitBlt(window_hdc, 0, 0, window_rect.right, window_rect.bottom, bmp_hdc,
		0, 0, kNtSrccopy);

	DeleteObject(bmp);
 	DeleteDC(bmp_hdc);
	ReleaseDC(hwnd, window_hdc);
}

static int64_t WindowProc(int64_t hwnd, uint32_t uMsg, uint64_t wParam, int64_t lParam) {
	if (uMsg == kNtWmDestroy) {
		exit(0);
		return 0;
	} else {
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

void windows_init() {
	// This is a hack to get cosmocc to set the subsystem of the output
	// .exe file to GUI instead of console.
	// Cosmo looks to see if GetMessage gets imported.
	map_set(0, 0, (long long)GetMessage, BRIGHT_RED);

	int64_t hGDI = LoadLibraryA("gdi32.dll");
	SetBkColor = GetProcAddress(hGDI, "SetBkColor");
	TextOutW = GetProcAddress(hGDI, "TextOutW");
	CreateFontA = GetProcAddress(hGDI, "CreateFontA");

	int64_t hUser = LoadLibraryA("user32.dll");
	PeekMessageW = GetProcAddress(hUser, "PeekMessageW");
	ReleaseDC = GetProcAddress(hUser, "ReleaseDC");
	GetDC = GetProcAddress(hUser, "GetDC");
	AdjustWindowRectEx = GetProcAddress(hUser, "AdjustWindowRectEx");

	static const char16_t kClassName[] = u"tractron";
	struct NtWndClass wc = {0};
	wc.lpfnWndProc = NT2SYSV(WindowProc);
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = kClassName;
	wc.hbrBackground = kNtColorInactivecaptiontext;
	RegisterClass(&wc);

	uint32_t style = kNtWsVisible | kNtWsBorder | kNtWsCaption | kNtWsSysmenu;
	struct NtRect window_rect = {
		0, 0,
		screen_width * char_width,
		screen_height * char_height - 17
	};
	AdjustWindowRectEx(&window_rect, style, true, kNtWsExAppwindow);

	hwnd = CreateWindowEx(
		kNtWsExAppwindow, kClassName,
		kClassName,
		style,
		kNtCwUsedefault, kNtCwUsedefault,
		window_rect.right - window_rect.left,
		window_rect.bottom - window_rect.top,
		0, 0, wc.hInstance, 0);

	hfont = CreateFontA(
		-char_height, char_width, 0, 0, 400,
		false, false, false,
		0/*ANSI_CHARSET*/, 0/*OUT_DEFAULT_PRECIS*/,
		0/*CLIP_DEFAULT_PRECIS*/, 0/*DEFAULT_QUALITY*/, 1/*FIXED_PITCH*/, NULL);
}

char windows_read_input() {
	struct NtMsg msg;
	while(PeekMessageW(&msg, 0, 0, 0, 1/*PM_REMOVE*/)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.dwMessage == kNtWmChar) {
			if (msg.wParam == '\b') {
				return '\x7f';
			}
			return msg.wParam;
		} else if (msg.dwMessage == kNtWmKeydown) {
			switch(msg.wParam) {
			case 0x26:
				return 'w';
			case 0x27:
				return 'd';
			case 0x28:
				return 's';
			case 0x25:
				return 'a';
			}
		} else if (msg.dwMessage == kNtWmPaint) {
			struct NtPaintStruct ps;
			BeginPaint(hwnd, &ps);
			windows_draw_window();
			EndPaint(hwnd, &ps);
		}
	}
	return EOF;
}
