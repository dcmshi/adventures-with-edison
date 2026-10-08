#include "formats/paths.h"

#include <cctype>
#include <filesystem>
#include <system_error>

namespace edison {
namespace {

bool sameName(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

}  // namespace

std::string findPath(const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path given(path);
    if (path.empty() || fs::exists(given, ec)) return path;
    // Part by part from the root (or the working folder), each existing one
    // kept, the others looked for in their folder.
    fs::path found = given.root_path();
    for (const fs::path& part : given.relative_path()) {
        const std::string name = part.string();
        fs::path next = found / part;
        if (name == "." || name == ".." || fs::exists(next, ec)) {
            found = next;
            continue;
        }
        const fs::path dir = found.empty() ? fs::path(".") : found;
        bool matched = false;
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (sameName(e.path().filename().string(), name)) {
                found /= e.path().filename();
                matched = true;
                break;
            }
        }
        if (!matched) return path;
    }
    // (A trailing separator, as in a folder's "dir/", kept.)
    std::string out = found.string();
    if (!path.empty() && (path.back() == '/' || path.back() == '\\') && !out.empty() && out.back() != '/' &&
        out.back() != '\\')
        out += '/';
    return out;
}

}  // namespace edison
