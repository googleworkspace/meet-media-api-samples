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

#include "meet_clients/internal/curl_connector.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include <curl/curl.h>
#include "third_party/icu/source/tools/toolutil/json-json.hpp"
#include "meet_clients/internal/testing/mock_curl_api_wrapper.h"

namespace meet {
namespace {

using Json = ::nlohmann::json;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::StrEq;
using ::testing::status::StatusIs;

TEST(CurlConnectorTest, PopulatesRequest) {
  CURLoption populated_option;
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptInt)
      .WillOnce([&populated_option](CURL* curl, CURLoption option, int value) {
        populated_option = option;
        return CURLE_OK;
      });
  std::string url;
  EXPECT_CALL(*mock_curl_api, EasySetOptStr(_, CURLOPT_URL, _))
      .WillOnce(
          [&url](CURL* curl, CURLoption option, const std::string& value) {
            url = value;
            return CURLE_OK;
          });
  std::vector<std::string> headers;
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce([&headers](CURL* curl, CURLoption option, void* value) {
        auto headers_list = static_cast<struct curl_slist*>(value);
        while (headers_list != nullptr) {
          headers.push_back(headers_list->data);
          headers_list = headers_list->next;
        }
        return CURLE_OK;
      });
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce(
          [](CURL* curl, CURLoption option, void* value) { return CURLE_OK; });
  std::string body;
  EXPECT_CALL(*mock_curl_api, EasySetOptStr(_, CURLOPT_POSTFIELDS, _))
      .WillOnce(
          [&body](CURL* curl, CURLoption option, const std::string& value) {
            body = value;
            return CURLE_OK;
          });
  EXPECT_CALL(*mock_curl_api,
              EasySetOptStr(_, CURLOPT_CAINFO, "some_ca_cert_path"))
      .WillOnce([](CURL* curl, CURLoption option, const std::string& value) {
        return CURLE_OK;
      });
  CurlConnector curl_connector(std::move(mock_curl_api));
  curl_connector.SetCaCertPath("some_ca_cert_path");

  curl_connector
      .ConnectActiveConference("https://meet.googleapis.com", "abcdefg",
                               "bearer_token", "some sdp offer")
      .IgnoreError();

