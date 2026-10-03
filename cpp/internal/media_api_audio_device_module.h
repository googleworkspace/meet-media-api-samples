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

#ifndef CPP_INTERNAL_MEDIA_API_AUDIO_DEVICE_MODULE_H_
#define CPP_INTERNAL_MEDIA_API_AUDIO_DEVICE_MODULE_H_

#include <stdbool.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "api/audio/audio_device.h"
#include "api/audio/audio_device_defines.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/audio_device/include/audio_device_default.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {

// Audio is sampled at 48000Hz.
constexpr int kAudioSampleRatePerMillisecond = 48;
// Produce mono audio (i.e. 1 channel).
constexpr int kNumberOfAudioChannels = 1;
constexpr int kBytesPerSample = sizeof(int16_t);

// Very simple implementation of an AudioDeviceModule.
//
// WebRTC has platform dependent implementations. However they are not fully
// supported and have no guarantees of future compatibility. This is because
// the only truly supported AudioDeviceModule (ADM) is the one in Chrome.
// Everything else is a "use at your own risk" implementation.
//
// Because we cannot guarantee the platform this client will always run on,
// there's no guarantee an implementation won't compile using the
// `DummyAudioDeviceModule`. That WebRTC implementation does nothing and no
// audio will be provided.
//
// To overcome these challenges, this is a provided implementation that does
// the bare minimum to provide audio. Nothing more, nothing less. If an end
// user requires more functionality and complexity, they are relegated to
// rolling their own implementation.
class MediaApiAudioDeviceModule
    : public webrtc::webrtc_impl::AudioDeviceModuleDefault<
          webrtc::AudioDeviceModule> {
 public:
  // Default constructor for production use.
  //
  // In production, audio should be sampled at 48000 Hz every 10ms.
  MediaApiAudioDeviceModule();

  // Constructor for testing with configurable sampling interval; the default
  // sampling interval of 10ms is too small to write non-flaky tests with.
  explicit MediaApiAudioDeviceModule(webrtc::TimeDelta sampling_interval);

  int32_t RegisterAudioCallback(
      webrtc::AudioTransport* absl_nullable callback) override;
  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  int32_t Terminate() override;
  bool Playing() const override;

 private:
  // Periodically calls the registered audio callback, registered by WebRTC
  // internals, to provide audio data. It is to be invoked every 10ms with a
  // sampling rate of 48000 Hz. If this is not done, no audio will be provided
  // to the audio sinks registered with the RTPReceiver of the RTPTransceiver
  // that remote audio is being received on.
  void ProcessPlayData() RTC_RUN_ON(playout_sequence_checker_);

  const webrtc::TimeDelta sampling_interval_;

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS webrtc::SequenceChecker thread_checker_;
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS webrtc::SequenceChecker
      playout_sequence_checker_;

  bool is_playing_ ABSL_GUARDED_BY(thread_checker_) = false;
  webrtc::AudioTransport* absl_nullable audio_callback_
      ABSL_GUARDED_BY(playout_sequence_checker_) = nullptr;
  webrtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> safety_flag_
      ABSL_GUARDED_BY(playout_sequence_checker_);
  webrtc::Timestamp next_run_time_ ABSL_GUARDED_BY(playout_sequence_checker_) =
      webrtc::Timestamp::Zero();
  std::vector<int16_t> sample_buffer_
      ABSL_GUARDED_BY(playout_sequence_checker_);

  // Dedicated internal task queue for pulling decoded audio frames via
  // `NeedMorePlayData()` off the WebRTC network/worker thread.
  // Declared last so that the task queue is stopped and joined before any other
  // members accessed by posted tasks are destroyed.
  absl_nullable std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
      playout_thread_ ABSL_GUARDED_BY(thread_checker_);
};

}  // namespace meet

#endif  // CPP_INTERNAL_MEDIA_API_AUDIO_DEVICE_MODULE_H_
