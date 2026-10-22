#include "Decorator.hpp"
#include "Learn.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <unordered_set>

using namespace geode::prelude;

namespace deco {

namespace {

std::mt19937& rng() {
	static std::mt19937 gen{ std::random_device{}() };
	return gen;
}

float rand01() {
	return std::uniform_real_distribution<float>(0.f, 1.f)(rng());
}

size_t randIndex(size_t n) {
	return std::uniform_int_distribution<size_t>(0, n - 1)(rng());
}

// Rotate a sample counter-clockwise by k * 90 degrees around the face center.
Sample rotateSample(Sample s, int k) {
	k = ((k % 4) + 4) % 4;
	for (int i = 0; i < k; i++) {
		float x = s.dx;
		float y = s.dy;
		s.dx = -y;
		s.dy = x;
		s.rotation -= 90.f; // GD rotation is clockwise-positive
	}
	if (k != 0) {
		s.flipX = false;
		s.flipY = false;
		s.rotated = true;
	}
	return s;
}

struct Pool {
	std::vector<Sample> samples;
	std::unordered_map<int, std::vector<size_t>> byId;
};

// Samples for one direction. If the style has none for it (e.g. the source level only
// decorated tops), borrow the other directions' samples rotated into place.
Pool buildPool(Profile const& profile, Dir d) {
	Pool pool;
	int di = static_cast<int>(d);
	if (!profile.pools[di].empty()) {
		pool.samples = profile.pools[di];
	} else {
		for (int from = 0; from < 4; from++) {
			if (from == di || profile.pools[from].empty()) continue;
			int k = ((di - from) % 4 + 4) % 4;
			for (auto const& s : profile.pools[from]) {
				pool.samples.push_back(rotateSample(s, k));
			}
		}
	}
	for (size_t i = 0; i < pool.samples.size(); i++) {
		pool.byId[pool.samples[i].id].push_back(i);
	}
	return pool;
}

struct Face {
	int cx;
	int cy;
	Dir dir;
};

bool horizontal(Dir d) {
	return d == Dir::Top || d == Dir::Bottom;
}

// Faces on the same row (horizontal faces) or column (vertical faces) share a "line".
int lineOf(Face const& f) {
	return horizontal(f.dir) ? f.cy : f.cx;
}

// Position along that line.
int tangentOf(Face const& f) {
	return horizontal(f.dir) ? f.cx : f.cy;
}

float normalizeAngle(float a) {
	a = std::fmod(a, 360.f);
	if (a > 180.f) a -= 360.f;
	if (a <= -180.f) a += 360.f;
	return a;
}

uint64_t dedupeKey(int id, float x, float y) {
	int64_t qx = static_cast<int64_t>(std::lround(x / 6.f));
	int64_t qy = static_cast<int64_t>(std::lround(y / 6.f));
	uint64_t h = static_cast<uint64_t>(id) * 0x9E3779B97F4A7C15ull;
	h ^= static_cast<uint64_t>(qx) + 0x7F4A7C15ull + (h << 6) + (h >> 2);
	h ^= static_cast<uint64_t>(qy) + 0x9E3779B9ull + (h << 6) + (h >> 2);
	return h;
}

} // namespace

Result decorateSelection(EditorUI* ui, Profile const& profile, Options const& opt) {
	Result res;

	auto layer = ui ? ui->m_editorLayer : nullptr;
	if (!layer) {
		res.error = "The editor isn't ready yet.";
		return res;
	}
	if (!profile.usable()) {
		res.error = "That style has no usable decoration around blocks. Try a different source.";
		return res;
	}

	auto selected = ui->getSelectedObjects();
	if (!selected || selected->count() == 0) {
		res.error = "Select some blocks in the editor first!";
		return res;
	}

	// Every solid cell in the level: an edge touching another block is not "exposed".
	std::unordered_set<int64_t> allSolid;
	for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
		if (!obj) continue;
		Kind k = classifyObject(obj);
		if (k == Kind::Solid || k == Kind::Slope) {
			allSolid.insert(cellKey(cellOf(obj->getPositionX()), cellOf(obj->getPositionY())));
		}
	}

	auto wanted = [&](Dir d) {
		switch (d) {
			case Dir::Top: return opt.top;
			case Dir::Bottom: return opt.bottom;
			default: return opt.sides;
		}
	};

	// Detect what the selection contains and gather exposed block faces.
	std::vector<Face> faces;
	std::unordered_set<int64_t> seen;
	for (auto obj : CCArrayExt<GameObject*>(selected)) {
		if (!obj) continue;
		switch (classifyObject(obj)) {
			case Kind::Hazard: res.hazards++; continue;
			case Kind::Deco: res.decos++; continue;
			case Kind::Slope: res.slopes++; continue;
			case Kind::Other: res.others++; continue;
			case Kind::Solid: break;
		}
		if (!isBlockLike(obj)) {
			res.others++;
			continue;
		}
		res.solids++;

		int cx = cellOf(obj->getPositionX());
		int cy = cellOf(obj->getPositionY());
		if (!seen.insert(cellKey(cx, cy)).second) continue;

		for (int di = 0; di < 4; di++) {
			Dir d = static_cast<Dir>(di);
			if (!wanted(d)) continue;
			if (allSolid.count(cellKey(cx + stepX(d), cy + stepY(d)))) continue;
			faces.push_back({ cx, cy, d });
		}
	}

