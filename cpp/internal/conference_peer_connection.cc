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

#include "meet_clients/internal/conference_peer_connection.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "third_party/icu/source/tools/toolutil/json-json.hpp"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"

namespace meet {
namespace {

using Json = ::nlohmann::json;

// Lambda-based implementation of `SetLocalDescriptionObserverInterface`.
class SetLocalDescriptionObserver
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  explicit SetLocalDescriptionObserver(
      webrtc::PeerConnectionInterface &peer_connection)
      : peer_connection_(peer_connection) {}
  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
    if (error.ok()) {
      const webrtc::SessionDescriptionInterface *desc =
          peer_connection_.local_description();
      std::string local_description;
      // Note that this callback is called on the signaling thread, so
      // dereferencing the local description here is safe.
      desc->ToString(&local_description);
      local_description_ = std::move(local_description);
    } else {
      local_description_ = absl::InternalError(
          absl::StrCat("Error setting local description: ", error.message()));
    }
    notification_.Notify();
  }

  absl::StatusOr<std::string> GetLocalDescription() {
    notification_.WaitForNotification();
    return local_description_;
  }

 private:
  webrtc::PeerConnectionInterface &peer_connection_;
  absl::Notification notification_;
  absl::StatusOr<std::string> local_description_ =
      absl::InternalError("Local description not set.");
};

// Lambda-based implementation of `SetRemoteDescriptionObserverInterface`.
class SetRemoteDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    if (error.ok()) {
      remote_description_status_ = absl::OkStatus();
    } else {
      remote_description_status_ = absl::InternalError(
          absl::StrCat("Error setting remote description: ", error.message()));
    }
    notification_.Notify();
  }

  absl::Status GetRemoteDescription() {
    notification_.WaitForNotification();
    return remote_description_status_;
  }

 private:
  absl::Notification notification_;
  absl::Status remote_description_status_ =
      absl::InternalError("Remote description not set.");
};

}  // namespace

void ConferencePeerConnection::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  VLOG(1) << "OnSignalingChange: "
          << webrtc::PeerConnectionInterface::AsString(new_state);
  if (new_state !=
      webrtc::PeerConnectionInterface::PeerConnectionState::kClosed) {
    return;
  }

  if (disconnect_callback_ == nullptr) {
    LOG(WARNING) << "PeerConnection closed without disconnect callback.";
    return;
  }

  disconnect_callback_(absl::InternalError("Peer connection closed."));
}

void ConferencePeerConnection::OnTrack(
    webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
  if (track_signaled_callback_ == nullptr) {
    LOG(WARNING)
        << "ConferencePeerConnection::OnTrack called without callback.";
    return;
  }
  track_signaled_callback_(std::move(transceiver));
}

absl::Status ConferencePeerConnection::Connect(absl::string_view join_endpoint,
                                               absl::string_view conference_id,
                                               absl::string_view access_token) {
  if (peer_connection_ == nullptr) {
    return absl::InternalError("Peer connection is null.");
  }

  auto local_description_observer =
      webrtc::make_ref_counted<SetLocalDescriptionObserver>(*peer_connection_);
  peer_connection_->SetLocalDescription(local_description_observer);
  absl::StatusOr<std::string> local_description =
      local_description_observer->GetLocalDescription();
  if (!local_description.ok()) {
    return local_description.status();
  }

  absl::StatusOr<std::string> remote_description =
      http_connector_->ConnectActiveConference(join_endpoint, conference_id,
                                               access_token,
                                               local_description.value());
  if (!remote_description.ok()) {
    return remote_description.status();
  }

  webrtc::SdpParseError sdp_parse_error;
  std::unique_ptr<webrtc::SessionDescriptionInterface> answer_desc =
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer,
                                       std::move(remote_description).value(),
                                       &sdp_parse_error);
  if (answer_desc == nullptr) {
    return absl::InternalError(absl::StrCat("Failed to parse answer SDP: ",
                                            sdp_parse_error.description));
  }
  auto remote_description_observer =
      webrtc::make_ref_counted<SetRemoteDescriptionObserver>();
  peer_connection_->SetRemoteDescription(std::move(answer_desc),
                                         remote_description_observer);
  absl::Status remote_description_result =
      remote_description_observer->GetRemoteDescription();
  return remote_description_result;
}

}  // namespace meet
