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

#ifndef CPP_INTERNAL_HTTP_CONNECTOR_INTERFACE_H_
#define CPP_INTERNAL_HTTP_CONNECTOR_INTERFACE_H_

#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace meet {

// Interface for connecting to Meet's `ConnectActiveConference` endpoint.
//
// Mock implementations can be used for testing.
class HttpConnectorInterface {
 public:
  virtual ~HttpConnectorInterface() = default;

  // Sends an HTTP request to Meet's `ConnectActiveConference` endpoint and
  // returns the response, or an error if the request fails.
  virtual absl::StatusOr<std::string> ConnectActiveConference(
      absl::string_view join_endpoint, absl::string_view conference_id,
      absl::string_view access_token, absl::string_view sdp_offer,
      std::optional<int> connection_timeout_ms,
      std::optional<int> request_timeout_ms) = 0;
};

}  // namespace meet

#endif  // CPP_INTERNAL_HTTP_CONNECTOR_INTERFACE_H_
