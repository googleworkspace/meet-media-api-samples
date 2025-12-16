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

#include "meet_clients/internal/stats_request_from_report.h"

#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "meet_clients/api/media_stats_resource.h"
#include "api/scoped_refptr.h"
#include "api/stats/rtc_stats_report.h"
#include "api/stats/rtcstats_objects.h"
#include "api/units/timestamp.h"

namespace meet {
namespace {

TEST(StatsRequestFromReportTest, PopulatesRequestId) {
  auto report = webrtc::RTCStatsReport::Create(webrtc::Timestamp::Zero());
  MediaStatsChannelFromClient request =
      StatsRequestFromReport(report, /*stats_request_id=*/7, /*allowlist=*/{});
  EXPECT_EQ(request.request.request_id, 7);
}

TEST(StatsRequestFromReportTest, PopulatesSectionsThatAreInAllowlist) {
  auto report = webrtc::RTCStatsReport::Create(webrtc::Timestamp::Zero());
  auto candidate_pair_section =
      std::make_unique<webrtc::RTCIceCandidatePairStats>(
          "candidate_pair_id", webrtc::Timestamp::Zero());
  candidate_pair_section->last_packet_sent_timestamp = 100;
  candidate_pair_section->last_packet_received_timestamp = 200;
  auto rtc_transport_section = std::make_unique<webrtc::RTCTransportStats>(
      "rtc_transport_id", webrtc::Timestamp::Zero());
  rtc_transport_section->bytes_sent = 1000;
  rtc_transport_section->bytes_received = 2000;
  report->AddStats(std::move(candidate_pair_section));
  report->AddStats(std::move(rtc_transport_section));
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> allowlist =
      {
          {"candidate-pair",
           {"lastPacketSentTimestamp", "lastPacketReceivedTimestamp"}},
          {"transport", {"bytesSent", "bytesReceived"}},
      };

  MediaStatsChannelFromClient request =
      StatsRequestFromReport(report, /*stats_request_id=*/7, allowlist);

  EXPECT_EQ(request.request.request_id, 7);
  ASSERT_EQ(request.request.upload_media_stats->sections.size(), 2);
  EXPECT_EQ(request.request.upload_media_stats->sections[0].id,
            "candidate_pair_id");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].type,
            "candidate-pair");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].values.size(), 2);
  EXPECT_EQ(request.request.upload_media_stats->sections[0]
                .values["lastPacketSentTimestamp"],
            "100");
  EXPECT_EQ(request.request.upload_media_stats->sections[0]
                .values["lastPacketReceivedTimestamp"],
            "200");
  EXPECT_EQ(request.request.upload_media_stats->sections[1].id,
            "rtc_transport_id");
  EXPECT_EQ(request.request.upload_media_stats->sections[1].type, "transport");
  EXPECT_EQ(request.request.upload_media_stats->sections[1].values.size(), 2);
  EXPECT_EQ(request.request.upload_media_stats->sections[1].values["bytesSent"],
            "1000");
  EXPECT_EQ(
      request.request.upload_media_stats->sections[1].values["bytesReceived"],
      "2000");
}

TEST(StatsRequestFromReportTest, IgnoresSectionsThatAreNotInAllowlist) {
  auto report = webrtc::RTCStatsReport::Create(webrtc::Timestamp::Zero());
  auto candidate_pair_section =
      std::make_unique<webrtc::RTCIceCandidatePairStats>(
          "candidate_pair_id", webrtc::Timestamp::Zero());
  candidate_pair_section->last_packet_sent_timestamp = 100;
  candidate_pair_section->last_packet_received_timestamp = 200;
  auto rtc_transport_section = std::make_unique<webrtc::RTCTransportStats>(
      "rtc_transport_id", webrtc::Timestamp::Zero());
  rtc_transport_section->bytes_sent = 1000;
  rtc_transport_section->bytes_received = 2000;
  report->AddStats(std::move(candidate_pair_section));
  report->AddStats(std::move(rtc_transport_section));
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> allowlist =
      {
          {"transport", {"bytesSent", "bytesReceived"}},
      };

  MediaStatsChannelFromClient request =
      StatsRequestFromReport(report, /*stats_request_id=*/7, allowlist);

  ASSERT_EQ(request.request.upload_media_stats->sections.size(), 1);
  EXPECT_EQ(request.request.upload_media_stats->sections[0].id,
            "rtc_transport_id");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].type, "transport");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].values.size(), 2);
  EXPECT_EQ(request.request.upload_media_stats->sections[0].values["bytesSent"],
            "1000");
  EXPECT_EQ(
      request.request.upload_media_stats->sections[0].values["bytesReceived"],
      "2000");
}

