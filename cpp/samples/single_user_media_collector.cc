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

#include "meet_clients/samples/single_user_media_collector.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "meet_clients/api/media_api_client_interface.h"
#include "meet_clients/samples/media_writing.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame_buffer.h"
#include "rtc_base/thread.h"

// TODO: Add ABSL_POINTERS_DEFAULT_NONNULL once absl can be bumped
// to a version that supports it.

namespace media_api_samples {

void SingleUserMediaCollector::OnAudioFrame(meet::AudioFrame frame) {
  // Copy the audio frame, since the frame is simply a view into an audio
  // buffer.
  std::vector<int16_t> pcm16(frame.pcm16.begin(), frame.pcm16.end());

  // Move audio processing to a separate thread since `OnAudioFrame`
  // implementations should move expensive work to a separate thread.
  collector_thread_->PostTask([this, pcm16 = std::move(pcm16)] {
    HandleAudioBuffer(std::move(pcm16));
  });
}

void SingleUserMediaCollector::OnVideoFrame(meet::VideoFrame frame) {
  // Move video processing to a separate thread since `OnVideoFrame`
  // implementations should move expensive work to a separate thread.
  collector_thread_->PostTask(
      [this, buffer = frame.frame.video_frame_buffer()] {
        HandleVideoBuffer(std::move(buffer));
      });
}

void SingleUserMediaCollector::HandleAudioBuffer(std::vector<int16_t> pcm16) {
  DCHECK(collector_thread_->IsCurrent());

  if (audio_writer_ == nullptr) {
    std::string audio_output_file_name =
        absl::StrCat(output_file_prefix_, "audio.pcm");

    LOG(INFO) << "Creating audio file: " << audio_output_file_name;
    audio_writer_ = output_writer_provider_(audio_output_file_name);
  }

  WritePcm16(pcm16, *audio_writer_);
}

void SingleUserMediaCollector::HandleVideoBuffer(
    webrtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer) {
  DCHECK(collector_thread_->IsCurrent());

  // Meet video frames are always in YUV420p format.
  const webrtc::I420BufferInterface* i420 = buffer->GetI420();
  if (i420 == nullptr) {
    LOG(ERROR) << "Failed to get I420 buffer from video frame buffer.";
    return;
  }

  // If the video frame size changes, or if this is the first video frame,
  // create a new video file.
  if (video_segment_ == nullptr || video_segment_->width != i420->width() ||
      video_segment_->height != i420->height()) {
    int segment_number =
        video_segment_ == nullptr ? 0 : video_segment_->segment_number + 1;
    std::string video_output_file_name =
        absl::StrCat(output_file_prefix_, "video_", segment_number, "_",
                     i420->width(), "x", i420->height(), ".yuv");

    LOG(INFO) << "Creating video file: " << video_output_file_name;
    video_segment_ = std::make_unique<VideoSegment>(
        segment_number, i420->width(), i420->height(),
        output_writer_provider_(video_output_file_name));
  }

  WriteYuv420(*i420, *video_segment_->writer);
}

}  // namespace media_api_samples
