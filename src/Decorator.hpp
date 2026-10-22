#pragma once

#include "DecoTypes.hpp"

namespace deco {

// Decorates the blocks in the editor's current selection using the given style.
// Only exposed block edges are decorated; hazards, slopes, triggers and existing
// decoration in the selection are left alone.
Result decorateSelection(EditorUI* ui, Profile const& profile, Options const& opt);

} // namespace deco
