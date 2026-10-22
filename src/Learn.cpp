#include "Learn.hpp"

#include <algorithm>
#include <cstdlib>
#include <random>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

using namespace geode::prelude;

namespace deco {

namespace {

constexpr int kSearchRadius = 3;       // cells around a deco object to look for a block
constexpr float kMaxOffset = 150.f;    // ignore deco farther than this from its block face
constexpr size_t kMaxPerPool = 4000;   // cap on samples kept per direction

std::mt19937& rng() {
	static std::mt19937 gen{ std::random_device{}() };
	return gen;
}

Kind classifyType(GameObjectType t) {
	switch (t) {
		case GameObjectType::Solid: return Kind::Solid;
		case GameObjectType::Slope: return Kind::Slope;
		case GameObjectType::Hazard: return Kind::Hazard;
		case GameObjectType::Decoration: return Kind::Deco;
		default: return Kind::Other;
	}
}

int toInt(std::string_view v) {
	std::string t(v);
	return std::atoi(t.c_str());
}

float toFloat(std::string_view v) {
	std::string t(v);
	return std::strtof(t.c_str(), nullptr);
}

bool blockScale(float sx, float sy) {
	return std::abs(sx - 1.f) < 0.1f && std::abs(sy - 1.f) < 0.1f;
}

void parseObject(std::string_view seg, std::vector<RawObject>& out) {
	RawObject o;
	bool hasId = false;
	bool hasScaleXY = false;
	float uniformScale = 1.f;

	size_t pos = 0;
	while (pos < seg.size()) {
		size_t kEnd = seg.find(',', pos);
		if (kEnd == std::string_view::npos) break;
		size_t vEnd = seg.find(',', kEnd + 1);
		if (vEnd == std::string_view::npos) vEnd = seg.size();

		int key = toInt(seg.substr(pos, kEnd - pos));
		std::string_view val = seg.substr(kEnd + 1, vEnd - kEnd - 1);
		pos = vEnd + 1;

		switch (key) {
			case 1: o.id = toInt(val); hasId = o.id > 0; break;
			case 2: o.x = toFloat(val); break;
			case 3: o.y = toFloat(val); break;
			case 4: o.flipX = toInt(val) != 0; break;
			case 5: o.flipY = toInt(val) != 0; break;
			case 6: o.rotation = toFloat(val); break;
			case 21: o.mainColor = toInt(val); break;
			case 22: o.detailColor = toInt(val); break;
			case 24: o.zLayer = toInt(val); break;
			case 25: o.zOrder = toInt(val); break;
			case 32: uniformScale = toFloat(val); break;
			case 128: o.scaleX = toFloat(val); hasScaleXY = true; break;
			case 129: o.scaleY = toFloat(val); hasScaleXY = true; break;
			default: break;
		}
	}

	if (!hasId) return;
	if (!hasScaleXY) {
		o.scaleX = uniformScale;
		o.scaleY = uniformScale;
	}
	if (!(o.scaleX > 0.01f)) o.scaleX = 1.f;
	if (!(o.scaleY > 0.01f)) o.scaleY = 1.f;
	out.push_back(o);
}

} // namespace

Kind classifyId(int id) {
	static std::unordered_map<int, Kind> cache;
	auto it = cache.find(id);
	if (it != cache.end()) return it->second;

	Kind k = Kind::Other;
	if (id > 0 && id < 6000) {
		if (auto obj = GameObject::createWithKey(id)) {
			k = classifyType(obj->m_objectType);
		}
	}
	cache[id] = k;
	return k;
}

Kind classifyObject(GameObject* obj) {
	return obj ? classifyType(obj->m_objectType) : Kind::Other;
}

bool isBlockLike(GameObject* obj) {
	if (!obj) return false;
	float sx = obj->m_scaleX > 0.01f ? obj->m_scaleX : 1.f;
	float sy = obj->m_scaleY > 0.01f ? obj->m_scaleY : 1.f;
	return blockScale(sx, sy);
}

std::vector<RawObject> parseLevelString(std::string data) {
	std::vector<RawObject> out;
	if (data.empty()) return out;

	// Uncompressed level strings contain ';' separators; compressed ones do not.
	if (data.find(';') == std::string::npos) {
		std::string unzipped = ZipUtils::decompressString(data, false, 11);
		data = std::move(unzipped);
	}
	if (data.empty()) return out;

	std::string_view sv(data);
	size_t start = 0;
	bool first = true;
	while (start < sv.size()) {
		size_t end = sv.find(';', start);
		if (end == std::string_view::npos) end = sv.size();
		if (first) {
			first = false; // header segment (level settings)
		} else if (end > start) {
			parseObject(sv.substr(start, end - start), out);
		}
		start = end + 1;
	}
	return out;
}

std::vector<RawObject> collectEditorObjects(LevelEditorLayer* layer, bool skipSelected) {
	std::vector<RawObject> out;
	if (!layer || !layer->m_objects) return out;

	for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
		if (!obj) continue;
		if (skipSelected && obj->m_isSelected) continue;

		RawObject r;
		r.id = obj->m_objectID;
		r.x = obj->getPositionX();
		r.y = obj->getPositionY();
		r.rotation = obj->getRotation();
		r.scaleX = obj->m_scaleX > 0.01f ? obj->m_scaleX : 1.f;
		r.scaleY = obj->m_scaleY > 0.01f ? obj->m_scaleY : 1.f;
		r.flipX = obj->isFlipX();
		r.flipY = obj->isFlipY();
		r.zLayer = static_cast<int>(obj->m_zLayer);
		r.zOrder = obj->m_zOrder;
		r.mainColor = obj->m_baseColor ? obj->m_baseColor->m_colorID : 0;
		r.detailColor = obj->m_detailColor ? obj->m_detailColor->m_colorID : 0;
		out.push_back(r);
	}
	return out;
}

