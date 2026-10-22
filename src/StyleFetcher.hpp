#pragma once

#include "DecoTypes.hpp"

#include <functional>
#include <string>
#include <vector>

namespace deco {

enum class Source : int { ThisLevel = 0, OnlineLevel = 1, Player = 2 };

// Builds a style Profile from a source. ThisLevel is instant; the online sources talk to
// the GD servers through GameLevelManager and answer through the callback.
class StyleFetcher : public LevelDownloadDelegate, public LevelManagerDelegate {
public:
	using Callback = std::function<void(bool ok, Profile profile, std::string message)>;

	StyleFetcher() = default;
	~StyleFetcher();

	void start(Source source, std::string text, LevelEditorLayer* layer, Callback cb);

	// LevelDownloadDelegate
	void levelDownloadFinished(GJGameLevel* level) override;
	void levelDownloadFailed(int code) override;

	// LevelManagerDelegate
	void loadLevelsFinished(cocos2d::CCArray* levels, char const* key) override;
	void loadLevelsFinished(cocos2d::CCArray* levels, char const* key, int type) override;
	void loadLevelsFailed(char const* key) override;
	void loadLevelsFailed(char const* key, int type) override;

private:
	enum class Stage { Idle, SearchByName, DownloadFirst, SearchUserLevels, DownloadMore };

	void attach();
	void detach();
	void finish(bool ok, Profile profile, std::string message);
	void fail(std::string message);
	void downloadNext();
	void onLevelsLoaded(cocos2d::CCArray* levels);

	Stage m_stage = Stage::Idle;
	Source m_source = Source::ThisLevel;
	std::string m_text;
	Callback m_callback;
	Profile m_collected;
	std::vector<int> m_queue;
	int m_firstId = 0;
	bool m_attached = false;
	LevelDownloadDelegate* m_prevDownload = nullptr;
	LevelManagerDelegate* m_prevManager = nullptr;
};

} // namespace deco
