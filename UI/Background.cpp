#include <cmath>
#include "Common/GPU/thin3d.h"
#include "Common/Data/Text/I18n.h"
#include "Common/Math/geom2d.h"
#include "Common/Math/curves.h"
#include "Common/Data/Random/Rng.h"
#include "Common/Data/Color/RGBAUtil.h"
#include "Common/UI/Context.h"
#include "Common/UI/UI.h"
#include "Common/File/Path.h"
#include "Common/File/FileUtil.h"
#include "Common/TimeUtil.h"
#include "Common/Render/ManagedTexture.h"
#include "Common/System/System.h"
#include "Common/System/Display.h"
#include "Common/System/Request.h"
#include "Core/ConfigValues.h"
#include "Core/Config.h"
#include "Core/System.h"
#include "Core/Util/RecentFiles.h"
#include "UI/Background.h"
#include "UI/GameInfoCache.h"


#ifdef _MSC_VER
#pragma execution_character_set("utf-8")
#endif

static Draw::Texture *bgTexture;

class Animation {
public:
	virtual ~Animation() = default;
	virtual void Draw(UIContext &dc, double t, float alpha, float x, float y, float z) = 0;
};

class MovingBackground : public Animation {
public:
	void Draw(UIContext &dc, double t, float alpha, float x, float y, float z) override {
		if (!bgTexture)
			return;

		dc.Flush();
		dc.GetDrawContext()->BindTexture(0, bgTexture);
		Bounds bounds = dc.GetBounds();

		x = std::min(std::max(x / bounds.w, 0.0f), 1.0f) * XFAC;
		y = std::min(std::max(y / bounds.h, 0.0f), 1.0f) * YFAC;
		z = 1.0f + std::max(XFAC, YFAC) + (z - 1.0f) * ZFAC;

		lastX_ = abs(x - lastX_) > 0.001f ? x * XSPEED + lastX_ * (1.0f - XSPEED) : x;
		lastY_ = abs(y - lastY_) > 0.001f ? y * YSPEED + lastY_ * (1.0f - YSPEED) : y;
		lastZ_ = abs(z - lastZ_) > 0.001f ? z * ZSPEED + lastZ_ * (1.0f - ZSPEED) : z;

		float u1 = lastX_ / lastZ_;
		float v1 = lastY_ / lastZ_;
		float u2 = (1.0f + lastX_) / lastZ_;
		float v2 = (1.0f + lastY_) / lastZ_;

		dc.Draw()->DrawTexRect(bounds, u1, v1, u2, v2, whiteAlpha(alpha));

		dc.Flush();
		dc.RebindTexture();
	}

private:
	static constexpr float XFAC = 0.3f;
	static constexpr float YFAC = 0.3f;
	static constexpr float ZFAC = 0.12f;
	static constexpr float XSPEED = 0.05f;
	static constexpr float YSPEED = 0.05f;
	static constexpr float ZSPEED = 0.1f;

	float lastX_ = 0.0f;
	float lastY_ = 0.0f;
	float lastZ_ = 1.0f + std::max(XFAC, YFAC);
};

class WaveAnimation : public Animation {
public:
	void Draw(UIContext &dc, double t, float alpha, float x, float y, float z) override {
		const uint32_t color = colorAlpha(0xFFFFFFFF, alpha * 0.2f);
		const float speed = 1.0;

		Bounds bounds = dc.GetBounds();
		dc.Flush();
		dc.BeginNoTex();

		// 500 is enough for any resolution really. 24 * 500 = 12000 which fits handily in our UI vertex buffer (max 65536 per flush).
		const int steps = std::max(20, std::min((int)g_display.dp_xres, 500));
		float step = (float)g_display.dp_xres / (float)steps;
		t *= speed;

		for (int n = 0; n < steps; n++) {
			float x = (float)n * step;
			float nextX = (float)(n + 1) * step;
			float i = x * 1280 / bounds.w;

			float wave0 = sin(i * 0.005 + t * 0.8) * 0.05 + sin(i * 0.002 + t * 0.25) * 0.02 + sin(i * 0.001 + t * 0.3) * 0.03 + 0.625;
			float wave1 = sin(i * 0.0044 + t * 0.4) * 0.07 + sin(i * 0.003 + t * 0.1) * 0.02 + sin(i * 0.001 + t * 0.3) * 0.01 + 0.625;
			dc.Draw()->RectVGradient(x, wave0 * bounds.h, nextX, bounds.h, color, 0x00000000);
			dc.Draw()->RectVGradient(x, wave1 * bounds.h, nextX, bounds.h, color, 0x00000000);

			// Add some "antialiasing"
			dc.Draw()->RectVGradient(x, wave0 * bounds.h - 3.0f * g_display.pixel_in_dps_y, nextX, wave0 * bounds.h, 0x00000000, color);
			dc.Draw()->RectVGradient(x, wave1 * bounds.h - 3.0f * g_display.pixel_in_dps_y, nextX, wave1 * bounds.h, 0x00000000, color);
		}

		dc.Flush();
		dc.Begin();
	}
};

