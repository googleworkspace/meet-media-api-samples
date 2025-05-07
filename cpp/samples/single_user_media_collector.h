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

#ifndef CPP_SAMPLES_SINGLE_USER_MEDIA_COLLECTOR_H_
#define CPP_SAMPLES_SINGLE_USER_MEDIA_COLLECTOR_H_

#include <cstdint>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/samples/output_file.h"
#include "cpp/samples/output_writer_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/video_frame_buffer.h"
#include "webrtc/rtc_base/thread.h"

// TODO: Add ABSL_POINTERS_DEFAULT_NONNULL once absl can be bumped
// to a version that supports it.

namespace media_api_samples {

// A basic media collector that collects audio and video streams from the
// conference. This is primarily useful for experimenting with media processing
// without having to worry about managing participant metadata. All audio and
// video frames are handled without checking their sources; therefore, this
// collector is best used for collecting data in a conference with a single
// participant.
class SingleUserMediaCollector : public meet::MediaApiClientObserverInterface {
 public:
  // Default constructor that writes media to real files.
  SingleUserMediaCollector(absl::string_view output_file_prefix,
                           std::unique_ptr<rtc::Thread> collector_thread)
      : output_file_prefix_(output_file_prefix),
        collector_thread_(std::move(collector_thread)) {
    output_writer_provider_ = [](absl::string_view file_name) {
      std::ofstream file(std::string(file_name),
                         std::ios::binary | std::ios::out | std::ios::trunc);
      if (file.is_open()) {
        LOG(INFO) << "Opened file: " << file_name;
      } else {
        // Files should normally open successfully.
        //
        // Potential causes for failure include:
        // - The parent directory does not exist.
        // - The system is out of disk space.
        //
        // If a file cannot be opened, the sample will still run, but written
        // data will be lost.
        LOG(ERROR) << "Failed to open file: " << file_name;
      }
      return std::make_unique<OutputFile>(std::move(file));
    };
  }

  // Constructor that allows injecting a custom writer provider for testing.
  SingleUserMediaCollector(absl::string_view output_file_prefix,
                           std::unique_ptr<rtc::Thread> collector_thread,
                           OutputWriterProvider output_writer_provider)
      : output_file_prefix_(output_file_prefix),
        output_writer_provider_(std::move(output_writer_provider)),
        collector_thread_(std::move(collector_thread)) {}

  ~SingleUserMediaCollector() override {
    // Stop the thread to ensure that enqueued tasks do not access member fields
    // after they have been destroyed.
    collector_thread_->Stop();
  }

  void OnResourceUpdate(meet::ResourceUpdate update) override {
    // This sample does not handle resource updates.
  }

  void OnJoined() override {
    LOG(INFO) << "SingleUserMediaCollector::OnJoined";
    join_notification_.Notify();
  }
  void OnDisconnected(absl::Status status) override {
    LOG(INFO) << "SingleUserMediaCollector::OnDisconnected " << status;
    disconnect_notification_.Notify();
  }

  absl::Status WaitForJoined(absl::Duration timeout) {
    if (!join_notification_.WaitForNotificationWithTimeout(timeout)) {
      return absl::DeadlineExceededError(
          "Timed out waiting for joined notification");
    }
    return absl::OkStatus();
  }
  absl::Status WaitForDisconnected(absl::Duration timeout) {
    if (!disconnect_notification_.WaitForNotificationWithTimeout(timeout)) {
      return absl::DeadlineExceededError(
          "Timed out waiting for disconnected notification");
    }
    return absl::OkStatus();
  }

  void OnAudioFrame(meet::AudioFrame frame) override;
  void OnVideoFrame(meet::VideoFrame frame) override;

 private:
  struct VideoSegment {
    int segment_number;
    int width;
    int height;
    std::unique_ptr<OutputWriterInterface> writer;
  };

  void HandleAudioBuffer(std::vector<int16_t> pcm16);
  void HandleVideoBuffer(
      rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer);

  std::string output_file_prefix_;
  OutputWriterProvider output_writer_provider_;
  // Audio writer for all audio frames.
  //
  // The audio writer is created when the first audio frame is received. Audio
  // format does not change, so a single writer can be used for all audio
  // frames.
  /*absl_nullable*/ std::unique_ptr<OutputWriterInterface> audio_writer_;
  // The current video segment, or nullptr if no video frames have been received
  // yet.
  //
  // The first video segment is created when the first video frame is received.
  // If the video frame size changes, a new video segment is created.
  /*absl_nullable*/ std::unique_ptr<VideoSegment> video_segment_;

  absl::Notification join_notification_;
  absl::Notification disconnect_notification_;

  // The media collector's internal thread. Used for moving work off of the
  // MediaApiClient's threads and synchronizing access to member variables.
  std::unique_ptr<rtc::Thread> collector_thread_;
};

}  // namespace media_api_samples

#endif  // CPP_SAMPLES_SINGLE_USER_MEDIA_COLLECTOR_H_
