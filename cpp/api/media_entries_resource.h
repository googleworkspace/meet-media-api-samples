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

#ifndef CPP_API_MEDIA_ENTRIES_RESOURCE_H_
#define CPP_API_MEDIA_ENTRIES_RESOURCE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {

struct MediaEntry {
  /// Participant resource name, not display name. There is a many
  /// (participant) to one (media entry) relationship.
  ///
  /// **Format:**
  /// `conferenceRecords/{conference_record}/participants/{participant}`
  ///
  /// Use this to correlate with other media entries produced by the same
  /// participant. For example, a participant with multiple devices active in
  /// the same conference.
  ///
  /// Unused for now.
  ///
  /// @see [Meet REST API:
  /// conferenceRecords.participants](https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants)
  std::optional<std::string> participant;
  /// Participant key of associated participant. The user must construct the
  /// resource name from this field to create a Meet API reference.
  ///
  /// **Format:** `participants/{participant}`
  ///
  /// You can retrieve the conference record using [this
  /// guide](https://developers.google.com/meet/api/guides/conferences) and use
  /// the conference record to construct the participant name in the format of
  /// `conferenceRecords/{conference_record}/participants/{participant}`
  ///
  /// @see [Meet REST API: Work with
  /// conferences](https://developers.google.com/meet/api/guides/conferences)
  std::optional<std::string> participant_key;
  /// Participant session name. There should be a one to one mapping of session
  /// to Media Entry.
  ///
  /// **Format:**
  /// `conferenceRecords/{conference_record}/participants/{participant}/participantSessions/{participant_session}`
  ///
  /// Unused for now.
  ///
  /// @see [Meet REST API:
  /// conferenceRecords.participants.participantSessions](https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants.participantSessions)
  std::optional<std::string> session;
  /// The session ID of the media entry. The user must construct the
  /// session name from this field to create an Meet API reference.
  /// This can be done by combining the conference record, participant key, and
  /// session ID.
  ///
  /// **Format:**
  /// `participants/{participant}/participantSessions/{participant_session}`
  ///
  /// You can retrieve the conference record using [this
  /// guide](https://developers.google.com/meet/api/guides/conferences)  and use
  /// the conference record to construct the participant name in the format of
  /// `conferenceRecords/{conference_record}/participants/{participant}`
  ///
  /// @see [Meet REST API: Work with
  /// conferences](https://developers.google.com/meet/api/guides/conferences)
  std::optional<std::string> session_name;
  /// The CSRC for any audio stream contributed by this participant. Will be
  /// zero if no stream is provided.
  uint32_t audio_csrc = 0;
  /// The CSRC for any video stream contributed by this participant. Will be
  /// empty if no stream is provided.
  std::vector<uint32_t> video_csrcs;
  /// Signals if the current entry is presenting.
  bool presenter = false;
  /// Signals if the current entry is a screenshare.
  bool screenshare = false;
  /// Signals if the audio stream is currently muted by the remote participant.
  bool audio_muted = false;
  /// Signals if the video stream is currently muted by the remote participant.
  bool video_muted = false;
};

struct MediaEntriesResourceSnapshot {
  /// The resource ID of the resource being updated.
  int64_t id = 0;
  std::optional<MediaEntry> media_entry;
};

struct MediaEntriesDeletedResource {
  /// The resource ID of the resource being deleted.
  int64_t id = 0;
  std::optional<bool> media_entry;
};

/// The top-level transport container for messages sent from server to
/// client in the `media-entries` data channel.
struct MediaEntriesChannelToClient {
  /// Resource snapshots. There is no implied order between the snapshots in the
  /// list.
  std::vector<MediaEntriesResourceSnapshot> resources;
  /// The list of deleted resources. There is no order between the entries in
  /// the list.
  std::vector<MediaEntriesDeletedResource> deleted_resources;
};

}  // namespace meet

#endif  // CPP_API_MEDIA_ENTRIES_RESOURCE_H_