class FloatingSymbolsAnimation : public Animation {
public:
	FloatingSymbolsAnimation(bool is_colored) {
		this->is_colored = is_colored;
	}

	void Draw(UIContext &dc, double t, float alpha, float x, float y, float z) override {
		dc.Flush();
		dc.Begin();
		float xres = dc.GetBounds().w;
		float yres = dc.GetBounds().h;
		if (last_xres != xres || last_yres != yres) {
			Regenerate(xres, yres);
		}

		for (int i = 0; i < COUNT; i++) {
			float x = xbase[i] + dc.GetBounds().x;
			float y = ybase[i] + dc.GetBounds().y + 40 * cosf(i * 7.2f + t * 1.3f);
			float angle = (float)sin(i + t);
			int n = i & 3;
			Color color = is_colored ? colorAlpha(COLORS[n], alpha * 0.25f) : colorAlpha(DEFAULT_COLOR, alpha * 0.1f);
			ui_draw2d.DrawImageRotated(SYMBOLS[n], x, y, 1.0f, angle, color);
		}
		dc.Flush();
	}

private:
	static constexpr int COUNT = 100;
	static constexpr Color DEFAULT_COLOR = 0xC0FFFFFF;
	static constexpr Color COLORS[4] = {0xFFE3B56F, 0xFF615BFF, 0xFFAA88F3, 0xFFC2CC7A,}; // X O D A
	static const ImageID SYMBOLS[4];

	bool is_colored = false;
	float xbase[COUNT]{};
	float ybase[COUNT]{};
	float last_xres = 0;
	float last_yres = 0;

	void Regenerate(int xres, int yres) {
		GMRng rng;
		for (int i = 0; i < COUNT; i++) {
			xbase[i] = rng.F() * xres;
			ybase[i] = rng.F() * yres;
		}

		last_xres = xres;
		last_yres = yres;
	}
};

const ImageID FloatingSymbolsAnimation::SYMBOLS[4] = {
	ImageID("I_CROSS"),
	ImageID("I_CIRCLE"),
	ImageID("I_SQUARE"),
	ImageID("I_TRIANGLE"),
};

class RecentGamesAnimation : public Animation {
public:
	void Draw(UIContext &dc, double t, float alpha, float x, float y, float z) override {
		if (lastIndex_ == nextIndex_) {
			CheckNext(dc, t);
		} else if (t > nextT_) {
			lastIndex_ = nextIndex_;
		}

		if (g_recentFiles.HasAny()) {
			std::shared_ptr<GameInfo> lastInfo = GetInfo(dc, lastIndex_);
			std::shared_ptr<GameInfo> nextInfo = GetInfo(dc, nextIndex_);
			dc.Flush();

			float lastAmount = Clamp((float)(nextT_ - t) * 1.0f / TRANSITION, 0.0f, 1.0f);
			DrawTex(dc, lastInfo, lastAmount * alpha * 0.2f);

			float nextAmount = lastAmount <= 0.0f ? 1.0f : 1.0f - lastAmount;
			DrawTex(dc, nextInfo, nextAmount * alpha * 0.2f);

			dc.RebindTexture();
		}
	}

private:
	void CheckNext(UIContext &dc, double t) {
		if (!g_recentFiles.HasAny()) {
			return;
		}

		std::vector<std::string> recents = g_recentFiles.GetRecentFiles();

		for (int index = lastIndex_ + 1; index != lastIndex_; ++index) {
			if (index < 0 || index >= (int)recents.size()) {
				if (lastIndex_ == -1)
					break;
				index = 0;
			}

			std::shared_ptr<GameInfo> ginfo = GetInfo(dc, index);
			if (ginfo && !ginfo->Ready(GameInfoFlags::PIC1)) {
				// Wait for it to load.  It might be the next one.
				break;
			}
			if (ginfo && ginfo->pic1.texture) {
				nextIndex_ = index;
				nextT_ = t + INTERVAL;
				break;
			}

			// Otherwise, keep going.  This skips games with no BG.
		}
	}

