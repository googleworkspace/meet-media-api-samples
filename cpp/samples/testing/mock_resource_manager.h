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

#ifndef CPP_SAMPLES_TESTING_MOCK_RESOURCE_MANAGER_H_
#define CPP_SAMPLES_TESTING_MOCK_RESOURCE_MANAGER_H_

#include <cstdint>
#include <string>

#include "gmock/gmock.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "meet_clients/api/media_entries_resource.h"
#include "meet_clients/api/participants_resource.h"
#include "meet_clients/samples/resource_manager_interface.h"

namespace media_api_samples {

class MockResourceManager : public ResourceManagerInterface {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, GetOutputFileIdentifier,
              (uint32_t contributing_source), (override));
  MOCK_METHOD(void, OnMediaEntriesResourceUpdate,
              (const meet::MediaEntriesChannelToClient& update,
               absl::Time received_time),
              (override));
  MOCK_METHOD(void, OnParticipantResourceUpdate,
              (const meet::ParticipantsChannelToClient& update,
               absl::Time received_time),
              (override));
};

}  // namespace media_api_samples

#endif  // CPP_SAMPLES_TESTING_MOCK_RESOURCE_MANAGER_H_
