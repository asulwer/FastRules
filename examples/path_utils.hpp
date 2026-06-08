// examples/path_utils.hpp
// Helper to resolve data file paths relative to project root

#pragma once
#include <filesystem>
#include <string>

namespace fastrules_examples {

    // Walk up from exe directory to find project root (contains data/ folder)
    inline std::filesystem::path findProjectRoot() {
        auto exePath = std::filesystem::current_path();
        auto checkPath = exePath;

        // Check current dir first (if running from project root)
        if (std::filesystem::exists(checkPath / "data")) {
            return checkPath;
        }

        // Walk up looking for data/ directory
        for (int i = 0; i < 5; ++i) {
            if (std::filesystem::exists(checkPath / "data")) {
                return checkPath;
            }
            auto parent = checkPath.parent_path();
            if (parent == checkPath) break;
            checkPath = parent;
        }

        // Fallback: return current directory and let caller fail gracefully
        return exePath;
    }

    inline std::string resolveDataPath(const std::string& relativePath) {
        auto root = findProjectRoot();
        auto fullPath = root / relativePath;
        return fullPath.string();
    }

}