	static std::shared_ptr<GameInfo> GetInfo(UIContext &dc, int index) {
		if (index < 0) {
			return nullptr;
		}
		const auto recentIsos = g_recentFiles.GetRecentFiles();
		if (index >= (int)recentIsos.size())
			return std::shared_ptr<GameInfo>();
		return g_gameInfoCache->GetInfo(dc.GetDrawContext(), Path(recentIsos[index]), GameInfoFlags::PIC1);
	}

	static void DrawTex(UIContext &dc, std::shared_ptr<GameInfo> &ginfo, float amount) {
		if (!ginfo || amount <= 0.0f)
			return;
		GameInfoTex *pic = ginfo->GetPIC1();
		if (!pic)
			return;

		dc.GetDrawContext()->BindTexture(0, pic->texture);
		uint32_t color = whiteAlpha(amount) & 0xFFc0c0c0;
		dc.Draw()->DrawTexRect(dc.GetBounds(), 0, 0, 1, 1, color);
		dc.Flush();
	}

	static constexpr double INTERVAL = 8.0;
	static constexpr float TRANSITION = 3.0f;

	int lastIndex_ = -1;
	int nextIndex_ = -1;
	double nextT_ = -INTERVAL;
};

class BouncingIconAnimation : public Animation {
public:
	void Draw(UIContext &dc, double t, float alpha, float x, float y, float z) override {
		dc.Flush();
		dc.Begin();

		// Handle change in resolution.
		float xres = dc.GetBounds().w;
		float yres = dc.GetBounds().h;
		if (last_xres != xres || last_yres != yres) {
			Recalculate(xres, yres);
		}

		// Draw the image.
		float xpos = xbase + dc.GetBounds().x;
		float ypos = ybase + dc.GetBounds().y;
		ImageID icon = !color_ix && System_GetPropertyBool(SYSPROP_APP_GOLD) ? ImageID("I_ICON_GOLD") : ImageID("I_ICON");
		ui_draw2d.DrawImage(icon, xpos, ypos, scale, COLORS[color_ix], ALIGN_CENTER);
		dc.Flush();

		// Switch direction if within border.
		bool should_recolor = true;
		if (xbase > xres - border || xbase < border) {
			xspeed *= -1.0f;
			RandomizeColor();
			should_recolor = false;
		}

		if (ybase > yres - border || ybase < border) {
			yspeed *= -1.0f;

			if (should_recolor) {
				RandomizeColor();
			}
		}

		// Place to border if out of bounds.
		if (xbase > xres - border) xbase = xres - border;
		else if (xbase < border) xbase = border;
		if (ybase > yres - border) ybase = yres - border;
		else if (ybase < border) ybase = border;

		// Update location.
		xbase += xspeed;
		ybase += yspeed;
	}

private:
	static constexpr int COLOR_COUNT = 11;
	static constexpr Color COLORS[COLOR_COUNT] = {0xFFFFFFFF, 0xFFFFFF00, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF,
			0xFF00FFFF, 0xFFFF00FF, 0xFF4111D1, 0xFF3577F3, 0xFFAA77FF, 0xFF623B84};

	float xbase = 0.0f;
	float ybase = 0.0f;
	float last_xres = 0.0f;
	float last_yres = 0.0f;
	float xspeed = 1.0f;
	float yspeed = 1.0f;
	float scale = 1.0f;
	float border = 35.0f;
	int color_ix = 0;
	int last_color_ix = -1;
	GMRng rng;

	void Recalculate(int xres, int yres) {
		// First calculation.
		if (last_color_ix == -1) {
			xbase = xres / 2.0f;
			ybase = yres / 2.0f;
			last_color_ix = 0;

			// Determine initial direction.
			if ((int)(rng.F() * xres) % 2) xspeed *= -1.0f;
			if ((int)(rng.F() * yres) % 2) yspeed *= -1.0f;
		}

		// Scale certain attributes to resolution.
		scale = std::min(xres, yres) / 400.0f;
		float speed = scale < 2.5f ? scale * 0.58f : scale * 0.46f;
		xspeed = std::signbit(xspeed) ? speed * -1.0f : speed;
		yspeed = std::signbit(yspeed) ? speed * -1.0f : speed;
		border = 35.0f * scale;

		last_xres = xres;
		last_yres = yres;
	}

	void RandomizeColor() {
		do {
			color_ix = (int)(rng.F() * xbase) % COLOR_COUNT;
		} while (color_ix == last_color_ix);

		last_color_ix = color_ix;
	}
};

// TODO: Add more styles. Remember to add to the enum in ConfigValues.h and the selector in GameSettings too.

static BackgroundAnimation g_CurBackgroundAnimation = BackgroundAnimation::OFF;
static std::unique_ptr<Animation> g_Animation;
static bool bgTextureInited = false;  // Separate variable because init could fail.

