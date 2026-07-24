#!/usr/bin/env bash
set -euo pipefail

# Deploy CommanderMarkers.dll to a local GW2 addons folder.
#
# Usage:
#   ./scripts/deploy-to-gw2.sh
#   ./scripts/deploy-to-gw2.sh --release
#   ./scripts/deploy-to-gw2.sh --local-test
#   ./scripts/deploy-to-gw2.sh --local-test --release

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
DIST_DIR="$ROOT/dist"
TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-toolchain.cmake"
ADDONS_DIR="${GW2_ADDONS_DIR:-/home/soeed/bottles/GW2/drive_c/Program Files/Guild Wars 2/addons}"
DLL_NAME="CommanderMarkers.dll"

RELEASE_MODE=false
LOCAL_TEST=false
for arg in "$@"; do
  case "$arg" in
    --release) RELEASE_MODE=true ;;
    --local-test) LOCAL_TEST=true ;;
    *)
      echo "error: unknown argument: $arg" >&2
      echo "usage: $0 [--release] [--local-test]" >&2
      exit 1
      ;;
  esac
done

if $LOCAL_TEST && $RELEASE_MODE; then
  echo "error: --local-test cannot be combined with --release" >&2
  exit 1
fi

if $LOCAL_TEST; then
  if [[ ! -f "$TOOLCHAIN_FILE" ]]; then
    echo "error: missing toolchain file: $TOOLCHAIN_FILE" >&2
    exit 1
  fi
  echo "Configuring local-test build (CM server http://localhost:3000)..."
  cmake -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCM_LOCAL_TEST=ON \
    "$ROOT"
  echo "Building $DLL_NAME..."
  cmake --build "$BUILD_DIR"
fi

if $RELEASE_MODE; then
  DLL_SRC="$DIST_DIR/$DLL_NAME"
  if [[ ! -f "$DLL_SRC" ]]; then
    echo "error: $DLL_SRC not found. Run ./scripts/build-release.sh first." >&2
    exit 1
  fi
else
  DLL_SRC="$BUILD_DIR/$DLL_NAME"
  if [[ ! -f "$DLL_SRC" ]]; then
    echo "error: $DLL_SRC not found. Build first (or pass --local-test to build)." >&2
    exit 1
  fi
fi

mkdir -p "$ADDONS_DIR"
cp -f "$DLL_SRC" "$ADDONS_DIR/$DLL_NAME"
echo "Deployed $DLL_NAME to $ADDONS_DIR"
if $LOCAL_TEST; then
  echo "Local test: ensure ggg server is running at http://localhost:3000 (make dev-api)"
fi
