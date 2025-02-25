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

#ifndef CPP_API_SESSION_CONTROL_RESOURCE_H_
#define CPP_API_SESSION_CONTROL_RESOURCE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/status/status.h"

namespace meet {

/// Tells the server that the client is about to disconnect.
///
/// See `MeetMediaApiClientInterface::LeaveConference` for more
/// information.
struct LeaveRequest {};

struct SessionControlRequest {
  int64_t request_id = 0;
  std::optional<LeaveRequest> leave_request;
};

/// The top-level transport container for messages sent from client to
/// server in the `session-control` data channel. Any combination of fields may
/// be set, but the message is never empty.
struct SessionControlChannelFromClient {
  SessionControlRequest request;
};

/// This is a singleton resource containing the status of the media session.
struct SessionStatus {
  enum class ConferenceConnectionState {
    kUnknown,
    /// Session is waiting to be admitted into the conference.
    /// The client may never observe this state if it was admitted or rejected
    /// quickly.
    kWaiting,
    /// Session has fully joined the conference.
    kJoined,
    /// Session is not connected to the conference.
    ///
    /// This will be sent from the server when the client is no longer connected
    /// to the conference. This can occur for a variety of reasons, including
    /// the client being kicked from the conference, the client not being
    /// admitted into the conference, or the conference ending.
    kDisconnected,
  };

  ConferenceConnectionState connection_state =
      ConferenceConnectionState::kUnknown;

  enum MeetingDisconnectReason {
    /// The Media API client sent a leave request.
    kClientLeft,
    /// A conference participant explicitly stopped the Media API session.
    kUserStopped,
    /// The conference ended.
    kConferenceEnded,
    /// Something else went wrong with the session.
    kSessionUnhealthy
  };

  // Indicates the reason for the disconnection from the meeting.
  // Only set if the `connection_state` is `kDisconnected`.
  std::optional<MeetingDisconnectReason> disconnect_reason = std::nullopt;
};

struct SessionControlResourceSnapshot {
  /// The session control resource is a singleton resource. Therefore, this ID
  /// is always 0.
  int64_t id;
  SessionStatus session_status;
};

struct LeaveResponse {};

/// An optional response from Meet servers to an incoming request.
struct SessionControlResponse {
  int64_t request_id;
  /// The response status from Meet servers to an incoming request. This should
  /// be used by clients to determine the outcome of the request.
  absl::Status status;
  LeaveResponse leave_response;
};

/// The top-level transport container for messages sent from server to
/// client in the `session-control` data channel. Any combination of fields may
/// be set, but the message is never empty.
struct SessionControlChannelToClient {
  std::optional<SessionControlResponse> response;
  std::vector<SessionControlResourceSnapshot> resources;
};

}  // namespace meet

#endif  // CPP_API_SESSION_CONTROL_RESOURCE_H_
