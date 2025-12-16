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

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/base/log_severity.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "meet_clients/api/media_entries_resource.h"
#include "meet_clients/api/participants_resource.h"
#include "meet_clients/samples/testing/mock_output_writer.h"

namespace media_api_samples {
namespace {

using ::base_logging::ERROR;
using ::base_logging::WARNING;
using ::testing::_;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::ScopedMockLog;
using ::testing::status::StatusIs;

TEST(ResourceManagerTest, OnParticipantResourceUpdateLogsEvents) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  std::vector<std::string> written_events;
  EXPECT_CALL(*event_writer, Write(_, _))
      .WillRepeatedly([&](const char* data, size_t size) {
        written_events.push_back(std::string(data, size));
      });
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                      .participant =
                          meet::Participant{
                              .participant_key =
                                  "participants/signed_in_user_participant_key",
                              .signed_in_user =
                                  meet::SignedInUser{
                                      .display_name =
                                          "signed_in_user_display_name",
                                  },
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 234,
                      .participant =
                          meet::Participant{
                              .participant_key =
                                  "participants/anonymous_user_participant_key",
                              .anonymous_user =
                                  meet::AnonymousUser{
                                      .display_name =
                                          "anonymous_user_display_name",
                                  },
                          },
                  },
              },
      },
      absl::FromUnixSeconds(200));
  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 345,
                      .participant =
                          meet::Participant{
                              .participant_key =
                                  "participants/phone_user_participant_key",
                              .phone_user =
                                  meet::PhoneUser{
                                      .display_name = "phone_user_display_name",
                                  },
                          },
                  },
              },
      },
      absl::FromUnixSeconds(300));

  ASSERT_EQ(written_events.size(), 3);
  EXPECT_EQ(written_events[0],
            "time=1969-12-31T16:01:40-08:00,"
            "event=updated participant resource,"
            "display_name=signed_in_user_display_name,"
            "participant_key=signed_in_user_participant_key,"
            "participant_id=123\n");
  EXPECT_EQ(written_events[1],
            "time=1969-12-31T16:03:20-08:00,"
            "event=updated participant resource,"
            "display_name=anonymous_user_display_name,"
            "participant_key=anonymous_user_participant_key,"
            "participant_id=234\n");
  EXPECT_EQ(written_events[2],
            "time=1969-12-31T16:05:00-08:00,"
            "event=updated participant resource,"
            "display_name=phone_user_display_name,"
            "participant_key=phone_user_participant_key,"
            "participant_id=345\n");
}

TEST(ResourceManagerTest, OnParticipantResourceUpdateLogsDeletedEvents) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  std::vector<std::string> written_events;
  EXPECT_CALL(*event_writer, Write(_, _))
      .WillRepeatedly([&](const char* data, size_t size) {
        written_events.push_back(std::string(data, size));
      });
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .deleted_resources =
              {
                  meet::ParticipantDeletedResource{
                      .id = 123,
                      .participant = true,
                  },
              },
      },
      absl::FromUnixSeconds(100));

  ASSERT_EQ(written_events.size(), 1);
  EXPECT_EQ(written_events[0],
            "time=1969-12-31T16:01:40-08:00,"
            "event=deleted participant resource,"
            "participant_id=123\n");
}

TEST(ResourceManagerTest, OnMediaEntriesResourceUpdateLogsEvents) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  std::vector<std::string> written_events;
  EXPECT_CALL(*event_writer, Write(_, _))
      .WillOnce([&](const char* data, size_t size) {
        written_events.push_back(std::string(data, size));
      });
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 123,
                      .media_entry =
                          meet::MediaEntry{
                              .participant_key =
                                  "participants/signed_in_user_participant_key",
                              .session_name = "participants/"
                                              "signed_in_user_participant_key/"
                                              "participantSessions/"
                                              "signed_in_user_session_name",
                              .audio_csrc = 1,
                              .video_csrcs = {2, 3},
                              .audio_muted = true,
                              .video_muted = true,
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));

  ASSERT_EQ(written_events.size(), 1);
  EXPECT_EQ(written_events[0],
            "time=1969-12-31T16:01:40-08:00,"
            "event=updated media entry resource,"
            "participant_session_name=signed_in_user_session_name,"
            "participant_key=signed_in_user_participant_key,"
            "media_entry_id=123,"
            "audio_csrc=1,"
            "video_csrcs=2|3,"
            "audio_muted=1,"
            "video_muted=1\n");
}

TEST(ResourceManagerTest, OnMediaEntriesResourceUpdateLogsDeletedEvents) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  std::vector<std::string> written_events;
  EXPECT_CALL(*event_writer, Write(_, _))
      .WillRepeatedly([&](const char* data, size_t size) {
        written_events.push_back(std::string(data, size));
      });
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .deleted_resources =
              {
                  meet::MediaEntriesDeletedResource{
                      .id = 123,
                      .media_entry = true,
                  },
              },
      },
      absl::FromUnixSeconds(100));

  ASSERT_EQ(written_events.size(), 1);
  EXPECT_EQ(written_events[0],
            "time=1969-12-31T16:01:40-08:00,"
            "event=deleted media entry resource,"
            "media_entry_id=123\n");
}

