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

#include "cpp/internal/curl_connector.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "nlohmann/json.hpp"
#include "cpp/internal/curl_request.h"

namespace meet {
namespace {

using ::nlohmann::basic_json;
using ::nlohmann::json;

const json* FindOrNull(const json& json, absl::string_view key) {
  auto it = json.find(key);
  return it != json.cend() ? &*it : nullptr;
}

absl::StatusCode GetStatusCodeFromString(absl::string_view status_str) {
  if (status_str == "OK") return absl::StatusCode::kOk;
  if (status_str == "CANCELLED") return absl::StatusCode::kCancelled;
  if (status_str == "UNKNOWN") return absl::StatusCode::kUnknown;
  if (status_str == "INVALID_ARGUMENT")
    return absl::StatusCode::kInvalidArgument;
  if (status_str == "DEADLINE_EXCEEDED")
    return absl::StatusCode::kDeadlineExceeded;
  if (status_str == "NOT_FOUND") return absl::StatusCode::kNotFound;
  if (status_str == "ALREADY_EXISTS") return absl::StatusCode::kAlreadyExists;
  if (status_str == "PERMISSION_DENIED")
    return absl::StatusCode::kPermissionDenied;
  if (status_str == "UNAUTHENTICATED")
    return absl::StatusCode::kUnauthenticated;
  if (status_str == "RESOURCE_EXHAUSTED")
    return absl::StatusCode::kResourceExhausted;
  if (status_str == "FAILED_PRECONDITION")
    return absl::StatusCode::kFailedPrecondition;
  if (status_str == "ABORTED") return absl::StatusCode::kAborted;
  if (status_str == "OUT_OF_RANGE") return absl::StatusCode::kOutOfRange;
  if (status_str == "UNIMPLEMENTED") return absl::StatusCode::kUnimplemented;
  if (status_str == "INTERNAL") return absl::StatusCode::kInternal;
  if (status_str == "UNAVAILABLE") return absl::StatusCode::kUnavailable;
  if (status_str == "DATA_LOSS") return absl::StatusCode::kDataLoss;
  return absl::StatusCode::kUnknown;
}

}  // namespace

absl::StatusOr<std::string> CurlConnector::ConnectActiveConference(
    absl::string_view join_endpoint, absl::string_view conference_id,
    absl::string_view access_token, absl::string_view sdp_offer) {
  std::string full_join_endpoint = absl::StrCat(
      join_endpoint, "/spaces/", conference_id, ":connectActiveConference");

  VLOG(1) << "Connecting to " << full_join_endpoint;

  CurlRequest curl_request(*curl_api_wrapper_);
  curl_request.SetRequestUrl(std::move(full_join_endpoint));
  curl_request.SetRequestHeader("Content-Type",
                                "application/json;charset=UTF-8");
  curl_request.SetRequestHeader("Authorization",
                                absl::StrCat("Bearer ", access_token));
  if (ca_cert_path_.has_value()) {
    curl_request.SetCaCertPath(*ca_cert_path_);
  }

  nlohmann::basic_json<> offer_json;
  offer_json["offer"] = sdp_offer;
  std::string offer_json_string = offer_json.dump();

  VLOG(1) << "Join request offer: " << offer_json_string;
  curl_request.SetRequestBody(std::move(offer_json_string));

  absl::Status response_status = curl_request.Send();
  if (!response_status.ok()) {
    return response_status;
  }

  const json json_request_response =
      json::parse(curl_request.GetResponseData(), /*cb=*/nullptr,
                  /*allow_exceptions=*/false);

  VLOG(1) << "Parsing response from Meet servers: "
          << json_request_response.dump();

  if (!json_request_response.is_object()) {
    return absl::UnknownError(
        absl::StrCat("Unparseable or non-json response from Meet servers, ",
                     curl_request.GetResponseData()));
  }

  if (const json* answer_field = FindOrNull(json_request_response, "answer");
      answer_field != nullptr) {
    return answer_field->get<std::string>();
  }

  if (const json* error_field = FindOrNull(json_request_response, "error");
      error_field != nullptr) {
    absl::StatusCode status_code = absl::StatusCode::kUnknown;
    if (const json* code_field = FindOrNull(*error_field, "status");
        code_field != nullptr) {
      status_code = GetStatusCodeFromString(code_field->get<std::string>());
    }

    std::string response_data = curl_request.GetResponseData();

    return absl::Status(status_code, response_data);
  }

  return absl::UnknownError(
      absl::StrCat("Received response without `answer` or `error` field: ",
                   json_request_response.dump()));
};

}  // namespace meet
