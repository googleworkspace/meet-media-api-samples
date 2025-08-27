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

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["@media_api_samples//cpp:__subpackages__"])

cc_library(
    name = "media_api_client_interface",
    hdrs = [
        "media_api_client_interface.h",
    ],
    deps = [
        ":media_entries_resource",
        ":media_stats_resource",
        ":participants_resource",
        ":session_control_resource",
        ":video_assignment_resource",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
        "@webrtc",
    ],
)

cc_library(
    name = "media_api_client_factory_interface",
    hdrs = ["media_api_client_factory_interface.h"],
    deps = [
        ":media_api_client_interface",
        "@com_google_absl//absl/status:statusor",
        "@webrtc",
    ],
)

cc_library(
    name = "media_stats_resource",
    hdrs = [
        "media_stats_resource.h",
    ],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "participants_resource",
    hdrs = [
        "participants_resource.h",
    ],
)

cc_library(
    name = "video_assignment_resource",
    hdrs = [
        "video_assignment_resource.h",
    ],
    deps = ["@com_google_absl//absl/status"],
)

cc_library(
    name = "media_entries_resource",
    hdrs = [
        "media_entries_resource.h",
    ],
)

cc_library(
    name = "session_control_resource",
    hdrs = [
        "session_control_resource.h",
    ],
    deps = ["@com_google_absl//absl/status"],
)