TEST(ResourceManagerTest, DeletingParticipantThatDoesNotExistLogsWarning) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(WARNING, _,
                       "Deleted participant resource with id 123 was not "
                       "found. Skipping..."));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .deleted_resources =
              {
                  meet::ParticipantDeletedResource{
                      .id = 123,
                      .participant = true,
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest, DeletingMediaEntryThatDoesNotExistLogsWarning) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(WARNING, _,
                       "Deleted media entry resource with id 123 was not "
                       "found. Skipping..."));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .deleted_resources =
              {
                  meet::MediaEntriesDeletedResource{
                      .id = 123,
                      .media_entry = true,
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest, ParticipantResourceUpdateWithNoParticipantLogsError) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _,
                       "Participant resource snapshot with id 123 does not "
                       "have a participant. Skipping..."));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest,
     ParticipantResourceUpdateWithNoParticipantKeyLogsError) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _,
                       absl::StrCat("Failed to parse participant key: ",
                                    "Participant key is empty")));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                      .participant = meet::Participant{},
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest,
     ParticipantResourceUpdateWithMalformedParticipantKeyLogsError) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _,
                       absl::StrCat("Failed to parse participant key: ",
                                    "Participant key is not in the expected "
                                    "format: malformed_participant_key")));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                      .participant =
                          meet::Participant{
                              .participant_key = "malformed_participant_key",
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest, ParticipantResourceUpdateWithNoUserLogsError) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _,
                       absl::StrCat("Participant resource snapshot with id 123 "
                                    "does not have a user. Skipping...")));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                      .participant =
                          meet::Participant{
                              .participant_key = "participants/participant_key",
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest, MediaEntriesResourceUpdateWithNoMediaEntryLogsError) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _,
                       "Media entry resource snapshot with id 123 does not "
                       "have a media entry. Skipping..."));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 123,
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest,
     MediaEntriesResourceUpdateWithNoParticipantSessionNameLogsError) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log,
              Log(ERROR, _,
                  absl::StrCat("Failed to parse participant session name: ",
                               "Participant session name is empty")));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 123,
                      .media_entry =
                          meet::MediaEntry{
                              .participant_key =
                                  "participants/signed_in_user_participant_key",
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest,
     MediaEntriesResourceUpdateWithMalformedParticipantSessionNameLogsError) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(
      log, Log(ERROR, _,
               absl::StrCat("Failed to parse participant session name: ",
                            "Participant session name is not in the expected "
                            "format: malformed_participant_session_name")));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 123,
                      .media_entry =
                          meet::MediaEntry{
                              .participant_key =
                                  "participants/signed_in_user_participant_key",
                              .session_name =
                                  "malformed_participant_session_name",
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest,
     MediaEntriesResourceUpdateWithNoParticipantKeyLogsError) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _,
                       absl::StrCat("Failed to parse participant key: ",
                                    "Participant key is empty")));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 123,
                      .media_entry =
                          meet::MediaEntry{
                              .session_name =
                                  "participants/participant_key/"
                                  "participantSessions/session_name",
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest,
     MediaEntriesResourceUpdateWithMalformedParticipantKeyLogsError) {
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _,
                       absl::StrCat("Failed to parse participant key: ",
                                    "Participant key is not in the expected "
                                    "format: malformed_participant_key")));
  log.StartCapturingLogs();
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 123,
                      .media_entry =
                          meet::MediaEntry{
                              .participant_key = "malformed_participant_key",
                              .session_name =
                                  "participants/participant_key/"
                                  "participantSessions/session_name",
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
}

TEST(ResourceManagerTest,
     GetOutputFileIdentifierWithAudioCsrcReturnsIdentifier) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                      .participant =
                          meet::Participant{
                              .participant_key = "participants/participant_key",
                              .signed_in_user =
                                  meet::SignedInUser{
                                      .display_name = "display_name",
                                  },
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 234,
                      .media_entry =
                          meet::MediaEntry{
                              .participant_key = "participants/participant_key",
                              .session_name =
                                  "participants/participant_key/"
                                  "participantSessions/session_name",
                              .audio_csrc = 111,
                              .video_csrcs = {222, 333},
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));

  absl::StatusOr<std::string> output_file_identifier =
      resource_manager.GetOutputFileIdentifier(111);

  ASSERT_OK(output_file_identifier);
  EXPECT_EQ(output_file_identifier.value(),
            "display_name_participant_key_session_name");
}

