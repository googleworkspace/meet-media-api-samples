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

#include "meet_clients/internal/media_stats_resource_handler.h"

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "third_party/icu/source/tools/toolutil/json-json.hpp"
#include "meet_clients/api/media_api_client_interface.h"
#include "meet_clients/api/media_stats_resource.h"
#include "meet_clients/api/session_control_resource.h"

namespace meet {
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using Json = ::nlohmann::json;

TEST(MediaStatsResourceHandlerTest, ParsesPopulatedResource) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [{
        "configuration":{
          "uploadIntervalSeconds":10,
          "allowlist":{
            "remote-candidate":{
              "keys":[
                "id",
                "address"
              ]},
            "codec":{
              "keys":[
                "id",
                "mimeType",
                "payloadType"
              ]}
            }}}]})json");

  ASSERT_TRUE(parsed_update.ok());
  auto media_stats_update =
      std::get<MediaStatsChannelToClient>(std::move(parsed_update).value());
  EXPECT_EQ(media_stats_update.resources->size(), 1);
  EXPECT_EQ(
      media_stats_update.resources->at(0).configuration.upload_interval_seconds,
      10);
  EXPECT_THAT(
      media_stats_update.resources->at(0).configuration.allowlist,
      UnorderedElementsAre(
          Pair("remote-candidate", UnorderedElementsAre("id", "address")),
          Pair("codec",
               UnorderedElementsAre("id", "mimeType", "payloadType"))));
}

TEST(MediaStatsResourceHandlerTest, ParsesEmptyAllowlist) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [{
        "configuration":{
            "uploadIntervalSeconds":10,
            "allowlist":{}
          }}]})json");

  ASSERT_TRUE(parsed_update.ok());
  auto media_stats_update =
      std::get<MediaStatsChannelToClient>(std::move(parsed_update).value());
  EXPECT_EQ(media_stats_update.resources->size(), 1);
  EXPECT_THAT(media_stats_update.resources->at(0).configuration.allowlist,
              IsEmpty());
}

TEST(MediaStatsResourceHandlerTest, ZeroResourcesReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": []
      })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(parsed_update.status().message(),
              HasSubstr("Invalid media-stats json format. Expected resources "
                        "field to be an array with exactly one element"));
}

TEST(MediaStatsResourceHandlerTest, MultipleResourcesReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "configuration":{
            "uploadIntervalSeconds":10,
            "allowlist":{}
          }
        },
        {
          "configuration":{
            "uploadIntervalSeconds":20,
            "allowlist":{}
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected resources field to be "
          "an array with exactly one element"));
}

TEST(MediaStatsResourceHandlerTest, NonArrayResourcesReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": "non-array-value"
      })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected resources field to be "
          "an array with exactly one element"));
}

TEST(MediaStatsResourceHandlerTest, MissingConfigurationReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {}
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty configuration "
          "field"));
}

TEST(MediaStatsResourceHandlerTest,
     MissingUploadIntervalSecondsReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "configuration":{
            "allowlist":{}
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(parsed_update.status().message(),
              HasSubstr("Invalid media-stats json format. Expected non-empty "
                        "uploadIntervalSeconds field"));
}

TEST(MediaStatsResourceHandlerTest, MissingAllowlistReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "configuration":{
            "uploadIntervalSeconds":10
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr("Invalid media-stats json format. Expected non-empty allowlist "
                "field"));
}

TEST(MediaStatsResourceHandlerTest, MissingKeysReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "configuration":{
            "uploadIntervalSeconds":10,
            "allowlist":{
              "remote-candidate":{}
            }
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty keys array"));
}

TEST(MediaStatsResourceHandlerTest, NonArrayKeysReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "configuration":{
            "uploadIntervalSeconds":10,
            "allowlist":{
              "remote-candidate":{
                "keys": "non-array-value"
              }
            }
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty keys array"));
}

TEST(MediaStatsResourceHandlerTest, ParsesResponse) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "status": {
        "code": 13,
        "message": "Test Error"
      },
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_TRUE(parsed_update.ok());
  auto media_stats_update =
      std::get<MediaStatsChannelToClient>(std::move(parsed_update).value());
  EXPECT_EQ(media_stats_update.response->request_id, 1);
  EXPECT_EQ(media_stats_update.response->status.code(),
            absl::StatusCode::kInternal);
  EXPECT_EQ(media_stats_update.response->status.message(), "Test Error");
  EXPECT_TRUE(media_stats_update.response->upload_media_stats.has_value());
}

TEST(MediaStatsResourceHandlerTest, ParsesResponseWithoutUploadMediaStats) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "status": {
        "code": 13,
        "message": "Test Error"
      }
    }
  })json");
  ASSERT_TRUE(parsed_update.ok());
  auto media_stats_update =
      std::get<MediaStatsChannelToClient>(std::move(parsed_update).value());
  EXPECT_EQ(media_stats_update.response->request_id, 1);
  EXPECT_EQ(media_stats_update.response->status.code(),
            absl::StatusCode::kInternal);
  EXPECT_EQ(media_stats_update.response->status.message(), "Test Error");
  EXPECT_FALSE(media_stats_update.response->upload_media_stats.has_value());
}