	if (res.solids == 0) {
		res.error = "No solid blocks in your selection. Auto Decorate only decorates solid blocks.";
		return res;
	}
	if (faces.empty()) {
		res.error = "None of the selected blocks have an open edge to decorate (or those edges are turned off).";
		return res;
	}
	res.faces = static_cast<int>(faces.size());

	// Group faces into continuous runs so the decoration along an edge stays consistent.
	std::sort(faces.begin(), faces.end(), [](Face const& a, Face const& b) {
		if (a.dir != b.dir) return static_cast<int>(a.dir) < static_cast<int>(b.dir);
		int la = lineOf(a);
		int lb = lineOf(b);
		if (la != lb) return la < lb;
		return tangentOf(a) < tangentOf(b);
	});

	std::array<Pool, 4> pools;
	std::array<bool, 4> built{ false, false, false, false };
	std::unordered_set<uint64_t> placedKeys;

	float density = std::clamp(opt.density, 0.05f, 1.f);
	float flow = std::clamp(opt.flow, 0.f, 1.f);
	int maxObjects = std::max(1, opt.maxObjects);

	size_t i = 0;
	while (i < faces.size() && !res.capped) {
		size_t j = i + 1;
		while (j < faces.size() &&
			faces[j].dir == faces[i].dir &&
			lineOf(faces[j]) == lineOf(faces[i]) &&
			tangentOf(faces[j]) - tangentOf(faces[j - 1]) == 1) {
			j++;
		}

		int di = static_cast<int>(faces[i].dir);
		if (!built[di]) {
			pools[di] = buildPool(profile, faces[i].dir);
			built[di] = true;
		}
		Pool const& pool = pools[di];
		if (pool.samples.empty()) {
			i = j;
			continue;
		}

		// Each run gets a "theme" object that most placements favour -> a coherent edge.
		int primaryId = pool.samples[randIndex(pool.samples.size())].id;

		for (size_t f = i; f < j && !res.capped; f++) {
			Face const& face = faces[f];

			int count = 1;
			if (density > 0.6f && rand01() < (density - 0.6f) * 1.5f) count = 2;

			for (int c = 0; c < count; c++) {
				if (rand01() > density) continue;

				Sample const* s;
				auto themed = pool.byId.find(primaryId);
				if (themed != pool.byId.end() && rand01() < 0.65f) {
					s = &pool.samples[themed->second[randIndex(themed->second.size())]];
				} else {
					s = &pool.samples[randIndex(pool.samples.size())];
				}

				auto fc = faceCenter(face.cx, face.cy, face.dir);
				float x = fc.x + s->dx;
				float y = fc.y + s->dy;

				if (!opt.clean) {
					float j1 = (rand01() - 0.5f) * 10.f;
					if (horizontal(face.dir)) x += j1;
					else y += j1;
				}

				// Flow: a soft wave along the edge so neighbours bend together.
				float along = horizontal(face.dir) ? x : y;
				float wave = flow * 9.f * std::sin(along * 0.045f);
				float rot = s->rotation + wave;
				rot = opt.clean ? std::round(rot / 5.f) * 5.f : rot;
				rot = normalizeAngle(rot);

				float scaleBoost = 1.f + (rand01() - 0.5f) * (opt.clean ? 0.08f : 0.24f);
				float sx = s->scaleX * scaleBoost;
				float sy = s->scaleY * scaleBoost;

				if (!placedKeys.insert(dedupeKey(s->id, x, y)).second) continue;

				auto obj = layer->createObject(s->id, ccp(x, y), true);
				if (!obj) continue;

				obj->setRotation(rot);
				obj->updateCustomScaleX(sx);
				obj->updateCustomScaleY(sy);
				if (!s->rotated) {
					if (s->flipX) obj->setFlipX(true);
					if (s->flipY) obj->setFlipY(true);
				}
				if (s->zLayer != 0) obj->m_zLayer = static_cast<ZLayer>(s->zLayer);
				if (s->zOrder != 0) obj->m_zOrder = s->zOrder;
				if (s->mainColor > 0 && obj->m_baseColor) obj->m_baseColor->m_colorID = s->mainColor;
				if (s->detailColor > 0 && obj->m_detailColor) obj->m_detailColor->m_colorID = s->detailColor;

				layer->addToUndoList(UndoObject::create(obj, UndoCommand::New), false);

				res.placed++;
				if (res.placed >= maxObjects) {
					res.capped = true;
					break;
				}
			}
		}

		i = j;
	}

	return res;
}

} // namespace deco
