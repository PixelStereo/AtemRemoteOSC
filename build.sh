#!/usr/bin/env bash
# Installs the build dependencies (Qt 6, CMake, compiler) and builds atemOSC.
#
#   ./build.sh                 build in ./build (Release)
#   ./build.sh --debug         Debug build
#   ./build.sh --clean         wipe ./build first
#   ./build.sh --no-deps       skip dependency installation
#   ./build.sh --no-test       skip ctest
#
# macOS: Homebrew. Linux: apt, dnf, pacman or zypper (uses sudo).
# Windows: run from Git Bash/MSYS with Qt 6 + Visual Studio installed, and set QT_DIR
#          (e.g. QT_DIR=C:/Qt/6.7.0/msvc2019_64 ./build.sh --no-deps).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT/build"
BUILD_TYPE="Release"
INSTALL_DEPS=1
RUN_TESTS=1
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        --debug)   BUILD_TYPE="Debug" ;;
        --clean)   CLEAN=1 ;;
        --no-deps) INSTALL_DEPS=0 ;;
        --no-test) RUN_TESTS=0 ;;
        -h|--help) sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

log() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

SUDO=""
if [ "$(id -u)" -ne 0 ] && have sudo; then SUDO="sudo"; fi

OS="$(uname -s)"
CMAKE_ARGS=()
JOBS=1

install_deps() {
    case "$OS" in
        Darwin)
            have brew || die "Homebrew is required: https://brew.sh"
            if ! xcode-select -p >/dev/null 2>&1; then
                log "Installing Xcode command line tools (follow the dialog, then re-run)"
                xcode-select --install || true
                exit 1
            fi
            log "Installing Qt 6 and CMake with Homebrew"
            brew list qt >/dev/null 2>&1 || brew install qt
            brew list cmake >/dev/null 2>&1 || brew install cmake
            ;;
        Linux)
            if have apt-get; then
                log "Installing dependencies with apt"
                $SUDO apt-get update
                $SUDO apt-get install -y build-essential cmake pkg-config qt6-base-dev \
                    libgl1-mesa-dev libxkbcommon-dev
            elif have dnf; then
                log "Installing dependencies with dnf"
                $SUDO dnf install -y gcc-c++ cmake make qt6-qtbase-devel mesa-libGL-devel libxkbcommon-devel
            elif have pacman; then
                log "Installing dependencies with pacman"
                $SUDO pacman -S --needed --noconfirm base-devel cmake qt6-base
            elif have zypper; then
                log "Installing dependencies with zypper"
                $SUDO zypper install -y gcc-c++ cmake make qt6-base-devel Mesa-libGL-devel libxkbcommon-devel
            else
                die "Unsupported package manager. Install CMake, a C++17 compiler and Qt 6 (base) manually, then use --no-deps."
            fi
            ;;
        MINGW*|MSYS*|CYGWIN*)
            log "Windows: dependencies are not installed automatically."
            have cmake || die "CMake not found. Install it from https://cmake.org/download/ and add it to PATH."
            ;;
        *)
            die "Unsupported OS: $OS"
            ;;
    esac
}

configure_platform() {
    case "$OS" in
        Darwin)
            QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
            [ -n "$QT_PREFIX" ] && CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=$QT_PREFIX")
            JOBS="$(sysctl -n hw.ncpu)"
            ;;
        Linux)
            JOBS="$(nproc)"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            [ -n "${QT_DIR:-}" ] || die "Set QT_DIR to your Qt kit, e.g. QT_DIR=C:/Qt/6.7.0/msvc2019_64"
            CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=$QT_DIR")
            JOBS="${NUMBER_OF_PROCESSORS:-4}"
            ;;
    esac
}

[ "$INSTALL_DEPS" -eq 1 ] && install_deps
have cmake || die "cmake not found (run without --no-deps)"

if [ "$OS" = "Darwin" ] || [ "$OS" = "Linux" ]; then
    [ -d "$ROOT/sdk" ] || log "Note: sdk/ not found. macOS builds need the Blackmagic ATEM Switchers SDK 11.0 there."
fi

configure_platform

[ "$CLEAN" -eq 1 ] && { log "Cleaning $BUILD_DIR"; rm -rf "$BUILD_DIR"; }

log "Configuring ($BUILD_TYPE)"
cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"}

log "Building with $JOBS jobs"
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "$JOBS"

if [ "$RUN_TESTS" -eq 1 ]; then
    log "Running tests"
    ctest --test-dir "$BUILD_DIR" -C "$BUILD_TYPE" --output-on-failure
fi

case "$OS" in
    Darwin) OUT="$BUILD_DIR/atemOSC.app" ;;
    MINGW*|MSYS*|CYGWIN*) OUT="$BUILD_DIR/$BUILD_TYPE/atemOSC.exe" ;;
    *) OUT="$BUILD_DIR/atemOSC" ;;
esac
log "Done: $OUT"