TEST(StatsRequestFromReportTest, IgnoresAttributesThatAreNotInAllowlist) {
  auto report = webrtc::RTCStatsReport::Create(webrtc::Timestamp::Zero());
  auto candidate_pair_section =
      std::make_unique<webrtc::RTCIceCandidatePairStats>(
          "candidate_pair_id", webrtc::Timestamp::Zero());
  candidate_pair_section->last_packet_sent_timestamp = 100;
  auto rtc_transport_section = std::make_unique<webrtc::RTCTransportStats>(
      "rtc_transport_id", webrtc::Timestamp::Zero());
  rtc_transport_section->bytes_sent = 1000;
  report->AddStats(std::move(candidate_pair_section));
  report->AddStats(std::move(rtc_transport_section));
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> allowlist =
      {
          {"candidate-pair", {"lastPacketSentTimestamp"}},
          {"transport", {"bytesSent"}},
      };

  MediaStatsChannelFromClient request =
      StatsRequestFromReport(report, /*stats_request_id=*/7, allowlist);

  ASSERT_EQ(request.request.upload_media_stats->sections.size(), 2);
  EXPECT_EQ(request.request.upload_media_stats->sections[0].id,
            "candidate_pair_id");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].type,
            "candidate-pair");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].values.size(), 1);
  EXPECT_EQ(request.request.upload_media_stats->sections[0]
                .values["lastPacketSentTimestamp"],
            "100");
  EXPECT_EQ(request.request.upload_media_stats->sections[1].id,
            "rtc_transport_id");
  EXPECT_EQ(request.request.upload_media_stats->sections[1].type, "transport");
  EXPECT_EQ(request.request.upload_media_stats->sections[1].values.size(), 1);
  EXPECT_EQ(request.request.upload_media_stats->sections[1].values["bytesSent"],
            "1000");
}

TEST(StatsRequestFromReportTest, IgnoresSectionsThatHaveNoAttributes) {
  auto report = webrtc::RTCStatsReport::Create(webrtc::Timestamp::Zero());
  auto candidate_pair_section =
      std::make_unique<webrtc::RTCIceCandidatePairStats>(
          "candidate_pair_id", webrtc::Timestamp::Zero());
  candidate_pair_section->last_packet_sent_timestamp = 100;
  candidate_pair_section->last_packet_received_timestamp = 200;
  auto rtc_transport_section = std::make_unique<webrtc::RTCTransportStats>(
      "rtc_transport_id", webrtc::Timestamp::Zero());
  report->AddStats(std::move(candidate_pair_section));
  report->AddStats(std::move(rtc_transport_section));
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> allowlist =
      {
          {"candidate-pair",
           {"lastPacketSentTimestamp", "lastPacketReceivedTimestamp"}},
          {"transport", {"bytesSent", "bytesReceived"}},
      };

  MediaStatsChannelFromClient request =
      StatsRequestFromReport(report, /*stats_request_id=*/7, allowlist);

  ASSERT_EQ(request.request.upload_media_stats->sections.size(), 1);
  EXPECT_EQ(request.request.upload_media_stats->sections[0].id,
            "candidate_pair_id");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].type,
            "candidate-pair");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].values.size(), 2);
  EXPECT_EQ(request.request.upload_media_stats->sections[0]
                .values["lastPacketSentTimestamp"],
            "100");
  EXPECT_EQ(request.request.upload_media_stats->sections[0]
                .values["lastPacketReceivedTimestamp"],
            "200");
}

TEST(StatsRequestFromReportTest, IgnoresAllowedAttributesThatHaveNoValue) {
  auto report = webrtc::RTCStatsReport::Create(webrtc::Timestamp::Zero());
  auto candidate_pair_section =
      std::make_unique<webrtc::RTCIceCandidatePairStats>(
          "candidate_pair_id", webrtc::Timestamp::Zero());
  candidate_pair_section->last_packet_sent_timestamp = 100;
  candidate_pair_section->last_packet_received_timestamp = 200;
  auto rtc_transport_section = std::make_unique<webrtc::RTCTransportStats>(
      "rtc_transport_id", webrtc::Timestamp::Zero());
  rtc_transport_section->bytes_sent = 1000;
  report->AddStats(std::move(candidate_pair_section));
  report->AddStats(std::move(rtc_transport_section));
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> allowlist =
      {
          {"candidate-pair",
           {"lastPacketSentTimestamp", "lastPacketReceivedTimestamp"}},
          {"transport", {"bytesSent", "bytesReceived"}},
      };

  MediaStatsChannelFromClient request =
      StatsRequestFromReport(report, /*stats_request_id=*/7, allowlist);

  ASSERT_EQ(request.request.upload_media_stats->sections.size(), 2);
  EXPECT_EQ(request.request.upload_media_stats->sections[0].id,
            "candidate_pair_id");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].type,
            "candidate-pair");
  EXPECT_EQ(request.request.upload_media_stats->sections[0].values.size(), 2);
  EXPECT_EQ(request.request.upload_media_stats->sections[0]
                .values["lastPacketSentTimestamp"],
            "100");
  EXPECT_EQ(request.request.upload_media_stats->sections[0]
                .values["lastPacketReceivedTimestamp"],
            "200");
  EXPECT_EQ(request.request.upload_media_stats->sections[1].id,
            "rtc_transport_id");
  EXPECT_EQ(request.request.upload_media_stats->sections[1].type, "transport");
  EXPECT_EQ(request.request.upload_media_stats->sections[1].values.size(), 1);
  EXPECT_EQ(request.request.upload_media_stats->sections[1].values["bytesSent"],
            "1000");
}

}  // namespace
}  // namespace meet
