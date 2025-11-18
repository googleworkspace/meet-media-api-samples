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

#include "cpp/samples/multi_user_media_collector.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/api/media_entries_resource.h"
#include "cpp/api/participants_resource.h"
#include "cpp/samples/media_writing.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/video_frame_buffer.h"

namespace media_api_samples {
namespace {

constexpr absl::string_view kTmpAudioFormat = "%saudio_%s_tmp.pcm";
constexpr absl::string_view kTmpVideoFormat = "%svideo_%s_tmp_%dx%d.yuv";
constexpr absl::string_view kFinishedAudioFormat = "%saudio_%s_%s_%s.pcm";
constexpr absl::string_view kFinishedVideoFormat = "%svideo_%s_%s_%s_%dx%d.yuv";

}  // namespace

void MultiUserMediaCollector::OnAudioFrame(meet::AudioFrame frame) {
  absl::Time received_time = absl::Now();
  std::vector<int16_t> samples(frame.pcm16.begin(), frame.pcm16.end());

  collector_thread_->PostTask([this, samples = std::move(samples),
                               contributing_source = frame.contributing_source,
                               received_time = received_time] {
    HandleAudioData(std::move(samples), contributing_source, received_time);
  });
}

void MultiUserMediaCollector::OnVideoFrame(meet::VideoFrame frame) {
  absl::Time received_time = absl::Now();
  rtc::scoped_refptr<webrtc::I420BufferInterface> buffer =
      frame.frame.video_frame_buffer()->ToI420();

  collector_thread_->PostTask([this, buffer = std::move(buffer),
                               contributing_source = frame.contributing_source,
                               received_time = received_time] {
    HandleVideoData(std::move(buffer), contributing_source, received_time);
  });
}

void MultiUserMediaCollector::HandleAudioData(std::vector<int16_t> samples,
                                              uint32_t contributing_source,
                                              absl::Time received_time) {
  DCHECK(collector_thread_->IsCurrent());

  AudioSegment* audio_segment = nullptr;

  if (auto it = audio_segments_.find(contributing_source);
      it != audio_segments_.end()) {
    AudioSegment* current_audio_segment = it->second.get();
    if (received_time - current_audio_segment->last_frame_time <
        segment_gap_threshold_) {
      // Reuse the existing segment if the received frame is within the gap of
      // the previous frame.
      audio_segment = current_audio_segment;
      // TODO: Make this heuristic calculation more testable.
      audio_segment->last_frame_time = received_time;
    } else {
      // If there is an existing segment, but the received frame is beyond the
      // gap of the previous frame, close the existing segment.
      CloseAudioSegment(*current_audio_segment);
      audio_segments_.erase(it);
    }
  }

  if (audio_segment == nullptr) {
    // If there is no existing segment (because one did not exist or the
    // previous segment was closed), create a new segment.
    absl::StatusOr<std::string> file_identifier_status =
        resource_manager_->GetOutputFileIdentifier(contributing_source);
    if (!file_identifier_status.ok()) {
      // It is expected that resource updates will not be available for a short
      // period of time while a participant is joining. Therefore, missing a
      // file identifier is not always an error.
      //
      // However, this can be an error, so log it in a way that is easy to
      // filter out.
      VLOG(1) << "No audio file identifier found for contributing source "
              << contributing_source << ": "
              << file_identifier_status.status().message();
      return;
    }

    std::string file_identifier = std::move(file_identifier_status).value();
    auto new_audio_segment = std::make_unique<AudioSegment>(
        output_writer_provider_(absl::StrFormat(
            kTmpAudioFormat, output_file_prefix_, file_identifier)),
        std::move(file_identifier), received_time, received_time);
    audio_segment = new_audio_segment.get();
    audio_segments_[contributing_source] = std::move(new_audio_segment);
  }

  DCHECK(audio_segment != nullptr);
  // At this point, either an existing segment is being appended to or a new
  // segment has been created.
  WritePcm16(samples, *audio_segment->writer);
}

void MultiUserMediaCollector::HandleVideoData(
    rtc::scoped_refptr<webrtc::I420BufferInterface> buffer,
    uint32_t contributing_source, absl::Time received_time) {
  DCHECK(collector_thread_->IsCurrent());

  // Meet video frames are always in YUV420p format.
  const webrtc::I420BufferInterface* i420 = buffer->GetI420();
  if (i420 == nullptr) {
    LOG(ERROR) << "Failed to get I420 buffer from video frame buffer.";
    return;
  }

  VideoSegment* video_segment = nullptr;

  if (auto it = video_segments_.find(contributing_source);
      it != video_segments_.end()) {
    VideoSegment* current_video_segment = it->second.get();
    if (received_time - current_video_segment->last_frame_time <
            segment_gap_threshold_ &&
        current_video_segment->width == i420->width() &&
        current_video_segment->height == i420->height()) {
      // Reuse the existing segment if the received frame is within the gap of
      // the previous frame and the resolution is the same.
      video_segment = current_video_segment;
      // TODO: Make this heuristic calculation more testable.
      video_segment->last_frame_time = received_time;
    } else {
      // If there is an existing segment, but the received frame is beyond the
      // gap of the previous frame or the resolution is different, close the
      // existing segment.
      CloseVideoSegment(*current_video_segment);
      video_segments_.erase(it);
    }
  }

  if (video_segment == nullptr) {
    // If there is no existing segment (because one did not exist or the
    // previous segment was closed), create a new segment.
    absl::StatusOr<std::string> file_identifier_status =
        resource_manager_->GetOutputFileIdentifier(contributing_source);
    if (!file_identifier_status.ok()) {
      // It is expected that resource updates will not be available for a short
      // period of time while a participant is joining. Therefore, missing a
      // file identifier is not always an error.
      //
      // However, this can be an error, so log it in a way that is easy to
      // filter out.
      VLOG(1) << "No video file identifier found for contributing source "
              << contributing_source << ": "
              << file_identifier_status.status().message();
      return;
    }

    std::string file_identifier = std::move(file_identifier_status).value();
    std::string video_segment_name =
        absl::StrFormat(kTmpVideoFormat, output_file_prefix_, file_identifier,
                        buffer->width(), buffer->height());
    auto new_video_segment = std::make_unique<VideoSegment>(
        output_writer_provider_(std::move(video_segment_name)),
        std::move(file_identifier), buffer->width(), buffer->height(),
        received_time, received_time);
    video_segment = new_video_segment.get();
    video_segments_[contributing_source] = std::move(new_video_segment);
  }

  DCHECK(video_segment != nullptr);
  // At this point, either an existing segment is being appended to or a new
  // segment has been created.
  WriteYuv420(*i420, *video_segment->writer);
}

void MultiUserMediaCollector::OnResourceUpdate(meet::ResourceUpdate update) {
  absl::Time received_time = absl::Now();
  collector_thread_->PostTask(
      [this, update = std::move(update), received_time = received_time] {
        if (std::holds_alternative<meet::MediaEntriesChannelToClient>(update)) {
          resource_manager_->OnMediaEntriesResourceUpdate(
              std::move(std::get<meet::MediaEntriesChannelToClient>(update)),
              received_time);
        } else if (std::holds_alternative<meet::ParticipantsChannelToClient>(
                       update)) {
          resource_manager_->OnParticipantResourceUpdate(
              std::move(std::get<meet::ParticipantsChannelToClient>(update)),
              received_time);
        }
      });
}

void MultiUserMediaCollector::OnDisconnected(absl::Status status) {
  // The `MediaApiClient` will only call this method once.
  DCHECK(!disconnect_notification_.HasBeenNotified());

  LOG(INFO) << "MultiUserMediaCollector::OnDisconnected " << status;
  collector_thread_->PostTask([this, status = std::move(status)] {
    disconnect_status_ = std::move(status);
    for (auto& [contributing_source, audio_segment] : audio_segments_) {
      CloseAudioSegment(*audio_segment);
    }
    audio_segments_.clear();
    for (auto& [contributing_source, video_segment] : video_segments_) {
      CloseVideoSegment(*video_segment);
    }
    video_segments_.clear();

    disconnect_notification_.Notify();

    if (!join_notification_.HasBeenNotified()) {
      // This notification is used to break out of the join/leave wait loop in
      // the sample. We only want to notify if we have not joined the
      // conference. If we have, then the user will have been notified by the
      // disconnect notification.
      join_notification_.Notify();
    }
  });
}

void MultiUserMediaCollector::CloseAudioSegment(AudioSegment& audio_segment) {
  DCHECK(collector_thread_->IsCurrent());

  audio_segment.writer->Close();
  segment_renamer_(
      absl::StrFormat(kTmpAudioFormat, output_file_prefix_,
                      audio_segment.file_identifier),
      absl::StrFormat(kFinishedAudioFormat, output_file_prefix_,
                      audio_segment.file_identifier,
                      absl::FormatTime(audio_segment.first_frame_time),
                      absl::FormatTime(audio_segment.last_frame_time)));
}

void MultiUserMediaCollector::CloseVideoSegment(VideoSegment& video_segment) {
  DCHECK(collector_thread_->IsCurrent());

  video_segment.writer->Close();
  segment_renamer_(
      absl::StrFormat(kTmpVideoFormat, output_file_prefix_,
                      video_segment.file_identifier, video_segment.width,
                      video_segment.height),
      absl::StrFormat(kFinishedVideoFormat, output_file_prefix_,
                      video_segment.file_identifier,
                      absl::FormatTime(video_segment.first_frame_time),
                      absl::FormatTime(video_segment.last_frame_time),
                      video_segment.width, video_segment.height));
}

}  // namespace media_api_samples
