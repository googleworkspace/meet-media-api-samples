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

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["@media_api_samples//cpp:__subpackages__"])

cc_binary(
    name = "multi_user_media_sample",
    srcs = ["multi_user_media_sample.cc"],
    deps = [
        ":multi_user_media_collector",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:video_assignment_resource",
        "@media_api_samples//cpp/internal:media_api_client_factory",
        "@webrtc",
    ],
)

cc_library(
    name = "multi_user_media_collector",
    srcs = ["multi_user_media_collector.cc"],
    hdrs = ["multi_user_media_collector.h"],
    deps = [
        ":media_writing",
        ":output_file",
        ":output_writer_interface",
        ":resource_manager",
        ":resource_manager_interface",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:media_entries_resource",
        "@media_api_samples//cpp/api:participants_resource",
        "@webrtc",
    ],
)

cc_library(
    name = "resource_manager_interface",
    hdrs = ["resource_manager_interface.h"],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@media_api_samples//cpp/api:media_entries_resource",
        "@media_api_samples//cpp/api:participants_resource",
    ],
)

cc_library(
    name = "resource_manager",
    srcs = ["resource_manager.cc"],
    hdrs = ["resource_manager.h"],
    deps = [
        ":output_writer_interface",
        ":resource_manager_interface",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@media_api_samples//cpp/api:media_entries_resource",
        "@media_api_samples//cpp/api:participants_resource",
    ],
)

cc_binary(
    name = "single_user_media_sample",
    srcs = ["single_user_media_sample.cc"],
    deps = [
        ":single_user_media_collector",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:video_assignment_resource",
        "@media_api_samples//cpp/internal:media_api_client_factory",
        "@webrtc",
    ],
)

cc_library(
    name = "single_user_media_collector",
    srcs = ["single_user_media_collector.cc"],
    hdrs = ["single_user_media_collector.h"],
    deps = [
        ":media_writing",
        ":output_file",
        ":output_writer_interface",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "output_writer_interface",
    hdrs = ["output_writer_interface.h"],
    deps = [
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "output_file",
    srcs = ["output_file.cc"],
    hdrs = ["output_file.h"],
    deps = [":output_writer_interface"],
)

cc_library(
    name = "media_writing",
    srcs = ["media_writing.cc"],
    hdrs = ["media_writing.h"],
    deps = [
        ":output_writer_interface",
        "@webrtc",
    ],
)
