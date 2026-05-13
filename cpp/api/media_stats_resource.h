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

#ifndef CPP_API_MEDIA_STATS_RESOURCE_H_
#define CPP_API_MEDIA_STATS_RESOURCE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {

struct MediaStatsResponse {
  struct UploadMediaStatsResponse {};

  int64_t request_id = 0;
  /// The response status from Meet servers to an incoming request. This should
  /// be used by clients to determine the outcome of the request.
  absl::Status status;
  std::optional<UploadMediaStatsResponse> upload_media_stats;
};

/// The configuration for the media stats upload. This will be sent by the
/// server to the client when the data channel is opened. The client is then
/// expected to start uploading media stats at the specified interval.
///
/// This configuration is immutable and a singleton and will only be sent once
/// when the data channel is opened.
struct MediaStatsConfiguration {
  /// The interval between each upload of media stats. If this is zero, the
  /// client should not upload any media stats.
  int32_t upload_interval_seconds = 0;
  /// A map of allowlisted `RTCStats` sections. The key is the section type, and
  /// the value is a vector of the names of data that are allowlisted for that
  /// section.
  ///
  /// Allowlisted sections and section data are expected to be uploaded by the
  /// client. Other data will be ignored by the server and can be safely
  /// omitted.
  ///
  /// @see [WebRTC
  /// Stats](https://w3c.github.io/webrtc-pc/#mandatory-to-implement-stats)
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> allowlist;
};

/// A resource snapshot managed by the server and replicated to the client.
struct MediaStatsResourceSnapshot {
  /// The media stats resource is a singleton resource. Therefore, this ID is
  /// always 0.
  int64_t id = 0;
  MediaStatsConfiguration configuration;
};

/// The top-level transport container for messages sent from server to client in
/// the `media-stats` data channel. Any combination of fields may be set, but
/// the message is never empty.
struct MediaStatsChannelToClient {
  /// An optional response to an incoming request.
  std::optional<MediaStatsResponse> response;
  /// Resource snapshots.
  std::optional<std::vector<MediaStatsResourceSnapshot>> resources;
};

/// This type represents an `RTCStats`-derived dictionary which is returned by
/// calling `RTCPeerConnection::getStats`.
///
/// @see [WebRTC
/// Stats](https://w3c.github.io/webrtc-pc/#mandatory-to-implement-stats)
struct MediaStatsSection {
  /// The
  /// [RTCStatsType](https://www.w3.org/TR/webrtc-stats/#rtcstatstype-str*) of
  /// the section.
  ///
  /// For example, `codec` or `candidate-pair`.
  std::string type;
  /// The WebRTC-generated ID of the section.
  std::string id;
  /// The stats and their values for this section.
  /// @see [WebRTC
  /// Stats](https://w3c.github.io/webrtc-pc/#mandatory-to-implement-stats)
  absl::flat_hash_map<std::string, std::string> values;
};

/// Uploads media stats from the client to the server. The stats are retrieved
/// from WebRTC by calling `RTCPeerConnection::getStats` and the returned
/// [RTCStatsReport](https://w3c.github.io/webrtc-pc/#dom-rtcstatsreport) can
/// be easily mapped to the sections below.
struct UploadMediaStatsRequest {
  /// Represents the entries in
  /// [RTCStatsReport](https://w3c.github.io/webrtc-pc/#dom-rtcstatsreport).
  std::vector<MediaStatsSection> sections;
};

struct MediaStatsRequest {
  /// A unique client-generated identifier for this request. Different requests
  /// must never have the same request ID.
  int64_t request_id = 0;
  /// Request payload.
  std::optional<UploadMediaStatsRequest> upload_media_stats;
};

/// The top-level transport container for messages sent from client to server in
/// the `media-stats` data channel.
struct MediaStatsChannelFromClient {
  MediaStatsRequest request;
};

}  // namespace meet

#endif  // CPP_API_MEDIA_STATS_RESOURCE_H_
