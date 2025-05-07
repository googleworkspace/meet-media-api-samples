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

#ifndef CPP_INTERNAL_MEDIA_API_CLIENT_FACTORY_H_
#define CPP_INTERNAL_MEDIA_API_CLIENT_FACTORY_H_

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/statusor.h"
#include "cpp/api/media_api_client_factory_interface.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/internal/http_connector_interface.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {

class MediaApiClientFactory : public MediaApiClientFactoryInterface {
 public:
  using PeerConnectionFactoryProvider = absl::AnyInvocable<
      rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>(
          rtc::Thread* signaling_thread, rtc::Thread* worker_thread)>;
  using HttpConnectorProvider =
      absl::AnyInvocable<std::unique_ptr<HttpConnectorInterface>()>;

  // Default constructor that builds clients with real dependencies.
  MediaApiClientFactory();

  // Constructor with dependency providers, useful for testing.
  explicit MediaApiClientFactory(
      PeerConnectionFactoryProvider peer_connection_factory_provider,
      HttpConnectorProvider http_connector_provider)
      : peer_connection_factory_provider_(
            std::move(peer_connection_factory_provider)),
        http_connector_provider_(std::move(http_connector_provider)) {}

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>> CreateMediaApiClient(
      const MediaApiClientConfiguration& api_config,
      rtc::scoped_refptr<MediaApiClientObserverInterface>
          api_session_observer) override;

 private:
  PeerConnectionFactoryProvider peer_connection_factory_provider_;
  HttpConnectorProvider http_connector_provider_;
};

}  // namespace meet

#endif  // CPP_INTERNAL_MEDIA_API_CLIENT_FACTORY_H_
