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

#include "meet_clients/internal/conference_data_channel.h"

#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "meet_clients/api/media_api_client_interface.h"
#include "api/data_channel_interface.h"
#include "api/rtc_error.h"

namespace meet {

void ConferenceDataChannel::OnMessage(const webrtc::DataBuffer& buffer) {
  // Short-circuit if there is no callback for the message.
  if (!callback_) {
    LOG(WARNING) << label()
                 << " data channel received message but has no callback.";
    return;
  }

  // Meet servers should always send JSON updates.
  if (buffer.binary) {
    LOG(ERROR) << label() << " data channel received unexpected binary update.";
    return;
  }

  absl::string_view message(buffer.data.cdata<char>(), buffer.size());
  absl::StatusOr<ResourceUpdate> update_parse_status =
      resource_handler_->ParseUpdate(message);
  if (!update_parse_status.ok()) {
    LOG(ERROR) << "Received " << label()
               << " resource update but it failed to parse: "
               << update_parse_status.status();
    return;
  }

  VLOG(1) << label() << " data channel received update: " << message;

  callback_(*std::move(update_parse_status));
}

absl::Status ConferenceDataChannel::SendRequest(ResourceRequest request) {
  absl::StatusOr<std::string> stringify_status =
      resource_handler_->StringifyRequest(request);
  if (!stringify_status.ok()) {
    return stringify_status.status();
  }

  VLOG(1) << "Sending " << label() << " request: " << *stringify_status;

  data_channel_->SendAsync(
      // Closing the associated peer connection prevents new tasks from being
      // enqueued and blocks until any pending tasks complete. Therefore, the
      // conference data channel will exist if this is called and null
      // dereferences should not be possible.
      webrtc::DataBuffer(*std::move(stringify_status)),
      [this](webrtc::RTCError error) {
        if (!error.ok()) {
          LOG(ERROR) << "Error sending request via data channel: " << label()
                     << " " << error.message();
        }
      });
  return absl::OkStatus();
}

}  // namespace meet
