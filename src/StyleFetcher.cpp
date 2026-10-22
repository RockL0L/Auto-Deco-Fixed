#include "StyleFetcher.hpp"
#include "Learn.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <utility>

using namespace geode::prelude;

namespace deco {

namespace {

// downloadLevel() has gained an extra parameter in some GD versions. Pick whichever
// signature exists so the mod builds either way.
template <class M>
auto callDownload(M* m, int id, int) -> decltype(m->downloadLevel(id, false, 0), void()) {
	m->downloadLevel(id, false, 0);
}

template <class M>
auto callDownload(M* m, int id, long) -> decltype(m->downloadLevel(id, false), void()) {
	m->downloadLevel(id, false);
}

// Some GJGameLevel fields are plain ints, others are wrapped (SeedValueRSV).
template <class T>
int asInt(T v) {
	if constexpr (requires { v.value(); }) {
		return static_cast<int>(v.value());
	} else {
		return static_cast<int>(v);
	}
}

std::string trim(std::string s) {
	auto notSpace = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	return s;
}

bool parsePositiveInt(std::string const& s, int& out) {
	if (s.empty() || s.size() > 9) return false;
	for (char c : s) {
		if (c < '0' || c > '9') return false;
	}
	out = std::atoi(s.c_str());
	return out > 0;
}

} // namespace

StyleFetcher::~StyleFetcher() {
	detach();
}

void StyleFetcher::attach() {
	if (m_attached) return;
	auto mgr = GameLevelManager::get();
	m_prevDownload = mgr->m_levelDownloadDelegate;
	m_prevManager = mgr->m_levelManagerDelegate;
	mgr->m_levelDownloadDelegate = this;
	mgr->m_levelManagerDelegate = this;
	m_attached = true;
}

void StyleFetcher::detach() {
	if (!m_attached) return;
	auto mgr = GameLevelManager::get();
	if (mgr->m_levelDownloadDelegate == static_cast<LevelDownloadDelegate*>(this)) {
		mgr->m_levelDownloadDelegate = m_prevDownload;
	}
	if (mgr->m_levelManagerDelegate == static_cast<LevelManagerDelegate*>(this)) {
		mgr->m_levelManagerDelegate = m_prevManager;
	}
	m_attached = false;
	m_stage = Stage::Idle;
}

void StyleFetcher::finish(bool ok, Profile profile, std::string message) {
	detach();
	auto cb = std::move(m_callback);
	m_callback = nullptr;
	if (cb) cb(ok, std::move(profile), std::move(message));
}

void StyleFetcher::fail(std::string message) {
	finish(false, Profile{}, std::move(message));
}

void StyleFetcher::start(Source source, std::string text, LevelEditorLayer* layer, Callback cb) {
	detach();
	m_callback = std::move(cb);
	m_source = source;
	m_text = trim(std::move(text));
	m_collected = Profile{};
	m_queue.clear();
	m_firstId = 0;

	if (source == Source::ThisLevel) {
		auto raw = collectEditorObjects(layer, true);
		auto prof = learn(raw, "this level");
		if (!prof.usable()) {
			return fail("No decoration found around blocks in this level yet. Try another source.");
		}
		auto msg = fmt::format("Style ready: this level ({} decorations learned)", prof.sampleCount());
		return finish(true, std::move(prof), std::move(msg));
	}

	if (m_text.empty()) {
		return fail(source == Source::Player ? "Type a level ID made by that player." : "Type a level ID or name.");
	}

	attach();
	auto mgr = GameLevelManager::get();

	int id = 0;
	if (parsePositiveInt(m_text, id)) {
		m_stage = Stage::DownloadFirst;
		m_firstId = id;
		callDownload(mgr, id, 0);
		return;
	}

	if (source == Source::OnlineLevel) {
		m_stage = Stage::SearchByName;
		mgr->getOnlineLevels(GJSearchObject::create(SearchType::Search, m_text));
		return;
	}

	fail("Player mode needs the ID of a level made by that player (usernames can't be looked up yet).");
}

void StyleFetcher::levelDownloadFinished(GJGameLevel* level) {
	if (m_stage != Stage::DownloadFirst && m_stage != Stage::DownloadMore) return;
	if (!level) return;

	std::string name = level->m_levelName.c_str();
	auto raw = parseLevelString(std::string(level->m_levelString.c_str()));
	auto prof = learn(raw, name);

	if (m_stage == Stage::DownloadFirst) {
		if (m_source == Source::OnlineLevel) {
			if (!prof.usable()) {
				return fail("That level has no decoration around its blocks to learn from.");
			}
			auto msg = fmt::format("Style ready: {} ({} decorations learned)", name, prof.sampleCount());
			return finish(true, std::move(prof), std::move(msg));
		}

		// Player mode: keep this level, then look up the rest of that player's levels.
		m_collected.merge(prof);
		int userId = asInt(level->m_userID);
		if (userId <= 0) {
			if (!m_collected.usable()) return fail("Couldn't read the creator of that level.");
			auto msg = fmt::format("Style ready: {} ({} decorations learned)", m_collected.name, m_collected.sampleCount());
			return finish(true, m_collected, std::move(msg));
		}
		m_stage = Stage::SearchUserLevels;
		GameLevelManager::get()->getOnlineLevels(
			GJSearchObject::create(SearchType::UsersLevels, std::to_string(userId))
		);
		return;
	}

	// DownloadMore
	m_collected.merge(prof);
	downloadNext();
}

void StyleFetcher::levelDownloadFailed(int) {
	if (m_stage == Stage::DownloadMore) {
		return downloadNext(); // skip a level that won't download
	}
	if (m_stage == Stage::DownloadFirst) {
		return fail("Couldn't download that level. Check the ID and your connection.");
	}
}

void StyleFetcher::loadLevelsFinished(CCArray* levels, char const*) {
	onLevelsLoaded(levels);
}

void StyleFetcher::loadLevelsFinished(CCArray* levels, char const*, int) {
	onLevelsLoaded(levels);
}

void StyleFetcher::loadLevelsFailed(char const*) {
	if (m_stage == Stage::SearchByName) {
		return fail("The level search failed. Check your connection.");
	}
	if (m_stage == Stage::SearchUserLevels) {
		// We still have the first level - use it on its own.
		if (!m_collected.usable()) return fail("Couldn't load that player's levels.");
		auto msg = fmt::format("Style ready: {} ({} decorations learned)", m_collected.name, m_collected.sampleCount());
		return finish(true, m_collected, std::move(msg));
	}
}

void StyleFetcher::loadLevelsFailed(char const* key, int) {
	loadLevelsFailed(key);
}

void StyleFetcher::onLevelsLoaded(CCArray* levels) {
	if (m_stage == Stage::SearchByName) {
		if (!levels || levels->count() == 0) {
			return fail("No level found with that name.");
		}
		auto first = static_cast<GJGameLevel*>(levels->objectAtIndex(0));
		int id = first ? asInt(first->m_levelID) : 0;
		if (id <= 0) return fail("No level found with that name.");
		m_stage = Stage::DownloadFirst;
		m_firstId = id;
		callDownload(GameLevelManager::get(), id, 0);
		return;
	}

	if (m_stage == Stage::SearchUserLevels) {
		m_queue.clear();
		if (levels) {
			for (auto level : CCArrayExt<GJGameLevel*>(levels)) {
				if (!level) continue;
				int id = asInt(level->m_levelID);
				if (id <= 0 || id == m_firstId) continue;
				m_queue.push_back(id);
				if (m_queue.size() >= 3) break;
			}
		}
		m_stage = Stage::DownloadMore;
		downloadNext();
		return;
	}
}

void StyleFetcher::downloadNext() {
	if (m_queue.empty()) {
		if (!m_collected.usable()) {
			return fail("Couldn't find enough decoration in that player's levels.");
		}
		auto msg = fmt::format("Style ready: {} ({} decorations learned)", m_collected.name, m_collected.sampleCount());
		return finish(true, m_collected, std::move(msg));
	}
	int id = m_queue.front();
	m_queue.erase(m_queue.begin());
	callDownload(GameLevelManager::get(), id, 0);
}

} // namespace deco