Profile learn(std::vector<RawObject> const& objects, std::string name) {
	Profile prof;
	prof.name = std::move(name);

	// 1) Find every block-like cell.
	std::unordered_set<int64_t> solids;
	for (auto const& o : objects) {
		Kind k = classifyId(o.id);
		if ((k == Kind::Solid || k == Kind::Slope) && blockScale(o.scaleX, o.scaleY)) {
			solids.insert(cellKey(cellOf(o.x), cellOf(o.y)));
		}
	}
	prof.solidCount = solids.size();
	if (solids.empty()) return prof;

	// 2) For each decoration object, find the closest block and which side it sits on.
	std::uniform_real_distribution<float> unit(0.f, 1.f);
	for (auto const& o : objects) {
		if (classifyId(o.id) != Kind::Deco) continue;
		prof.decoCount++;

		int cx = cellOf(o.x);
		int cy = cellOf(o.y);

		int bestDist = 1 << 30;
		int bx = 0, by = 0;
		for (int dy = -kSearchRadius; dy <= kSearchRadius; dy++) {
			for (int dx = -kSearchRadius; dx <= kSearchRadius; dx++) {
				if (!solids.count(cellKey(cx + dx, cy + dy))) continue;
				int d = dx * dx + dy * dy;
				if (d < bestDist) {
					bestDist = d;
					bx = cx + dx;
					by = cy + dy;
				}
			}
		}
		if (bestDist == (1 << 30)) continue; // floating decoration, not tied to a block

		float vx = o.x - (bx * CELL + CELL / 2);
		float vy = o.y - (by * CELL + CELL / 2);
		Dir dir;
		if (std::abs(vy) >= std::abs(vx)) dir = vy >= 0 ? Dir::Top : Dir::Bottom;
		else dir = vx >= 0 ? Dir::Right : Dir::Left;

		auto fc = faceCenter(bx, by, dir);
		Sample s;
		s.id = o.id;
		s.dx = o.x - fc.x;
		s.dy = o.y - fc.y;
		if (std::sqrt(s.dx * s.dx + s.dy * s.dy) > kMaxOffset) continue;

		s.rotation = o.rotation;
		s.scaleX = o.scaleX;
		s.scaleY = o.scaleY;
		s.flipX = o.flipX;
		s.flipY = o.flipY;
		s.zLayer = o.zLayer;
		s.zOrder = o.zOrder;
		s.mainColor = o.mainColor;
		s.detailColor = o.detailColor;

		auto& pool = prof.pools[static_cast<int>(dir)];
		if (pool.size() < kMaxPerPool) {
			pool.push_back(s);
		} else {
			// reservoir-style replacement so huge levels are still sampled evenly
			size_t idx = std::uniform_int_distribution<size_t>(0, pool.size() - 1)(rng());
			if (unit(rng()) < 0.25f) pool[idx] = s;
		}
	}

	return prof;
}

} // namespace deco
