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

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "cpp/api/media_api_client_interface.h"
#include "webrtc/api/rtp_packet_info.h"
#include "webrtc/api/rtp_packet_infos.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/test/mock_rtpreceiver.h"
#include "webrtc/api/transport/rtp/rtp_source.h"
#include "webrtc/api/units/timestamp.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"

namespace meet {
namespace {

using ::base_logging::ERROR;
using ::base_logging::INFO;
using ::testing::_;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ScopedMockLog;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

TEST(ConferenceAudioTrackTest, CallsObserverWithAudioFrameFromLoudestSpeaker) {
  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver(
      new webrtc::MockRtpReceiver());
  // Expect that only the first CSRC and SSRC are used.
  //
  // WebRTC returns the sources list ordered from most recent to oldest.
  webrtc::RtpSource csrc_rtp_source_1(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/111, webrtc::RtpSourceType::CSRC,
      /*rtp_timestamp=*/1111111,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  webrtc::RtpSource csrc_rtp_source_2(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/222, webrtc::RtpSourceType::CSRC,
      /*rtp_timestamp=*/1111111,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  webrtc::RtpSource loudest_speaker_csrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/kLoudestSpeakerCsrc, webrtc::RtpSourceType::CSRC,
      /*rtp_timestamp=*/1111111,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  webrtc::RtpSource ssrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/333, webrtc::RtpSourceType::SSRC,
      /*rtp_timestamp=*/2222222,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  webrtc::RtpSource ssrc_rtp_source_2(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/444, webrtc::RtpSourceType::SSRC,
      /*rtp_timestamp=*/2222222,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  EXPECT_CALL(*mock_receiver, GetSources)
      .WillOnce(Return(std::vector<webrtc::RtpSource>{
          std::move(csrc_rtp_source_1), std::move(csrc_rtp_source_2),
          std::move(loudest_speaker_csrc_rtp_source),
          std::move(ssrc_rtp_source), std::move(ssrc_rtp_source_2)}));
  MockFunction<void(AudioFrame)> mock_function;
  std::optional<AudioFrame> received_frame;
  EXPECT_CALL(mock_function, Call)
      .WillOnce([&received_frame](AudioFrame frame) {
        received_frame = std::move(frame);
      });
  ConferenceAudioTrack audio_track("mid", mock_receiver,
                                   mock_function.AsStdFunction());
  int16_t pcm_data[2 * 100];

  audio_track.OnData(pcm_data,
                     /*bits_per_sample=*/16,
                     /*sample_rate=*/48000,
                     /*number_of_channels=*/2,
                     /*number_of_frames=*/100,
                     /*absolute_capture_timestamp_ms=*/std::nullopt);

  ASSERT_TRUE(received_frame.has_value());
  EXPECT_THAT(received_frame->pcm16, SizeIs(100 * 2));
  EXPECT_EQ(received_frame->bits_per_sample, 16);
  EXPECT_EQ(received_frame->sample_rate, 48000);
  EXPECT_EQ(received_frame->number_of_channels, 2);
  EXPECT_EQ(received_frame->number_of_frames, 100);
  EXPECT_TRUE(received_frame->is_from_loudest_speaker);
  EXPECT_EQ(received_frame->contributing_source, 111);
  EXPECT_EQ(received_frame->synchronization_source, 333);
}

TEST(ConferenceAudioTrackTest,
     CallsObserverWithAudioFrameFromNonLoudestSpeaker) {
  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver(
      new webrtc::MockRtpReceiver());
  webrtc::RtpSource csrc_rtp_source_1(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/111, webrtc::RtpSourceType::CSRC,
      /*rtp_timestamp=*/1111111,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  webrtc::RtpSource csrc_rtp_source_2(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/222, webrtc::RtpSourceType::CSRC,
      /*rtp_timestamp=*/1111111,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  webrtc::RtpSource ssrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/333, webrtc::RtpSourceType::SSRC,
      /*rtp_timestamp=*/2222222,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  webrtc::RtpSource ssrc_rtp_source_2(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/444, webrtc::RtpSourceType::SSRC,
      /*rtp_timestamp=*/2222222,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  EXPECT_CALL(*mock_receiver, GetSources)
      .WillOnce(Return(std::vector<webrtc::RtpSource>{
          std::move(csrc_rtp_source_1), std::move(csrc_rtp_source_2),
          std::move(ssrc_rtp_source), std::move(ssrc_rtp_source_2)}));
  MockFunction<void(AudioFrame)> mock_function;
  std::optional<AudioFrame> received_frame;
  EXPECT_CALL(mock_function, Call)
      .WillOnce([&received_frame](AudioFrame frame) {
        received_frame = std::move(frame);
      });
  ConferenceAudioTrack audio_track("mid", mock_receiver,
                                   mock_function.AsStdFunction());
  int16_t pcm_data[2 * 100];

  audio_track.OnData(pcm_data,
                     /*bits_per_sample=*/16,
                     /*sample_rate=*/48000,
                     /*number_of_channels=*/2,
                     /*number_of_frames=*/100,
                     /*absolute_capture_timestamp_ms=*/std::nullopt);

  ASSERT_TRUE(received_frame.has_value());
  EXPECT_THAT(received_frame->pcm16, SizeIs(100 * 2));
  EXPECT_EQ(received_frame->bits_per_sample, 16);
  EXPECT_EQ(received_frame->sample_rate, 48000);
  EXPECT_EQ(received_frame->number_of_channels, 2);
  EXPECT_EQ(received_frame->number_of_frames, 100);
  EXPECT_FALSE(received_frame->is_from_loudest_speaker);
  EXPECT_EQ(received_frame->contributing_source, 111);
  EXPECT_EQ(received_frame->synchronization_source, 333);
}

TEST(ConferenceAudioTrackTest, LogsErrorWithUnsupportedBitsPerSample) {
  ConferenceAudioTrack audio_track("mid", nullptr, [](AudioFrame /*frame*/) {});
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(ERROR, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();
  int16_t pcm_data[2 * 100];

  audio_track.OnData(pcm_data,
                     /*bits_per_sample=*/8,
                     /*sample_rate=*/48000,
                     /*number_of_channels=*/2,
                     /*number_of_frames=*/100,
                     /*absolute_capture_timestamp_ms=*/std::nullopt);

  EXPECT_EQ(message, "Unsupported bits per sample: 8. Expected 16.");
}

TEST(ConferenceAudioTrackTest, LogsErrorWithMissingCsrc) {
  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver(
      new webrtc::MockRtpReceiver());
  webrtc::RtpSource ssrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/333, webrtc::RtpSourceType::SSRC,
      /*rtp_timestamp=*/2222222,
      {.audio_level = 0, .absolute_capture_time = std::nullopt});
  EXPECT_CALL(*mock_receiver, GetSources)
      .WillOnce(
          Return(std::vector<webrtc::RtpSource>{std::move(ssrc_rtp_source)}));
  ConferenceAudioTrack audio_track("mid", mock_receiver,
                                   [](AudioFrame /*frame*/) {});
  ScopedMockLog log(kDoNotCaptureLogsYet);
  absl::SetVLogLevel("conference_media_tracks", 2);
  std::string message;
  EXPECT_CALL(log, Log(INFO, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();
  int16_t pcm_data[2 * 100];

  audio_track.OnData(pcm_data,
                     /*bits_per_sample=*/16,
                     /*sample_rate=*/48000,
                     /*number_of_channels=*/2,
                     /*number_of_frames=*/100,
                     /*absolute_capture_timestamp_ms=*/std::nullopt);

  EXPECT_EQ(message, "AudioFrame is missing CSRC for mid: mid");
}

TEST(ConferenceAudioTrackTest, LogsErrorWithMissingSsrc) {
  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver(
      new webrtc::MockRtpReceiver());
  webrtc::RtpSource csrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/111, webrtc::RtpSourceType::CSRC,
      /*rtp_timestamp=*/1111111,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  EXPECT_CALL(*mock_receiver, GetSources)
      .WillOnce(
          Return(std::vector<webrtc::RtpSource>{std::move(csrc_rtp_source)}));
  ConferenceAudioTrack audio_track("mid", mock_receiver,
                                   [](AudioFrame /*frame*/) {});
  ScopedMockLog log(kDoNotCaptureLogsYet);
  absl::SetVLogLevel("conference_media_tracks", 2);
  std::string message;
  EXPECT_CALL(log, Log(INFO, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();
  int16_t pcm_data[2 * 100];

  audio_track.OnData(pcm_data,
                     /*bits_per_sample=*/16,
                     /*sample_rate=*/48000,
                     /*number_of_channels=*/2,
                     /*number_of_frames=*/100,
                     /*absolute_capture_timestamp_ms=*/std::nullopt);

  EXPECT_EQ(message, "AudioFrame is missing SSRC for mid: mid");
}
TEST(ConferenceAudioTrackTest, LogsErrorWithMissingCsrcAndSsrc) {
  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver(
      new webrtc::MockRtpReceiver());
  EXPECT_CALL(*mock_receiver, GetSources)
      .WillOnce(Return(std::vector<webrtc::RtpSource>()));
  ConferenceAudioTrack audio_track("mid", mock_receiver,
                                   [](AudioFrame /*frame*/) {});
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::vector<std::string> messages;
  absl::SetVLogLevel("conference_media_tracks", 2);
  EXPECT_CALL(log, Log(INFO, _, _))
      .Times(2)
      .WillRepeatedly(
          [&messages](int, const std::string &, const std::string &msg) {
            messages.push_back(msg);
          });
  log.StartCapturingLogs();
  int16_t pcm_data[2 * 100];

  audio_track.OnData(pcm_data,
                     /*bits_per_sample=*/16,
                     /*sample_rate=*/48000,
                     /*number_of_channels=*/2,
                     /*number_of_frames=*/100,
                     /*absolute_capture_timestamp_ms=*/std::nullopt);

  EXPECT_THAT(messages,
              UnorderedElementsAre("AudioFrame is missing CSRC for mid: mid",
                                   "AudioFrame is missing SSRC for mid: mid"));
}

TEST(ConferenceAudioTrackTest, LogsErrorWithOnlyLoudestSpeakerCsrc) {
  rtc::scoped_refptr<webrtc::MockRtpReceiver> mock_receiver(
      new webrtc::MockRtpReceiver());
  webrtc::RtpSource csrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/kLoudestSpeakerCsrc, webrtc::RtpSourceType::CSRC,
      /*rtp_timestamp=*/1111111,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  webrtc::RtpSource ssrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/333, webrtc::RtpSourceType::SSRC,
      /*rtp_timestamp=*/2222222,
      {.audio_level = 100, .absolute_capture_time = std::nullopt});
  EXPECT_CALL(*mock_receiver, GetSources)
      .WillOnce(Return(std::vector<webrtc::RtpSource>{
          std::move(csrc_rtp_source), std::move(ssrc_rtp_source)}));
  ConferenceAudioTrack audio_track("mid", mock_receiver,
                                   [](AudioFrame /*frame*/) {});
  ScopedMockLog log(kDoNotCaptureLogsYet);
  absl::SetVLogLevel("conference_media_tracks", 2);
  std::string message;
  EXPECT_CALL(log, Log(INFO, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();
  int16_t pcm_data[2 * 100];

  audio_track.OnData(pcm_data,
                     /*bits_per_sample=*/16,
                     /*sample_rate=*/48000,
                     /*number_of_channels=*/2,
                     /*number_of_frames=*/100,
                     /*absolute_capture_timestamp_ms=*/std::nullopt);

  EXPECT_EQ(message, "AudioFrame is missing CSRC for mid: mid");
}

TEST(ConferenceVideoTrackTest, CallsObserverWithVideoFrame) {
  MockFunction<void(VideoFrame)> mock_function;
  std::optional<VideoFrame> received_frame;
  EXPECT_CALL(mock_function, Call)
      .WillOnce([&received_frame](VideoFrame frame) {
        // meet::VideoFrame is not copyable as it contains a reference to a
        // webrtc::VideoFrame. Therefore, construct a new VideoFrame to store
        // the received frame.
        received_frame.emplace(frame);
      });
  ConferenceVideoTrack video_track("mid", mock_function.AsStdFunction());
  webrtc::VideoFrame::Builder builder;
  webrtc::RtpPacketInfo packet_info;
  packet_info.set_csrcs({123});
  packet_info.set_ssrc(456);
  webrtc::RtpPacketInfos packet_infos({packet_info});
  builder.set_packet_infos(packet_infos);
  builder.set_video_frame_buffer(webrtc::I420Buffer::Create(42, 42));

  // The `OnFrame` callback returns a reference to the frame, so a local
  // variable must be maintained so the frame is not destroyed.
  webrtc::VideoFrame frame = builder.build();
  video_track.OnFrame(frame);

  EXPECT_TRUE(received_frame.has_value());
  EXPECT_EQ(received_frame->frame.size(), 42 * 42);
  EXPECT_EQ(received_frame->contributing_source, 123);
  EXPECT_EQ(received_frame->synchronization_source, 456);
}

TEST(ConferenceVideoTrackTest, LogsErrorWithMissingPackingInfos) {
  ConferenceVideoTrack video_track("mid", [](VideoFrame /*frame*/) {});
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(ERROR, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();
  webrtc::VideoFrame::Builder builder;
  builder.set_video_frame_buffer(webrtc::I420Buffer::Create(42, 42));

  video_track.OnFrame(builder.build());

  EXPECT_EQ(message, "VideoFrame is missing packet infos for mid: mid");
}

TEST(ConferenceVideoTrackTest, LogsErrorWithMissingPacketCsrcs) {
  ConferenceVideoTrack video_track("mid", [](VideoFrame /*frame*/) {});
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(ERROR, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();
  webrtc::VideoFrame::Builder builder;
  webrtc::RtpPacketInfo packet_info;
  packet_info.set_ssrc(456);
  webrtc::RtpPacketInfos packet_infos({packet_info});
  builder.set_packet_infos(packet_infos);
  builder.set_video_frame_buffer(webrtc::I420Buffer::Create(42, 42));

  video_track.OnFrame(builder.build());

  EXPECT_EQ(message, "VideoFrame is missing CSRC for mid: mid");
}

}  // namespace
}  // namespace meet
