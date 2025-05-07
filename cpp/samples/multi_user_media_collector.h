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

#ifndef CPP_SAMPLES_MULTI_USER_MEDIA_COLLECTOR_H_
#define CPP_SAMPLES_MULTI_USER_MEDIA_COLLECTOR_H_

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/samples/output_file.h"
#include "cpp/samples/output_writer_interface.h"
#include "cpp/samples/resource_manager.h"
#include "cpp/samples/resource_manager_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/video_frame_buffer.h"
#include "webrtc/rtc_base/thread.h"

// TODO: Add ABSL_POINTERS_DEFAULT_NONNULL and
// ABSL_REQUIRE_EXPLICIT_INIT once absl can be bumped to a version that supports
// it.

namespace media_api_samples {

// A basic media collector that collects audio and video streams from the
// conference.
//
// Audio and video streams are logically broken up into media "segments", where
// a single participant's uninterrupted audio or video stream is considered a
// single segment. Therefore, a participant may have multiple segments over the
// course of a conference, especially if there are many participants or if the
// participant mutes and unmutes.
//
// This class will write ongoing audio and video segments to files of the
// format:
//
// Audio:
//   <output_file_prefix>audio_<participant_identifiers>_tmp.pcm
// Video:
//   <output_file_prefix>video_<participant_identifiers>_tmp_<width>x<height>.yuv
//
// Once a segment is finished, the `tmp` token will be replaced with the start
// and end times of the segment:
//
// Audio:
//   <output_file_prefix>audio_<participant_identifiers>_<start_time>_<end_time>.pcm
// Video:
//   <output_file_prefix>video_<participant_identifiers>_<start_time>_<end_time>_<width>x<height>.yuv
//
// For video segments, the resolution of the segment will also be included in
// the file name. The resolution is appended to the end of the file name so that
// files are lexicographically ordered by display name and start time.
//
// `participant_identifiers` is a string that uniquely identifies the media
// stream. This is handled by the participant manager implementation.
class MultiUserMediaCollector : public meet::MediaApiClientObserverInterface {
 public:
  // Lambda for renaming media segments when they are closed.
  using SegmentRenamer =
      absl::AnyInvocable<void(/*tmp_name=*/absl::string_view,
                              /*timestamped_name=*/absl::string_view)>;

  // Default constructor that writes media to real files and uses a real
  // participant manager.
  MultiUserMediaCollector(absl::string_view output_file_prefix,
                          absl::Duration segment_gap_threshold,
                          std::unique_ptr<rtc::Thread> collector_thread)
      : output_file_prefix_(output_file_prefix),
        segment_gap_threshold_(segment_gap_threshold),
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
    segment_renamer_ = [](absl::string_view tmp_file_name,
                          absl::string_view finished_file_name) {
      std::rename(tmp_file_name.data(), finished_file_name.data());
    };
    resource_manager_ =
        std::make_unique<ResourceManager>(output_writer_provider_(
            absl::StrCat(output_file_prefix_, "event_log.csv")));
  }

  // Constructor that allows injecting dependencies for testing.
  MultiUserMediaCollector(
      absl::string_view output_file_prefix,
      OutputWriterProvider output_writer_provider,
      SegmentRenamer segment_renamer, absl::Duration segment_gap_threshold,
      std::unique_ptr<ResourceManagerInterface> resource_manager,
      std::unique_ptr<rtc::Thread> collector_thread)
      : output_file_prefix_(output_file_prefix),
        output_writer_provider_(std::move(output_writer_provider)),
        segment_renamer_(std::move(segment_renamer)),
        segment_gap_threshold_(segment_gap_threshold),
        resource_manager_(std::move(resource_manager)),
        collector_thread_(std::move(collector_thread)) {}

  ~MultiUserMediaCollector() override {
    // Stop the thread to ensure that enqueued tasks do not access member fields
    // after they have been destroyed.
    collector_thread_->Stop();
  }

  void OnAudioFrame(meet::AudioFrame frame) override;
  void OnVideoFrame(meet::VideoFrame frame) override;
  void OnResourceUpdate(meet::ResourceUpdate update) override;

  void OnJoined() override {
    // The `MediaApiClient` will only call this method once.
    DCHECK(!join_notification_.HasBeenNotified());

    LOG(INFO) << "MultiUserMediaCollector::OnJoined";
    join_notification_.Notify();
  }
  void OnDisconnected(absl::Status status) override;

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

 private:
  using ContributingSource = uint32_t;

  // Audio and video streams are logically broken up into media "segments".
  //
  // The first time a frame is received for a particular contributing source,
  // a new segment is created. This means that a participant may have multiple
  // segments if they are streaming audio and video.
  //
  // Segments end when either:
  // 1. A frame is received after a sufficiently long time from the last frame.
  //    As this app is meant to be demonstrative, a simple heuristic is used,
  //    specified by `segment_gap_threshold_`. That is to say, a new segment
  //    will begin if a frame is received more than `segment_gap_threshold_`
  //    from the last frame. This approach is meant to account for muting,
  //    contributing sources being switched, and other scenarios where a break
  //    in a participant's media stream occurred.
  // 2. The media collector is disconnected.
  // 3. For video segments, segments also end when a frame is received that has
  //    a different resolution than the current segment.
  struct AudioSegment {
    std::unique_ptr<OutputWriterInterface> writer;
    std::string file_identifier;
    absl::Time first_frame_time;
    absl::Time last_frame_time;
  };
  struct VideoSegment {
    std::unique_ptr<OutputWriterInterface> writer;
    std::string file_identifier;
    int width = 0;
    int height = 0;
    absl::Time first_frame_time;
    absl::Time last_frame_time;
  };

  void HandleAudioData(std::vector<int16_t> samples,
                       ContributingSource contributing_source,
                       absl::Time received_time);
  void HandleVideoData(
      rtc::scoped_refptr<webrtc::I420BufferInterface> buffer,
      ContributingSource contributing_source, absl::Time received_time);

  // Closes the audio or video segment. This will rename the file to include
  // the start and end times of the segment.
  void CloseAudioSegment(AudioSegment& audio_segment);
  void CloseVideoSegment(VideoSegment& video_segment);

  std::string output_file_prefix_;
  OutputWriterProvider output_writer_provider_;
  SegmentRenamer segment_renamer_;
  // If a media frame is received more than `segment_gap_threshold_` after
  // the previous frame for a given segment, a new media segment will be
  // created and the previous segment will be closed.
  absl::Duration segment_gap_threshold_;

  // Maps from contributing source to the current audio or video segment for
  // that source.
  //
  // Values in these maps are never null.
  // TODO: Remove comment once nullability annotations are added.
  absl::flat_hash_map<ContributingSource, std::unique_ptr<AudioSegment>>
      audio_segments_;
  absl::flat_hash_map<ContributingSource, std::unique_ptr<VideoSegment>>
      video_segments_;

  std::unique_ptr<ResourceManagerInterface> resource_manager_;

  absl::Notification join_notification_;
  absl::Notification disconnect_notification_;

  // The media collector's internal thread. Used for moving work off of the
  // MediaApiClient's threads and synchronizing access to member variables.
  std::unique_ptr<rtc::Thread> collector_thread_;
};

}  // namespace media_api_samples

#endif  // CPP_SAMPLES_MULTI_USER_MEDIA_COLLECTOR_H_
