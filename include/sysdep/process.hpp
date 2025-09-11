#pragma once

#include <common.hpp>

namespace NG::System::Process
{

    /**
     * @brief Returns the path to the current executable.
     *
     * Windows uses `GetModuleFileNameA` (ANSI, MAX_PATH-limited).
     * Linux uses `readlink("/proc/self/exe")`.
     * macOS uses `_NSGetExecutablePath`.
     *
     * The returned path is not canonicalized beyond OS behavior;
     * encoding/truncation are platform-dependent.
     *
     * @return Path to the current executable (absolute when available; empty string on failure).
     */
    NG::Str current_executable_path();
} // namespace NG::System::Process
