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

#include "meet_clients/internal/media_api_audio_device_module.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/base/nullability.h"
#include "api/audio/audio_device_defines.h"
#include "api/sequence_checker.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {

MediaApiAudioDeviceModule::MediaApiAudioDeviceModule()
    : MediaApiAudioDeviceModule(webrtc::TimeDelta::Millis(10)) {}

MediaApiAudioDeviceModule::MediaApiAudioDeviceModule(
    webrtc::TimeDelta sampling_interval)
    : sampling_interval_(sampling_interval),
      sample_buffer_(kAudioSampleRatePerMillisecond * sampling_interval_.ms() *
                     kNumberOfAudioChannels) {}

int32_t MediaApiAudioDeviceModule::RegisterAudioCallback(
    webrtc::AudioTransport* absl_nullable callback) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (playout_thread_ != nullptr) {
    RTC_DCHECK_NOTREACHED();
    return -1;
  }
  RTC_DCHECK_RUN_ON(&playout_sequence_checker_);
  audio_callback_ = callback;
  return 0;
}

int32_t MediaApiAudioDeviceModule::StartPlayout() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (playout_thread_ != nullptr) {
    return 0;
  }
  playout_sequence_checker_.Detach();
  playout_thread_ = webrtc::CreateDefaultTaskQueueFactory()->CreateTaskQueue(
      "media_api_adm", webrtc::TaskQueueFactory::Priority::kAudio);
  RTC_CHECK(playout_thread_ != nullptr);

  playout_thread_->PostTask([this]() {
    RTC_DCHECK_RUN_ON(&playout_sequence_checker_);
    next_run_time_ = webrtc::Timestamp::Micros(webrtc::TimeMicros());
    ProcessPlayData();
  });
  return 0;
}

int32_t MediaApiAudioDeviceModule::StopPlayout() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  if (playout_thread_ == nullptr) {
    return 0;
  }
  playout_thread_ = nullptr;
  playout_sequence_checker_.Detach();
  return 0;
}

bool MediaApiAudioDeviceModule::Playing() const {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return playout_thread_ != nullptr;
}

int32_t MediaApiAudioDeviceModule::Terminate() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  return StopPlayout();
}

void MediaApiAudioDeviceModule::ProcessPlayData() {
  RTC_DCHECK_RUN_ON(&playout_sequence_checker_);

  size_t samples_out = 0;
  int64_t elapsed_time_ms = -1;
  int64_t ntp_time_ms = -1;

  if (audio_callback_ != nullptr) {
    audio_callback_->NeedMorePlayData(
        sample_buffer_.size() / kNumberOfAudioChannels,
        kBytesPerSample * kNumberOfAudioChannels, kNumberOfAudioChannels,
        // Sampling rate in samples per second (i.e. Hz).
        kAudioSampleRatePerMillisecond * 1000, sample_buffer_.data(),
        samples_out, &elapsed_time_ms, &ntp_time_ms);
  }

  // Delay the next sampling for either:
  // 1. (sampling interval) - (lost time since target `next_run_time_`, covering
  //    both callback execution time and task-queue wakeup jitter)
  // 2. No delay if current processing fell behind the next target tick
  // TODO: Improve testing around this computation.
  webrtc::Timestamp now = webrtc::Timestamp::Micros(webrtc::TimeMicros());
  webrtc::TimeDelta lost_time = now - next_run_time_;
  next_run_time_ = std::max(next_run_time_ + sampling_interval_, now);
  webrtc::TimeDelta delay =
      std::max(sampling_interval_ - lost_time, webrtc::TimeDelta::Zero());
  webrtc::TaskQueueBase* absl_nullable current_queue =
      webrtc::TaskQueueBase::Current();
  RTC_DCHECK(current_queue != nullptr);
  current_queue->PostDelayedHighPrecisionTask(
      [this]() {
        RTC_DCHECK_RUN_ON(&playout_sequence_checker_);
        ProcessPlayData();
      },
      delay);
}

}  // namespace meet
