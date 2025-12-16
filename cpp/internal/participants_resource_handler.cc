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

#include "meet_clients/internal/participants_resource_handler.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "third_party/icu/source/tools/toolutil/json-json.hpp"
#include "meet_clients/api/media_api_client_interface.h"
#include "meet_clients/api/participants_resource.h"

namespace meet {
namespace {
using Json = ::nlohmann::json;

// Participants resource channel is always opened with this label.
constexpr absl::string_view kParticipantsResourceName = "participants";

const Json* FindOrNull(const Json& json, absl::string_view key) {
  auto it = json.find(key);
  return it != json.cend() ? &*it : nullptr;
}

}  // namespace

absl::StatusOr<ResourceUpdate> ParticipantsResourceHandler::ParseUpdate(
    absl::string_view update) {
  VLOG(1) << kParticipantsResourceName
          << " resource update received: " << update;

  const Json json_resource_update = Json::parse(update, /*cb=*/nullptr,
                                                /*allow_exceptions=*/false);

  if (!json_resource_update.is_object()) {
    return absl::InternalError(absl::StrCat(
        "Invalid ", kParticipantsResourceName, " json format: ", update));
  }

  ParticipantsChannelToClient participants_update;
  // Resources
  if (const Json* resources_field =
          FindOrNull(json_resource_update, "resources");
      resources_field != nullptr) {
    if (!resources_field->is_array()) {
      return absl::InternalError(absl::StrCat(
          "Invalid ", kParticipantsResourceName,
          " json format. Expected resources field to be an array: ", update));
    }

    std::vector<ParticipantResourceSnapshot>& resources =
        participants_update.resources;
    for (const Json& resource_field : *resources_field) {
      // Resources.resourceSnapshot
      ParticipantResourceSnapshot resource;

      // Resources.resourceSnapshot.id
      if (const Json* id_field = FindOrNull(resource_field, "id");
          id_field != nullptr) {
        resource.id = id_field->get<int64_t>();
      } else {
        resource.id = 0;
      }

      // Resources.resourceSnapshot.participant
      if (const Json* participant_field =
              FindOrNull(resource_field, "participant");
          participant_field != nullptr) {
        Participant participant;

        // Resources.resourceSnapshot.participant.participantId
        if (const Json* id_field =
                FindOrNull(*participant_field, "participantId");
            id_field != nullptr) {
          participant.participant_id = id_field->get<int32_t>();
        }

        // Resources.resourceSnapshot.participant.name
        if (const Json* id_field = FindOrNull(*participant_field, "name");
            id_field != nullptr) {
          participant.name = id_field->get<std::string>();
        }

        if (const Json* id_field =
                FindOrNull(*participant_field, "participantKey");
            id_field != nullptr) {
          participant.participant_key = id_field->get<std::string>();
        }

        // Resources.resourceSnapshot.participant.signedInUser
        if (const Json* signed_in_user_field =
                FindOrNull(*participant_field, "signedInUser");
            signed_in_user_field != nullptr) {
          participant.type = Participant::Type::kSignedInUser;

          SignedInUser signed_in_user;

          // Resources.resourceSnapshot.participant.signedInUser.user
          if (const Json* user_field =
                  FindOrNull(*signed_in_user_field, "user");
              user_field != nullptr) {
            signed_in_user.user = user_field->get<std::string>();
          }

          // Resources.resourceSnapshot.participant.signedInUser.displayName
          if (const Json* display_name_field =
                  FindOrNull(*signed_in_user_field, "displayName");
              display_name_field != nullptr) {
            signed_in_user.display_name =
                display_name_field->get<std::string>();
          };

          participant.signed_in_user = std::move(signed_in_user);
        }

        // Resources.resourceSnapshot.participant.anonymousUser
        if (const Json* anonymous_user_field =
                FindOrNull(*participant_field, "anonymousUser");
            anonymous_user_field != nullptr) {
          participant.type = Participant::Type::kAnonymousUser;

          // Resources.resourceSnapshot.participant.anonymousUser.displayName
          if (const Json* display_name_field =
                  FindOrNull(*anonymous_user_field, "displayName");
              display_name_field != nullptr) {
            participant.anonymous_user = AnonymousUser{
                .display_name = display_name_field->get<std::string>()};
          }
        }

        // Resources.resourceSnapshot.participant.phoneUser
        if (const Json* phone_user_field =
                FindOrNull(*participant_field, "phoneUser");
            phone_user_field != nullptr) {
          participant.type = Participant::Type::kPhoneUser;

          // Resources.resourceSnapshot.participant.phoneUser.displayName
          if (const Json* display_name_field =
                  FindOrNull(*phone_user_field, "displayName");
              display_name_field != nullptr) {
            participant.phone_user = PhoneUser{
                .display_name = display_name_field->get<std::string>()};
          }
        }
        resource.participant = std::move(participant);
      } else {
        resource.participant = std::nullopt;
      }
      resources.push_back(std::move(resource));
    }
  }

  // Deleted resources
  if (const Json* deleted_resources_field =
          FindOrNull(json_resource_update, "deletedResources");
      deleted_resources_field != nullptr) {
    if (!deleted_resources_field->is_array()) {
      return absl::InternalError(absl::StrCat(
          "Invalid ", kParticipantsResourceName,
          " json format. Expected deletedResources field to be an array: ",
          update));
    }

    std::vector<ParticipantDeletedResource>& deleted_resources =
        participants_update.deleted_resources;
    for (const Json& deleted_resource_field : *deleted_resources_field) {
      // DeletedResources.deletedResource
      ParticipantDeletedResource deleted_resource;

      // DeletedResources.deletedResource.id
      if (const Json* id_field = FindOrNull(deleted_resource_field, "id");
          id_field != nullptr) {
        deleted_resource.id = id_field->get<int64_t>();
      } else {
        deleted_resource.id = 0;
      }

      // DeletedResources.deletedResource.participant
      if (const Json* participant_field =
              FindOrNull(deleted_resource_field, "participant");
          participant_field != nullptr) {
        deleted_resource.participant = participant_field->get<bool>();
      }
      deleted_resources.push_back(std::move(deleted_resource));
    }
  }

  return std::move(participants_update);
}

}  // namespace meet
