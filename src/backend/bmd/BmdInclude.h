#pragma once

#if defined(__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
    #include "BMDSwitcherAPI.h"
#elif defined(_WIN32)
    #include <windows.h>
    #include <objbase.h>
    #include "BMDSwitcherAPI_h.h"
#else
    // Dummy / fallback declarations if needed
#endif
