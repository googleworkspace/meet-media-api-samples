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

#include "meet_clients/samples/resource_manager.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "meet_clients/api/media_entries_resource.h"
#include "meet_clients/api/participants_resource.h"

namespace media_api_samples {
namespace {

constexpr absl::string_view kParticipantResourceUpdateFormat =
    "time=%s,"
    "event=updated participant resource,"
    "display_name=%s,"
    "participant_key=%s,"
    "participant_id=%d\n";
constexpr absl::string_view kParticipantResourceDeleteFormat =
    "time=%s,"
    "event=deleted participant resource,"
    "participant_id=%d\n";
constexpr absl::string_view kMediaEntryResourceUpdateFormat =
    "time=%s,"
    "event=updated media entry resource,"
    "participant_session_name=%s,"
    "participant_key=%s,"
    "media_entry_id=%d,"
    "audio_csrc=%d,"
    // Because there may be multiple video contributing sources, they will be
    // concatenated using `|` as a delimiter.
    "video_csrcs=%s,"
    "audio_muted=%d,"
    "video_muted=%d\n";
constexpr absl::string_view kMediaEntryResourceDeleteFormat =
    "time=%s,"
    "event=deleted media entry resource,"
    "media_entry_id=%d\n";

// The output file identifier is formatted as:
//   <display_name>_<participant_key>_<participant_session_name>
constexpr absl::string_view kOutputFileIdentifierFormat = "%s_%s_%s";

}  // namespace

void ResourceManager::OnParticipantResourceUpdate(
    const meet::ParticipantsChannelToClient& update, absl::Time received_time) {
  for (const meet::ParticipantResourceSnapshot& resource : update.resources) {
    if (!resource.participant.has_value()) {
      LOG(ERROR) << "Participant resource snapshot with id " << resource.id
                 << " does not have a participant. Skipping...";
      continue;
    }

    const meet::Participant& resource_participant = *resource.participant;

    absl::StatusOr<std::string> participant_key_parsed =
        ParseParticipantKey(resource_participant.participant_key);
    if (!participant_key_parsed.ok()) {
      LOG(ERROR) << "Failed to parse participant key: "
                 << participant_key_parsed.status().message();
      continue;
    }
    std::string participant_key = std::move(participant_key_parsed).value();

    std::string display_name;
    if (resource_participant.anonymous_user.has_value()) {
      display_name = resource_participant.anonymous_user->display_name;
    } else if (resource_participant.phone_user.has_value()) {
      display_name = resource_participant.phone_user->display_name;
    } else if (resource_participant.signed_in_user.has_value()) {
      display_name = resource_participant.signed_in_user->display_name;
    } else {
      LOG(ERROR) << "Participant resource snapshot with id " << resource.id
                 << " does not have a user. Skipping...";
      continue;
    }

    auto participant = std::make_unique<Participant>(std::move(participant_key),
                                                     resource.id, display_name);

    std::string event_log_message = absl::StrFormat(
        kParticipantResourceUpdateFormat, absl::FormatTime(received_time),
        participant->display_name, participant->participant_key,
        participant->participant_id);
    event_log_file_->Write(event_log_message.data(), event_log_message.size());

    // Since these are resource "snapshots", they are intended to be complete
    // representations of the data. Therefore, existing data can be entirely
    // replaced with the new data.
    participants_by_id_[participant->participant_id] = participant.get();
    participants_by_key_[participant->participant_key] = std::move(participant);
  }

  for (const meet::ParticipantDeletedResource& resource :
       update.deleted_resources) {
    std::string event_log_message =
        absl::StrFormat(kParticipantResourceDeleteFormat,
                        absl::FormatTime(received_time), resource.id);
    event_log_file_->Write(event_log_message.data(), event_log_message.size());

    auto node = participants_by_id_.extract(resource.id);
    if (node.empty()) {
      LOG(WARNING) << "Deleted participant resource with id " << resource.id
                   << " was not found. Skipping...";
      continue;
    }

    Participant* removed_participant = std::move(node.mapped());
    DCHECK(removed_participant != nullptr);
    // This map holds the unique pointer to the participant, so it must be
    // erased after the participant is removed from the other map.
    participants_by_key_.erase(removed_participant->participant_key);
  }
}

void ResourceManager::OnMediaEntriesResourceUpdate(
    const meet::MediaEntriesChannelToClient& update, absl::Time received_time) {
  for (const meet::MediaEntriesResourceSnapshot& resource : update.resources) {
    if (!resource.media_entry.has_value()) {
      LOG(ERROR) << "Media entry resource snapshot with id " << resource.id
                 << " does not have a media entry. Skipping...";
      continue;
    }

    const meet::MediaEntry& resource_media_entry = *resource.media_entry;

    absl::StatusOr<std::string> participant_session_name_parsed =
        ParseParticipantSessionName(resource_media_entry.session_name);
    if (!participant_session_name_parsed.ok()) {
      LOG(ERROR) << "Failed to parse participant session name: "
                 << participant_session_name_parsed.status().message();
      continue;
    }
    std::string participant_session_name =
        std::move(participant_session_name_parsed).value();

    absl::StatusOr<std::string> participant_key_parsed =
        ParseParticipantKey(resource_media_entry.participant_key);
    if (!participant_key_parsed.ok()) {
      LOG(ERROR) << "Failed to parse participant key: "
                 << participant_key_parsed.status().message();
      continue;
    }
    std::string participant_key = std::move(participant_key_parsed).value();

    auto media_entry = std::make_unique<MediaEntry>(
        std::move(participant_session_name), std::move(participant_key),
        resource.id, resource_media_entry.audio_csrc,
        std::move(resource_media_entry.video_csrcs));

    std::string event_log_message = absl::StrFormat(
        kMediaEntryResourceUpdateFormat, absl::FormatTime(received_time),
        media_entry->participant_session_name, media_entry->participant_key,
        media_entry->media_entry_id, media_entry->audio_csrc,
        absl::StrJoin(media_entry->video_csrcs, "|"),
        resource_media_entry.audio_muted, resource_media_entry.video_muted);
    event_log_file_->Write(event_log_message.data(), event_log_message.size());

    // Since these are resource "snapshots", they are intended to be complete
    // representations of the data. Therefore, existing data can be entirely
    // replaced with the new data.
    media_entries_by_csrc_[media_entry->audio_csrc] = media_entry.get();
    for (uint32_t video_csrc : media_entry->video_csrcs) {
      media_entries_by_csrc_[video_csrc] = media_entry.get();
    }
    media_entries_by_id_[resource.id] = media_entry.get();
    media_entries_by_session_name_[media_entry->participant_session_name] =
        std::move(media_entry);
  }

  for (meet::MediaEntriesDeletedResource resource : update.deleted_resources) {
    std::string event_log_message =
        absl::StrFormat(kMediaEntryResourceDeleteFormat,
                        absl::FormatTime(received_time), resource.id);
    event_log_file_->Write(event_log_message.data(), event_log_message.size());

    auto node = media_entries_by_id_.extract(resource.id);
    if (node.empty()) {
      LOG(WARNING) << "Deleted media entry resource with id " << resource.id
                   << " was not found. Skipping...";
      continue;
    }

    MediaEntry* removed_media_entry = std::move(node.mapped());
    DCHECK(removed_media_entry != nullptr);
    media_entries_by_csrc_.erase(removed_media_entry->audio_csrc);
    for (uint32_t video_csrc : removed_media_entry->video_csrcs) {
      media_entries_by_csrc_.erase(video_csrc);
    }
    // This map holds the unique pointer to the media entry, so it must be
    // erased after the media entry is removed from the other maps.
    media_entries_by_session_name_.erase(
        removed_media_entry->participant_session_name);
  }
}

absl::StatusOr<std::string> ResourceManager::GetOutputFileIdentifier(
    uint32_t contributing_source) {
  auto media_entry_it = media_entries_by_csrc_.find(contributing_source);
  if (media_entry_it == media_entries_by_csrc_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Media entry not found for CSRC: ", contributing_source));
  }

  auto participant_it =
      participants_by_key_.find(media_entry_it->second->participant_key);
  if (participant_it == participants_by_key_.end()) {
    return absl::NotFoundError(
        absl::StrCat("Participant not found for CSRC: ", contributing_source));
  }

  return absl::StrFormat(kOutputFileIdentifierFormat,
                         participant_it->second->display_name,
                         participant_it->second->participant_key,
                         media_entry_it->second->participant_session_name);
}

absl::StatusOr<std::string> ResourceManager::ParseParticipantKey(
    const std::optional<std::string>& participant_key) const {
  if (!participant_key.has_value()) {
    return absl::InvalidArgumentError("Participant key is empty");
  }
  std::vector<std::string> participant_key_split =
      absl::StrSplit(*participant_key, '/');
  // Participant keys are expected to be in the format:
  //   participants/<participant_key>
  if (participant_key_split.size() != 2) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Participant key is not in the expected format: ", *participant_key));
  }
  return participant_key_split[1];
}

absl::StatusOr<std::string> ResourceManager::ParseParticipantSessionName(
    const std::optional<std::string>& participant_session_name) const {
  if (!participant_session_name.has_value()) {
    return absl::InvalidArgumentError("Participant session name is empty");
  }
  std::vector<std::string> participant_session_name_split =
      absl::StrSplit(*participant_session_name, '/');
  // Participant session names are expected to be in the format:
  //   participants/<participant_key>/mediaEntries/<media_entry_key>
  if (participant_session_name_split.size() != 4) {
    return absl::InvalidArgumentError(
        absl::StrCat("Participant session name is not in the expected format: ",
                     *participant_session_name));
  }
  return participant_session_name_split[3];
}

}  // namespace media_api_samples
