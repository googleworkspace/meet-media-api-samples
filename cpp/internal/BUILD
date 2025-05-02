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

package(default_visibility = ["@media_api_samples//cpp:__subpackages__"])

cc_library(
    name = "media_api_client",
    srcs = ["media_api_client.cc"],
    hdrs = ["media_api_client.h"],
    deps = [
        ":conference_data_channel_interface",
        ":conference_media_tracks",
        ":conference_peer_connection_interface",
        ":stats_request_from_report",
        ":variant_utils",
        ":webrtc_forward_decls",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:media_stats_resource",
        "@media_api_samples//cpp/api:session_control_resource",
        "@media_api_samples//cpp/api:video_assignment_resource",
        "@webrtc",
    ],
)

cc_library(
    name = "media_api_client_factory",
    srcs = ["media_api_client_factory.cc"],
    hdrs = ["media_api_client_factory.h"],
    deps = [
        ":conference_data_channel",
        ":conference_peer_connection",
        ":curl_connector",
        ":curl_request",
        ":http_connector_interface",
        ":media_api_audio_device_module",
        ":media_api_client",
        ":media_entries_resource_handler",
        ":media_stats_resource_handler",
        ":participants_resource_handler",
        ":session_control_resource_handler",
        ":video_assignment_resource_handler",
        ":webrtc_forward_decls",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@media_api_samples//cpp/api:media_api_client_factory_interface",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "variant_utils",
    hdrs = ["variant_utils.h"],
)

cc_library(
    name = "media_api_audio_device_module",
    srcs = ["media_api_audio_device_module.cc"],
    hdrs = ["media_api_audio_device_module.h"],
    deps = [
        ":webrtc_forward_decls",
        "@com_google_absl//absl/log:check",
        "@webrtc",
    ],
)

cc_library(
    name = "conference_peer_connection_interface",
    hdrs = ["conference_peer_connection_interface.h"],
    deps = [
        ":webrtc_forward_decls",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:string_view",
        "@webrtc",
    ],
)

cc_library(
    name = "conference_peer_connection",
    srcs = ["conference_peer_connection.cc"],
    hdrs = ["conference_peer_connection.h"],
    deps = [
        ":conference_media_tracks",
        ":conference_peer_connection_interface",
        ":http_connector_interface",
        ":webrtc_forward_decls",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@nlohmann_json//:json",
        "@webrtc",
    ],
)

cc_library(
    name = "sdp_constants",
    hdrs = ["testing/sdp_constants.h"],
)

cc_library(
    name = "http_connector_interface",
    hdrs = ["http_connector_interface.h"],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "curl_connector",
    srcs = ["curl_connector.cc"],
    hdrs = ["curl_connector.h"],
    deps = [
        ":curl_request",
        ":http_connector_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "curl_connector_test",
    srcs = ["curl_connector_test.cc"],
    deps = [
        ":curl_connector",
        ":mock_curl_api_wrapper",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googletest//:gtest_main",
        "@curl",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "curl_request",
    srcs = ["curl_request.cc"],
    hdrs = ["curl_request.h"],
    deps = [
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@curl",
    ],
)

cc_library(
    name = "mock_curl_api_wrapper",
    testonly = 1,
    hdrs = [
        "testing/mock_curl_api_wrapper.h",
    ],
    deps = [
        ":curl_request",
        "@com_google_googletest//:gtest",
        "@curl",
    ],
)

cc_test(
    name = "curl_request_test",
    srcs = ["curl_request_test.cc"],
    deps = [
        ":curl_request",
        ":mock_curl_api_wrapper",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googletest//:gtest_main",
        "@curl",
    ],
)

cc_library(
    name = "stats_request_from_report",
    srcs = ["stats_request_from_report.cc"],
    hdrs = ["stats_request_from_report.h"],
    deps = [
        ":webrtc_forward_decls",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@media_api_samples//cpp/api:media_stats_resource",
        "@webrtc",
    ],
)

cc_library(
    name = "conference_data_channel_interface",
    hdrs = ["conference_data_channel_interface.h"],
    deps = [
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@media_api_samples//cpp/api:media_api_client_interface",
    ],
)

cc_library(
    name = "conference_data_channel",
    srcs = ["conference_data_channel.cc"],
    hdrs = ["conference_data_channel.h"],
    deps = [
        ":conference_data_channel_interface",
        ":resource_handler_interface",
        ":webrtc_forward_decls",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "resource_handler_interface",
    hdrs = [
        "resource_handler_interface.h",
    ],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@media_api_samples//cpp/api:media_api_client_interface",
    ],
)

cc_library(
    name = "media_entries_resource_handler",
    srcs = ["media_entries_resource_handler.cc"],
    hdrs = [
        "media_entries_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:media_entries_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "media_entries_resource_handler_test",
    srcs = ["media_entries_resource_handler_test.cc"],
    deps = [
        ":media_entries_resource_handler",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:media_entries_resource",
    ],
)

cc_library(
    name = "media_stats_resource_handler",
    srcs = ["media_stats_resource_handler.cc"],
    hdrs = [
        "media_stats_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:media_stats_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "media_stats_resource_handler_test",
    srcs = ["media_stats_resource_handler_test.cc"],
    deps = [
        ":media_stats_resource_handler",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:media_stats_resource",
        "@media_api_samples//cpp/api:session_control_resource",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "participants_resource_handler",
    srcs = ["participants_resource_handler.cc"],
    hdrs = [
        "participants_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:participants_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "participants_resource_handler_test",
    srcs = ["participants_resource_handler_test.cc"],
    deps = [
        ":participants_resource_handler",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:participants_resource",
    ],
)

cc_library(
    name = "session_control_resource_handler",
    srcs = ["session_control_resource_handler.cc"],
    hdrs = [
        "session_control_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:session_control_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "session_control_resource_handler_test",
    srcs = ["session_control_resource_handler_test.cc"],
    deps = [
        ":session_control_resource_handler",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:media_stats_resource",
        "@media_api_samples//cpp/api:session_control_resource",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "video_assignment_resource_handler",
    srcs = ["video_assignment_resource_handler.cc"],
    hdrs = [
        "video_assignment_resource_handler.h",
    ],
    deps = [
        ":resource_handler_interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:video_assignment_resource",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "video_assignment_resource_handler_test",
    srcs = ["video_assignment_resource_handler_test.cc"],
    deps = [
        ":video_assignment_resource_handler",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@media_api_samples//cpp/api:session_control_resource",
        "@media_api_samples//cpp/api:video_assignment_resource",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "conference_media_tracks",
    srcs = ["conference_media_tracks.cc"],
    hdrs = ["conference_media_tracks.h"],
    deps = [
        ":webrtc_forward_decls",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@media_api_samples//cpp/api:media_api_client_interface",
        "@webrtc",
    ],
)

cc_library(
    name = "testing/sdp_constants",
    hdrs = ["testing/sdp_constants.h"],
)

cc_library(
    name = "mock_media_api_client_observer",
    testonly = 1,
    hdrs = ["testing/mock_media_api_client_observer.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest",
        "@media_api_samples//cpp/api:media_api_client_interface",
    ],
)

# TODO: Remove once build has updated to a recent WebRTC version.
cc_library(
    name = "webrtc_forward_decls",
    hdrs = ["webrtc_forward_decls.h"],
    visibility = ["@media_api_samples//cpp:__subpackages__"],
    deps = [
    ],
)