void UIBackgroundInit(UIContext &dc) {
	const Path bgPng = GetSysDirectory(DIRECTORY_SYSTEM) / "background.png";
	const Path bgJpg = GetSysDirectory(DIRECTORY_SYSTEM) / "background.jpg";
	if (File::Exists(bgPng) || File::Exists(bgJpg)) {
		const Path &bgFile = File::Exists(bgPng) ? bgPng : bgJpg;
		bgTexture = CreateTextureFromFile(dc.GetDrawContext(), bgFile.c_str(), ImageFileType::DETECT, true);
	}
}

void UIBackgroundShutdown() {
	if (bgTexture) {
		bgTexture->Release();
		bgTexture = nullptr;
	}
	bgTextureInited = false;
	g_Animation.reset(nullptr);
	g_CurBackgroundAnimation = BackgroundAnimation::OFF;
}

void DrawBackground(UIContext &dc, float alpha, float x, float y, float z) {
	if (!bgTextureInited) {
		UIBackgroundInit(dc);
		bgTextureInited = true;
	}
	if (g_CurBackgroundAnimation != (BackgroundAnimation)g_Config.iBackgroundAnimation) {
		g_CurBackgroundAnimation = (BackgroundAnimation)g_Config.iBackgroundAnimation;

		switch (g_CurBackgroundAnimation) {
		case BackgroundAnimation::FLOATING_SYMBOLS:
			g_Animation.reset(new FloatingSymbolsAnimation(false));
			break;
		case BackgroundAnimation::RECENT_GAMES:
			g_Animation.reset(new RecentGamesAnimation());
			break;
		case BackgroundAnimation::WAVE:
			g_Animation.reset(new WaveAnimation());
			break;
		case BackgroundAnimation::MOVING_BACKGROUND:
			g_Animation.reset(new MovingBackground());
			break;
		case BackgroundAnimation::BOUNCING_ICON:
			g_Animation.reset(new BouncingIconAnimation());
			break;
		case BackgroundAnimation::FLOATING_SYMBOLS_COLORED:
			g_Animation.reset(new FloatingSymbolsAnimation(true));
			break;
		default:
			g_Animation.reset(nullptr);
		}
	}

	uint32_t bgColor = whiteAlpha(alpha);

	if (bgTexture != nullptr) {
		dc.Flush();
		dc.Begin();
		dc.GetDrawContext()->BindTexture(0, bgTexture);
		dc.Draw()->DrawTexRect(dc.GetBounds(), 0, 0, 1, 1, bgColor);

		dc.Flush();
		dc.RebindTexture();
	} else {
		// I_BG original color: 0xFF754D24
		ImageID img = ImageID("I_BG");
		dc.Begin();
		dc.Draw()->DrawImageStretch(img, dc.GetBounds(), bgColor & dc.GetTheme().backgroundColor);
		dc.Flush();
	}

#if PPSSPP_PLATFORM(IOS)
	// iOS uses an old screenshot when restoring the task, so to avoid an ugly
	// jitter we accumulate time instead.
	static int frameCount = 0.0;
	frameCount++;
	double t = (double)frameCount / System_GetPropertyFloat(SYSPROP_DISPLAY_REFRESH_RATE);
#else
	double t = time_now_d();
#endif

	if (g_Animation) {
		g_Animation->Draw(dc, t, alpha, x, y, z);
	}
}

uint32_t GetBackgroundColorWithAlpha(const UIContext &dc) {
	return colorAlpha(colorBlend(dc.GetTheme().backgroundColor, 0, 0.5f), 0.65f);  // 0.65 = 166 = A6
}

void DrawGameBackground(UIContext &dc, const Path &gamePath, float x, float y, float z) {
	using namespace Draw;
	using namespace UI;
	dc.Flush();

	std::shared_ptr<GameInfo> ginfo;
	if (!gamePath.empty()) {
		ginfo = g_gameInfoCache->GetInfo(dc.GetDrawContext(), gamePath, GameInfoFlags::PIC1);
	}

	GameInfoTex *pic = (ginfo && ginfo->Ready(GameInfoFlags::PIC1)) ? ginfo->GetPIC1() : nullptr;
	if (pic) {
		dc.GetDrawContext()->BindTexture(0, pic->texture);
		uint32_t color = whiteAlpha(ease((time_now_d() - pic->timeLoaded) * 3)) & 0xFFc0c0c0;
		dc.Draw()->DrawTexRect(dc.GetBounds(), 0, 0, 1, 1, color);
		dc.Flush();
		dc.RebindTexture();
	} else {
		::DrawBackground(dc, 1.0f, x, y, z);
		dc.RebindTexture();
		dc.Flush();
	}
}
