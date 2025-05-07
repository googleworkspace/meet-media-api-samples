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

#include "cpp/internal/media_api_audio_device_module.h"

#include <cstddef>
#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/units/time_delta.h"
#include "webrtc/modules/audio_device/include/mock_audio_transport.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {
namespace {
using ::testing::_;

std::unique_ptr<rtc::Thread> CreateWorkerThread() {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->SetName("worker_thread", nullptr);
  EXPECT_TRUE(thread->Start());
  return thread;
}

TEST(MediaApiAudioDeviceModuleTest, SuccessfullyInvokeStartPlayout) {
  std::unique_ptr<rtc::Thread> worker_thread = CreateWorkerThread();
  auto adm =
      webrtc::make_ref_counted<MediaApiAudioDeviceModule>(*worker_thread);

  worker_thread->BlockingCall([&]() {
    EXPECT_EQ(adm->StartPlayout(), 0);
    EXPECT_TRUE(adm->Playing());
    adm->Terminate();
  });
}

TEST(MediaApiAudioDeviceModuleTest, StartingPlayoutTwiceIsNoOp) {
  std::unique_ptr<rtc::Thread> worker_thread = CreateWorkerThread();
  auto adm =
      webrtc::make_ref_counted<MediaApiAudioDeviceModule>(*worker_thread);
  webrtc::test::MockAudioTransport audio_transport;
  EXPECT_CALL(audio_transport, NeedMorePlayData(_, _, _, _, _, _, _, _))
      // Expect that only one processing task is enqueued if playout is
      // triggered while already playing.
      .Times(1);

  worker_thread->BlockingCall([&]() {
    adm->RegisterAudioCallback(&audio_transport);
    EXPECT_EQ(adm->StartPlayout(), 0);
    EXPECT_EQ(adm->StartPlayout(), 0);
  });

  // Terminate in a separate block to allow the first processing task to
  // complete.
  worker_thread->BlockingCall([&]() { adm->Terminate(); });
}

TEST(MediaApiAudioDeviceModuleTest, SuccessfullyInvokeStopPlayout) {
  std::unique_ptr<rtc::Thread> worker_thread = CreateWorkerThread();
  auto adm =
      webrtc::make_ref_counted<MediaApiAudioDeviceModule>(*worker_thread);

  worker_thread->BlockingCall([&]() {
    EXPECT_EQ(adm->StartPlayout(), 0);
    EXPECT_EQ(adm->StopPlayout(), 0);
    EXPECT_FALSE(adm->Playing());
    adm->Terminate();
  });
}

TEST(MediaApiAudioDeviceModuleTest, StartPlayoutAfterStopPlayoutIsSuccessful) {
  std::unique_ptr<rtc::Thread> worker_thread = CreateWorkerThread();
  auto adm =
      webrtc::make_ref_counted<MediaApiAudioDeviceModule>(*worker_thread);

  worker_thread->BlockingCall([&]() {
    EXPECT_EQ(adm->StartPlayout(), 0);
    EXPECT_EQ(adm->StopPlayout(), 0);
    EXPECT_EQ(adm->StartPlayout(), 0);
    EXPECT_TRUE(adm->Playing());
    adm->Terminate();
  });
}

// When processing audio data, if the callback takes less time than the sampling
// interval, the next sampling should be delayed by the difference.
//
// This test simulates processing that finishes immediately.
TEST(MediaApiAudioDeviceModuleTest,
     StartPlayoutWithCallbackRegisteredInvokesCallback) {
  std::unique_ptr<rtc::Thread> worker_thread = CreateWorkerThread();
  auto adm = webrtc::make_ref_counted<MediaApiAudioDeviceModule>(
      *worker_thread, /*sampling_interval=*/webrtc::TimeDelta::Seconds(1));
  webrtc::test::MockAudioTransport audio_transport;
  const size_t number_of_samples =
      kAudioSampleRatePerMillisecond * 1000 * kNumberOfAudioChannels;
  EXPECT_CALL(audio_transport,
              NeedMorePlayData(
                  number_of_samples, kBytesPerSample, kNumberOfAudioChannels,
                  kAudioSampleRatePerMillisecond * 1000, _, _, _, _))
      .Times(6);

  worker_thread->BlockingCall([&]() {
    EXPECT_EQ(adm->RegisterAudioCallback(&audio_transport), 0);
    EXPECT_EQ(adm->StartPlayout(), 0);
  });

  // In this test, audio processing finishes immediately. Therefore, sampling
  // should occur at the sampling interval (i.e. 1 sample per second).
  //
  // After 5.5 seconds, there should be 6 rounds of processing, at T=0, T=1,
  // T=2, T=3, T=4, and T=5.
  absl::SleepFor(absl::Seconds(5.5));

  worker_thread->BlockingCall([&]() { adm->Terminate(); });
}

// When processing audio data, if the callback takes more time than the sampling
// interval, the next sampling should be triggered immediately.
//
// This test simulates processing that takes 2 seconds, which is longer than the
// sampling interval of 1 second.
TEST(MediaApiAudioDeviceModuleTest,
     StartPlayoutImmediatelyCallsCallbackWhenCallbackTakesNonTrivialTime) {
  std::unique_ptr<rtc::Thread> worker_thread = CreateWorkerThread();
  auto adm = webrtc::make_ref_counted<MediaApiAudioDeviceModule>(
      *worker_thread, /*sampling_interval=*/webrtc::TimeDelta::Seconds(1));
  webrtc::test::MockAudioTransport audio_transport;
  EXPECT_CALL(audio_transport, NeedMorePlayData(_, _, _, _, _, _, _, _))
      .Times(3)
      .WillRepeatedly([&]() {
        absl::SleepFor(absl::Seconds(2));
        return 0;
      });

  worker_thread->BlockingCall([&]() {
    EXPECT_EQ(adm->RegisterAudioCallback(&audio_transport), 0);
    EXPECT_EQ(adm->StartPlayout(), 0);
  });

  // Audio processing is set to take 2 seconds. One round of processing is
  // triggered immediately after starting playout. Because the processing time
  // is longer than the sampling interval, the next rounds of processing are
  // triggered immediately.
  //
  // Therefore, there should be 3 rounds of processing in total after 5.5
  // seconds, at: T=0, T=2, and T=4.
  absl::SleepFor(absl::Seconds(5.5));

  worker_thread->BlockingCall([&]() { adm->Terminate(); });
}

TEST(MediaApiAudioDeviceModuleTest,
     StopPlayoutStopsInvokingCallbackForEnqueuedTasks) {
  std::unique_ptr<rtc::Thread> worker_thread = CreateWorkerThread();
  auto adm = webrtc::make_ref_counted<MediaApiAudioDeviceModule>(
      *worker_thread, /*sampling_interval=*/webrtc::TimeDelta::Seconds(1));
  webrtc::test::MockAudioTransport audio_transport;
  EXPECT_CALL(audio_transport, NeedMorePlayData(_, _, _, _, _, _, _, _))
      .Times(0);

  worker_thread->BlockingCall([&]() {
    EXPECT_EQ(adm->RegisterAudioCallback(&audio_transport), 0);
    // Start playout to enqueue the first audio processing task.
    EXPECT_EQ(adm->StartPlayout(), 0);
    // Immediately stop playout to make any enqueued audio processing tasks
    // no-op and stop eneuqueing new tasks.
    EXPECT_EQ(adm->StopPlayout(), 0);
  });

  // Sleep for the sampling interval plus a small amount of time to ensure that
  // no audio processing is triggered.
  absl::SleepFor(absl::Seconds(1.5));

  worker_thread->BlockingCall([&]() { adm->Terminate(); });
}

}  // namespace
}  // namespace meet
