// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "native.hpp"

namespace NG
{
  /// Registers the imgui binding (`lib/std/imgui.ng`) into a run. All native
  /// names carry an `imgui` prefix; the GUI state is session-local to this
  /// registration and driven by the NG program through imguiInit/imguiRender.
  void registerImguiNatives(vm::NativeRegistry &registry);
} // namespace NG