TEST(ResourceManagerTest,
     GetOutputFileIdentifierWithVideoCsrcReturnsIdentifier) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                      .participant =
                          meet::Participant{
                              .participant_key = "participants/participant_key",
                              .signed_in_user =
                                  meet::SignedInUser{
                                      .display_name = "display_name",
                                  },
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 234,
                      .media_entry =
                          meet::MediaEntry{
                              .participant_key = "participants/participant_key",
                              .session_name =
                                  "participants/participant_key/"
                                  "participantSessions/session_name",
                              .audio_csrc = 111,
                              .video_csrcs = {222, 333},
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));

  absl::StatusOr<std::string> output_file_identifier =
      resource_manager.GetOutputFileIdentifier(222);

  ASSERT_OK(output_file_identifier);
  EXPECT_EQ(output_file_identifier.value(),
            "display_name_participant_key_session_name");
}

TEST(ResourceManagerTest, GetOutputFileIdentifierWithNoMediaEntryReturnsError) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));
  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                      .participant =
                          meet::Participant{
                              .participant_key = "participants/participant_key",
                              .signed_in_user =
                                  meet::SignedInUser{
                                      .display_name = "display_name",
                                  },
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));

  absl::StatusOr<std::string> output_file_identifier =
      resource_manager.GetOutputFileIdentifier(111);

  ASSERT_THAT(output_file_identifier,
              StatusIs(absl::StatusCode::kNotFound,
                       "Media entry not found for CSRC: 111"));
}

TEST(ResourceManagerTest,
     GetOutputFileIdentifierWithNoParticipantReturnsError) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));
  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 234,
                      .media_entry =
                          meet::MediaEntry{
                              .participant_key = "participants/participant_key",
                              .session_name =
                                  "participants/participant_key/"
                                  "participantSessions/session_name",
                              .audio_csrc = 111,
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));

  absl::StatusOr<std::string> output_file_identifier =
      resource_manager.GetOutputFileIdentifier(111);

  EXPECT_THAT(output_file_identifier,
              StatusIs(absl::StatusCode::kNotFound,
                       "Participant not found for CSRC: 111"));
}

TEST(ResourceManagerTest,
     GetOutputFileIdentifierAfterDeletingMediaEntryReturnsError) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));
  // Populate the resource manager with a participant and media entry.
  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                      .participant =
                          meet::Participant{
                              .participant_key = "participants/participant_key",
                              .signed_in_user =
                                  meet::SignedInUser{
                                      .display_name = "display_name",
                                  },
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 234,
                      .media_entry =
                          meet::MediaEntry{
                              .participant_key = "participants/participant_key",
                              .session_name =
                                  "participants/participant_key/"
                                  "participantSessions/session_name",
                              .audio_csrc = 111,
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
  // Delete the media entry.
  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .deleted_resources =
              {
                  meet::MediaEntriesDeletedResource{
                      .id = 234,
                      .media_entry = true,
                  },
              },
      },
      absl::FromUnixSeconds(100));

  absl::StatusOr<std::string> output_file_identifier =
      resource_manager.GetOutputFileIdentifier(111);

  ASSERT_THAT(output_file_identifier,
              StatusIs(absl::StatusCode::kNotFound,
                       "Media entry not found for CSRC: 111"));
}

TEST(ResourceManagerTest,
     GetOutputFileIdentifierAfterDeletingParticipantReturnsError) {
  auto event_writer = std::make_unique<MockOutputWriter>();
  ResourceManager resource_manager(std::move(event_writer));

  // Populate the resource manager with a participant and media entry.
  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .resources =
              {
                  meet::ParticipantResourceSnapshot{
                      .id = 123,
                      .participant =
                          meet::Participant{
                              .participant_key = "participants/participant_key",
                              .signed_in_user =
                                  meet::SignedInUser{
                                      .display_name = "display_name",
                                  },
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
  resource_manager.OnMediaEntriesResourceUpdate(
      meet::MediaEntriesChannelToClient{
          .resources =
              {
                  meet::MediaEntriesResourceSnapshot{
                      .id = 234,
                      .media_entry =
                          meet::MediaEntry{
                              .participant_key = "participants/participant_key",
                              .session_name =
                                  "participants/participant_key/"
                                  "participantSessions/session_name",
                              .audio_csrc = 111,
                          },
                  },
              },
      },
      absl::FromUnixSeconds(100));
  // Delete the participant.
  resource_manager.OnParticipantResourceUpdate(
      meet::ParticipantsChannelToClient{
          .deleted_resources =
              {
                  meet::ParticipantDeletedResource{
                      .id = 123,
                      .participant = true,
                  },
              },
      },
      absl::FromUnixSeconds(100));

  absl::StatusOr<std::string> output_file_identifier =
      resource_manager.GetOutputFileIdentifier(111);

  EXPECT_THAT(output_file_identifier,
              StatusIs(absl::StatusCode::kNotFound,
                       "Participant not found for CSRC: 111"));
}

}  // namespace
}  // namespace media_api_samples
