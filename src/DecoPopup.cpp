#include "DecoPopup.hpp"
#include "Decorator.hpp"

#include <algorithm>
#include <iterator>

using namespace geode::prelude;

namespace {

struct InfoText {
	char const* title;
	char const* body;
};

// Index = tag of the info button that opens it.
constexpr InfoText kInfo[] = {
	{ "Style Source",
	  "Where the decoration style comes from.\n\n"
	  "<cg>This Level</c>: copies the decoration already in the level you are editing "
	  "(your selected blocks are ignored while learning).\n\n"
	  "<cg>Online Level</c>: copies the style of any online level. Type its ID or its name.\n\n"
	  "<cg>Player</c>: type the ID of any level made by a player. The mod learns from that "
	  "level plus up to 3 more of that player's levels, so you get their overall style." },
	{ "Level / Player",
	  "Type what to copy the style from, then press <cg>Confirm</c>.\n\n"
	  "Online Level: a level ID (like 128) or a level name.\n"
	  "Player: the ID of a level that player made.\n"
	  "This Level: nothing to type.\n\n"
	  "Confirm downloads and studies the level. You can only press Decorate once the style is ready." },
	{ "Density",
	  "How much decoration is placed along each edge.\n\n"
	  "Low = sparse and minimal. High = lush and detailed." },
	{ "Flow",
	  "How strongly the decoration bends and sways along an edge so neighbouring objects "
	  "look connected, like a soft wave.\n\n"
	  "0% = every object keeps exactly the rotation it had in the source style." },
	{ "Top Edges",
	  "Decorate the tops of your selected blocks (grass, plants, caps, things that grow upward)." },
	{ "Bottom Edges",
	  "Decorate the undersides of your selected blocks (roots, drips, things that hang down)." },
	{ "Side Edges",
	  "Decorate the left and right sides of your selected blocks (walls, vines, side trims)." },
	{ "Clean Mode",
	  "Keeps everything tidy and rate-worthy.\n\n"
	  "ON: rotations snap to 5 degrees, no random nudging, tiny size variation.\n"
	  "OFF: a more organic, hand-placed look with a little random jitter." },
	{ "Decorate",
	  "Decorates only the <cg>solid blocks in your current selection</c>, along their open edges.\n\n"
	  "Hazards, slopes, portals, triggers and existing decoration in your selection are left "
	  "untouched so gameplay stays readable.\n\n"
	  "Every object it adds goes on the undo list, so Ctrl+Z removes them." },
};

constexpr int kInfoSource = 0;
constexpr int kInfoInput = 1;
constexpr int kInfoDensity = 2;
constexpr int kInfoFlow = 3;
constexpr int kInfoTop = 4;
constexpr int kInfoBottom = 5;
constexpr int kInfoSides = 6;
constexpr int kInfoClean = 7;
constexpr int kInfoDecorate = 8;

constexpr int kTagTop = 1;
constexpr int kTagBottom = 2;
constexpr int kTagSides = 3;
constexpr int kTagClean = 4;

constexpr int kTagDensity = 1;
constexpr int kTagFlow = 2;

char const* placeholderFor(int source) {
	switch (source) {
		case 1: return "Level ID or name";
		case 2: return "ID of a level by the player";
		default: return "Uses decoration in this level";
	}
}

} // namespace

