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

#ifndef CPP_API_VIDEO_ASSIGNMENT_RESOURCE_H_
#define CPP_API_VIDEO_ASSIGNMENT_RESOURCE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {

/// Required dimensions of the canvas.
struct CanvasDimensions {
  /// The vertical space, in pixels, for this canvas.
  int32_t height = 480;
  /// The horizontal space, in pixels, for this canvas.
  int32_t width = 640;
};

struct VideoCanvas {
  enum class AssignmentProtocol {
    kRelevant,
    kDirect,
  };
  /// An identifier for the video canvas.
  /// This is required and must be unique within the containing `LayoutModel`.
  /// Clients should prudently reuse `VideoCanvas` IDs. This allows the backend
  /// to keep assigning video streams to the same canvas as much as possible.
  int32_t id = 0;

  /// The dimensions for this video canvas. Failure to provide this
  /// will result in an error.
  CanvasDimensions dimensions;

  /// The protocol that governs how the backend should assign a video
  /// feed to this canvas.
  AssignmentProtocol assignment_protocol = AssignmentProtocol::kRelevant;
};

struct LayoutModel {
  /// A client-specified identifier for this assignment. The identifier
  /// will be used to reference a given `LayoutModel` in subsequent
  /// `VideoAssignment` resource update pushed from server to client.
  std::string label;

  /// The canvases that videos are assigned to from each virtual SSRC.
  /// Providing more canvases than exists virtual streams will result in
  /// an error status.
  std::vector<VideoCanvas> canvases;
};

struct VideoResolution {
  /// The height and width are in square pixels. For cameras that can change
  /// orientation, the width refers to the measurement on the horizontal axis,
  /// and the height on the vertical.
  int32_t height = 480;
  int32_t width = 640;
  /// The frame rate in frames per second.
  int32_t frame_rate = 30;
};

struct SetVideoAssignmentRequest {
  /// The new video layout to use. This replaces any previously active video
  /// layout.
  LayoutModel layout_model;
  /// The maximum video resolution the client wants to receive for any video
  /// feed.
  VideoResolution video_resolution;
};

struct VideoAssignmentRequest {
  /// A unique client-generated identifier for this request. Different requests
  /// must never have the same request ID.
  int64_t request_id;

  std::optional<SetVideoAssignmentRequest> set_video_assignment_request;
};

/// The top-level transport container for messages sent from client to
/// server in the `video-assignment` data channel.
struct VideoAssignmentChannelFromClient {
  VideoAssignmentRequest request;
};

struct VideoAssignmentResponse {
  struct SetVideoAssignmentResponse {};

  /// The request ID in the request this is the response to.
  int64_t request_id;
  /// The response status for this request. This should be used by clients to
  /// determine the RPC result.
  absl::Status status;
  std::optional<SetVideoAssignmentResponse> set_assignment;
};

struct VideoCanvasAssignment {
  /// The video canvas the video should be shown in.
  int32_t canvas_id = 0;
  /// The virtual video SSRC that the video will be sent over, or zero if
  /// there is no video from the participant.
  uint32_t ssrc = 0;
  /// ID of the `MediaEntry` of the media whose video is being shown.
  int32_t media_entry_id = 0;
};

struct VideoAssignment {
  /// The `LayoutModel` that this assignment is based on. Taken from
  /// `LayoutModel::label`.
  std::string label;
  /// The individual canvas assignments, in no particular order.
  std::vector<VideoCanvasAssignment> canvases;
};

/// A resource snapshot managed by the server and replicated to the client.
struct VideoAssignmentResourceSnapshot {
  /// The video assignment resource is a singleton resource. Therefore, this ID
  /// is always 0.
  int64_t id = 0;

  std::optional<VideoAssignment> assignment;
};

/// The top-level transport container for messages sent from server to
/// client in the `video-assignment` data channel. Any combination of fields may
/// be set, but the message is never empty.
struct VideoAssignmentChannelToClient {
  /// An optional response to a incoming request.
  std::optional<VideoAssignmentResponse> response;
  /// Resource snapshots. There is no implied order between the snapshots in the
  /// list.
  std::vector<VideoAssignmentResourceSnapshot> resources;
};

}  // namespace meet

#endif  // CPP_API_VIDEO_ASSIGNMENT_RESOURCE_H_
