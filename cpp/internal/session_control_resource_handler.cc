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

#include "cpp/internal/session_control_resource_handler.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "nlohmann/json.hpp"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/api/session_control_resource.h"

namespace meet {
namespace {
using Json = ::nlohmann::json;

// Session control resource channel is always opened with this label.
constexpr absl::string_view kSessionControlResourceName = "session-control";

const Json* FindOrNull(const Json& json, absl::string_view key) {
  auto it = json.find(key);
  return it != json.cend() ? &*it : nullptr;
}

SessionStatus::ConferenceConnectionState StringToMeetingConnectionState(
    absl::string_view state) {
  if (state == "STATE_WAITING") {
    return SessionStatus::ConferenceConnectionState::kWaiting;
  } else if (state == "STATE_JOINED") {
    return SessionStatus::ConferenceConnectionState::kJoined;
  } else if (state == "STATE_DISCONNECTED") {
    return SessionStatus::ConferenceConnectionState::kDisconnected;
  } else {
    return SessionStatus::ConferenceConnectionState::kUnknown;
  }
}

std::optional<SessionStatus::MeetingDisconnectReason>
StringToMeetingDisconnectReason(absl::string_view reason) {
  if (reason == "REASON_CLIENT_LEFT") {
    return SessionStatus::MeetingDisconnectReason::kClientLeft;
  } else if (reason == "REASON_USER_STOPPED") {
    return SessionStatus::MeetingDisconnectReason::kUserStopped;
  } else if (reason == "REASON_CONFERENCE_ENDED") {
    return SessionStatus::MeetingDisconnectReason::kConferenceEnded;
  } else if (reason == "REASON_SESSION_UNHEALTHY") {
    return SessionStatus::MeetingDisconnectReason::kSessionUnhealthy;
  } else {
    return std::nullopt;
  }
}

}  // namespace

absl::StatusOr<ResourceUpdate> SessionControlResourceHandler::ParseUpdate(
    absl::string_view update) {
  VLOG(1) << kSessionControlResourceName
          << " resource update received: " << update;

  const Json json_resource_update = Json::parse(update, /*cb=*/nullptr,
                                                /*allow_exceptions=*/false);

  if (!json_resource_update.is_object()) {
    return absl::InternalError(absl::StrCat(
        "Invalid ", kSessionControlResourceName, " json format: ", update));
  }

  SessionControlChannelToClient session_control_update;
  // Response
  if (const Json* response_field = FindOrNull(json_resource_update, "response");
      response_field != nullptr) {
    session_control_update.response = SessionControlResponse();
    // Response.requestId
    if (const Json* request_id_field = FindOrNull(*response_field, "requestId");
        request_id_field != nullptr) {
      session_control_update.response->request_id =
          request_id_field->get<int64_t>();
    }

    // Response.status
    if (const Json* status_field = FindOrNull(*response_field, "status");
        status_field != nullptr) {
      absl::StatusCode status_code = absl::StatusCode::kUnknown;

      // Response.status.code
      if (const Json* code_field = FindOrNull(*status_field, "code");
          code_field != nullptr) {
        status_code = static_cast<absl::StatusCode>(code_field->get<int32_t>());
      }

      // Response.status.message
      std::string message;
      if (const Json* message_field = FindOrNull(*status_field, "message");
          message_field != nullptr) {
        message = message_field->get<std::string>();
      }
      session_control_update.response->status =
          absl::Status(status_code, message);
    }

    // Response.leaveResponse
    if (const Json* leave_field = FindOrNull(*response_field, "leave");
        leave_field != nullptr) {
      session_control_update.response->leave_response = LeaveResponse();
    }
  }

  // Resources
  if (const Json* resources_field =
          FindOrNull(json_resource_update, "resources");
      resources_field != nullptr) {
    if (!resources_field->is_array()) {
      return absl::InternalError(absl::StrCat(
          "Invalid ", kSessionControlResourceName,
          " json format. Expected resources field to be an array: ", update));
    }

    std::vector<SessionControlResourceSnapshot> resources;
    for (const Json& resource : *resources_field) {
      // Resources.resourceSnapshot
      SessionControlResourceSnapshot snapshot;

      // Resources.resourceSnapshot.sessionStatus
      if (const Json* session_status_field =
              FindOrNull(resource, "sessionStatus");
          session_status_field != nullptr) {
        // Resources.resourceSnapshot.sessionStatus.connectionState
        if (const Json* connection_state_field =
                FindOrNull(*session_status_field, "connectionState");
            connection_state_field != nullptr) {
          snapshot.session_status = {
              .connection_state = StringToMeetingConnectionState(
                  connection_state_field->get<std::string>())};
        } else {
          snapshot.session_status = {
              .connection_state =
                  SessionStatus::ConferenceConnectionState::kUnknown};
        }

        // Resources.resourceSnapshot.sessionStatus.disconnectReason
        if (const Json* disconnect_reason_field =
                FindOrNull(*session_status_field, "disconnectReason");
            disconnect_reason_field != nullptr) {
          snapshot.session_status.disconnect_reason =
              StringToMeetingDisconnectReason(
                  disconnect_reason_field->get<std::string>());
        } else {
          snapshot.session_status.disconnect_reason = std::nullopt;
        }
      }
      resources.push_back(snapshot);
    }
    session_control_update.resources = resources;
  }

  return std::move(session_control_update);
}

absl::StatusOr<std::string> SessionControlResourceHandler::StringifyRequest(
    const ResourceRequest& request) {
  if (!std::holds_alternative<SessionControlChannelFromClient>(request)) {
    return absl::InvalidArgumentError(
        "SessionControlResourceHandler only supports "
        "SessionControlChannelFromClient");
  }

  auto session_control_request =
      std::get<SessionControlChannelFromClient>(request);

  nlohmann::basic_json<> session_control_channel_from_client;

  if (session_control_request.request.request_id == 0) {
    return absl::InvalidArgumentError("Request ID must be set");
  }

  session_control_channel_from_client["request"]["requestId"] =
      session_control_request.request.request_id;

  if (session_control_request.request.leave_request.has_value()) {
    session_control_channel_from_client["request"]["leave"] =
        nlohmann::json::value_t::object;
  }
  return session_control_channel_from_client.dump();
}

}  // namespace meet
