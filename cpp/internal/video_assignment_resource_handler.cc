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

#include "meet_clients/internal/video_assignment_resource_handler.h"

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "third_party/icu/source/tools/toolutil/json-json.hpp"
#include "meet_clients/api/media_api_client_interface.h"
#include "meet_clients/api/video_assignment_resource.h"

namespace meet {
namespace {
using Json = ::nlohmann::json;
using enum VideoCanvas::AssignmentProtocol;

// Video assignment resource channel is always opened with this label.
constexpr absl::string_view kVideoAssignmentResourceName = "video-assignment";

const Json* FindOrNull(const Json& json, absl::string_view key) {
  auto it = json.find(key);
  return it != json.cend() ? &*it : nullptr;
}

}  // namespace

absl::StatusOr<ResourceUpdate> VideoAssignmentResourceHandler::ParseUpdate(
    absl::string_view update) {
  VLOG(1) << kVideoAssignmentResourceName
          << " resource update received: " << update;

  const Json json_resource_update = Json::parse(update, /*cb=*/nullptr,
                                                /*allow_exceptions=*/false);

  if (!json_resource_update.is_object()) {
    return absl::InternalError(absl::StrCat(
        "Invalid ", kVideoAssignmentResourceName, " json format: ", update));
  }

  VideoAssignmentChannelToClient video_assignment_update;
  // Response
  if (const Json* response_field = FindOrNull(json_resource_update, "response");
      response_field != nullptr) {
    VideoAssignmentResponse response;

    // Response.requestId
    if (const Json* request_id_field = FindOrNull(*response_field, "requestId");
        request_id_field != nullptr) {
      response.request_id = request_id_field->get<int64_t>();
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
      response.status = absl::Status(status_code, message);
    }

    // Response.setAssignment
    if (const Json* set_assignment_field =
            FindOrNull(*response_field, "setAssignment");
        set_assignment_field != nullptr) {
      response.set_assignment =
          VideoAssignmentResponse::SetVideoAssignmentResponse();
    }

    video_assignment_update.response = std::move(response);
  }

  // Resources
  if (const Json* resources_field =
          FindOrNull(json_resource_update, "resources");
      resources_field != nullptr) {
    if (!resources_field->is_array()) {
      return absl::InternalError(absl::StrCat(
          "Invalid ", kVideoAssignmentResourceName,
          " json format. Expected resources field to be an array: ", update));
    }

    // Resources.resourceSnapshot
    std::vector<VideoAssignmentResourceSnapshot>& resources =
        video_assignment_update.resources;
    for (const Json& resource : *resources_field) {
      VideoAssignmentResourceSnapshot snapshot;

      // Resources.resourceSnapshot.assignment
      if (const Json* assignment_field =
              FindOrNull(resource, "videoAssignment");
          assignment_field != nullptr) {
        VideoAssignment assignment;

        // Resources.resourceSnapshot.assignment.label
        if (const Json* label_field = FindOrNull(*assignment_field, "label");
            label_field != nullptr) {
          assignment.label = label_field->get<std::string>();
        }

        // Resources.resourceSnapshot.assignment.canvases
        if (const Json* canvases_field =
                FindOrNull(*assignment_field, "canvases");
            canvases_field != nullptr) {
          if (!canvases_field->is_array()) {
            return absl::InternalError(absl::StrCat(
                "Invalid ", kVideoAssignmentResourceName,
                " json format. Expected canvases field to be an array: ",
                update));
          }

          std::vector<VideoCanvasAssignment>& canvases = assignment.canvases;
          for (const Json& canvas : *canvases_field) {
            VideoCanvasAssignment canvas_assignment;

            // Resources.resourceSnapshot.assignment.canvases.canvasId
            if (const Json* canvas_id_field = FindOrNull(canvas, "canvasId");
                canvas_id_field != nullptr) {
              canvas_assignment.canvas_id = canvas_id_field->get<int32_t>();
            }

            // Resources.resourceSnapshot.assignment.canvases.ssrcs
            if (const Json* ssrcs_field = FindOrNull(canvas, "ssrc");
                ssrcs_field != nullptr) {
              canvas_assignment.ssrc = ssrcs_field->get<uint32_t>();
            }

            // Resources.resourceSnapshot.assignment.canvases.mediaEntryId
            if (const Json* media_entry_id_field =
                    FindOrNull(canvas, "mediaEntryId");
                media_entry_id_field != nullptr) {
              canvas_assignment.media_entry_id =
                  media_entry_id_field->get<int32_t>();
            }
            canvases.push_back(std::move(canvas_assignment));
          }
        }
        snapshot.assignment = std::move(assignment);
      }
      resources.push_back(std::move(snapshot));
    }
  }
  return std::move(video_assignment_update);
}

absl::StatusOr<std::string> VideoAssignmentResourceHandler::StringifyRequest(
    const ResourceRequest& request) {
  if (!std::holds_alternative<VideoAssignmentChannelFromClient>(request)) {
    return absl::InvalidArgumentError(
        "VideoAssignmentResourceHandler only supports "
        "VideoAssignmentChannelFromClient");
  }

  auto video_assignment_request =
      std::get<VideoAssignmentChannelFromClient>(request);

  nlohmann::basic_json<> video_assignment_channel_from_client;

  if (video_assignment_request.request.request_id == 0) {
    return absl::InvalidArgumentError("Request ID must be set");
  }

  // Request.requestId
  video_assignment_channel_from_client["request"]["requestId"] =
      video_assignment_request.request.request_id;
  // Request.setAssignment
  if (video_assignment_request.request.set_video_assignment_request
          .has_value()) {
    // Request.setAssignment.layoutModel.label
    video_assignment_channel_from_client["request"]["setAssignment"]
                                        ["layoutModel"]["label"] =
                                            video_assignment_request.request
                                                .set_video_assignment_request
                                                ->layout_model.label;

    // Request.setAssignment.layoutModel.canvases
    std::vector<VideoCanvas> canvases =
        video_assignment_request.request.set_video_assignment_request
            ->layout_model.canvases;
    for (const auto& canvas : canvases) {
      if (canvas.id == 0) {
        return absl::InvalidArgumentError("Canvas ID must be set");
      }

      nlohmann::basic_json<> video_canvas;
      video_canvas["id"] = canvas.id;
      video_canvas["dimensions"]["height"] = canvas.dimensions.height;
      video_canvas["dimensions"]["width"] = canvas.dimensions.width;
      if (canvas.assignment_protocol == kDirect) {
        video_canvas["direct"] = nlohmann::json::value_t::object;
      } else {
        video_canvas["relevant"] = nlohmann::json::value_t::object;
      }
      video_assignment_channel_from_client["request"]["setAssignment"]
                                          ["layoutModel"]["canvases"]
                                              .push_back(
                                                  std::move(video_canvas));
    }

    // Request.setAssignment.videoResolution
    video_assignment_channel_from_client["request"]["setAssignment"]
                                        ["maxVideoResolution"]["height"] =
                                            video_assignment_request.request
                                                .set_video_assignment_request
                                                ->video_resolution.height;
    video_assignment_channel_from_client["request"]["setAssignment"]
                                        ["maxVideoResolution"]["width"] =
                                            video_assignment_request.request
                                                .set_video_assignment_request
                                                ->video_resolution.width;
    video_assignment_channel_from_client["request"]["setAssignment"]
                                        ["maxVideoResolution"]["frameRate"] =
                                            video_assignment_request.request
                                                .set_video_assignment_request
                                                ->video_resolution.frame_rate;
  }
  return video_assignment_channel_from_client.dump();
}

}  // namespace meet
