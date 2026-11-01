#pragma once

#include "IAtemBackend.h"

namespace Atem {

enum class BackendType {
    Auto,
    OfficialSdk,
    DirectNetwork
};

class BackendFactory {
public:
    static IAtemBackend* createBackend(BackendType type = BackendType::Auto, QObject *parent = nullptr);
};

} // namespace Atem
