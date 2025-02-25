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

package(default_visibility = ["@media_api_samples//web:__subpackages__"])

ts_library(
    name = "channel_handlers",
    srcs = [
        "channel_logger.ts",
        "media_entries_channel_handler.ts",
        "media_stats_channel_handler.ts",
        "participants_channel_handler.ts",
        "session_control_channel_handler.ts",
        "video_assignment_channel_handler.ts",
    ],
    deps = [
        "@media_api_samples//web/internal:internal_types",
        "@media_api_samples//web/internal:subscribable_impl",
        "@media_api_samples//web/internal:utils",
        "@media_api_samples//web/types:datachannels",
        "@media_api_samples//web/types:enums",
        "@media_api_samples//web/types:media_types",
        "@media_api_samples//web/types:meetmediaapiclient_interface",
    ],
)
