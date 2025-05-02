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

cc_library(
  name = "webrtc",
  includes = ["webrtc"],
  visibility = ["//visibility:public"],
  defines = ["FORCE_ADD_WEBRTC_NAMESPACE_EXPORTS"],
  deps = [
    ":webrtc-lib",
  ],
)

cc_import(
  name = "webrtc-lib",
  static_library = "webrtc/out/Default/obj/libwebrtc.a",
  hdrs = glob(["webrtc/**/*.h"]),
  visibility = ["//visibility:public"],
  deps = [
    "@com_google_absl//absl/algorithm:container",
    "@com_google_absl//absl/base:core_headers",
    "@com_google_absl//absl/base:nullability",
    "@com_google_absl//absl/container:inlined_vector",
    "@com_google_absl//absl/functional:any_invocable",
    "@com_google_absl//absl/strings",
    "@com_google_absl//absl/strings:string_view",
    "@com_google_absl//absl/types:optional",
    "@com_google_absl//absl/types:variant",
  ],
)
