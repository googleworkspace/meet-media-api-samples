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

#ifndef CPP_INTERNAL_RESOURCE_HANDLER_INTERFACE_H_
#define CPP_INTERNAL_RESOURCE_HANDLER_INTERFACE_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "meet_clients/api/media_api_client_interface.h"

namespace meet {

// Interface used by `ResourceDataChannel` instances to serialize and
// deserialize resource updates and requests.
//
// Data is serialized and deserialized using the JSON format.
class ResourceHandlerInterface {
 public:
  virtual ~ResourceHandlerInterface() = default;

  virtual absl::StatusOr<ResourceUpdate> ParseUpdate(
      absl::string_view update) = 0;

  virtual absl::StatusOr<std::string> StringifyRequest(
      const ResourceRequest& request) = 0;
};

}  // namespace meet

#endif  // CPP_INTERNAL_RESOURCE_HANDLER_INTERFACE_H_
