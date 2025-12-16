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

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "third_party/icu/source/tools/toolutil/json-json.hpp"
#include "meet_clients/api/media_api_client_interface.h"
#include "meet_clients/api/media_stats_resource.h"

namespace meet {
namespace {
using Json = ::nlohmann::json;

// Media stats resource channel is always opened with this label.
constexpr absl::string_view kMediaStatsResourceName = "media-stats";

const Json* FindOrNull(const Json& json, absl::string_view key) {
  auto it = json.find(key);
  return it != json.cend() ? &*it : nullptr;
}

}  // namespace

absl::StatusOr<ResourceUpdate> MediaStatsResourceHandler::ParseUpdate(
    absl::string_view update) {
  VLOG(1) << kMediaStatsResourceName << " resource update received: " << update;

  const Json json_resource_update = Json::parse(update, /*cb=*/nullptr,
                                                /*allow_exceptions=*/false);

  if (!json_resource_update.is_object()) {
    return absl::InternalError(absl::StrCat("Invalid ", kMediaStatsResourceName,
                                            " json format: ", update));
  }

  MediaStatsChannelToClient media_stats_update;
  // Response
  if (const Json* response_field = FindOrNull(json_resource_update, "response");
      response_field != nullptr) {
    MediaStatsResponse response;

    // Response.requestId
    const Json* request_id_field = FindOrNull(*response_field, "requestId");
    if (request_id_field == nullptr) {
      return absl::InternalError(
          absl::StrCat("Invalid ", kMediaStatsResourceName,
                       " json format. Expected non-empty requestId field"));
    }
    response.request_id = request_id_field->get<int64_t>();

    // If no status field is present, the status is assumed to be OK.
    absl::Status status = absl::OkStatus();
    // Response.status
    if (const Json* status_field = FindOrNull(*response_field, "status");
        status_field != nullptr) {
      absl::StatusCode status_code = absl::StatusCode::kUnknown;

      // Response.status.code
      const Json* code_field = FindOrNull(*status_field, "code");
      if (code_field == nullptr) {
        return absl::InternalError(
            absl::StrCat("Invalid ", kMediaStatsResourceName,
                         " json format. Expected non-empty code field"));
      }
      status_code = static_cast<absl::StatusCode>(code_field->get<int32_t>());

      // Response.status.message
      std::string message;
      const Json* message_field = FindOrNull(*status_field, "message");
      if (message_field == nullptr) {
        return absl::InternalError(
            absl::StrCat("Invalid ", kMediaStatsResourceName,
                         " json format. Expected non-empty message field"));
      }
      message = message_field->get<std::string>();
      status = absl::Status(status_code, std::move(message));
    }
    response.status = std::move(status);

    // Response.uploadMediaStats
    if (const Json* upload_media_stats_field =
            FindOrNull(*response_field, "uploadMediaStats");
        upload_media_stats_field != nullptr) {
      response.upload_media_stats =
          MediaStatsResponse::UploadMediaStatsResponse();
    }

    media_stats_update.response = std::move(response);
  }

  // Resources
  if (const Json* resources_field =
          FindOrNull(json_resource_update, "resources");
      resources_field != nullptr) {
    // Currently, exactly one media stats resource is expected if resources
    // field is present.
    if (!resources_field->is_array() || resources_field->size() != 1) {
      return absl::InternalError(
          absl::StrCat("Invalid ", kMediaStatsResourceName,
                       " json format. Expected resources field to be an array "
                       "with exactly one element: ",
                       update));
    }
    // Resources.resourceSnapshot
    std::vector<MediaStatsResourceSnapshot> resources;

    const Json& resource_field = resources_field->at(0);
    MediaStatsResourceSnapshot snapshot;

    // Resources.resourceSnapshot.configuration
    const Json* configuration_field =
        FindOrNull(resource_field, "configuration");
    if (configuration_field == nullptr) {
      return absl::InternalError(
          absl::StrCat("Invalid ", kMediaStatsResourceName,
                       " json format. Expected non-empty configuration field"));
    }

    // Resources.resourceSnapshot.configuration.uploadIntervalSeconds
    const Json* upload_interval_seconds_field =
        FindOrNull(*configuration_field, "uploadIntervalSeconds");
    if (upload_interval_seconds_field == nullptr) {
      return absl::InternalError(
          absl::StrCat("Invalid ", kMediaStatsResourceName,
                       " json format. Expected non-empty "
                       "uploadIntervalSeconds field"));
    }
    snapshot.configuration.upload_interval_seconds =
        upload_interval_seconds_field->get<int32_t>();

    // Resources.resourceSnapshot.configuration.allowlist
    const Json* allowlist_field = FindOrNull(*configuration_field, "allowlist");
    if (allowlist_field == nullptr) {
      return absl::InternalError(
          absl::StrCat("Invalid ", kMediaStatsResourceName,
                       " json format. Expected non-empty allowlist field"));
    }
    absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>
        allowlist;
    for (auto& [section_name, section_values_field] :
         allowlist_field->items()) {
      const Json* section_keys_field = FindOrNull(section_values_field, "keys");
      if (section_keys_field == nullptr || !section_keys_field->is_array()) {
        return absl::InternalError(absl::StrCat(
            "Invalid ", kMediaStatsResourceName,
            " json format. Expected non-empty keys array: ", update));
      }

      absl::flat_hash_set<std::string> section_keys;
      for (const Json& section_key_field : *section_keys_field) {
        section_keys.insert(section_key_field.get<std::string>());
      }
      allowlist[section_name] = std::move(section_keys);
    }
    snapshot.configuration.allowlist = std::move(allowlist);
    resources.push_back(std::move(snapshot));
    media_stats_update.resources = std::move(resources);
  }
  return media_stats_update;
}

absl::StatusOr<std::string> MediaStatsResourceHandler::StringifyRequest(
    const ResourceRequest& request) {
  if (!std::holds_alternative<MediaStatsChannelFromClient>(request)) {
    return absl::InvalidArgumentError(
        "MediaStatsResourceHandler only supports MediaStatsChannelFromClient");
  }

  auto media_stats_request = std::get<MediaStatsChannelFromClient>(request);

  if (media_stats_request.request.request_id == 0) {
    return absl::InvalidArgumentError("Request ID must be set");
  }

  nlohmann::basic_json<> json_request;

  // Request.requestId
  json_request["request"]["requestId"] = media_stats_request.request.request_id;
  // Request.uploadMediaStats
  if (media_stats_request.request.upload_media_stats.has_value()) {
    // Request.uploadMediaStats.sections
    for (const MediaStatsSection& section :
         media_stats_request.request.upload_media_stats->sections) {
      nlohmann::basic_json<> section_json;
      section_json["id"] = section.id;
      for (const auto& [name, value] : section.values) {
        // TODO: Refactor media stats data handling to avoid
        // making copies of `value`. Each media stats report can include
        // multiple kilobytes of data and therefore we should avoid copying it.
        section_json[section.type][name] = value;
      }
      json_request["request"]["uploadMediaStats"]["sections"].push_back(
          std::move(section_json));
    }
  }
  return json_request.dump();
}

}  // namespace meet
