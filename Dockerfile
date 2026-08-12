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

FROM ubuntu:24.04
WORKDIR /app
COPY . /app
RUN apt-get update && apt-get install -y curl gpg locales && localedef -i en_US -c -f UTF-8 -A /usr/share/locale/locale.alias en_US.UTF-8
ENV LANG en_US.utf8
RUN curl -fsSL https://releases.bazel.build/bazel-release.pub.gpg | gpg --dearmor >bazel-archive-keyring.gpg
RUN mv bazel-archive-keyring.gpg /usr/share/keyrings
RUN echo "deb [arch=amd64 signed-by=/usr/share/keyrings/bazel-archive-keyring.gpg] https://storage.googleapis.com/bazel-apt stable jdk1.8" | tee /etc/apt/sources.list.d/bazel.list
RUN apt-get update && apt-get install -y git curl wget python3 xz-utils lsb-release pkg-config bazel libicu-dev apt-transport-https gnupg
RUN mkdir /src
WORKDIR /src
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH="/src/depot_tools:${PATH}"
RUN mkdir webrtc-checkout
WORKDIR /src/webrtc-checkout
RUN fetch --no-history --nohooks webrtc
RUN sed -i -e "s|'src/resources'],|'src/resources'],'condition':'rtc_include_tests==true',|" src/DEPS
RUN gclient sync
RUN mv src webrtc
WORKDIR /src/webrtc-checkout/webrtc
RUN gn gen out/Default --args='is_debug=false use_custom_libcxx=false rtc_include_tests=false rtc_build_examples=false dcheck_always_on=true rtc_use_x11=false use_rtti=true'
RUN ninja -C out/Default
WORKDIR /app
RUN sed -i -e 's|webrtc_path = ".*"|webrtc_path = "/src/webrtc-checkout/"|g' WORKSPACE
RUN bazel build ...
RUN bazel test ...