TEST(MediaStatsResourceHandlerTest, MissingRequestIdReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "status": {
        "code": 13,
        "message": "Test Error"
      },
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr("Invalid media-stats json format. Expected non-empty requestId "
                "field"));
}

TEST(MediaStatsResourceHandlerTest, MissingStatusReturnsOKStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_TRUE(parsed_update.ok());
  EXPECT_TRUE(parsed_update.status().ok());
}

TEST(MediaStatsResourceHandlerTest, MissingStatusCodeReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "status": {
        "message": "Test Error"
      },
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty code field"));
}

TEST(MediaStatsResourceHandlerTest, MissingStatusMessageReturnsErrorStatus) {
  absl::StatusOr<ResourceUpdate> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "status": {
        "code": 13
      },
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty message field"));
}

TEST(MediaStatsResourceHandlerTest, ParsesClientRequest) {
  absl::StatusOr<std::string> json_request =
      MediaStatsResourceHandler().StringifyRequest(MediaStatsChannelFromClient{
          .request = MediaStatsRequest{
              .request_id = 7,
              .upload_media_stats = UploadMediaStatsRequest{
                  .sections = {
                      MediaStatsSection{.type = "codec",
                                        .id = "1",
                                        .values = {{"mimeType", "video/VP8"},
                                                   {"payloadType", "111"}}},
                      MediaStatsSection{
                          .type = "remote-candidate",
                          .id = "2",
                          .values = {{"id", "2"},
                                     {"address", "127.0.0.1"}}}}}}});
  ASSERT_TRUE(json_request.ok());
  EXPECT_THAT(*std::move(json_request), StrEq(Json::parse(R"json({
        "request": {
          "requestId": 7,
          "uploadMediaStats": {
            "sections": [
              {
                "id": "1",
                "codec": {
                  "mimeType": "video/VP8",
                  "payloadType": "111"
                }
              },
              {
                "id": "2",
                "remote-candidate": {
                  "id": "2",
                  "address": "127.0.0.1"
                }
              }
            ]
          }
        }
      })json")
                                                  .dump()));
}

TEST(MediaStatsResourceHandlerTest, NoClientRequestIdReturnsErrorStatus) {
  absl::StatusOr<std::string> json_request =
      MediaStatsResourceHandler().StringifyRequest(
          MediaStatsChannelFromClient());
  ASSERT_FALSE(json_request.ok());
  EXPECT_EQ(json_request.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(json_request.status().message(),
              HasSubstr("Request ID must be set"));
}

TEST(MediaStatsResourceHandlerTest,
     StringifyWrongRequestTypeReturnsErrorStatus) {
  absl::StatusOr<std::string> json_request =
      MediaStatsResourceHandler().StringifyRequest(
          SessionControlChannelFromClient());
  ASSERT_FALSE(json_request.ok());
  EXPECT_EQ(json_request.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(json_request.status().message(),
              HasSubstr("MediaStatsResourceHandler only supports "
                        "MediaStatsChannelFromClient"));
}

}  // namespace
}  // namespace meet