  EXPECT_EQ(populated_option, CURLOPT_POST);
  EXPECT_EQ(
      url,
      "https://meet.googleapis.com/spaces/abcdefg:connectActiveConference");
  EXPECT_THAT(headers,
              UnorderedElementsAre(
                  StrEq("Content-Type: application/json;charset=UTF-8"),
                  StrEq("Authorization: Bearer bearer_token")));
  EXPECT_EQ(body, "{\"offer\":\"some sdp offer\"}");
}

TEST(CurlConnectorTest, ReturnsResponse) {
  nlohmann::basic_json<> response_body;
  response_body["answer"] = "some sdp answer";
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([&response_body](CURL* curl, CURLoption option, void* value) {
        absl::Cord curl_response(response_body.dump());
        absl::Cord* str_response = reinterpret_cast<absl::Cord*>(value);
        *str_response = curl_response;
        return CURLE_OK;
      });
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(
          [](CURL* curl, CURLoption option, void* value) { return CURLE_OK; });
  CurlConnector curl_connector(std::move(mock_curl_api));

  absl::StatusOr<std::string> response = curl_connector.ConnectActiveConference(
      "https://meet.googleapis.com", "abcdefg", "bearer_token",
      "some sdp offer");

  ASSERT_TRUE(response.ok());
  EXPECT_EQ(response.value(), "some sdp answer");
}

struct StatusCodeTestParams {
  std::string status_str;
  absl::StatusCode status_code;
};

using CurlConnectorStatusCodeTest =
    ::testing::TestWithParam<StatusCodeTestParams>;

TEST_P(CurlConnectorStatusCodeTest, ReturnsErrorFromResponse) {
  const StatusCodeTestParams& params = GetParam();
  nlohmann::basic_json<> response_body;
  response_body["error"]["status"] = params.status_str;
  response_body["error"]["message"] = "some error message";
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([&response_body](CURL* curl, CURLoption option, void* value) {
        absl::Cord curl_response(response_body.dump());
        absl::Cord* str_response = reinterpret_cast<absl::Cord*>(value);
        *str_response = curl_response;
        return CURLE_OK;
      });
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(
          [](CURL* curl, CURLoption option, void* value) { return CURLE_OK; });
  CurlConnector curl_connector(std::move(mock_curl_api));

  absl::StatusOr<std::string> response = curl_connector.ConnectActiveConference(
      "https://meet.googleapis.com", "abcdefg", "bearer_token",
      "some sdp offer");

  EXPECT_THAT(response, StatusIs(params.status_code,
                                 AllOf(HasSubstr(params.status_str),
                                       HasSubstr("some error message"))));
}

INSTANTIATE_TEST_SUITE_P(
    CurlConnectorStatusCodeTest, CurlConnectorStatusCodeTest,
    ::testing::Values(
        StatusCodeTestParams{"CANCELLED", absl::StatusCode::kCancelled},
        StatusCodeTestParams{"UNKNOWN", absl::StatusCode::kUnknown},
        StatusCodeTestParams{"INVALID_ARGUMENT",
                             absl::StatusCode::kInvalidArgument},
        StatusCodeTestParams{"DEADLINE_EXCEEDED",
                             absl::StatusCode::kDeadlineExceeded},
        StatusCodeTestParams{"NOT_FOUND", absl::StatusCode::kNotFound},
        StatusCodeTestParams{"ALREADY_EXISTS",
                             absl::StatusCode::kAlreadyExists},
        StatusCodeTestParams{"PERMISSION_DENIED",
                             absl::StatusCode::kPermissionDenied},
        StatusCodeTestParams{"UNAUTHENTICATED",
                             absl::StatusCode::kUnauthenticated},
        StatusCodeTestParams{"RESOURCE_EXHAUSTED",
                             absl::StatusCode::kResourceExhausted},
        StatusCodeTestParams{"FAILED_PRECONDITION",
                             absl::StatusCode::kFailedPrecondition},
        StatusCodeTestParams{"ABORTED", absl::StatusCode::kAborted},
        StatusCodeTestParams{"OUT_OF_RANGE", absl::StatusCode::kOutOfRange},
        StatusCodeTestParams{"UNIMPLEMENTED", absl::StatusCode::kUnimplemented},
        StatusCodeTestParams{"INTERNAL", absl::StatusCode::kInternal},
        StatusCodeTestParams{"UNAVAILABLE", absl::StatusCode::kUnavailable},
        StatusCodeTestParams{"DATA_LOSS", absl::StatusCode::kDataLoss},
        StatusCodeTestParams{"SOME_GARBAGE_STATUS",
                             absl::StatusCode::kUnknown}));

TEST(CurlConnectorTest,
     ReturnsDefaultErrorWhenErrorDetailsAreMissingInResponse) {
  nlohmann::basic_json<> response_body;
  response_body["error"][""] = "";
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([&response_body](CURL* curl, CURLoption option, void* value) {
        absl::Cord curl_response(response_body.dump());
        absl::Cord* str_response = reinterpret_cast<absl::Cord*>(value);
        *str_response = curl_response;
        return CURLE_OK;
      });
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(
          [](CURL* curl, CURLoption option, void* value) { return CURLE_OK; });
  CurlConnector curl_connector(std::move(mock_curl_api));

  absl::StatusOr<std::string> response = curl_connector.ConnectActiveConference(
      "https://meet.googleapis.com", "abcdefg", "bearer_token",
      "some sdp offer");

  EXPECT_THAT(response, StatusIs(absl::StatusCode::kUnknown,
                                 StrEq(response_body.dump())));
}

TEST(CurlConnectorTest, ReturnsErrorWhenResponseIsEmpty) {
  nlohmann::basic_json<> response_body;
  response_body[""] = "";
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([&response_body](CURL* curl, CURLoption option, void* value) {
        absl::Cord curl_response(response_body.dump());
        absl::Cord* str_response = reinterpret_cast<absl::Cord*>(value);
        *str_response = curl_response;
        return CURLE_OK;
      });
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(
          [](CURL* curl, CURLoption option, void* value) { return CURLE_OK; });
  CurlConnector curl_connector(std::move(mock_curl_api));

  absl::StatusOr<std::string> response = curl_connector.ConnectActiveConference(
      "https://meet.googleapis.com", "abcdefg", "bearer_token",
      "some sdp offer");

  ASSERT_FALSE(response.ok());
  EXPECT_THAT(response.status().message(),
              HasSubstr("Received response without `answer` or `error` field"));
}

TEST(CurlConnectorTest, ReturnsErrorWhenResponseIsNotJson) {
  std::string response_body = "not json";
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([&response_body](CURL* curl, CURLoption option, void* value) {
        absl::Cord* str_response = reinterpret_cast<absl::Cord*>(value);
        *str_response = response_body;
        return CURLE_OK;
      });
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(
          [](CURL* curl, CURLoption option, void* value) { return CURLE_OK; });
  CurlConnector curl_connector(std::move(mock_curl_api));

  absl::StatusOr<std::string> response = curl_connector.ConnectActiveConference(
      "https://meet.googleapis.com", "abcdefg", "bearer_token",
      "some sdp offer");

  ASSERT_FALSE(response.ok());
  EXPECT_THAT(response.status().message(),
              HasSubstr("Unparseable or non-json response from Meet servers"));
}

}  // namespace
}  // namespace meet
