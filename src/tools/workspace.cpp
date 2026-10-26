// src/tools/workspace.cpp — YOU IMPLEMENT (spec §4.4).
//
// Workspace::resolve: a purely lexical check, steps 1–6 of the spec.
// Do not touch the filesystem here (no exists(), canonical(), ...).
// The constructor, root() and check_symlinks() are provided in
// workspace_provided.cpp.

#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    try {
        if (user_path.empty()) {
            *reason = "empty path";
            return false;
        }
        if (user_path.find('\0') != std::string_view::npos) {
            *reason = "path contains a NUL byte";
            return false;
        }
        const fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path()) {
            *reason = "path must be workspace-relative";
            return false;
        }
        const fs::path n = p.lexically_normal();
        if (n.begin() != n.end() && *n.begin() == "..") {
            *reason = "path escapes the workspace";
            return false;
        }
        *out = n == "." ? root() : root() / n;
        return true;
    } catch (...) {
        *reason = "invalid path";
        return false;
    }
}
