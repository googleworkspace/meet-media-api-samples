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

#ifndef CPP_SAMPLES_TESTING_MEDIA_DATA_H_
#define CPP_SAMPLES_TESTING_MEDIA_DATA_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "meet_clients/api/media_api_client_interface.h"
#include "api/video/video_frame.h"

// TODO: Add ABSL_POINTERS_DEFAULT_NONNULL once absl can be bumped
// to a version that supports it.

namespace media_api_samples {

struct AudioTestData {
  // The input frame that is sent to the media collector.
  meet::AudioFrame frame;
  // The expected content that should be written to the output writer.
  std::vector<int16_t> pcm16;
};

struct VideoTestData {
  // The input frame that is sent to the media collector.
  meet::VideoFrame meet_frame;
  // The expected content that should be written to the output writer.
  std::vector<char> yuv_data;
  // A reference to this object is stored in `meet_frame`. Therefore, this field
  // keeps the object alive until the `meet_frame` is no longer needed.
  std::unique_ptr<webrtc::VideoFrame> webrtc_frame;
};

// Creates input audio and video frames, and the expected output for those
// frames.
AudioTestData CreateAudioTestData(int num_samples);
VideoTestData CreateVideoTestData(int width, int height);

}  // namespace media_api_samples

#endif  // CPP_SAMPLES_TESTING_MEDIA_DATA_H_
