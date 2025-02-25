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

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "nlohmann/json.hpp"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/api/media_stats_resource.h"
#include "cpp/api/session_control_resource.h"

namespace meet {
namespace {
using ::testing::HasSubstr;
using ::testing::SizeIs;
using ::testing::StrEq;
using Json = ::nlohmann::json;

TEST(SessionControlResourceHandlerTest, ParsesMultipleResourceSnapshots) {
  SessionControlResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "resources": [
          {
            "sessionStatus": {
              "connectionState": "STATE_JOINED"
            }
          },
          {
            "sessionStatus": {
              "connectionState": "STATE_DISCONNECTED"
            }
          },
          {
            "sessionStatus": {
              "connectionState": "STATE_WAITING"
            }
          },
          {
            "sessionStatus": {
              "connectionState": "STATE_UNKNOWN"
            }
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto session_control_update = std::get<SessionControlChannelToClient>(
      std::move(status_or_parsed_update).value());

  ASSERT_THAT(session_control_update.resources, SizeIs(4));
  EXPECT_EQ(session_control_update.resources[0].session_status.connection_state,
            SessionStatus::ConferenceConnectionState::kJoined);
  EXPECT_EQ(session_control_update.resources[1].session_status.connection_state,
            SessionStatus::ConferenceConnectionState::kDisconnected);
  EXPECT_EQ(session_control_update.resources[2].session_status.connection_state,
            SessionStatus::ConferenceConnectionState::kWaiting);
  EXPECT_EQ(session_control_update.resources[3].session_status.connection_state,
            SessionStatus::ConferenceConnectionState::kUnknown);
}

TEST(SessionControlResourceHandlerTest, ParsesDisconnectReasons) {
  SessionControlResourceHandler handler;
  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "resources": [
          {
            "sessionStatus": { "disconnectReason": "REASON_UNKNOWN" }
          },
          {
            "sessionStatus": { "disconnectReason": "REASON_CLIENT_LEFT" }
          },
          {
            "sessionStatus": { "disconnectReason": "REASON_USER_STOPPED" }
          },
          {
            "sessionStatus": { "disconnectReason": "REASON_CONFERENCE_ENDED" }
          },
          {
            "sessionStatus": { "disconnectReason": "REASON_SESSION_UNHEALTHY" }
          }
        ]
    })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  auto session_control_update = std::get<SessionControlChannelToClient>(
      std::move(status_or_parsed_update).value());

  ASSERT_THAT(session_control_update.resources, SizeIs(5));
  EXPECT_EQ(
      session_control_update.resources[0].session_status.disconnect_reason,
      std::nullopt);
  EXPECT_EQ(
      session_control_update.resources[1].session_status.disconnect_reason,
      SessionStatus::MeetingDisconnectReason::kClientLeft);
  EXPECT_EQ(
      session_control_update.resources[2].session_status.disconnect_reason,
      SessionStatus::MeetingDisconnectReason::kUserStopped);
  EXPECT_EQ(
      session_control_update.resources[3].session_status.disconnect_reason,
      SessionStatus::MeetingDisconnectReason::kConferenceEnded);
  EXPECT_EQ(
      session_control_update.resources[4].session_status.disconnect_reason,
      SessionStatus::MeetingDisconnectReason::kSessionUnhealthy);
}

TEST(SessionControlResourceHandlerTest, ResourcesUpdateEmptyArrayParsesJson) {
  SessionControlResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "resources": []
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto session_control_update = std::get<SessionControlChannelToClient>(
      std::move(status_or_parsed_update).value());

  EXPECT_THAT(session_control_update.resources, SizeIs(0));
}

TEST(SessionControlResourceHandlerTest, ParsesResponseField) {
  SessionControlResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "response": {
          "requestId": 123,
          "status": {
            "code": 13,
            "message": "The answer to life is 42"},
          "leave": {}
        }
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto session_control_update = std::get<SessionControlChannelToClient>(
      std::move(status_or_parsed_update).value());

  ASSERT_TRUE(session_control_update.response.has_value());
  EXPECT_EQ(session_control_update.response->request_id, 123);
  EXPECT_EQ(session_control_update.response->status.code(),
            absl::StatusCode::kInternal);
  EXPECT_EQ(session_control_update.response->status.message(),
            "The answer to life is 42");
}

TEST(SessionControlResourceHandlerTest, MalformedJsonReturnsErrorStatus) {
  SessionControlResourceHandler handler;
  absl::StatusOr<ResourceUpdate> parsed_update =
      handler.ParseUpdate(" random garbage that is not json!");

  EXPECT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_EQ(
      parsed_update.status().message(),
      "Invalid session-control json format:  random garbage that is not json!");
}

TEST(SessionControlResourceHandlerTest, UnexpectedResourcesReturnsErrorStatus) {
  SessionControlResourceHandler handler;
  absl::StatusOr<ResourceUpdate> parsed_update = handler.ParseUpdate(R"json({
        "resources": {
          "sessionStatus": {
            "connectionState": "STATE_JOINED"
          }
        }
    })json");

