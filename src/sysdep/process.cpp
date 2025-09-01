#include <sysdep/process.hpp>

#include <iostream> // For potential error output

#ifdef _WIN32
#include <windows.h> // For GetModuleFileNameA, MAX_PATH
#elif __linux__
#include <unistd.h> // For readlink
#include <limits.h> // For PATH_MAX
#elif __APPLE__
#include <mach-o/dyld.h> // For _NSGetExecutablePath
#include <limits.h>      // For PATH_MAX
#endif

namespace NG::System::Process
{

    using namespace NG;

    Str current_executable_path()
    {
        // --- Windows Implementation ---
#ifdef _WIN32
        char path[MAX_PATH];
        DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);
        if (length > 0 && length < MAX_PATH)
        {
            return std::string(path);
        }
        else
        {
            std::cerr << "Error: Could not retrieve executable path on Windows (error " << GetLastError() << ")" << std::endl;
            return ""; // Return empty string on error
        }

        // --- Linux Implementation ---
#elif __linux__
        char path[PATH_MAX]; // PATH_MAX is defined in <limits.h>
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1)
        {
            path[len] = '\0'; // Null-terminate the string
            return std::string(path);
        }
        else
        {
            perror("Error: Could not read /proc/self/exe on Linux");
            return ""; // Return empty string on error
        }

        // --- macOS Implementation ---
#elif __APPLE__
        char path[PATH_MAX];          // PATH_MAX is defined in <limits.h>
        uint32_t size = sizeof(path); // _NSGetExecutablePath modifies this
        if (_NSGetExecutablePath(path, &size) == 0)
        {
            return std::string(path);
        }
        else
        {
            // If the buffer was too small, _NSGetExecutablePath returns -1 and updates 'size'
            // with the required buffer size. This simple example just reports the error.
            std::cerr << "Error: Could not retrieve executable path on macOS (buffer too small or other error). Required size: " << size << std::endl;
            return ""; // Return empty string on error
        }

        // --- Fallback for unsupported platforms ---
#else
        std::cerr << "Error: Executable path retrieval not implemented for this platform." << std::endl;
        return ""; // Return empty string for unsupported platforms
#endif
    }
}