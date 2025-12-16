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

#include "meet_clients/internal/media_entries_resource_handler.h"

#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "meet_clients/api/media_api_client_interface.h"
#include "meet_clients/api/media_entries_resource.h"

namespace meet {
namespace {
using ::testing::ElementsAre;
using ::testing::SizeIs;

TEST(MediaEntriesResourceHandlerTest, ParsesMultipleResourceSnapshots) {
  MediaEntriesResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "resources": [
          {
            "id": 424242,
            "mediaEntry": {
              "participant": "some-participant",
              "participantKey": "some-participant-key",
              "session": "some-session",
              "sessionName": "some-session-name",
              "audioCsrc": 111,
              "videoCsrcs": [
                123,
                456
              ],
              "presenter": true,
              "screenshare": true,
              "audioMuted": true,
              "videoMuted": true
            }
          },
          {
            "id": 242424,
            "mediaEntry": {
              "participant": "another-participant",
              "participantKey": "another-participant-key",
              "session": "another-session",
              "sessionName": "another-session-name",
              "audioCsrc": 222,
              "videoCsrcs": [
                555,
                666
              ]
            }
          }
        ]
    })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  auto parsed_update = std::get<MediaEntriesChannelToClient>(
      std::move(status_or_parsed_update).value());

  ASSERT_THAT(parsed_update.resources, SizeIs(2));
  ASSERT_TRUE(parsed_update.resources[0].media_entry.has_value());
  MediaEntry media_entry1 = *parsed_update.resources[0].media_entry;

  EXPECT_EQ(media_entry1.participant, "some-participant");
  EXPECT_EQ(media_entry1.participant_key, "some-participant-key");
  EXPECT_EQ(media_entry1.session, "some-session");
  EXPECT_EQ(media_entry1.session_name, "some-session-name");
  EXPECT_EQ(media_entry1.audio_csrc, 111);
  EXPECT_THAT(media_entry1.video_csrcs, ElementsAre(123, 456));
  EXPECT_TRUE(media_entry1.presenter);
  EXPECT_TRUE(media_entry1.screenshare);
  EXPECT_TRUE(media_entry1.audio_muted);
  EXPECT_EQ(parsed_update.resources[0].id, 424242);

  ASSERT_TRUE(parsed_update.resources[1].media_entry.has_value());
  MediaEntry media_entry2 = parsed_update.resources[1].media_entry.value();

  EXPECT_EQ(media_entry2.participant, "another-participant");
  EXPECT_EQ(media_entry2.participant_key, "another-participant-key");
  EXPECT_EQ(media_entry2.session, "another-session");
  EXPECT_EQ(media_entry2.session_name, "another-session-name");
  EXPECT_EQ(media_entry2.audio_csrc, 222);
  EXPECT_THAT(media_entry2.video_csrcs, ElementsAre(555, 666));
  EXPECT_FALSE(media_entry2.presenter);
  EXPECT_FALSE(media_entry2.screenshare);
  EXPECT_FALSE(media_entry2.audio_muted);
  EXPECT_EQ(parsed_update.resources[1].id, 242424);
}

TEST(MediaEntriesResourceHandlerTest,
     ParsesSignedInUserWithoutOptionalFieldsFromSnapshot) {
  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      MediaEntriesResourceHandler().ParseUpdate(R"json({
        "resources": [
          {
            "id": 424242,
            "mediaEntry": {
              "audioCsrc": 111,
              "videoCsrcs": [
                123,
                456
              ],
              "presenter": true,
              "screenshare": true,
              "audioMuted": true,
              "videoMuted": true
            }
          }
        ]
    })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  auto parsed_update = std::get<MediaEntriesChannelToClient>(
      std::move(status_or_parsed_update).value());
  MediaEntry media_entry1 = *parsed_update.resources[0].media_entry;
  EXPECT_FALSE(media_entry1.participant.has_value());
  EXPECT_FALSE(media_entry1.participant_key.has_value());
  EXPECT_FALSE(media_entry1.session.has_value());
  EXPECT_FALSE(media_entry1.session_name.has_value());
}

