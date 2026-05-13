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

#ifndef CPP_API_PARTICIPANTS_RESOURCE_H_
#define CPP_API_PARTICIPANTS_RESOURCE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/nullability.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {

/// Signed in user type. Always has a unique ID and display name.
struct SignedInUser {
  /// Unique ID for the user.
  ///
  /// **Format:** `users/{user}`
  ///
  /// Interoperable with the [Admin SDK
  /// API](https://developers.google.com/admin-sdk/directory/reference/rest/v1/users)
  /// and the [People
  /// API](https://developers.google.com/people/api/rest/v1/people/get).
  std::string user;
  /// Display name of the user.
  ///
  /// - For a personal device, it's the user's first name and last name.
  /// - For a robot account, it's the administrator-specified device name. For
  /// example, "Altostrat Room".
  std::string display_name;
};

/// Anonymous user.
struct AnonymousUser {
  /// User provided name when they join a conference anonymously.
  std::string display_name;
};

/// Phone user, always has a display name. User dialing in from a phone where
/// the user's identity is unknown because they haven't signed in with a Google
/// Account.
struct PhoneUser {
  /// Partially redacted user's phone number when calling.
  std::string display_name;
};

struct Participant {
  enum class Type { kSignedInUser = 0, kAnonymousUser = 1, kPhoneUser = 2 };

  /// Numeric ID for the participant.
  ///
  /// Will eventually be deprecated in favor of `name`.
  int32_t participant_id;

  /// Participant resource name, not display name. There is a many
  /// (participant) to one (media entry) relationship.
  ///
  /// **Format:**
  /// `conferenceRecords/{conference_record}/participants/{participant}`
  ///
  /// Usethis to correlate with other media entries produced by the same
  /// participant. For example, a participant with multiple devices active in
  /// the same conference.
  ///
  /// Unused for now.
  ///
  /// @see [Meet REST API:
  /// conferenceRecords.participants](https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants)
  std::optional<std::string> name;

  /// Participant key of associated participant. The user must construct the
  /// resource name from this field to create a Meet API reference.
  ///
  /// **Format:** `participants/{participant}`
  ///
  /// You can retrieve the conference record using [this
  /// guide](https://developers.google.com/meet/api/guides/conferences) and use
  /// the conference record to construct the participant name in the format of
  /// `conferenceRecords/{conference_record}/participants/{participant}`.
  ///
  /// @see [Meet REST API: Work with
  /// conferences](https://developers.google.com/meet/api/guides/conferences)
  std::optional<std::string> participant_key;
  /// The type of participant.
  ///
  /// This is used to determine which of the following fields are populated.
  Type type;
  std::optional<SignedInUser> signed_in_user;
  std::optional<AnonymousUser> anonymous_user;
  std::optional<PhoneUser> phone_user;
};

/// A resource snapshot managed by the server and replicated to the client.
struct ParticipantResourceSnapshot {
  /// The resource ID of the resource being updated.
  int64_t id;

  std::optional<Participant> participant;
};

struct ParticipantDeletedResource {
  /// The resource ID of the resource being deleted.
  int64_t id;

  /// The type of resource being deleted.
  std::optional<bool> participant;
};

/// The top-level transport container for messages converted from proto to C++
/// struct.
struct ParticipantsChannelToClient {
  /// Resource snapshots. There is no implied order between the snapshots in the
  /// list.
  std::vector<ParticipantResourceSnapshot> resources;

  /// The list of deleted resources. There is no order between the entries in
  /// the list.
  std::vector<ParticipantDeletedResource> deleted_resources;
};

}  // namespace meet

#endif  // CPP_API_PARTICIPANTS_RESOURCE_H_
