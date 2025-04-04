#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <string_view>
#include <mutex>

class VFSInterface;

// Serial/id doesn't need including here since we look up by it.
struct GameDBInfo {
	std::string title;
	std::string foreignTitle;
	uint32_t crc = 0;
	uint64_t size = 0;
};

class GameDB {
public:
	// Warning: Linear search. Don't call it every frame if possible.
	// If this returns true, there is at least 1 entry in infos, so it's safe to go
	// look up element 0 for a quick guess.
	bool GetGameInfos(std::string_view id, std::vector<GameDBInfo> *infos);

private:
	// Call under lock.
	void LoadIfNeeded();
	size_t GetColumnIndex(std::string_view name) const;

	struct Line {
		// The exact same ISO can have multiple serials.
		std::vector<std::string_view> serials;
		// The below fields should match GameDBInfo.
		std::string_view title;
		std::string_view foreignTitle;
		std::string_view size;
		std::string_view crc;
	};

	std::string contents_;
	std::vector<Line> lines_;
	std::vector<std::string_view> columns_;
	bool loaded_ = false;

	std::mutex loadMutex_;
};

extern GameDB g_gameDB;
