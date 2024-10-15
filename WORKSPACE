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

workspace(name = "media_api_samples")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# See README.md - Replace this with the local path to your WebRTC checkout.
webrtc_path = "/usr/local/google/home/pareynolds/src/webrtc-checkout/"

http_archive(
    name = "rules_pkg",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/1.0.1/rules_pkg-1.0.1.tar.gz",
        "https://github.com/bazelbuild/rules_pkg/releases/download/1.0.1/rules_pkg-1.0.1.tar.gz",
    ],
    sha256 = "d20c951960ed77cb7b341c2a59488534e494d5ad1d30c4818c736d57772a9fef",
)

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")
rules_pkg_dependencies()

http_archive(
    name = "rules_license",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_license/releases/download/1.0.0/rules_license-1.0.0.tar.gz",
        "https://github.com/bazelbuild/rules_license/releases/download/1.0.0/rules_license-1.0.0.tar.gz",
    ],
    sha256 = "26d4021f6898e23b82ef953078389dd49ac2b5618ac564ade4ef87cced147b38",
)

# === Node ===

http_archive(
    name = "build_bazel_rules_nodejs",
    sha256 = "94070eff79305be05b7699207fbac5d2608054dd53e6109f7d00d923919ff45a",
    urls = ["https://github.com/bazelbuild/rules_nodejs/releases/download/5.8.2/rules_nodejs-5.8.2.tar.gz"],
)

load("@build_bazel_rules_nodejs//:repositories.bzl", "build_bazel_rules_nodejs_dependencies")
build_bazel_rules_nodejs_dependencies()

load("@build_bazel_rules_nodejs//:index.bzl", "node_repositories", "yarn_install")
node_repositories()
yarn_install(
    name = "npm",
    package_json = "@//:package.json",
    yarn_lock = "@//:yarn.lock",
)

# === Abseil ===

http_archive(
    name = "com_google_absl",
    strip_prefix = "abseil-cpp-20240722.0",
    urls = [
        "https://github.com/abseil/abseil-cpp/archive/refs/tags/20240722.0.tar.gz",
    ],
)

# Abseil-Python
http_archive(
    name = "com_google_absl_py",  # 2024-04-16
    integrity = "sha256-kpYxSP+2fHkqFfDL/9TDJvpcHil5IECWLTYFYRtl3c4=",
    strip_prefix = "abseil-py-fae7e951d46011fdaf62685893ef4efd48544c0a",
    urls = ["https://github.com/abseil/abseil-py/archive/fae7e951d46011fdaf62685893ef4efd48544c0a.zip"],
)

# === GoogleTest ===

http_archive(
    name = "com_google_googletest",
    sha256 = "ad7fdba11ea011c1d925b3289cf4af2c66a352e18d4c7264392fead75e919363",
    strip_prefix = "googletest-1.13.0",
    url = "https://github.com/google/googletest/archive/refs/tags/v1.13.0.tar.gz",
)


# === JSON ===

http_archive(
    name = "nlohmann_json",
    strip_prefix = "json-3.11.3",
    url = "https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz",
)

http_archive(
    name = "com_google_protobuf",
    sha256 = "930c2c3b5ecc6c9c12615cf5ad93f1cd6e12d0aba862b572e076259970ac3a53",
    strip_prefix = "protobuf-3.21.12",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.21.12.tar.gz"],
)

http_archive(
    name = "com_github_grpc_grpc",
    sha256 = "edf25f4db6c841853b7a29d61b0980b516dc31a1b6cdc399bcf24c1446a4a249",
    strip_prefix = "grpc-%s" % "1.47.0",
    urls = ["https://github.com/grpc/grpc/archive/v%s.zip" % "1.47.0"],
)

http_archive(
    name = "zlib",
    build_file = "@com_github_grpc_grpc//:third_party/zlib.BUILD",
    # Last updated 2024-01-22
    sha256 = "17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c",
    strip_prefix = "zlib-1.3.1",
    urls = ["https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz"],
)

http_archive(
    name = "curl",
    integrity = "sha256-KREkoAfuURGZeCWUCzh2swSPfTHnPpyqaBuA/kiy3NU=",
    urls = ["https://curl.se/download/curl-8.9.1.tar.gz"],
    strip_prefix = "curl-8.9.1",
    build_file = "curl.BUILD",
)

http_archive(
    name = "com_google_googleurl",
    # Last updated 2024-10-09
    sha256 = "3db90606ea9ca123d11f85c6fa3627a1e1e403d38c7fd1854a628cf1da3be5e2",
    urls = ["https://storage.googleapis.com/quiche-envoy-integration/googleurl-9cdb1f4d1a365ebdbcbf179dadf7f8aa5ee802e7.tar.gz"],
)

# === WebRTC ===
# Note: This is intended to point to a local build of WebRTC.
new_local_repository(
    name = "webrtc",
    path = webrtc_path,
    build_file = "webrtc.BUILD",
)

local_repository(
    name = "boringssl",
    path = webrtc_path + "webrtc/third_party/boringssl/src",
)

# === Python ===
http_archive(
    name = "rules_python",
    sha256 = "be04b635c7be4604be1ef20542e9870af3c49778ce841ee2d92fcb42f9d9516a",
    strip_prefix = "rules_python-0.35.0",
    url = "https://github.com/bazelbuild/rules_python/releases/download/0.35.0/rules_python-0.35.0.tar.gz",
)

load("@rules_python//python:repositories.bzl", "py_repositories")
py_repositories()

load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
   name = "util_deps",
   requirements_lock = "//web/utils:requirements.txt",
)

load("@util_deps//:requirements.bzl", "install_deps")
install_deps()
