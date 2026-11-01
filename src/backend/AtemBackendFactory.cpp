#include "AtemBackendFactory.h"

#if defined(__APPLE__) || defined(_WIN32)
#include "bmd/BmdSwitcherBackend.h"
#endif

#include "network/NetworkAtemBackend.h"

namespace Atem {

IAtemBackend* BackendFactory::createBackend(BackendType type, QObject *parent) {
    if (type == BackendType::DirectNetwork) {
        return new AtemNet::NetworkAtemBackend(parent);
    }

#if defined(__APPLE__) || defined(_WIN32)
    if (type == BackendType::OfficialSdk || type == BackendType::Auto) {
        return new BmdSwitcherBackend(parent);
    }
#endif

    // Linux or fallback
    return new AtemNet::NetworkAtemBackend(parent);
}

} // namespace Atem
