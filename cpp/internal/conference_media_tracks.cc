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

#include "cpp/internal/conference_media_tracks.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "cpp/api/media_api_client_interface.h"
#include "webrtc/api/rtp_packet_info.h"
#include "webrtc/api/rtp_packet_infos.h"
#include "webrtc/api/transport/rtp/rtp_source.h"
#include "webrtc/api/video/video_frame.h"

namespace meet {

void ConferenceAudioTrack::OnData(
    const void* audio_data, int bits_per_sample, int sample_rate,
    size_t number_of_channels, size_t number_of_frames,
    absl::optional<int64_t> absolute_capture_timestamp_ms) {
  if (bits_per_sample != 16) {
    LOG(ERROR) << "Unsupported bits per sample: " << bits_per_sample
               << ". Expected 16.";
    return;
  }

  // Audio data is expected to be in PCM format, where each sample is 16 bits.
  const auto* pcm_data = reinterpret_cast<const int16_t*>(audio_data);

  // Because one track may have multiple contributing sources multiplexed on
  // it, the receiver maintains an ordered list of contributing sources and
  // synchronization sources. Sources are in reverse chronological order (from
  // most recent to oldest).
  //
  // The most recent sources should be used for the audio frame that is
  // currently being processed.
  std::optional<uint32_t> most_recent_csrc;
  std::optional<uint32_t> most_recent_ssrc;
  // Meet sends a contributing source of `kLoudestSpeakerCsrc` to indicate the
  // loudest speaker. Knowing the loudest speaker can be useful, as it can be
  // used to determine which participant to prioritize when rendering audio or
  // video (although other methods may be used as well).
  bool is_from_loudest_speaker = false;
  for (const auto& rtp_source : receiver_->GetSources()) {
    if (rtp_source.source_type() == webrtc::RtpSourceType::CSRC) {
      if (rtp_source.source_id() == kLoudestSpeakerCsrc) {
        is_from_loudest_speaker = true;
      } else if (!most_recent_csrc.has_value()) {
        // Take the first CSRC that is not the loudest speaker because CSRCs are
        // ordered from most recent to oldest.
        most_recent_csrc = rtp_source.source_id();
      }
    } else if (rtp_source.source_type() == webrtc::RtpSourceType::SSRC &&
               !most_recent_ssrc.has_value()) {
      most_recent_ssrc = rtp_source.source_id();
    }
  }

  if (!most_recent_csrc.has_value() || !most_recent_ssrc.has_value()) {
    // Before real audio starts flowing, silent audio frames will be received.
    // These frames will not have a CSRC or SSRC. Because these frames will be
    // received frequently, log them at a lower level to avoid cluttering the
    // logs.
    //
    // However, this may still happen in error cases, so something should be
    // logged.
    if (!most_recent_csrc.has_value()) {
      VLOG(2) << "AudioFrame is missing CSRC for mid: " << mid_;
    }
    if (!most_recent_ssrc.has_value()) {
      VLOG(2) << "AudioFrame is missing SSRC for mid: " << mid_;
    }
    return;
  }

  // Audio data in PCM format is expected to be stored in a contiguous buffer,
  // where there are `number_of_channels * number_of_frames` audio frames.
  absl::Span<const int16_t> pcm_data_span =
      absl::MakeConstSpan(pcm_data, number_of_channels * number_of_frames);
  callback_(AudioFrame{.pcm16 = std::move(pcm_data_span),
                       .bits_per_sample = bits_per_sample,
                       .sample_rate = sample_rate,
                       .number_of_channels = number_of_channels,
                       .number_of_frames = number_of_frames,
                       .is_from_loudest_speaker = is_from_loudest_speaker,
                       .contributing_source = most_recent_csrc.value(),
                       .synchronization_source = most_recent_ssrc.value()});
};

void ConferenceVideoTrack::OnFrame(const webrtc::VideoFrame& frame) {
  const webrtc::RtpPacketInfos& packet_infos = frame.packet_infos();
  if (packet_infos.empty()) {
    LOG(ERROR) << "VideoFrame is missing packet infos for mid: " << mid_;
    return;
  }
  const webrtc::RtpPacketInfo& packet_info = packet_infos.front();
  if (packet_info.csrcs().empty()) {
    LOG(ERROR) << "VideoFrame is missing CSRC for mid: " << mid_;
    return;
  }

  callback_(VideoFrame{.frame = frame,
                       // It is expected that there will be only one CSRC per
                       // video frame.
                       .contributing_source = packet_info.csrcs().front(),
                       .synchronization_source = packet_info.ssrc()});
};

}  // namespace meet
