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

#ifndef CPP_SAMPLES_MEDIA_WRITING_H_
#define CPP_SAMPLES_MEDIA_WRITING_H_

#include <cstdint>
#include <vector>

#include "meet_clients/samples/output_writer_interface.h"
#include "api/video/video_frame_buffer.h"

namespace media_api_samples {

// Writes a PCM16 buffer to the output writer.
void WritePcm16(const std::vector<int16_t>& pcm16,
                OutputWriterInterface& writer);

// Writes a YUV420p buffer to the output writer.
void WriteYuv420(const webrtc::I420BufferInterface& i420,
                 OutputWriterInterface& writer);

}  // namespace media_api_samples

#endif  // CPP_SAMPLES_MEDIA_WRITING_H_
