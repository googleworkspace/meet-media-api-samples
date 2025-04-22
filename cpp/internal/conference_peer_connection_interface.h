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

#ifndef CPP_INTERNAL_CONFERENCE_PEER_CONNECTION_INTERFACE_H_
#define CPP_INTERNAL_CONFERENCE_PEER_CONNECTION_INTERFACE_H_

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/stats/rtc_stats_collector_callback.h"

namespace meet {

// Interface for establishing a peer connection to a Meet conference.
class ConferencePeerConnectionInterface {
 public:
  using DisconnectCallback = absl::AnyInvocable<void(absl::Status)>;
  using TrackSignaledCallback = absl::AnyInvocable<void(
      webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>)>;

  virtual ~ConferencePeerConnectionInterface() = default;

  virtual void SetDisconnectCallback(
      DisconnectCallback disconnect_callback) = 0;

  virtual void SetTrackSignaledCallback(
      TrackSignaledCallback track_signaled_callback) = 0;

  // Connects to the conference with the given arguments and blocks until the
  // conference peer connection connects or fails to connect.
  virtual absl::Status Connect(absl::string_view join_endpoint,
                               absl::string_view conference_id,
                               absl::string_view access_token) = 0;

  // Closes the conference peer connection, preventing any further callbacks.
  virtual void Close() = 0;
  virtual void GetStats(webrtc::RTCStatsCollectorCallback* callback) = 0;
};

}  // namespace meet

#endif  // CPP_INTERNAL_CONFERENCE_PEER_CONNECTION_INTERFACE_H_
