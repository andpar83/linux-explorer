#pragma once

#include "lxe/model/process_model.hpp"

#include <string>

namespace lxe::model {

/// The process tree as text in the style of pstree: one "name(pid)" per line, children
/// connected with box-drawing characters. Every root (init, kthreadd, ...) starts a block.
[[nodiscard]] std::string render_text_tree(const Model& model);

} // namespace lxe::model
