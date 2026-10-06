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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "meet_clients/api/media_api_client_interface.h"
#include "meet_clients/internal/media_api_client_factory.h"
#include "meet_clients/internal/testing/fake_meet_server.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {
namespace {

constexpr size_t kTestAudioSamplesPer10MsFrame = kTestAudioSampleRateHz / 100;
constexpr double kExpectedMeanAbsoluteLevel =
    (2.0 / M_PI) * kTestAudioSineAmplitude;
constexpr absl::Duration kDefaultTimeout = absl::Seconds(10);
constexpr size_t kNumAudioFramesToAwait = 50;
constexpr size_t kMaxNumAudioFramesUntilSteady = 10;

struct RecordedAudioFrame {
  std::vector<int16_t> pcm16;
  int sample_rate;
  size_t number_of_channels;
  uint32_t contributing_source;
};

double ComputeFrameMeanAbsoluteLevel(const RecordedAudioFrame& frame) {
  if (frame.pcm16.empty()) {
    return 0.0;
  }
  double sum = 0.0;
  for (int16_t sample : frame.pcm16) {
    sum += std::abs(static_cast<int>(sample));
  }
  return sum / static_cast<double>(frame.pcm16.size());
}

double ComputeFrameZeroCrossingRate(const RecordedAudioFrame& frame) {
  if (frame.pcm16.size() < 2) {
    return 0.0;
  }
  int zero_crossings = 0;
  for (size_t i = 1; i < frame.pcm16.size(); ++i) {
    if ((frame.pcm16[i - 1] < 0 && frame.pcm16[i] >= 0) ||
        (frame.pcm16[i - 1] >= 0 && frame.pcm16[i] < 0)) {
      ++zero_crossings;
    }
  }
  return static_cast<double>(zero_crossings * frame.sample_rate) /
         static_cast<double>(frame.pcm16.size());
}

class TestClientObserver : public MediaApiClientObserverInterface {
 public:
  void OnJoined() override {
    absl::MutexLock lock(mutex_);
    if (!joined_.HasBeenNotified()) {
      joined_.Notify();
    }
  }

  bool WaitUntilJoined(absl::Duration timeout) {
    return joined_.WaitForNotificationWithTimeout(timeout);
  }

  void OnDisconnected(absl::Status status) override {
    if (!status.ok()) {
      LOG(ERROR) << "TestClientObserver::OnDisconnected with error: " << status;
    }
    absl::MutexLock lock(mutex_);
    disconnected_status_ = status;
    if (!disconnected_.HasBeenNotified()) {
      disconnected_.Notify();
    }
  }

  bool WaitUntilDisconnected(absl::Duration timeout) {
    return disconnected_.WaitForNotificationWithTimeout(timeout);
  }

  absl::Status disconnected_status() {
    absl::MutexLock lock(mutex_);
    return disconnected_status_;
  }

  void OnMessageFromServer(MessageFromServer /*update*/) override {}

  void OnVideoFrame(VideoFrame /*frame*/) override {}

  void OnAudioFrame(AudioFrame frame) override {
    std::vector<int16_t> pcm16(frame.pcm16.begin(), frame.pcm16.end());
    absl::MutexLock lock(mutex_);
    frames_.push_back(RecordedAudioFrame{
        .pcm16 = std::move(pcm16),
        .sample_rate = frame.sample_rate,
        .number_of_channels = frame.number_of_channels,
        .contributing_source = frame.contributing_source,
    });
  }

  bool WaitForAudioFrames(size_t num_frames,
                          absl::Duration timeout = kDefaultTimeout) {
    auto has_frames = [this, num_frames]() ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
      return frames_.size() >= num_frames;
    };
    absl::MutexLock lock(mutex_);
    return mutex_.AwaitWithTimeout(absl::Condition(&has_frames), timeout);
  }

