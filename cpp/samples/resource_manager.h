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

#ifndef CPP_SAMPLES_RESOURCE_MANAGER_H_
#define CPP_SAMPLES_RESOURCE_MANAGER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "meet_clients/api/media_entries_resource.h"
#include "meet_clients/api/participants_resource.h"
#include "meet_clients/samples/output_writer_interface.h"
#include "meet_clients/samples/resource_manager_interface.h"

namespace media_api_samples {

// A participant manager that manages participant and media entry metadata.
//
// Additionally, this implementation outputs participant and media entry events
// to a log file in a format that is easy to read programmatically.
//
// This class is not thread-safe.
class ResourceManager : public ResourceManagerInterface {
 public:
  explicit ResourceManager(
      std::unique_ptr<OutputWriterInterface> event_log_file)
      : event_log_file_(std::move(event_log_file)) {}

  void OnParticipantResourceUpdate(
      const meet::ParticipantsChannelToClient& update,
      absl::Time received_time) override;
  void OnMediaEntriesResourceUpdate(
      const meet::MediaEntriesChannelToClient& update,
      absl::Time received_time) override;
  // Returns a unique string based on the participant and media entry resources
  // associated with the given contributing source.
  //
  // This implementation produces strings in the format:
  //   <display_name>_<participant_key>_<participant_session_name>
  absl::StatusOr<std::string> GetOutputFileIdentifier(
      uint32_t contributing_source) override;

 private:
  // Identifier for a participant.
  using ParticipantKey = std::string;
  // Identifier for a media entry.
  using ParticipantSessionName = std::string;
  // Identifier for a media stream.
  using ContributingSource = uint32_t;

  // Participant ID and media entry ID will be removed in the future in favor of
  // the participant and participant session keys. However, currently this is
  // the only identifier available when these resources are deleted.
  using ParticipantId = int64_t;
  using MediaEntryId = int64_t;

  // A participant is identified by its key. There may be multiple media entries
  // associated with a single participant.
  struct Participant {
    ParticipantKey participant_key;
    ParticipantId participant_id;
    std::string display_name;
  };

  // A media entry is identified by its participant session name. A media entry
  // is only associated with a single participant.
  //
  // A media entry may have multiple contributing sources.
  struct MediaEntry {
    ParticipantSessionName participant_session_name;
    ParticipantKey participant_key;
    MediaEntryId media_entry_id;
    ContributingSource audio_csrc = 0;
    std::vector<ContributingSource> video_csrcs;
  };

  // Parses the participant key value from the participant resource.
  //
  // Participant keys are expected to be in the format:
  //   participants/<participant_key>
  absl::StatusOr<std::string> ParseParticipantKey(
      const std::optional<std::string>& participant_key) const;
  // Parses the participant session name value from the media entry resource.
  //
  // Participant session names are expected to be in the format:
  //   participants/<participant_key>/mediaEntries/<media_entry_key>
  absl::StatusOr<std::string> ParseParticipantSessionName(
      const std::optional<std::string>& participant_session_name) const;

  std::unique_ptr<OutputWriterInterface> event_log_file_;

  // Participants and media entries are keyed by their unique identifiers.
  //
  // These maps are the owners of the participant and media entry objects, and
  // the following lookup maps point to the objects in these maps.
  absl::flat_hash_map<ParticipantKey, std::unique_ptr<Participant>>
      participants_by_key_;
  absl::flat_hash_map<ParticipantSessionName, std::unique_ptr<MediaEntry>>
      media_entries_by_session_name_;

  // When receiving audio and video frames, the contributing source is the only
  // available identifier. Therefore, a second map is used to look up the media
  // entry associated with the contributing source.
  //
  // The media entry's participant key can then be used to look up the
  // participant.
  absl::flat_hash_map<ContributingSource, MediaEntry*> media_entries_by_csrc_;

  // Another set of lookup maps used when participant and media entry resources
  // are deleted.
  //
  // These maps will be removed in the future when deletion updates include the
  // participant and media entry keys.
  absl::flat_hash_map<ParticipantId, Participant*> participants_by_id_;
  absl::flat_hash_map<MediaEntryId, MediaEntry*> media_entries_by_id_;
};

}  // namespace media_api_samples

#endif  // CPP_SAMPLES_RESOURCE_MANAGER_H_
