#include "menu.h"
#include "file.h"
#include "hub75.h"
#include "i2s.h"
#include "xprintf.h"
#include "font8x8_basic.h"
#include <algorithm>
#include <vector>

struct Elm;

static std::vector<Elm *> stack;
static std::vector<char> text;

static constexpr struct Curve {
	static constexpr int N = 16;
	static constexpr float A = .1f;
	constexpr Curve() : t() {
		for (int i = 0; i < N; i++) {
			float x = i / float(N - 1);
			t[i] = 0xffff * (A * x / (A + x * (1 - x)));
		}
	}
	u16 t[N];
} curve;

struct Elm {
	Elm() { v = vlast = vmin = vmax = 0; }
	virtual void left() { stack.pop_back(); }
	virtual void right() { v = vlast; update(); stack.pop_back(); }
	virtual void wheel(s8 d) { v = std::clamp(s8(v + d), vmin, vmax); update(); }
	virtual void update() {}
	virtual void draw() {}
	void push() { stack.push_back(this); vlast = v; }
	s8 v, vlast, vmin, vmax;
};

static struct File : Elm {
	void wheel(s8 d) { v = file_get_index(); vmax = file_getN() - 1; Elm::wheel(d); }
	void right() { left(); }
	void update() { file_set_next(v); }
	void draw() { xprintf("%s", file_get_name()); }
} m_file;

static struct Volume : Elm {
	Volume() { v = curve.N / 2; vmax = curve.N - 1; update(); }
	void update() { i2s_set_volume(curve.t[v]); }
	void draw() { xprintf("VOLUME %d", v); }
} m_volume;

static struct Gamma : Elm {
	Gamma() { v = GAMMA_STD; vmax = GAMMA_N - 1; update(); }
	void update() { hub75_set_gamma_index(v); }
	void draw() { xprintf("GAMMA %.1f", gamma_value(v)); }
} m_gamma;

static struct PWM : Elm {
	PWM() { v = vmax = PWMMAX; vmin = 1; update(); }
	void update() { hub75_set_pwmbits(v); }
	void draw() { xprintf("PWM %d", v); }
} m_pwm;

static struct RPS : Elm {
	RPS() { v = vmax = 12; vmin = 3; update(); }
	void update() { hub75_set_rps(10 * v); }
	void draw() { xprintf("RPS %d", 10 * v); }
} m_rps;

static struct Top : Elm {
	Top() { vmax = sizeof(title) / sizeof(char *) - 1; }
	void left() {
		switch (v) {
			case 0: m_file.push(); break;
			case 1: m_volume.push(); break;
			case 2: m_gamma.push(); break;
			case 3: m_pwm.push(); break;
			case 4: m_rps.push(); break;
		}
	}
	void draw() { xprintf("%s", title[v]); }
	static inline const char *title[] = { "FILE", "VOLUME", "GAMMA", "PWM", "RPS" };
} m_top;

static void menu_putchar(int c) {
	text.push_back(c);
}

void menu_init() {
	xdev_out(menu_putchar);
}

void menu_write(u32 *buf) {
	if (stack.empty()) return;
	stack.back()->draw();
	for (int i = 0; i < text.size(); i++) {
		u8 *sp = (u8 *)font8x8_basic[text[i]];
		u32 *dp = buf + 8 * i;
		for (int y = 0; y < 8; y++) {
			u8 d = *sp++;
			for (int x = 0; x < 8; x++)
				dp[x] = dp[x] >> 1 & 0x7f7f7f | (d >> x & 1 ? 0xffffff : 0);
			dp += WIDTH;
		}
	}
	text.clear();
}

void menu_left() {
	if (stack.empty()) stack.push_back(&m_top);
	else stack.back()->left();
}

void menu_right() {
	if (!stack.empty()) stack.back()->right();
}

void menu_wheel(s8 d) {
	if (!stack.empty()) stack.back()->wheel(d);
}
