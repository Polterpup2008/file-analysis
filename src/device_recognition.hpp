#pragma once
#include "core.hpp"

namespace fa {
// Resolves only retained capture-model fields; no file names, lenses or pixels.
Json recognizeDevices(const Json& photo, const fs::path& catalog);
Json deviceNamesForSharing(const Json& recognition);
}
