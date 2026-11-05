#include "AtemBackendFactory.h"

#if defined(ATEM_HAVE_BMD_SDK)
#include "bmd/BmdSwitcherBackend.h"
#else
#include "null/NullAtemBackend.h"
#endif

namespace Atem {

IAtemBackend* BackendFactory::createBackend(QObject *parent) {
#if defined(ATEM_HAVE_BMD_SDK)
    return new BmdSwitcherBackend(parent);
#else
    return new NullAtemBackend(parent);
#endif
}

} // namespace Atem