DecoPopup* DecoPopup::create(EditorUI* ui) {
	auto ret = new DecoPopup();
	if (ret->init(ui)) {
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

bool DecoPopup::init(EditorUI* ui) {
	if (!Popup::init(420.f, 292.f)) return false;

	m_ui = ui;
	m_fetcher = std::make_unique<deco::StyleFetcher>();
	this->setTitle("Auto Decorate");

	// Restore the last used settings.
	auto mod = Mod::get();
	m_source = std::clamp(mod->getSavedValue<int>("source", 0), 0, 2);
	m_opt.density = static_cast<float>(mod->getSavedValue<double>("density", 0.6));
	m_opt.flow = static_cast<float>(mod->getSavedValue<double>("flow", 0.5));
	m_opt.top = mod->getSavedValue<bool>("top", true);
	m_opt.bottom = mod->getSavedValue<bool>("bottom", true);
	m_opt.sides = mod->getSavedValue<bool>("sides", true);
	m_opt.clean = mod->getSavedValue<bool>("clean", true);
	m_opt.maxObjects = static_cast<int>(mod->getSettingValue<int64_t>("max-objects"));

	auto addLabel = [&](char const* text, CCPoint pos, float scale, char const* font, bool left) {
		auto label = CCLabelBMFont::create(text, font);
		label->setScale(scale);
		if (left) label->setAnchorPoint({ 0.f, 0.5f });
		m_mainLayer->addChildAtPosition(label, Anchor::Center, pos);
		return label;
	};

	auto addInfo = [&](int tag, CCPoint pos) {
		auto spr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
		spr->setScale(0.55f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(DecoPopup::onInfo));
		btn->setTag(tag);
		m_buttonMenu->addChildAtPosition(btn, Anchor::Center, pos);
	};

	// --- Style source -------------------------------------------------------
	addLabel("Style Source", { -195.f, 100.f }, 0.55f, "goldFont.fnt", true);
	addInfo(kInfoSource, { -92.f, 100.f });

	char const* sourceNames[3] = { "This Level", "Online Level", "Player" };
	for (int i = 0; i < 3; i++) {
		m_sourceSprites[i] = ButtonSprite::create(
			sourceNames[i], 110, true, "bigFont.fnt",
			i == m_source ? "GJ_button_01.png" : "GJ_button_04.png", 28.f, 0.5f
		);
		auto btn = CCMenuItemSpriteExtra::create(m_sourceSprites[i], this, menu_selector(DecoPopup::onSource));
		btn->setTag(i);
		m_buttonMenu->addChildAtPosition(btn, Anchor::Center, { -125.f + 125.f * i, 72.f });
	}

	// --- Input + Confirm ----------------------------------------------------
	addInfo(kInfoInput, { -190.f, 38.f });

	m_input = TextInput::create(240.f, placeholderFor(m_source), "chatFont.fnt");
	m_input->setMaxCharCount(40);
	m_input->setString(mod->getSavedValue<std::string>("query", ""));
	m_input->setCallback([this](std::string const&) {
		this->setReady(false);
		this->setStatus("Press Confirm to load this style.", { 255, 255, 255 });
	});
	m_mainLayer->addChildAtPosition(m_input, Anchor::Center, { -45.f, 38.f });

	auto confirmSpr = ButtonSprite::create("Confirm", 90, true, "goldFont.fnt", "GJ_button_01.png", 28.f, 0.6f);
	auto confirmBtn = CCMenuItemSpriteExtra::create(confirmSpr, this, menu_selector(DecoPopup::onConfirm));
	m_buttonMenu->addChildAtPosition(confirmBtn, Anchor::Center, { 145.f, 38.f });

	m_status = addLabel("", { 0.f, 8.f }, 0.38f, "bigFont.fnt", false);
	m_status->setScale(0.38f);

	// --- Sliders ------------------------------------------------------------
	addLabel("Density", { -195.f, -20.f }, 0.5f, "goldFont.fnt", true);
	auto densitySlider = Slider::create(this, menu_selector(DecoPopup::onSlider), 0.6f);
	densitySlider->getThumb()->setTag(kTagDensity);
	densitySlider->setValue(m_opt.density);
	m_buttonMenu->addChildAtPosition(densitySlider, Anchor::Center, { -10.f, -20.f });
	m_densityLabel = addLabel("", { 90.f, -20.f }, 0.4f, "bigFont.fnt", true);
	addInfo(kInfoDensity, { 150.f, -20.f });

	addLabel("Flow", { -195.f, -48.f }, 0.5f, "goldFont.fnt", true);
	auto flowSlider = Slider::create(this, menu_selector(DecoPopup::onSlider), 0.6f);
	flowSlider->getThumb()->setTag(kTagFlow);
	flowSlider->setValue(m_opt.flow);
	m_buttonMenu->addChildAtPosition(flowSlider, Anchor::Center, { -10.f, -48.f });
	m_flowLabel = addLabel("", { 90.f, -48.f }, 0.4f, "bigFont.fnt", true);
	addInfo(kInfoFlow, { 150.f, -48.f });

	// --- Toggles ------------------------------------------------------------
	struct ToggleDef {
		char const* name;
		int tag;
		int info;
		bool value;
	};
	ToggleDef toggles[4] = {
		{ "Top", kTagTop, kInfoTop, m_opt.top },
		{ "Bottom", kTagBottom, kInfoBottom, m_opt.bottom },
		{ "Sides", kTagSides, kInfoSides, m_opt.sides },
		{ "Clean", kTagClean, kInfoClean, m_opt.clean },
	};
	for (int i = 0; i < 4; i++) {
		float x0 = -190.f + 100.f * i;
		auto toggler = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(DecoPopup::onToggle), 0.6f);
		toggler->setTag(toggles[i].tag);
		toggler->toggle(toggles[i].value);
		m_buttonMenu->addChildAtPosition(toggler, Anchor::Center, { x0, -80.f });
		addLabel(toggles[i].name, { x0 + 18.f, -80.f }, 0.45f, "bigFont.fnt", true);
		addInfo(toggles[i].info, { x0 + 74.f, -80.f });
	}

	// --- Decorate -----------------------------------------------------------
	m_decorateSprite = ButtonSprite::create("Decorate", 150, true, "goldFont.fnt", "GJ_button_04.png", 32.f, 0.8f);
	m_decorateBtn = CCMenuItemSpriteExtra::create(m_decorateSprite, this, menu_selector(DecoPopup::onDecorate));
	m_buttonMenu->addChildAtPosition(m_decorateBtn, Anchor::Center, { 0.f, -115.f });
	addInfo(kInfoDecorate, { 100.f, -115.f });

	this->refreshSource();
	this->onSlider(nullptr); // refresh % labels
	this->setReady(false);
	this->setStatus(m_source == 0 ? "Press Confirm to learn this level's style." : "Type a level, then press Confirm.", { 255, 255, 255 });

	return true;
}

void DecoPopup::setStatus(std::string const& text, ccColor3B color) {
	if (!m_status) return;
	m_status->setString(text.c_str());
	m_status->setColor(color);
	// Keep long messages inside the popup.
	float maxWidth = 380.f;
	m_status->setScale(0.38f);
	float w = m_status->getContentSize().width * m_status->getScale();
	if (w > maxWidth) m_status->setScale(0.38f * maxWidth / w);
}

void DecoPopup::setReady(bool ready) {
	m_ready = ready;
	if (!ready) m_profile.reset();
	if (m_decorateBtn) m_decorateBtn->setEnabled(ready);
	if (m_decorateSprite) m_decorateSprite->updateBGImage(ready ? "GJ_button_01.png" : "GJ_button_04.png");
}

void DecoPopup::refreshSource() {
	for (int i = 0; i < 3; i++) {
		if (m_sourceSprites[i]) {
			m_sourceSprites[i]->updateBGImage(i == m_source ? "GJ_button_01.png" : "GJ_button_04.png");
		}
	}
	if (m_input) {
		m_input->setEnabled(m_source != 0);
		m_input->setPlaceholder(placeholderFor(m_source));
	}
}

void DecoPopup::saveOptions() {
	auto mod = Mod::get();
	mod->setSavedValue("source", m_source);
	mod->setSavedValue("density", static_cast<double>(m_opt.density));
	mod->setSavedValue("flow", static_cast<double>(m_opt.flow));
	mod->setSavedValue("top", m_opt.top);
	mod->setSavedValue("bottom", m_opt.bottom);
	mod->setSavedValue("sides", m_opt.sides);
	mod->setSavedValue("clean", m_opt.clean);
	if (m_input) mod->setSavedValue("query", m_input->getString());
}

void DecoPopup::onSource(CCObject* sender) {
	m_source = static_cast<CCNode*>(sender)->getTag();
	this->refreshSource();
	this->setReady(false);
	this->setStatus(m_source == 0 ? "Press Confirm to learn this level's style." : "Type a level, then press Confirm.", { 255, 255, 255 });
}

void DecoPopup::onConfirm(CCObject*) {
	this->setReady(false);
	this->setStatus("Loading style...", { 255, 220, 90 });

	m_fetcher->start(
		static_cast<deco::Source>(m_source),
		m_input->getString(),
		m_ui->m_editorLayer,
		[this](bool ok, deco::Profile profile, std::string message) {
			if (!ok) {
				this->setReady(false);
				this->setStatus(message, { 255, 90, 90 });
				return;
			}
			m_profile = std::make_shared<deco::Profile>(std::move(profile));
			this->setReady(true);
			this->setStatus(message, { 125, 255, 125 });
		}
	);
}

void DecoPopup::onDecorate(CCObject*) {
	if (!m_ready || !m_profile) {
		FLAlertLayer::create("Auto Decorate", "Press <cg>Confirm</c> first so the style can be loaded.", "OK")->show();
		return;
	}

	this->saveOptions();
	auto res = deco::decorateSelection(m_ui, *m_profile, m_opt);

	if (!res.error.empty()) {
		FLAlertLayer::create("Auto Decorate", res.error, "OK")->show();
		return;
	}

	auto msg = fmt::format("Placed {} decorations along {} edges", res.placed, res.faces);
	if (res.capped) msg += " (hit the object limit)";
	Notification::create(msg, NotificationIcon::Success)->show();

	if (res.hazards + res.slopes + res.decos + res.others > 0) {
		log::info(
			"Auto Decorate skipped {} hazards, {} slopes, {} decorations, {} other objects in the selection",
			res.hazards, res.slopes, res.decos, res.others
		);
	}

	this->onClose(nullptr);
}

void DecoPopup::onInfo(CCObject* sender) {
	int tag = static_cast<CCNode*>(sender)->getTag();
	if (tag < 0 || tag >= static_cast<int>(std::size(kInfo))) return;
	FLAlertLayer::create(nullptr, kInfo[tag].title, kInfo[tag].body, "OK", nullptr, 380.f)->show();
}

void DecoPopup::onToggle(CCObject* sender) {
	auto toggler = static_cast<CCMenuItemToggler*>(sender);
	// GD runs the callback before the toggle flips, so the new state is the opposite.
	bool value = !toggler->isToggled();
	switch (toggler->getTag()) {
		case kTagTop: m_opt.top = value; break;
		case kTagBottom: m_opt.bottom = value; break;
		case kTagSides: m_opt.sides = value; break;
		case kTagClean: m_opt.clean = value; break;
		default: break;
	}
	this->saveOptions();
}

void DecoPopup::onSlider(CCObject* sender) {
	if (sender) {
		auto thumb = static_cast<SliderThumb*>(sender);
		float v = thumb->getValue();
		if (thumb->getTag() == kTagDensity) m_opt.density = std::max(0.05f, v);
		else if (thumb->getTag() == kTagFlow) m_opt.flow = v;
		this->saveOptions();
	}
	if (m_densityLabel) m_densityLabel->setString(fmt::format("{}%", static_cast<int>(m_opt.density * 100.f + 0.5f)).c_str());
	if (m_flowLabel) m_flowLabel->setString(fmt::format("{}%", static_cast<int>(m_opt.flow * 100.f + 0.5f)).c_str());
}
