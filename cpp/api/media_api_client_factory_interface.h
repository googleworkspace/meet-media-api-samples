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

#ifndef CPP_API_MEDIA_API_CLIENT_FACTORY_INTERFACE_H_
#define CPP_API_MEDIA_API_CLIENT_FACTORY_INTERFACE_H_

#include <memory>

#include "absl/status/statusor.h"
#include "cpp/api/media_api_client_interface.h"
#include "webrtc/api/scoped_refptr.h"

namespace meet {

/// Interface for instantiating `MediaApiClientInterface`.
class MediaApiClientFactoryInterface {
 public:
  virtual ~MediaApiClientFactoryInterface() = default;

  /// Creates a `MediaApiClientInterface` instance.
  virtual absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
  CreateMediaApiClient(const MediaApiClientConfiguration& api_config,
                       webrtc::scoped_refptr<MediaApiClientObserverInterface>
                           api_session_observer) = 0;
};

}  // namespace meet

#endif  // CPP_API_MEDIA_API_CLIENT_FACTORY_INTERFACE_H_
