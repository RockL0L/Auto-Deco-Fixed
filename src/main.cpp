#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/EditorUI.hpp>

#include "DecoPopup.hpp"

using namespace geode::prelude;

class $modify(AutoDecoEditorUI, EditorUI) {
	bool init(LevelEditorLayer* editorLayer) {
		if (!EditorUI::init(editorLayer)) return false;

		// On-screen button (the hotkey from the mod settings works too).
		auto spr = ButtonSprite::create("Deco", 44, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.55f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(AutoDecoEditorUI::onOpenDecoMenu));
		btn->setID("auto-deco-button"_spr);

		auto menu = CCMenu::create();
		menu->setID("auto-deco-menu"_spr);
		auto win = CCDirector::get()->getWinSize();
		menu->setPosition({ win.width - 36.f, win.height * 0.62f });
		menu->addChild(btn);
		this->addChild(menu, 100);

		this->addEventListener(
			KeybindSettingPressedEventV3(Mod::get(), "open-menu-key"),
			[this](Keybind const&, bool down, bool repeat, double) {
				if (down && !repeat) this->openDecoMenu();
			}
		);

		return true;
	}

	void onOpenDecoMenu(CCObject*) {
		this->openDecoMenu();
	}

	void openDecoMenu() {
		// Don't stack menus if one is already open.
		auto scene = CCDirector::get()->getRunningScene();
		if (scene && scene->getChildByType<DecoPopup>(0)) return;
		if (auto popup = DecoPopup::create(this)) popup->show();
	}
};
