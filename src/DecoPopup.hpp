#pragma once

#include "DecoTypes.hpp"
#include "StyleFetcher.hpp"

#include <memory>

// The Auto Decorate menu: pick a style source, press Confirm, then Decorate.
class DecoPopup : public geode::Popup {
protected:
	EditorUI* m_ui = nullptr;

	std::unique_ptr<deco::StyleFetcher> m_fetcher;
	std::shared_ptr<deco::Profile> m_profile;
	deco::Options m_opt;
	int m_source = 0;
	bool m_ready = false;

	geode::TextInput* m_input = nullptr;
	cocos2d::CCLabelBMFont* m_status = nullptr;
	cocos2d::CCLabelBMFont* m_densityLabel = nullptr;
	cocos2d::CCLabelBMFont* m_flowLabel = nullptr;
	ButtonSprite* m_sourceSprites[3] = { nullptr, nullptr, nullptr };
	ButtonSprite* m_decorateSprite = nullptr;
	CCMenuItemSpriteExtra* m_decorateBtn = nullptr;

	bool init(EditorUI* ui);

	void setStatus(std::string const& text, cocos2d::ccColor3B color);
	void setReady(bool ready);
	void refreshSource();
	void saveOptions();

	void onSource(cocos2d::CCObject* sender);
	void onConfirm(cocos2d::CCObject* sender);
	void onDecorate(cocos2d::CCObject* sender);
	void onInfo(cocos2d::CCObject* sender);
	void onToggle(cocos2d::CCObject* sender);
	void onSlider(cocos2d::CCObject* sender);

public:
	static DecoPopup* create(EditorUI* ui);
};
