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

#ifndef CPP_INTERNAL_CONFERENCE_MEDIA_TRACKS_H_
#define CPP_INTERNAL_CONFERENCE_MEDIA_TRACKS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "absl/functional/any_invocable.h"
#include "absl/types/optional.h"
#include "meet_clients/api/media_api_client_interface.h"
#include "api/media_stream_interface.h"
#include "api/rtp_receiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

namespace meet {
// Meet uses this magic number to indicate the loudest speaker.
inline constexpr int kLoudestSpeakerCsrc = 42;

// Adapter class for webrtc::AudioTrackSinkInterface that converts
// webrtc::AudioFrames to meet::AudioFrame and calls the callback.
class ConferenceAudioTrack : public webrtc::AudioTrackSinkInterface {
 public:
  using AudioFrameCallback = absl::AnyInvocable<void(AudioFrame frame)>;

  ConferenceAudioTrack(
      std::string mid,
      webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      AudioFrameCallback callback)
      : mid_(std::move(mid)),
        receiver_(std::move(receiver)),
        callback_(std::move(callback)) {}

  void OnData(const void* audio_data, int bits_per_sample, int sample_rate,
              size_t number_of_channels, size_t number_of_frames,
              absl::optional<int64_t> absolute_capture_timestamp_ms) override;

 private:
  // Media line from the SDP offer/answer that identifies this track.
  std::string mid_;
  webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver_;
  AudioFrameCallback callback_;
};

// Adapter class for webrtc::VideoSinkInterface that converts
// webrtc::VideoFrames to meet::VideoFrames and calls the callback.
class ConferenceVideoTrack
    : public webrtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  using VideoFrameCallback = absl::AnyInvocable<void(VideoFrame frame)>;

  explicit ConferenceVideoTrack(std::string mid, VideoFrameCallback callback)
      : mid_(std::move(mid)), callback_(std::move(callback)) {}

  void OnFrame(const webrtc::VideoFrame& frame) override;

 private:
  // Media line from the SDP offer/answer that identifies this track.
  std::string mid_;
  VideoFrameCallback callback_;
};

// Convenience type for holding either an audio or video track.
using ConferenceMediaTrack =
    std::variant<std::unique_ptr<ConferenceAudioTrack>,
                 std::unique_ptr<ConferenceVideoTrack>>;

}  // namespace meet

#endif  // CPP_INTERNAL_CONFERENCE_MEDIA_TRACKS_H_
