#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
# Prefer the SDK shipped with Xcode when it exists: a newer standalone CLT SDK
# may contain architecture names the selected Xcode linker does not understand.
if [ "$(uname -s)" = Darwin ] && [ -d /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk ]; then
  set -- -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
else
  set --
fi
${CXX:-c++} "$@" -std=c++17 -Wall -Wextra -Werror -pedantic -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude \
  src/games/model.cpp src/app/app.cpp test/test_logic.cpp -o build/test_logic
./build/test_logic
