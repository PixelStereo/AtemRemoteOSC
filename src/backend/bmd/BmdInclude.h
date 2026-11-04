#pragma once

#if defined(__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
    #include "BMDSwitcherAPI.h"
    // Older generation of the switcher library (ATEM Software Control 10.2 to 10.4), see BmdSwitcherBackend
    #include "Legacy/BMDSwitcherAPI_v10_2_1.h"
    #include "Legacy/BMDSwitcherAPI_v10_4_1.h"
#elif defined(_WIN32)
    #include <windows.h>
    #include <objbase.h>
    #include "BMDSwitcherAPI_h.h"
#else
    // Dummy / fallback declarations if needed
#endif

// The SDK's boolean: a BOOL (int) in the Windows COM interfaces, a plain bool on macOS
#if defined(_WIN32)
using BmdBool = BOOL;
#else
using BmdBool = bool;
#endif
