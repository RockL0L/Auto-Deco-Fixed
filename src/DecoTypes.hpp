#pragma once

#include <Geode/Geode.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace deco {

constexpr float CELL = 30.f;

// Counter-clockwise order, so rotating a direction by +90 degrees CCW is (d + 1) % 4.
enum class Dir : int { Top = 0, Left = 1, Bottom = 2, Right = 3 };

enum class Kind { Solid, Slope, Hazard, Deco, Other };

inline int64_t cellKey(int cx, int cy) {
	return (static_cast<int64_t>(cx) << 32) | static_cast<uint32_t>(cy);
}

inline int cellOf(float v) {
	return static_cast<int>(std::floor(v / CELL));
}

inline int stepX(Dir d) {
	return d == Dir::Left ? -1 : (d == Dir::Right ? 1 : 0);
}

inline int stepY(Dir d) {
	return d == Dir::Top ? 1 : (d == Dir::Bottom ? -1 : 0);
}

// Center of the outer face of grid cell (cx, cy) on side `d`, in editor units.
inline cocos2d::CCPoint faceCenter(int cx, int cy, Dir d) {
	float x = cx * CELL;
	float y = cy * CELL;
	switch (d) {
		case Dir::Top:    return { x + CELL / 2, y + CELL };
		case Dir::Bottom: return { x + CELL / 2, y };
		case Dir::Left:   return { x, y + CELL / 2 };
		case Dir::Right:  return { x + CELL, y + CELL / 2 };
	}
	return { x, y };
}

// One learned decoration object, stored relative to the block face it was sitting on.
struct Sample {
	int id = 0;
	float dx = 0.f;
	float dy = 0.f;
	float rotation = 0.f; // GD rotation (clockwise positive)
	float scaleX = 1.f;
	float scaleY = 1.f;
	bool flipX = false;
	bool flipY = false;
	int zLayer = 0;
	int zOrder = 0;
	int mainColor = 0;
	int detailColor = 0;
	bool rotated = false; // true when derived from another direction
};

// A "style": decoration samples grouped by which side of a block they decorate.
struct Profile {
	std::array<std::vector<Sample>, 4> pools;
	std::string name;
	size_t decoCount = 0;
	size_t solidCount = 0;

	size_t sampleCount() const {
		size_t n = 0;
		for (auto const& p : pools) n += p.size();
		return n;
	}

	bool usable() const { return sampleCount() >= 3; }

	void merge(Profile const& other) {
		for (size_t i = 0; i < pools.size(); i++) {
			pools[i].insert(pools[i].end(), other.pools[i].begin(), other.pools[i].end());
		}
		decoCount += other.decoCount;
		solidCount += other.solidCount;
		if (name.empty()) name = other.name;
		else if (!other.name.empty()) name += " + " + other.name;
	}
};

// A level object in neutral form (from a level string or from the editor).
struct RawObject {
	int id = 0;
	float x = 0.f;
	float y = 0.f;
	float rotation = 0.f;
	float scaleX = 1.f;
	float scaleY = 1.f;
	bool flipX = false;
	bool flipY = false;
	int zLayer = 0;
	int zOrder = 0;
	int mainColor = 0;
	int detailColor = 0;
};

struct Options {
	float density = 0.6f; // 0..1
	float flow = 0.5f;    // 0..1
	bool top = true;
	bool bottom = true;
	bool sides = true;
	bool clean = true;
	int maxObjects = 4000;
};

struct Result {
	int placed = 0;
	int faces = 0;
	int solids = 0;
	int slopes = 0;
	int hazards = 0;
	int decos = 0;
	int others = 0;
	bool capped = false;
	std::string error;
};

} // namespace deco
