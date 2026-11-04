cmake_minimum_required(VERSION 3.16)
project(AtemRemoteOsc VERSION 5.0.3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    src/main.cpp
    src/app/AtemRemoteOscApp.h
    src/app/AtemRemoteOscApp.cpp
    src/osc/OscTypes.h
    src/osc/OscPacket.h
    src/osc/OscPacket.cpp
    src/osc/OscServer.h
    src/osc/OscServer.cpp
    src/osc/OscClient.h
    src/osc/OscClient.cpp
    src/osc/OscAddressRegistry.h
    src/osc/OscAddressRegistry.cpp
    src/osc/OscDispatcher.h
    src/osc/OscDispatcher.cpp
    src/backend/AtemTypes.h
    src/backend/IAtemBackend.h
    src/backend/AtemBackendFactory.h
    src/backend/AtemBackendFactory.cpp
    src/backend/null/NullAtemBackend.h
    src/gui/Theme.h
    src/gui/Theme.cpp
    src/gui/MainWindow.h
    src/gui/MainWindow.cpp
    src/gui/SwitcherListWidget.h
    src/gui/SwitcherListWidget.cpp
    src/gui/ConnectionSettingsWidget.h
    src/gui/ConnectionSettingsWidget.cpp
    src/gui/OscAddressBrowserWidget.h
    src/gui/OscAddressBrowserWidget.cpp
    src/gui/LogConsoleWidget.h
    src/gui/LogConsoleWidget.cpp
)

set(BMD_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sdk/Blackmagic ATEM Switchers SDK 11.0")

if(APPLE)
    list(APPEND SOURCES
        src/backend/bmd/BmdInclude.h
        src/backend/bmd/BmdSwitcherBackend.h
        src/backend/bmd/BmdSwitcherBackend.cpp
        src/backend/bmd/BmdCallbackMonitors.h
        src/backend/bmd/BmdCallbackMonitors.cpp
        "${BMD_SDK_DIR}/Mac OS X/include/BMDSwitcherAPIDispatch.cpp"
        "${BMD_SDK_DIR}/Mac OS X/include/Legacy/BMDSwitcherAPIDispatch_v10_2_1.cpp"
    )
    include_directories("${BMD_SDK_DIR}/Mac OS X/include" "${BMD_SDK_DIR}/Mac OS X/include/Legacy")
elseif(WIN32)
    # The Windows SDK ships an IDL file: generate the COM headers and IID definitions with MIDL
    set(BMD_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/bmd_gen")
    file(MAKE_DIRECTORY "${BMD_GEN_DIR}")
    add_custom_command(
        OUTPUT "${BMD_GEN_DIR}/BMDSwitcherAPI_h.h" "${BMD_GEN_DIR}/BMDSwitcherAPI_i.c"
        COMMAND midl /nologo /env x64 /out "${BMD_GEN_DIR}"
                /h BMDSwitcherAPI_h.h /iid BMDSwitcherAPI_i.c
                /proxy nul /dlldata nul /tlb BMDSwitcherAPI.tlb
                /I "${BMD_SDK_DIR}/Windows/include"
                "${BMD_SDK_DIR}/Windows/include/BMDSwitcherAPI.idl"
        DEPENDS "${BMD_SDK_DIR}/Windows/include/BMDSwitcherAPI.idl"
        COMMENT "Generating Blackmagic ATEM COM headers with MIDL"
        VERBATIM
    )
    list(APPEND SOURCES
        src/backend/bmd/BmdInclude.h
        src/backend/bmd/BmdSwitcherBackend.h
        src/backend/bmd/BmdSwitcherBackend.cpp
        src/backend/bmd/BmdCallbackMonitors.h
        src/backend/bmd/BmdCallbackMonitors.cpp
        "${BMD_GEN_DIR}/BMDSwitcherAPI_h.h"
        "${BMD_GEN_DIR}/BMDSwitcherAPI_i.c"
    )
    include_directories("${BMD_GEN_DIR}")
endif()

add_executable(AtemRemoteOsc ${SOURCES})

# Seen by moc too (unlike the compiler's own platform macros): the SDK backend only exists on macOS and Windows
if(APPLE OR WIN32)
    target_compile_definitions(AtemRemoteOsc PRIVATE ATEM_HAVE_BMD_SDK=1)
endif()

target_link_libraries(AtemRemoteOsc PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(APPLE)
    target_link_libraries(AtemRemoteOsc PRIVATE "-framework CoreFoundation")
    set_target_properties(AtemRemoteOsc PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "AtemRemoteOsc"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.pixelstereo.AtemRemoteOsc"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
elseif(WIN32)
    set_target_properties(AtemRemoteOsc PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(AtemRemoteOsc PRIVATE ole32 oleaut32)
endif()

enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(test_osc
    tests/test_osc.cpp
    src/osc/OscTypes.h
    src/osc/OscPacket.h
    src/osc/OscPacket.cpp
    src/osc/OscServer.h
    src/osc/OscServer.cpp
    src/osc/OscClient.h
    src/osc/OscClient.cpp
    src/osc/OscAddressRegistry.h
    src/osc/OscAddressRegistry.cpp
    src/osc/OscDispatcher.h
    src/osc/OscDispatcher.cpp
    src/backend/AtemTypes.h
    src/backend/IAtemBackend.h
    src/backend/null/NullAtemBackend.h
)
target_link_libraries(test_osc PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
add_test(NAME test_osc COMMAND test_osc)