  bool WaitForSteadyLevel(double min_level = kExpectedMeanAbsoluteLevel * 0.1,
                          double max_delta = kExpectedMeanAbsoluteLevel * 0.05,
                          size_t max_num_frames = kMaxNumAudioFramesUntilSteady,
                          absl::Duration timeout = kDefaultTimeout) {
    if (max_num_frames < 2) {
      LOG(ERROR) << "WaitForSteadyLevel: max_num_frames must be at least 2";
      return false;
    }
    if (min_level < 0.0) {
      LOG(ERROR) << "WaitForSteadyLevel: min_level must be non-negative";
      return false;
    }
    if (max_delta < 0.0) {
      LOG(ERROR) << "WaitForSteadyLevel: max_delta must be non-negative";
      return false;
    }

    auto has_steady_level = [this, min_level,
                             max_delta]() ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
      if (frames_.size() < 2) {
        return false;
      }

      const double previous_level =
          ComputeFrameMeanAbsoluteLevel(frames_[frames_.size() - 2]);
      const double current_level =
          ComputeFrameMeanAbsoluteLevel(frames_[frames_.size() - 1]);
      return current_level >= min_level &&
             std::abs(current_level - previous_level) <= max_delta;
    };
    absl::MutexLock lock(mutex_);
    bool did_stabilize =
        mutex_.AwaitWithTimeout(absl::Condition(&has_steady_level), timeout);
    return did_stabilize && frames_.size() <= max_num_frames;
  }

  std::vector<RecordedAudioFrame> TakeFrames() {
    absl::MutexLock lock(mutex_);
    return std::exchange(frames_, {});
  }

 private:
  absl::Notification joined_;
  absl::Notification disconnected_;
  absl::Mutex mutex_;
  absl::Status disconnected_status_ ABSL_GUARDED_BY(mutex_) =
      absl::InternalError("Not disconnected.");
  std::vector<RecordedAudioFrame> frames_ ABSL_GUARDED_BY(mutex_);
};

class MediaApiClientE2eTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MediaApiClientFactory client_factory =
        CreateLoopbackMediaApiClientFactory(server_);
    absl::StatusOr<std::unique_ptr<MediaApiClientInterface>> client =
        client_factory.CreateMediaApiClient(
            MediaApiClientConfiguration{.receiving_video_stream_count = 0,
                                        .enable_audio_streams = true},
            observer_);
    ASSERT_TRUE(client.ok()) << client.status();
    client_ = std::move(client).value_or(nullptr);

    absl::Status connect_status = client_->ConnectActiveConference(
        "https://meet.googleapis.com/v2beta", "spaces/test-space", "test-token",
        /*connection_timeout_ms=*/std::nullopt,
        /*request_timeout_ms=*/std::nullopt);
    ASSERT_TRUE(connect_status.ok()) << connect_status;
    ASSERT_TRUE(observer_->WaitUntilJoined(kDefaultTimeout));
  }

  FakeMeetServer server_;
  webrtc::scoped_refptr<TestClientObserver> observer_ =
      webrtc::make_ref_counted<TestClientObserver>();
  std::unique_ptr<MediaApiClientInterface> client_;
};

TEST_F(MediaApiClientE2eTest, ReceivedFramesHaveExpectedCsrc) {
  ASSERT_TRUE(observer_->WaitForAudioFrames(kNumAudioFramesToAwait));

  std::vector<RecordedAudioFrame> received_frames = observer_->TakeFrames();
  ASSERT_GE(received_frames.size(), kNumAudioFramesToAwait);

  for (const RecordedAudioFrame& recorded : received_frames) {
    EXPECT_EQ(recorded.contributing_source, kTestAudioCsrc);
    EXPECT_EQ(recorded.sample_rate, kTestAudioSampleRateHz);
    EXPECT_EQ(recorded.number_of_channels, kTestAudioChannels);
    EXPECT_EQ(recorded.pcm16.size(), kTestAudioSamplesPer10MsFrame);
  }
}

