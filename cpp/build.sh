# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#!/bin/bash
set -e

# --- CONFIGURATION ---
TARGET_NAME="meet_media_api_client" # The library target
MODULE_NAME="meet_clients"
# Relative path from cpp/ to where we want the webrtc checkout
WEBRTC_DIR="../../webrtc-checkout"
WEBRTC_SRC="$WEBRTC_DIR/src"
CURRENT_DIR=$(pwd)
# ---------------------

echo -e "\033[1;36m🔍  [1/5] Scanning for WebRTC Universe...\033[0m"

if [ ! -d "$WEBRTC_SRC" ]; then
    echo -e "\033[1;33m    ⚠️  WebRTC checkout not found. Initializing auto-fetch sequence...\033[0m"

    # Check for depot_tools first
    if ! command -v fetch &> /dev/null; then
        echo -e "\033[1;31m💥  Error: 'fetch' command not found!\033[0m"
        echo "    You need depot_tools in your PATH to download WebRTC."
        exit 1
    fi

    # Create directory and fetch
    mkdir -p "$WEBRTC_DIR"
    pushd "$WEBRTC_DIR" > /dev/null

    echo -e "\033[1;34m    ⬇️  Fetching WebRTC (This takes a while, maybe grab a snack 🥨)...\033[0m"
    fetch --nohooks webrtc

    echo -e "\033[1;34m    🔄  Syncing dependencies...\033[0m"
    gclient sync

    popd > /dev/null
    echo -e "\033[1;32m    ✅  WebRTC Universe created successfully.\033[0m"
else
    echo -e "\033[1;32m    ✅  Found existing WebRTC checkout.\033[0m"
fi

echo -e "\033[1;36m💉  [2/5] Injecting your code into the matrix...\033[0m"
# Clean replace of the destination folder
rm -rf "$WEBRTC_SRC/$MODULE_NAME"
mkdir -p "$WEBRTC_SRC/$MODULE_NAME"
# Copy contents of current directory (.) to the destination
cp -r . "$WEBRTC_SRC/$MODULE_NAME"
echo -e "\033[1;32m    ✅  Code transplant successful.\033[0m"

echo -e "\033[1;36m🌉  [2.5/5] Building Bridge to System Curl...\033[0m"
CURL_DIR="$WEBRTC_SRC/third_party/curl"
CURL_BUILD="$CURL_DIR/BUILD.gn"

# Create the directory if it doesn't exist
mkdir -p "$CURL_DIR"

# Create a BUILD.gn that links to the sysroot's libcurl
if [ ! -f "$CURL_BUILD" ]; then
    cat > "$CURL_BUILD" <<EOF
import("//build/config/linux/pkg_config.gni")

# Define a config that links against the system 'curl'
config("curl_config") {
  libs = [ "curl" ]
}

# Define the target your code can depend on
group("curl") {
  public_configs = [ ":curl_config" ]
}
EOF
    echo -e "\033[1;32m    ✅  Bridge created at //third_party/curl\033[0m"
else
    echo -e "\033[1;33m    ⚡  Bridge already exists. Skipping.\033[0m"
fi

echo -e "\033[1;36m🕸️   [3/5] Patching the build graph...\033[0m"
BUILD_FILE="$WEBRTC_SRC/BUILD.gn"
DEP_LINE="\"//$MODULE_NAME:$TARGET_NAME\","
if grep -q "$MODULE_NAME:$TARGET_NAME" "$BUILD_FILE"; then
    echo -e "\033[1;33m    ⚡  Already patched. Skipping.\033[0m"
else
    # FIXED: Used '|' as delimiter instead of '/' to avoid conflict with path slashes
    sed -i "/group(\"default\") {/,/deps = \[/ s|deps = \[|deps = \[ $DEP_LINE|" "$BUILD_FILE"
    echo -e "\033[1;32m    ✅  Build graph hacked successfully.\033[0m"
fi

echo -e "\033[1;36m⚙️   [4/5] Generating Build Files...\033[0m"
# Check if we need to generate ninja files
if [ ! -f "$WEBRTC_SRC/out/Default/build.ninja" ]; then
    echo -e "\033[1;34m    ✨  Running gn gen...\033[0m"
    (cd "$WEBRTC_SRC" && gn gen out/Default --args='is_debug=true symbol_level=1')
    echo -e "\033[1;32m    ✅  Build files generated successfully.\033[0m"
fi

echo -e "\033[1;36m🔨  [5/5] Compiling... (Grab a coffee ☕)\033[0m"
ninja -C "$WEBRTC_SRC/out/Default" "$MODULE_NAME:$TARGET_NAME" "$@"

echo -e "\033[1;35m==================================================\033[0m"
echo -e "\033[1;32m🎉  BUILD SUCCESS! You are ready to rock. 🎸\033[0m"
echo -e "\033[1;35m==================================================\033[0m"
