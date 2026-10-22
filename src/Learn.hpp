#pragma once

#include "DecoTypes.hpp"

namespace deco {

// What kind of object is this id? (cached; uses GameObject::createWithKey)
Kind classifyId(int id);

// What kind of object is this editor object?
Kind classifyObject(GameObject* obj);

// A solid block that is unscaled enough to count as a full grid cell.
bool isBlockLike(GameObject* obj);

// Parse a level string (compressed or not) into neutral objects.
std::vector<RawObject> parseLevelString(std::string data);

// Read objects straight from the editor. Optionally leave out the selected ones.
std::vector<RawObject> collectEditorObjects(LevelEditorLayer* layer, bool skipSelected);

// Learn a style: how decoration sits around blocks in these objects.
Profile learn(std::vector<RawObject> const& objects, std::string name);

} // namespace deco
