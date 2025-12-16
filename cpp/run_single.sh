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
BINARY_NAME="single_user_media_sample"
# Relative path from cpp/ to the webrtc build output
WEBRTC_SRC="../../webrtc-checkout/src"
BIN_PATH="$WEBRTC_SRC/out/Default/$BINARY_NAME"
# ---------------------

echo -e "\033[1;36m👤  Preparing Single User Sample...\033[0m"

if [ ! -f "$BIN_PATH" ]; then
    echo -e "\033[1;31m🚫  Oops! Binary not found at:\033[0m"
    echo "    $BIN_PATH"
    echo -e "\033[1;33m💡  Did you run ./build.sh yet?\033[0m"
    exit 1
fi

echo -e "\033[1;32m🚀  Launching $BINARY_NAME...\033[0m"
echo -e "\033[1;35m✨  Passing arguments: $@ \033[0m"
echo -e "\033[1;34m--------------------------------------------------\033[0m"

# Execute!
"$BIN_PATH" "$@"
