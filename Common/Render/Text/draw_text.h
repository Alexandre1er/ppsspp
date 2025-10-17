// draw_text

// Uses system fonts to draw text. 
// Platform support will be added over time, initially just Win32.

// Caches strings in individual textures. Might later combine them into a big one
// with dynamically allocated space but too much trouble for now.

#pragma once

#include "ppsspp_config.h"

#include <memory>
#include <cstdint>
#include <map>

#include "Common/Data/Text/WrapText.h"
#include "Common/Render/DrawBuffer.h"

namespace Draw {
	class DrawContext;
	class Texture;
}

struct TextStringEntry {
	TextStringEntry(int frameCount) : lastUsedFrame(frameCount) {}

	Draw::Texture *texture = nullptr;
	int width = 0;
	int height = 0;
	int bmWidth = 0;
	int bmHeight = 0;
	int lastUsedFrame;
};

struct TextMeasureEntry {
	int width;
	int height;
	int leading;  // only used with Cocoa
	int lastUsedFrame;
};

class TextDrawer {
public:
	virtual ~TextDrawer() = default;

	virtual bool IsReady() const { return true; }
	virtual uint32_t SetFont(const char *fontName, int size, int flags) = 0;
	virtual void SetFont(uint32_t fontHandle) = 0;  // Shortcut once you've set the font once.
	void SetFontScale(float xscale, float yscale);
	void MeasureString(std::string_view str, float *w, float *h);
	void MeasureStringRect(std::string_view str, const Bounds &bounds, float *w, float *h, int align = ALIGN_TOPLEFT);

	void DrawString(DrawBuffer &target, std::string_view str, float x, float y, uint32_t color, int align = ALIGN_TOPLEFT);
	void DrawStringRect(DrawBuffer &target, std::string_view str, const Bounds &bounds, uint32_t color, int align);
	virtual bool DrawStringBitmap(std::vector<uint8_t> &bitmapData, TextStringEntry &entry, Draw::DataFormat texFormat, std::string_view str, int align, bool fullColor) = 0;
	bool DrawStringBitmapRect(std::vector<uint8_t> &bitmapData, TextStringEntry &entry, Draw::DataFormat texFormat, std::string_view str, const Bounds &bounds, int align, bool fullColor);
	// Use for housekeeping like throwing out old strings.
	void OncePerFrame();

	float CalculateDPIScale() const;
	// This is used by PPGe, since we run at the PSP's DPI inside of there.
	void SetForcedDPIScale(float dpi) {
		dpiScale_ = dpi;
		ignoreGlobalDpi_ = true;
	}

	// Factory function that selects implementation.
	static TextDrawer *Create(Draw::DrawContext *draw);

	size_t GetStringCacheSize() const {
		return cache_.size();
	}
	size_t GetCacheDataSize() const;

protected:
	TextDrawer(Draw::DrawContext *draw);

	virtual void MeasureStringInternal(std::string_view str, float *w, float *h) = 0;

	void ClearCache();

	virtual bool SupportsColorEmoji() const = 0;
	virtual void ClearFonts() = 0;

	void WrapString(std::string &out, std::string_view str, float maxWidth, int flags);

	// v has the 8-bit alpha.
	static u16 AlphaToPremul4444(u32 v) {
		v = (v >> 4) & 0x0F;
		v |= v << 4;
		v |= v << 8;
		return v;
	}
	static u32 AlphaToPremul8888(u32 v) {
		v |= v << 8;
		v |= v << 16;
		return v;
	}
	static u32 RGBAToPremul8888(u32 v) {
		u32 a = (v >> 24) & 0xFF;
		if (a == 0xFF)
			return v;
		if (a == 0)
			return 0;
		u32 r = (v >> 16) & 0xFF;
		u32 g = (v >> 8) & 0xFF;
		u32 b = v & 0xFF;
		r = (r * a + 127) / 255;
		g = (g * a + 127) / 255;
		b = (b * a + 127) / 255;
		return (a << 24) | (r << 16) | (g << 8) | b;
	}

	struct CacheKey {
		bool operator < (const CacheKey &other) const {
			if (fontHash < other.fontHash)
				return true;
			if (fontHash > other.fontHash)
				return false;
			return text < other.text;
		}
		std::string text;
		uint32_t fontHash;
	};

	Draw::DrawContext *draw_;

	int frameCount_ = 0;
	float fontScaleX_ = 1.0f;
	float fontScaleY_ = 1.0f;
	float dpiScale_ = 1.0f;
	bool ignoreGlobalDpi_ = false;

	uint32_t fontHash_ = 0;

	std::map<CacheKey, std::unique_ptr<TextStringEntry>> cache_;
	std::map<CacheKey, std::unique_ptr<TextMeasureEntry>> sizeCache_;
};

class TextDrawerWordWrapper : public WordWrapper {
public:
	TextDrawerWordWrapper(TextDrawer *drawer, std::string_view str, float maxW, int flags)
		: WordWrapper(str, maxW, flags), drawer_(drawer) {}

protected:
	float MeasureWidth(std::string_view str) override;

	TextDrawer *drawer_;
};
