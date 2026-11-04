#pragma once

#include "IAtemBackend.h"

namespace Atem {

class BackendFactory {
public:
    // The Blackmagic SDK backend on macOS and Windows, a backend that controls nothing elsewhere
    static IAtemBackend* createBackend(QObject *parent = nullptr);
};

} // namespace Atem
