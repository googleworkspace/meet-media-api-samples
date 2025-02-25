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

load("//:build_defs.bzl", "ts_library")

_CONTENT = [
    "server.js",
    "webpack.config.js",
]

filegroup(
    name = "nodejs_server_files",
    srcs = _CONTENT,
)

ts_library(
    name = "script",
    srcs = ["script.ts"],
    deps = [
        "@media_api_samples//web/internal:meetmediaapiclient_impl",
        "@media_api_samples//web/types:enums",
        "@media_api_samples//web/types:media_types",
        "@media_api_samples//web/types:meetmediaapiclient_interface",
    ],
)
