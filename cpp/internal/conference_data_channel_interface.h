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

#ifndef CPP_INTERNAL_CONFERENCE_DATA_CHANNEL_INTERFACE_H_
#define CPP_INTERNAL_CONFERENCE_DATA_CHANNEL_INTERFACE_H_

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "meet_clients/api/media_api_client_interface.h"

namespace meet {

// Interface for sending resource requests and receiving resource updates to and
// from Meet servers.
class ConferenceDataChannelInterface {
 public:
  using ResourceUpdateCallback = absl::AnyInvocable<void(ResourceUpdate)>;

  virtual ~ConferenceDataChannelInterface() = default;

  // Sets the callback for receiving resource updates from Meet servers.
  virtual void SetCallback(ResourceUpdateCallback callback) = 0;

  // Sends a resource request to Meet servers.
  virtual absl::Status SendRequest(ResourceRequest request) = 0;
};

}  // namespace meet

#endif  // CPP_INTERNAL_CONFERENCE_DATA_CHANNEL_INTERFACE_H_
