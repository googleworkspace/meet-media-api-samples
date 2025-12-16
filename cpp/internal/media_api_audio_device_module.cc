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

#include "absl/log/check.h"
#include "api/audio/audio_device_defines.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/units/time_delta.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

namespace meet {

int32_t MediaApiAudioDeviceModule::RegisterAudioCallback(
    webrtc::AudioTransport* callback) {
  DCHECK(worker_thread_.IsCurrent());
  audio_callback_ = callback;
  return 0;
}

int32_t MediaApiAudioDeviceModule::StartPlayout() {
  DCHECK(worker_thread_.IsCurrent());
  if (is_playing_) {
    return 0;
  }
  is_playing_ = true;

  worker_thread_.PostTask(
      SafeTask(safety_flag_, [this]() { ProcessPlayData(); }));
  return 0;
}

int32_t MediaApiAudioDeviceModule::StopPlayout() {
  DCHECK(worker_thread_.IsCurrent());
  is_playing_ = false;
  return 0;
}

bool MediaApiAudioDeviceModule::Playing() const {
  DCHECK(worker_thread_.IsCurrent());
  return is_playing_;
}

int32_t MediaApiAudioDeviceModule::Terminate() {
  DCHECK(worker_thread_.IsCurrent());
  safety_flag_->SetNotAlive();
  return 0;
}

void MediaApiAudioDeviceModule::ProcessPlayData() {
  DCHECK(worker_thread_.IsCurrent());
  if (!is_playing_) {
    return;
  }

  int64_t process_start_time = webrtc::TimeMillis();
  const size_t number_of_samples = kAudioSampleRatePerMillisecond *
                                   sampling_interval_.ms() *
                                   kNumberOfAudioChannels;
  std::vector<int16_t> sample_buffer(number_of_samples);
  size_t samples_out = 0;
  int64_t elapsed_time_ms = -1;
  int64_t ntp_time_ms = -1;

  if (audio_callback_ != nullptr) {
    audio_callback_->NeedMorePlayData(
        number_of_samples, kBytesPerSample, kNumberOfAudioChannels,
        // Sampling rate in samples per second (i.e. Hz).
        kAudioSampleRatePerMillisecond * 1000, sample_buffer.data(),
        samples_out, &elapsed_time_ms, &ntp_time_ms);
  }
  int64_t process_end_time = webrtc::TimeMillis();

  // Delay the next sampling for either:
  // 1. (sampling interval) - (time to process current sample)
  // 2. No delay if current processing took longer than the desired 10ms
  // TODO: Improve testing around this computation.
  webrtc::TimeDelta delay = std::max(
      webrtc::TimeDelta::Millis((process_start_time + sampling_interval_.ms()) -
                                process_end_time),
      webrtc::TimeDelta::Zero());
  worker_thread_.PostDelayedHighPrecisionTask(
      SafeTask(safety_flag_, [this]() { ProcessPlayData(); }), delay);
}

}  // namespace meet
