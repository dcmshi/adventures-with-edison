// Paths into the CD's folders on case-sensitive filesystems.
#pragma once

#include <string>

namespace edison {

// The path as it is on disk: as given if it exists, else each part matched
// without regard to case (the CD's names are upper case, the games ask for
// some in lower case: Windows and macOS don't mind, Linux does). Unchanged
// if nothing matches.
std::string findPath(const std::string& path);

}  // namespace edison
