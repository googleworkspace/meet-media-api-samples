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
#include <utility>

#include "webrtc/api/audio/audio_device.h"
#include "webrtc/api/audio/audio_device_defines.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/task_queue/pending_task_safety_flag.h"
#include "webrtc/api/units/time_delta.h"
#include "webrtc/modules/audio_device/include/audio_device_default.h"
#include "webrtc/rtc_base/thread.h"

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
  explicit MediaApiAudioDeviceModule(webrtc::Thread& worker_thread)
      : MediaApiAudioDeviceModule(worker_thread,
                                  webrtc::TimeDelta::Millis(10)) {}

  // Constructor for testing with configurable sampling interval; the default
  // sampling interval of 10ms is too small to write non-flaky tests with.
  MediaApiAudioDeviceModule(webrtc::Thread& worker_thread,
                            webrtc::TimeDelta sampling_interval)
      : worker_thread_(worker_thread),
        sampling_interval_(std::move(sampling_interval)) {
    safety_flag_ = webrtc::PendingTaskSafetyFlag::CreateAttachedToTaskQueue(
        /*alive=*/true, &worker_thread_);
  };

  int32_t RegisterAudioCallback(webrtc::AudioTransport* callback) override;
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
  void ProcessPlayData();

  // Note that this MUST be the same worker thread used when creating the peer
  // connection.
  //
  // Not only does this remove the need for synchronization in this class (as
  // all methods are called on the worker thread by WebRTC), it also prevents
  // a deadlock when closing the peer connection:
  //
  // When audio data is passed to `ConferenceAudioTrack::OnData()`, it is
  // called on whatever thread `audio_callback_` is called on. When attempting
  // to read the audio csrcs and ssrcs from
  // `RtpReceiverInterface::GetSources()`, a blocking call will be made to the
  // worker thread (via the rtp receiver proxy layer) if the current thread is
  // NOT the worker thread.
  //
  // `ConferenceAudioTrack::OnData()` is called while holding a mutex in
  // WebRTC's `AudioMixerImpl::Mix()` method (also running on whatever thread
  // `audio_callback_` is called on).
  //
  // At the same time, when closing the peer connection,
  // `AudioMixerImpl::RemoveSource()` is called on the worker thread and
  // attempts to acquire the mutex held by `AudioMixerImpl::Mix()`, blocking
  // the worker thread.
  //
  // Therefore, it is possible for the worker thread to be blocked while
  // waiting for the `AudioMixerImpl` mutex, while
  // `ConferenceAudioTrack::OnData()` is blocked waiting for the worker thread
  // to read the audio csrcs and ssrcs.
  //
  // By ensuring that this class is always called on the worker thread, this
  // deadlock is avoided, as:
  //   1. The worker thread is a task queue, and task queue operatons are
  //   executed sequentially.
  //   2. `ConferenceAudioTrack::OnData()` is called on the worker thread and
  //   therefore does not need to switch to the worker thread to read the
  //   audio csrcs and ssrcs.
  webrtc::Thread& worker_thread_;
  // Used to ensure that tasks are not posted after `Terminate()` is called,
  // since this class does not own the worker thread.
  webrtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> safety_flag_;
  webrtc::TimeDelta sampling_interval_;

  webrtc::AudioTransport* audio_callback_ = nullptr;
  bool is_playing_ = false;
};

}  // namespace meet

#endif  // CPP_INTERNAL_MEDIA_API_AUDIO_DEVICE_MODULE_H_
