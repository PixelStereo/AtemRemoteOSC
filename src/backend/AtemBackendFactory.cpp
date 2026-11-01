#include "AtemBackendFactory.h"

#if defined(ATEM_HAVE_BMD_SDK)
#include "bmd/BmdSwitcherBackend.h"
#endif

#include "network/NetworkAtemBackend.h"

namespace Atem {

IAtemBackend* BackendFactory::createBackend(BackendType type, QObject *parent) {
    if (type == BackendType::DirectNetwork) {
        return new AtemNet::NetworkAtemBackend(parent);
    }

#if defined(ATEM_HAVE_BMD_SDK)
    if (type == BackendType::OfficialSdk || type == BackendType::Auto) {
        return new BmdSwitcherBackend(parent);
    }
#endif

    // Linux or fallback
    return new AtemNet::NetworkAtemBackend(parent);
}

} // namespace Atem
