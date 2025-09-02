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

package(default_visibility = [
    "@media_api_samples//cpp:__subpackages__",
])

cc_library(
    name = "media_data",
    testonly = True,
    srcs = ["media_data.cc"],
    hdrs = ["media_data.h"],
    deps = [
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "mock_output_writer",
    testonly = True,
    hdrs = ["mock_output_writer.h"],
    deps = [
        "@com_google_googletest//:gtest",
        "@media_api_samples//cpp/samples:output_writer_interface",
    ],
)

cc_library(
    name = "mock_resource_manager",
    testonly = True,
    hdrs = ["mock_resource_manager.h"],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
        "@media_api_samples//cpp/api:media_entries_resource",
        "@media_api_samples//cpp/api:participants_resource",
        "@media_api_samples//cpp/samples:resource_manager_interface",
    ],
)