  EXPECT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_TRUE(absl::StrContains(parsed_update.status().message(),
                                "Invalid session-control json format. Expected "
                                "resources field to be an array:"));
}

TEST(SessionControlResourceHandlerTest,
     NoConnectionStateIsUnknownConnectionState) {
  SessionControlResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": [
          {
            "sessionStatus": {}
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto session_control_update = std::get<SessionControlChannelToClient>(
      std::move(status_or_parsed_update).value());

  ASSERT_THAT(session_control_update.resources, SizeIs(1));
  EXPECT_EQ(session_control_update.resources[0].session_status.connection_state,
            SessionStatus::ConferenceConnectionState::kUnknown);
}

TEST(SessionControlResourceHandlerTest,
     NoDisconnectReasonIsUnknownDisconnectReason) {
  SessionControlResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": [
          {
            "sessionStatus": {}
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto session_control_update = std::get<SessionControlChannelToClient>(
      std::move(status_or_parsed_update).value());

  ASSERT_THAT(session_control_update.resources, SizeIs(1));
  EXPECT_EQ(
      session_control_update.resources[0].session_status.disconnect_reason,
      std::nullopt);
}

TEST(SessionControlResourceHandlerTest, ParsesClientRequestId) {
  SessionControlChannelFromClient resource_request;
  resource_request.request.request_id = 42;
  SessionControlResourceHandler handler;

  absl::StatusOr<std::string> status_or_json_request =
      handler.StringifyRequest(resource_request);
  ASSERT_TRUE(status_or_json_request.ok());
  std::string json_request = status_or_json_request.value();
  EXPECT_THAT(json_request, StrEq(Json::parse(R"json({
    "request": {
      "requestId": 42
    }
  })json")
                                      .dump()));
}

TEST(SessionControlResourceHandlerTest, ParseLeaveRequest) {
  SessionControlChannelFromClient resource_request;
  resource_request.request.request_id = 42;
  resource_request.request.leave_request = LeaveRequest();
  SessionControlResourceHandler handler;

  absl::StatusOr<std::string> status_or_json_request =
      handler.StringifyRequest(resource_request);
  ASSERT_TRUE(status_or_json_request.ok());
  std::string json_request = status_or_json_request.value();
  EXPECT_THAT(json_request, StrEq(Json::parse(R"json({
    "request": {
      "requestId": 42,
      "leave": {}
    }
  })json")
                                      .dump()));
}

TEST(SessionControlResourceHandlerTest, NoClientRequestIdReturnsErrorStatus) {
  SessionControlChannelFromClient resource_request;
  resource_request.request.request_id = 0;

  SessionControlResourceHandler handler;

  absl::StatusOr<std::string> status_or_string =
      handler.StringifyRequest(resource_request);
  EXPECT_EQ(status_or_string.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status_or_string.status().message(),
              HasSubstr("Request ID must be set"));
}

TEST(SessionControlResourceHandlerTest,
     StringifyWrongRequestTypeReturnsErrorStatus) {
  absl::StatusOr<std::string> json_request =
      SessionControlResourceHandler().StringifyRequest(
          MediaStatsChannelFromClient());
  ASSERT_FALSE(json_request.ok());
  EXPECT_EQ(json_request.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(json_request.status().message(),
              HasSubstr("SessionControlResourceHandler only supports "
                        "SessionControlChannelFromClient"));
}

}  // namespace
}  // namespace meet