TEST_F(MediaApiClientE2eTest, ReceivedFramesHaveExpectedLevel) {
  // The first few frames may have lower amplitude as webrtc ramps up the gain
  // from silence, but we expect the remaining frames to have consistent
  // amplitude.
  ASSERT_TRUE(observer_->WaitForSteadyLevel());
  observer_->TakeFrames();
  ASSERT_TRUE(observer_->WaitForAudioFrames(kNumAudioFramesToAwait));

  std::vector<RecordedAudioFrame> received_frames = observer_->TakeFrames();
  ASSERT_GE(received_frames.size(), kNumAudioFramesToAwait);

  for (size_t i = 0; i < received_frames.size(); ++i) {
    EXPECT_NEAR(ComputeFrameMeanAbsoluteLevel(received_frames[i]),
                kExpectedMeanAbsoluteLevel, kExpectedMeanAbsoluteLevel * 0.05)
        << "Frame " << i << " has unexpected level";
  }
}

TEST_F(MediaApiClientE2eTest, ReceivedFramesHaveExpectedFrequency) {
  // We need a non-zero level to determine the frequency.
  ASSERT_TRUE(observer_->WaitForSteadyLevel(
      /*min_level=*/kExpectedMeanAbsoluteLevel * 0.01,
      /*max_delta=*/kExpectedMeanAbsoluteLevel));
  observer_->TakeFrames();
  ASSERT_TRUE(observer_->WaitForAudioFrames(kNumAudioFramesToAwait));

  std::vector<RecordedAudioFrame> received_frames = observer_->TakeFrames();
  ASSERT_GE(received_frames.size(), kNumAudioFramesToAwait);

  constexpr double kExpectedZeroCrossingRate = 2.0 * kTestAudioSineFrequencyHz;
  for (size_t i = 0; i < received_frames.size(); ++i) {
    EXPECT_NEAR(ComputeFrameZeroCrossingRate(received_frames[i]),
                kExpectedZeroCrossingRate, kExpectedZeroCrossingRate * 0.05)
        << "Frame " << i << " has unexpected zero-crossing rate";
  }
}

TEST_F(MediaApiClientE2eTest, ReceivedFramesAreContinuous) {
  ASSERT_TRUE(observer_->WaitForAudioFrames(kNumAudioFramesToAwait));

  std::vector<RecordedAudioFrame> received_frames = observer_->TakeFrames();
  ASSERT_GE(received_frames.size(), kNumAudioFramesToAwait);

  constexpr double kMaxSampleStep = 2.0 * M_PI * kTestAudioSineFrequencyHz *
                                    kTestAudioSineAmplitude /
                                    kTestAudioSampleRateHz;
  for (size_t i = 1; i < received_frames.size(); ++i) {
    const std::vector<int16_t>& current_samples = received_frames[i].pcm16;
    const std::vector<int16_t>& previous_samples = received_frames[i - 1].pcm16;
    ASSERT_GT(current_samples.size(), 1);
    ASSERT_GT(previous_samples.size(), 1);
    EXPECT_NEAR(current_samples.front(), previous_samples.back(),
                kMaxSampleStep * 2.0)
        << "Discontinuity between frame " << (i - 1) << " and frame " << i;
  }
}

TEST_F(MediaApiClientE2eTest, ServerInitiatedShutdownExitsCleanly) {
  ASSERT_TRUE(observer_->WaitForAudioFrames(10));

  // Trigger server-initiated disconnection while audio is actively streaming
  // and verify that the peer connection closes as expected.
  server_.SendDisconnectedSessionStatus();

  ASSERT_TRUE(observer_->WaitUntilDisconnected(kDefaultTimeout));
  EXPECT_TRUE(observer_->disconnected_status().ok())
      << observer_->disconnected_status();
}

TEST_F(MediaApiClientE2eTest, ClientInitiatedShutdownExitsCleanly) {
  ASSERT_TRUE(observer_->WaitForAudioFrames(10));

  absl::Status leave_status = client_->LeaveConference(/*request_id=*/1);
  ASSERT_TRUE(leave_status.ok()) << leave_status;

  ASSERT_TRUE(observer_->WaitUntilDisconnected(kDefaultTimeout));
  EXPECT_TRUE(observer_->disconnected_status().ok())
      << observer_->disconnected_status();
}

}  // namespace
}  // namespace meet
