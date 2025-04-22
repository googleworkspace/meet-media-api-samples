/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cpp/samples/testing/media_data.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "cpp/api/media_api_client_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"

namespace media_api_samples {

AudioTestData CreateAudioTestData(int num_samples) {
  std::vector<int16_t> pcm16(num_samples);
  for (int i = 0; i < num_samples; ++i) {
    pcm16[i] = i;
  }

  return AudioTestData{.frame = meet::AudioFrame{.pcm16 = pcm16},
                       .pcm16 = std::move(pcm16)};
}

VideoTestData CreateVideoTestData(int width, int height) {
  const int chroma_width = (width + 1) / 2;
  const int chroma_height = (height + 1) / 2;

  // Add padding to the plane buffers (by incrementing the stride) to test that
  // only `width` (for Y) or `chroma_width` (for U and V) bytes are read from
  // each plane.
  const int stride_y = width + 1;
  const int stride_u = chroma_width + 2;
  const int stride_v = chroma_width + 3;

  std::vector<char> output_data;
  output_data.reserve(height * stride_y + chroma_height * stride_u +
                      chroma_height * stride_v);

  webrtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(width, height, stride_y, stride_u, stride_v);
  // Initialize each plane's buffer to all zeros to test that the correct data
  // is read from each plane.
  memset(buffer->MutableDataY(), 0, stride_y * height);
  memset(buffer->MutableDataU(), 0, stride_u * chroma_height);
  memset(buffer->MutableDataV(), 0, stride_v * chroma_height);

  for (int i = 0; i < height; ++i) {
    // Leave the range from `width` to `stride_y` initialized to 0 to test that
    // only `width` bytes are read from the Y plane.
    for (int j = 0; j < width; ++j) {
      buffer->MutableDataY()[i * stride_y + j] = 1;
      output_data.push_back(1);
    }
  }

  for (int i = 0; i < chroma_height; ++i) {
    // Leave the range from `chroma_width` to `stride_u` initialized to 0 to
    // test only `chroma_width` bytes are read from the U planes.
    for (int j = 0; j < chroma_width; ++j) {
      buffer->MutableDataU()[i * stride_u + j] = 2;
      output_data.push_back(2);
    }
  }

  for (int i = 0; i < chroma_height; ++i) {
    // Leave the range from `chroma_width` to `stride_v` initialized to 0 to
    // test only `chroma_width` bytes are read from the U planes.
    for (int j = 0; j < chroma_width; ++j) {
      buffer->MutableDataV()[i * stride_v + j] = 3;
      output_data.push_back(3);
    }
  }

  webrtc::VideoFrame::Builder builder;
  builder.set_video_frame_buffer(buffer);
  auto webrtc_frame = std::make_unique<webrtc::VideoFrame>(builder.build());

  return VideoTestData{.meet_frame = meet::VideoFrame{.frame = *webrtc_frame},
                       .yuv_data = std::move(output_data),
                       .webrtc_frame = std::move(webrtc_frame)};
}

}  // namespace media_api_samples