TEST(MediaEntriesResourceHandlerTest, NoMediaEntryIsOk) {
  MediaEntriesResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "resources": [
          {
            "id": 424242
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto parsed_update = std::get<MediaEntriesChannelToClient>(
      std::move(status_or_parsed_update).value());

  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  EXPECT_EQ(parsed_update.resources[0].id, 424242);
  EXPECT_FALSE(parsed_update.resources[0].media_entry.has_value());
}

TEST(MediaEntriesResourceHandlerTest, ResourceSnapshotIdIsZeroIfMissing) {
  MediaEntriesResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "resources": [
          {
            "mediaEntry": {
              "participant": "some-participant",
              "session": "some-session-name",
              "audioCsrc": 111,
              "videoCsrcs": [
                123,
                456
              ],
              "presenter": true,
              "screenshare": true,
              "audioMuted": true,
              "videoMuted": true
            }
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto parsed_update = std::get<MediaEntriesChannelToClient>(
      std::move(status_or_parsed_update).value());
  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  EXPECT_EQ(parsed_update.resources[0].id, 0);
}

TEST(MediaEntriesResourceHandlerTest, ResourcesUpdateEmptyArrayParsesJson) {
  MediaEntriesResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "resources": []
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto parsed_update = std::get<MediaEntriesChannelToClient>(
      std::move(status_or_parsed_update).value());

  EXPECT_THAT(parsed_update.resources, SizeIs(0));
}

TEST(MediaEntriesResourceHandlerTest, ParsesMultipleDeletedResources) {
  MediaEntriesResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "deletedResources": [
          {
            "id": 4242,
            "mediaEntry": true
          },
          {
            "id": 2424,
            "mediaEntry": true
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto parsed_update = std::get<MediaEntriesChannelToClient>(
      std::move(status_or_parsed_update).value());

  ASSERT_THAT(parsed_update.deleted_resources, SizeIs(2));
  EXPECT_EQ(parsed_update.deleted_resources[0].id, 4242);
  ASSERT_TRUE(parsed_update.deleted_resources[0].media_entry.has_value());
  EXPECT_TRUE(parsed_update.deleted_resources[0].media_entry.value());
  EXPECT_EQ(parsed_update.deleted_resources[1].id, 2424);
  ASSERT_TRUE(parsed_update.deleted_resources[1].media_entry.has_value());
  EXPECT_TRUE(parsed_update.deleted_resources[1].media_entry.value());
}

TEST(MediaEntriesResourceHandlerTest,
     DeletedResourcesUpdateEmptyArrayParsesJson) {
  MediaEntriesResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "deletedResources": []
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto parsed_update = std::get<MediaEntriesChannelToClient>(
      std::move(status_or_parsed_update).value());

  EXPECT_THAT(parsed_update.deleted_resources, SizeIs(0));
}

TEST(MediaEntriesResourceHandlerTest, DeletedResourcesIdIsZeroIfMissing) {
  MediaEntriesResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "deletedResources": [
          {
            "mediaEntry": true
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto parsed_update = std::get<MediaEntriesChannelToClient>(
      std::move(status_or_parsed_update).value());

  ASSERT_THAT(parsed_update.deleted_resources, SizeIs(1));
  EXPECT_EQ(parsed_update.deleted_resources[0].id, 0);
}

TEST(MediaEntriesResourceHandlerTest, DeletedResourcesMissingMediaEntryIsOk) {
  MediaEntriesResourceHandler handler;

  absl::StatusOr<ResourceUpdate> status_or_parsed_update =
      handler.ParseUpdate(R"json({
        "deletedResources": [
          {
            "id": 4242
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  auto parsed_update = std::get<MediaEntriesChannelToClient>(
      std::move(status_or_parsed_update).value());

  EXPECT_FALSE(parsed_update.deleted_resources[0].media_entry.has_value());
}

}  // namespace
}  // namespace meet
