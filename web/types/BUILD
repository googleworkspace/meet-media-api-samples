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

load(
    "//:build_defs.bzl",
    "ts_library",
)

package(default_visibility = ["//visibility:public"])

ts_library(
    name = "media_types",
    srcs = ["mediatypes.d.ts"],
    deps = [
        ":datachannels",
        ":enums",
        ":subcribable",
    ],
)

ts_library(
    name = "subcribable",
    srcs = ["subscribable.d.ts"],
)

ts_library(
    name = "enums",
    srcs = ["enums.ts"],
)

ts_library(
    name = "datachannels",
    srcs = ["datachannels.d.ts"],
)

ts_library(
    name = "mediacapture_transform",
    srcs = ["mediacapture_transform.d.ts"],
)
