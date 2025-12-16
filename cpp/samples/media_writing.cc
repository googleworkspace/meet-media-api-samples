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

#include "meet_clients/samples/media_writing.h"

#include <cstdint>
#include <vector>

#include "meet_clients/samples/output_writer_interface.h"
#include "api/video/video_frame_buffer.h"

namespace media_api_samples {

// This sample app writes media to files as a series of characters using
// `reinterpret_cast`. Because WebRTC internally will perform any appropriate
// endianness conversions, these casts are safe.
//
// See webrtc/rtc_base/byte_order.h.

void WritePcm16(const std::vector<int16_t>& pcm16,
                OutputWriterInterface& writer) {
  for (int16_t sample : pcm16) {
    writer.Write(reinterpret_cast<const char*>(&sample), sizeof(sample));
  }
}

void WriteYuv420(const webrtc::I420BufferInterface& i420,
                 OutputWriterInterface& writer) {
  int width = i420.width();
  int height = i420.height();
  // Chroma planes (U and V) are half the width and height of the luma plane
  // (Y).
  int chroma_width = (width + 1) / 2;    // Increment by 1 for rounding
  int chroma_height = (height + 1) / 2;  // Increment by 1 for rounding

  // When reading the Y, U, and V planes from their buffers, the stride for each
  // plane is expected to be greater than or equal to the width of the plane.
  // This is because `stride` is the width of the memory block, while `width` is
  // the width of the image.
  //
  // As a result, reading the planes works by advancing the pointer by `stride`
  // each time but only reading `width` bytes from that pointer.

  // Write Y plane (luma plane).
  const uint8_t* y_plane = i420.DataY();
  int y_stride = i420.StrideY();
  for (int i = 0; i < height; ++i) {
    writer.Write(reinterpret_cast<const char*>(y_plane + i * y_stride), width);
  }

  // Write U plane (first chroma plane).
  const uint8_t* u_plane = i420.DataU();
  int u_stride = i420.StrideU();
  for (int i = 0; i < chroma_height; ++i) {
    writer.Write(reinterpret_cast<const char*>(u_plane + i * u_stride),
                 chroma_width);
  }

  // Write V plane (second chroma plane).
  const uint8_t* v_plane = i420.DataV();
  int v_stride = i420.StrideV();
  for (int i = 0; i < chroma_height; ++i) {
    writer.Write(reinterpret_cast<const char*>(v_plane + i * v_stride),
                 chroma_width);
  }
}

}  // namespace media_api_samples
