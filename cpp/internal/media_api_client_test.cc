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

#include "cpp/internal/media_api_client.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/base/log_severity.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/api/media_stats_resource.h"
#include "cpp/api/session_control_resource.h"
#include "cpp/api/video_assignment_resource.h"
#include "cpp/internal/conference_data_channel_interface.h"
#include "cpp/internal/conference_peer_connection.h"
#include "cpp/internal/conference_peer_connection_interface.h"
#include "cpp/internal/testing/mock_media_api_client_observer.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/media_stream_interface.h"
#include "webrtc/api/media_types.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtp_headers.h"
#include "webrtc/api/rtp_packet_info.h"
#include "webrtc/api/rtp_packet_infos.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/stats/rtc_stats_collector_callback.h"
#include "webrtc/api/stats/rtc_stats_report.h"
#include "webrtc/api/stats/rtcstats_objects.h"
#include "webrtc/api/test/mock_media_stream_interface.h"
#include "webrtc/api/test/mock_rtp_transceiver.h"
#include "webrtc/api/test/mock_rtpreceiver.h"
#include "webrtc/api/test/mock_video_track.h"
#include "webrtc/api/transport/rtp/rtp_source.h"
#include "webrtc/api/units/timestamp.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/api/video/video_sink_interface.h"
#include "webrtc/api/video/video_source_interface.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {
namespace {

using ::base_logging::WARNING;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::Return;
using ::testing::ScopedMockLog;
using ::testing::SizeIs;
using ::testing::status::StatusIs;

std::unique_ptr<rtc::Thread> CreateThread(absl::string_view name) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->SetName(name, nullptr);
  EXPECT_TRUE(thread->Start());
  return thread;
}

class MockConferencePeerConnection : public ConferencePeerConnectionInterface {
 public:
  MockConferencePeerConnection() {
    ON_CALL(*this, SetTrackSignaledCallback).WillByDefault(Return());
    ON_CALL(*this, SetDisconnectCallback).WillByDefault(Return());
    ON_CALL(*this, Close).WillByDefault(Return());
  }

  MOCK_METHOD(absl::Status, Connect,
              (absl::string_view join_endpoint, absl::string_view conference_id,
               absl::string_view access_token),
              (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, SetTrackSignaledCallback, (TrackSignaledCallback callback),
              (override));
  MOCK_METHOD(void, SetDisconnectCallback, (DisconnectCallback callback),
              (override));
  MOCK_METHOD(void, GetStats, (webrtc::RTCStatsCollectorCallback * callback),
              (override));
};

class MockConferenceDataChannel : public ConferenceDataChannelInterface {
 public:
  MockConferenceDataChannel() {
    ON_CALL(*this, SetCallback).WillByDefault(Return());
  }

  MOCK_METHOD(void, SetCallback, (ResourceUpdateCallback), (override));
  MOCK_METHOD(absl::Status, SendRequest, (ResourceRequest), (override));
};

MediaApiClient::ConferenceDataChannels CreateConferenceDataChannels() {
  return MediaApiClient::ConferenceDataChannels(
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::make_unique<MockConferenceDataChannel>(),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });
}

TEST(MediaApiClientTest, ConnectActiveConferenceSucceeds) {
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  EXPECT_CALL(*observer, OnDisconnected).Times(0);
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  absl::Notification connect_called_notification;
  EXPECT_CALL(*peer_connection,
              Connect("join_endpoint", "conference_id", "access_token"))
      .WillOnce([&connect_called_notification] {
        connect_called_notification.Notify();
        return absl::OkStatus();
      });
  MediaApiClient client(CreateThread("client_thread"),
                        CreateThread("worker_thread"), std::move(observer),
                        std::move(peer_connection),
                        CreateConferenceDataChannels());

  absl::Status status = client.ConnectActiveConference(
      "join_endpoint", "conference_id", "access_token");
  connect_called_notification.WaitForNotificationWithTimeout(absl::Seconds(1));

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(connect_called_notification.HasBeenNotified());
}

TEST(MediaApiClientTest,
     ConnectActiveConferenceDisconnectsClientIfConnectionFails) {
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  absl::Status disconnected_status;
  absl::Notification disconnected_notification;
  EXPECT_CALL(*observer, OnDisconnected)
      .WillOnce([&disconnected_status,
                 &disconnected_notification](absl::Status status) {
        disconnected_status = status;
        disconnected_notification.Notify();
      });
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  EXPECT_CALL(*peer_connection,
              Connect("join_endpoint", "conference_id", "access_token"))
      .WillOnce([] { return absl::InternalError("Failed to connect."); });
  MediaApiClient client(CreateThread("client_thread"),
                        CreateThread("worker_thread"), std::move(observer),
                        std::move(peer_connection),
                        CreateConferenceDataChannels());

  absl::Status connect_status = client.ConnectActiveConference(
      "join_endpoint", "conference_id", "access_token");
  disconnected_notification.WaitForNotificationWithTimeout(absl::Seconds(1));

  EXPECT_TRUE(connect_status.ok());
  EXPECT_TRUE(disconnected_notification.HasBeenNotified());
  EXPECT_THAT(disconnected_status,
              StatusIs(absl::StatusCode::kInternal, "Failed to connect."));
}

TEST(MediaApiClientTest,
     ConnectActiveConferenceLogsWarningIfClientStateChanges) {
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  // Store a pointer, as the client will be used in the `Connect` lambda.
  MockConferencePeerConnection* peer_connection_ptr = peer_connection.get();
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      webrtc::make_ref_counted<MockMediaApiClientObserver>(),
      std::move(peer_connection), CreateConferenceDataChannels());
  EXPECT_CALL(*peer_connection_ptr,
              Connect("join_endpoint", "conference_id", "access_token"))
      .WillOnce([&client] {
        // Disconnect the client before the connection completes, changing the
        // client's state.
        (void)client.LeaveConference(/*request_id=*/7);
        return absl::OkStatus();
      });
  ScopedMockLog log(kDoNotCaptureLogsYet);
  absl::Notification log_notification;
  EXPECT_CALL(
      log, Log(WARNING, _,
               "Client in disconnected state instead of connecting state after "
               "starting connection."))
      .WillOnce(
          [&log_notification](int, const std::string&, const std::string& msg) {
            log_notification.Notify();
          });
  log.StartCapturingLogs();

  (void)client.ConnectActiveConference("join_endpoint", "conference_id",
                                       "access_token");

  EXPECT_TRUE(
      log_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MediaApiClientTest, HandlesPeerConnectionDisconnected) {
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  ConferencePeerConnection::DisconnectCallback
      peer_connection_disconnect_callback;
  EXPECT_CALL(*peer_connection, SetDisconnectCallback)
      .WillOnce([&peer_connection_disconnect_callback](
                    ConferencePeerConnection::DisconnectCallback callback) {
        peer_connection_disconnect_callback = std::move(callback);
      });
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  absl::Status client_disconnected_status;
  absl::Notification client_disconnected_notification;
  EXPECT_CALL(*observer, OnDisconnected)
      .WillOnce([&client_disconnected_status,
                 &client_disconnected_notification](absl::Status status) {
        client_disconnected_status = status;
        client_disconnected_notification.Notify();
      });
  MediaApiClient client(CreateThread("client_thread"),
                        CreateThread("worker_thread"), std::move(observer),
                        std::move(peer_connection),
                        CreateConferenceDataChannels());

  peer_connection_disconnect_callback(
      absl::InternalError("Peer connection disconnected."));

  ASSERT_TRUE(client_disconnected_notification.WaitForNotificationWithTimeout(
      absl::Seconds(1)));
  EXPECT_THAT(
      client_disconnected_status,
      StatusIs(absl::StatusCode::kInternal, "Peer connection disconnected."));
}

TEST(MediaApiClientTest, CallsObserverOnResourceUpdate) {
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  ResourceUpdate received_resource_update;
  absl::Notification resource_update_notification;
  EXPECT_CALL(*observer, OnResourceUpdate)
      .WillOnce([&received_resource_update, &resource_update_notification](
                    ResourceUpdate resource_update) {
        received_resource_update = std::move(resource_update);
        resource_update_notification.Notify();
      });
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ConferenceDataChannelInterface::ResourceUpdateCallback
      resource_update_callback;
  EXPECT_CALL(*session_control_data_channel, SetCallback)
      .WillOnce(
          [&](ConferenceDataChannelInterface::ResourceUpdateCallback callback) {
            resource_update_callback = std::move(callback);
          });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      std::move(observer), std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  resource_update_callback(SessionControlChannelToClient{
      .response = SessionControlResponse({.request_id = 7})});

  ASSERT_TRUE(resource_update_notification.WaitForNotificationWithTimeout(
      absl::Seconds(1)));
  ASSERT_TRUE(std::holds_alternative<SessionControlChannelToClient>(
      received_resource_update));
  EXPECT_TRUE(std::get<SessionControlChannelToClient>(received_resource_update)
                  .response->request_id == 7);
}

TEST(MediaApiClientTest,
     JoinsAfterReceivingJoinedSessionStatusWhileInJoiningState) {
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  absl::Notification joined_notification;
  EXPECT_CALL(*observer, OnJoined).WillOnce([&joined_notification] {
    joined_notification.Notify();
  });
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  absl::Notification connect_called_notification;
  ON_CALL(*peer_connection,
          Connect("join_endpoint", "conference_id", "access_token"))
      .WillByDefault([&connect_called_notification] {
        connect_called_notification.Notify();
        return absl::OkStatus();
      });
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ConferenceDataChannelInterface::ResourceUpdateCallback
      resource_update_callback;
  EXPECT_CALL(*session_control_data_channel, SetCallback)
      .WillOnce(
          [&](ConferenceDataChannelInterface::ResourceUpdateCallback callback) {
            resource_update_callback = std::move(callback);
          });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      std::move(observer), std::move(peer_connection),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });
  (void)client.ConnectActiveConference("join_endpoint", "conference_id",
                                       "access_token");
  ASSERT_TRUE(connect_called_notification.WaitForNotificationWithTimeout(
      absl::Seconds(1)));

  SessionControlChannelToClient session_control_update =
      SessionControlChannelToClient{
          .resources = std::vector<SessionControlResourceSnapshot>{
              SessionControlResourceSnapshot{
                  .session_status = SessionStatus{
                      .connection_state =
                          SessionStatus::ConferenceConnectionState::kJoined}}}};
  resource_update_callback(std::move(session_control_update));

  EXPECT_TRUE(
      joined_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MediaApiClientTest,
     LogsWarningIfReceivingJoinedSessionStatusWhileNotInJoiningState) {
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  EXPECT_CALL(*observer, OnJoined).Times(0);
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ConferenceDataChannelInterface::ResourceUpdateCallback
      resource_update_callback;
  EXPECT_CALL(*session_control_data_channel, SetCallback)
      .WillOnce(
          [&](ConferenceDataChannelInterface::ResourceUpdateCallback callback) {
            resource_update_callback = std::move(callback);
          });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      std::move(observer), std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  absl::Notification log_notification;
  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([&message, &log_notification](int, const std::string&,
                                              const std::string& msg) {
        message = msg;
        log_notification.Notify();
      });
  log.StartCapturingLogs();
  SessionControlChannelToClient session_control_update =
      SessionControlChannelToClient{
          .resources = std::vector<SessionControlResourceSnapshot>{
              SessionControlResourceSnapshot{
                  .session_status = SessionStatus{
                      .connection_state =
                          SessionStatus::ConferenceConnectionState::kJoined}}}};
  resource_update_callback(std::move(session_control_update));

  ASSERT_TRUE(
      log_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_THAT(message, HasSubstr("Received joined session status while in "
                                 "ready state instead of joining state."));
}

TEST(MediaApiClientTest, DisconnectsAfterReceivingDisconnectedSessionStatus) {
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  absl::Status client_disconnected_status;
  absl::Notification client_disconnected_notification;
  EXPECT_CALL(*observer, OnDisconnected)
      .WillOnce([&client_disconnected_status,
                 &client_disconnected_notification](absl::Status status) {
        client_disconnected_status = status;
        client_disconnected_notification.Notify();
      });
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ConferenceDataChannelInterface::ResourceUpdateCallback
      resource_update_callback;
  EXPECT_CALL(*session_control_data_channel, SetCallback)
      .WillOnce(
          [&](ConferenceDataChannelInterface::ResourceUpdateCallback callback) {
            resource_update_callback = std::move(callback);
          });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      std::move(observer), std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  SessionControlChannelToClient session_control_update =
      SessionControlChannelToClient{
          .resources = std::vector<SessionControlResourceSnapshot>{
              SessionControlResourceSnapshot{
                  .session_status = SessionStatus{
                      .connection_state = SessionStatus::
                          ConferenceConnectionState::kDisconnected}}}};
  resource_update_callback(std::move(session_control_update));

  ASSERT_TRUE(client_disconnected_notification.WaitForNotificationWithTimeout(
      absl::Seconds(1)));
  // Disconnections triggered by session control updates are considered OK.
  EXPECT_TRUE(client_disconnected_status.ok());
}

TEST(MediaApiClientTest, DisconnectingTwiceLogsWarning) {
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ConferenceDataChannelInterface::ResourceUpdateCallback
      resource_update_callback;
  EXPECT_CALL(*session_control_data_channel, SetCallback)
      .WillOnce(
          [&](ConferenceDataChannelInterface::ResourceUpdateCallback callback) {
            resource_update_callback = std::move(callback);
          });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      webrtc::make_ref_counted<MockMediaApiClientObserver>(),
      std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  absl::Notification log_notification;
  // Ignore other warnings.
  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([&message, &log_notification](int, const std::string&,
                                              const std::string& msg) {
        message = msg;
        log_notification.Notify();
      });
  log.StartCapturingLogs();
  SessionControlChannelToClient session_control_update1 =
      SessionControlChannelToClient{
          .resources = std::vector<SessionControlResourceSnapshot>{
              SessionControlResourceSnapshot{
                  .session_status = SessionStatus{
                      .connection_state = SessionStatus::
                          ConferenceConnectionState::kDisconnected}}}};
  resource_update_callback(std::move(session_control_update1));
  SessionControlChannelToClient session_control_update2 =
      SessionControlChannelToClient{
          .resources = std::vector<SessionControlResourceSnapshot>{
              SessionControlResourceSnapshot{
                  .session_status = SessionStatus{
                      .connection_state = SessionStatus::
                          ConferenceConnectionState::kDisconnected}}}};
  resource_update_callback(std::move(session_control_update2));

  ASSERT_TRUE(
      log_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_THAT(message, HasSubstr("Client attempted to disconnect with status"));
}

TEST(MediaApiClientTest, DisconnectingClosesConferencePeerConnection) {
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  absl::Notification close_called_notification;
  // The peer connection is closed twice; once when the client is
  // disconnected, and once when the client is destroyed. This test only
  // cares about the first close call.
  EXPECT_CALL(*peer_connection, Close);
  EXPECT_CALL(*peer_connection, Close)
      .WillOnce(
          [&close_called_notification] { close_called_notification.Notify(); })
      .RetiresOnSaturation();
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ConferenceDataChannelInterface::ResourceUpdateCallback
      resource_update_callback;
  EXPECT_CALL(*session_control_data_channel, SetCallback)
      .WillOnce(
          [&](ConferenceDataChannelInterface::ResourceUpdateCallback callback) {
            resource_update_callback = std::move(callback);
          });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      webrtc::make_ref_counted<MockMediaApiClientObserver>(),
      std::move(peer_connection),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  SessionControlChannelToClient session_control_update =
      SessionControlChannelToClient{
          .resources = std::vector<SessionControlResourceSnapshot>{
              SessionControlResourceSnapshot{
                  .session_status = SessionStatus{
                      .connection_state = SessionStatus::
                          ConferenceConnectionState::kDisconnected}}}};
  resource_update_callback(std::move(session_control_update));

  EXPECT_TRUE(close_called_notification.WaitForNotificationWithTimeout(
      absl::Seconds(1)));
}

TEST(MediaApiClientTest, StartsSendingStatsRequestsAfterReceivingStatsUpdate) {
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  absl::Notification disconnected_notification;
  EXPECT_CALL(*observer, OnDisconnected).WillOnce([&disconnected_notification] {
    disconnected_notification.Notify();
  });
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  ON_CALL(*peer_connection, GetStats)
      .WillByDefault([](webrtc::RTCStatsCollectorCallback* callback) {
        rtc::scoped_refptr<webrtc::RTCStatsReport> report =
            webrtc::RTCStatsReport::Create(webrtc::Timestamp::Zero());
        auto candidate_pair_section =
            std::make_unique<webrtc::RTCIceCandidatePairStats>(
                "candidate_pair_id", webrtc::Timestamp::Zero());
        candidate_pair_section->last_packet_sent_timestamp = 100;
        candidate_pair_section->last_packet_received_timestamp = 200;
        report->AddStats(std::move(candidate_pair_section));
        callback->OnStatsDelivered(std::move(report));
      });
  auto media_stats_data_channel = std::make_unique<MockConferenceDataChannel>();
  std::vector<ResourceRequest> received_requests;
  EXPECT_CALL(*media_stats_data_channel, SendRequest)
      // Expect 3 requests: 1 initial request, 2 periodic requests.
      .Times(3)
      .WillRepeatedly([&received_requests](ResourceRequest request) {
        received_requests.push_back(std::move(request));
        return absl::OkStatus();
      });
  ConferenceDataChannelInterface::ResourceUpdateCallback
      resource_update_callback;
  EXPECT_CALL(*media_stats_data_channel, SetCallback)
      .WillOnce(
          [&](ConferenceDataChannelInterface::ResourceUpdateCallback callback) {
            resource_update_callback = std::move(callback);
          });
  ConferenceDataChannelInterface::ResourceUpdateCallback
      session_control_update_callback;
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  EXPECT_CALL(*session_control_data_channel, SetCallback)
      .WillOnce(
          [&](ConferenceDataChannelInterface::ResourceUpdateCallback callback) {
            session_control_update_callback = std::move(callback);
          });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      std::move(observer), std::move(peer_connection),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::move(media_stats_data_channel),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  MediaStatsChannelToClient media_stats_update = MediaStatsChannelToClient{
      .resources =
          std::vector<MediaStatsResourceSnapshot>{MediaStatsResourceSnapshot{
              .configuration = MediaStatsConfiguration{
                  .upload_interval_seconds = 1,
                  .allowlist = {{"candidate-pair",
                                 {"lastPacketSentTimestamp",
                                  "lastPacketReceivedTimestamp"}}}}}}};
  resource_update_callback(std::move(media_stats_update));
  // The upload interval is 1 second, so wait for 2.5 seconds to ensure that 2
  // periodic requests are sent.
  absl::SleepFor(absl::Seconds(2.5));
  // Disconnect the client to stop stats collection.
  SessionControlChannelToClient session_control_update =
      SessionControlChannelToClient{
          .resources = std::vector<SessionControlResourceSnapshot>{
              SessionControlResourceSnapshot{
                  .session_status = SessionStatus{
                      .connection_state = SessionStatus::
                          ConferenceConnectionState::kDisconnected}}}};
  session_control_update_callback(std::move(session_control_update));

  ASSERT_TRUE(disconnected_notification.WaitForNotificationWithTimeout(
      absl::Seconds(1)));
  ASSERT_EQ(received_requests.size(), 3);
  MediaStatsChannelFromClient request1 =
      std::get<MediaStatsChannelFromClient>(received_requests[0]);
  MediaStatsChannelFromClient request2 =
      std::get<MediaStatsChannelFromClient>(received_requests[1]);
  MediaStatsChannelFromClient request3 =
      std::get<MediaStatsChannelFromClient>(received_requests[2]);
  EXPECT_EQ(request1.request.request_id, 1);
  EXPECT_EQ(request2.request.request_id, 2);
  EXPECT_EQ(request3.request.request_id, 3);
  ASSERT_EQ(request1.request.upload_media_stats->sections.size(), 1);
  ASSERT_EQ(request2.request.upload_media_stats->sections.size(), 1);
  ASSERT_EQ(request3.request.upload_media_stats->sections.size(), 1);
  EXPECT_EQ(request1.request.upload_media_stats->sections[0].id,
            "candidate_pair_id");
  EXPECT_EQ(request2.request.upload_media_stats->sections[0].id,
            "candidate_pair_id");
  EXPECT_EQ(request3.request.upload_media_stats->sections[0].id,
            "candidate_pair_id");
  ASSERT_EQ(request1.request.upload_media_stats->sections[0].values.size(), 2);
  ASSERT_EQ(request2.request.upload_media_stats->sections[0].values.size(), 2);
  ASSERT_EQ(request3.request.upload_media_stats->sections[0].values.size(), 2);
  EXPECT_EQ(request1.request.upload_media_stats->sections[0]
                .values["lastPacketSentTimestamp"],
            "100");
  EXPECT_EQ(request2.request.upload_media_stats->sections[0]
                .values["lastPacketSentTimestamp"],
            "100");
  EXPECT_EQ(request3.request.upload_media_stats->sections[0]
                .values["lastPacketSentTimestamp"],
            "100");
  EXPECT_EQ(request1.request.upload_media_stats->sections[0]
                .values["lastPacketReceivedTimestamp"],
            "200");
  EXPECT_EQ(request2.request.upload_media_stats->sections[0]
                .values["lastPacketReceivedTimestamp"],
            "200");
  EXPECT_EQ(request3.request.upload_media_stats->sections[0]
                .values["lastPacketReceivedTimestamp"],
            "200");
}

TEST(MediaApiClientTest, SendMediaStatsRequestReturnsError) {
  auto media_stats_data_channel = std::make_unique<MockConferenceDataChannel>();
  absl::Notification send_request_called_notification;
  ON_CALL(*media_stats_data_channel, SendRequest)
      .WillByDefault(
          [&send_request_called_notification](ResourceRequest request) {
            send_request_called_notification.Notify();
            return absl::OkStatus();
          });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      webrtc::make_ref_counted<MockMediaApiClientObserver>(),
      std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::move(media_stats_data_channel),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::make_unique<MockConferenceDataChannel>(),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  absl::Status status = client.SendRequest(MediaStatsChannelFromClient{
      .request = MediaStatsRequest{.request_id = 123}});

  EXPECT_THAT(
      status,
      StatusIs(absl::StatusCode::kInternal,
               "Media stats requests should not be sent directly. This client "
               "implementation handles stats collection internally."));
  EXPECT_FALSE(send_request_called_notification.HasBeenNotified());
}

TEST(MediaApiClientTest, SendSessionControlRequestSucceeds) {
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ResourceRequest received_resource_request;
  EXPECT_CALL(*session_control_data_channel, SendRequest)
      .WillOnce([&received_resource_request](ResourceRequest request) {
        received_resource_request = std::move(request);
        return absl::OkStatus();
      });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      webrtc::make_ref_counted<MockMediaApiClientObserver>(),
      std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  absl::Status status = client.SendRequest(SessionControlChannelFromClient{
      .request = SessionControlRequest{.request_id = 123}});

  EXPECT_OK(status);
  ASSERT_TRUE(std::holds_alternative<SessionControlChannelFromClient>(
      received_resource_request));
  EXPECT_EQ(std::get<SessionControlChannelFromClient>(received_resource_request)
                .request.request_id,
            123);
}

TEST(MediaApiClientTest, SendVideoAssignmentRequestSucceeds) {
  auto video_assignment_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ResourceRequest received_resource_request;
  EXPECT_CALL(*video_assignment_data_channel, SendRequest)
      .WillOnce([&received_resource_request](ResourceRequest request) {
        received_resource_request = std::move(request);
        return absl::OkStatus();
      });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      webrtc::make_ref_counted<MockMediaApiClientObserver>(),
      std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::make_unique<MockConferenceDataChannel>(),
          .video_assignment = std::move(video_assignment_data_channel),
      });

  absl::Status status = client.SendRequest(VideoAssignmentChannelFromClient{
      .request = VideoAssignmentRequest{.request_id = 123}});

  EXPECT_OK(status);
  ASSERT_TRUE(std::holds_alternative<VideoAssignmentChannelFromClient>(
      received_resource_request));
  EXPECT_EQ(
      std::get<VideoAssignmentChannelFromClient>(received_resource_request)
          .request.request_id,
      123);
}

TEST(MediaApiClientTest, SendRequestLogsWarningIfClientNotJoined) {
  auto video_assignment_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ON_CALL(*video_assignment_data_channel, SendRequest)
      .WillByDefault(Return(absl::OkStatus()));
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      webrtc::make_ref_counted<MockMediaApiClientObserver>(),
      std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::make_unique<MockConferenceDataChannel>(),
          .video_assignment = std::move(video_assignment_data_channel),
      });

  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([&message](int, const std::string&, const std::string& msg) {
        message = msg;
      });
  log.StartCapturingLogs();
  absl::Status status = client.SendRequest(VideoAssignmentChannelFromClient{
      .request = VideoAssignmentRequest{.request_id = 123}});

  EXPECT_THAT(message,
              "SendRequest called while client is in ready state instead of "
              "joined state. Requests are not guaranteed to be delivered if "
              "the client is not joined into the conference.");
}

TEST(MediaApiClientTest, LeaveConferenceSendsLeaveRequest) {
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ResourceRequest received_resource_request;
  EXPECT_CALL(*session_control_data_channel, SendRequest)
      .WillOnce([&received_resource_request](ResourceRequest request) {
        received_resource_request = std::move(request);
        return absl::OkStatus();
      });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      webrtc::make_ref_counted<MockMediaApiClientObserver>(),
      std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  absl::Status status = client.LeaveConference(/*request_id=*/123);

  EXPECT_OK(status);
  ASSERT_TRUE(std::holds_alternative<SessionControlChannelFromClient>(
      received_resource_request));
  EXPECT_EQ(std::get<SessionControlChannelFromClient>(received_resource_request)
                .request.request_id,
            123);
  EXPECT_TRUE(
      std::get<SessionControlChannelFromClient>(received_resource_request)
          .request.leave_request.has_value());
}

TEST(MediaApiClientTest, LeaveConferenceDisconnectsClientIfNotJoined) {
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  absl::Status client_disconnect_status;
  absl::Notification disconnected_notification;
  EXPECT_CALL(*observer, OnDisconnected)
      .WillOnce([&client_disconnect_status,
                 &disconnected_notification](absl::Status status) {
        client_disconnect_status = status;
        disconnected_notification.Notify();
      });
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ON_CALL(*session_control_data_channel, SendRequest)
      .WillByDefault(Return(absl::OkStatus()));
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      std::move(observer), std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });

  absl::Status status = client.LeaveConference(/*request_id=*/123);

  ASSERT_TRUE(disconnected_notification.WaitForNotificationWithTimeout(
      absl::Seconds(1)));
  EXPECT_THAT(
      client_disconnect_status,
      StatusIs(
          absl::StatusCode::kInternal,
          "LeaveConference called when in ready state instead of joined state. "
          "Requests are not guaranteed to be delivered unless the client is "
          "joined into the conference. Therefore, the client was disconnected "
          "immediately."));
}

TEST(MediaApiClientTest, LeaveConferenceFailsIfDisconnected) {
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  absl::Notification client_disconnected_notification;
  EXPECT_CALL(*observer, OnDisconnected)
      .WillOnce([&client_disconnected_notification](absl::Status status) {
        client_disconnected_notification.Notify();
      });
  auto session_control_data_channel =
      std::make_unique<MockConferenceDataChannel>();
  ConferenceDataChannelInterface::ResourceUpdateCallback
      resource_update_callback;
  EXPECT_CALL(*session_control_data_channel, SetCallback)
      .WillOnce(
          [&](ConferenceDataChannelInterface::ResourceUpdateCallback callback) {
            resource_update_callback = std::move(callback);
          });
  absl::Notification notification;
  ON_CALL(*session_control_data_channel, SendRequest)
      .WillByDefault([&notification](ResourceRequest request) {
        notification.Notify();
        return absl::OkStatus();
      });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      std::move(observer), std::make_unique<MockConferencePeerConnection>(),
      MediaApiClient::ConferenceDataChannels{
          .media_entries = std::make_unique<MockConferenceDataChannel>(),
          .media_stats = std::make_unique<MockConferenceDataChannel>(),
          .participants = std::make_unique<MockConferenceDataChannel>(),
          .session_control = std::move(session_control_data_channel),
          .video_assignment = std::make_unique<MockConferenceDataChannel>(),
      });
  SessionControlChannelToClient session_control_update =
      SessionControlChannelToClient{
          .resources = std::vector<SessionControlResourceSnapshot>{
              SessionControlResourceSnapshot{
                  .session_status = SessionStatus{
                      .connection_state = SessionStatus::
                          ConferenceConnectionState::kDisconnected}}}};
  resource_update_callback(std::move(session_control_update));

  ASSERT_TRUE(client_disconnected_notification.WaitForNotificationWithTimeout(
      absl::Seconds(1)));
  absl::Status status = client.LeaveConference(/*request_id=*/123);

  EXPECT_THAT(status,
              StatusIs(absl::StatusCode::kInternal,
                       "LeaveConference called in disconnected state."));
  EXPECT_FALSE(notification.HasBeenNotified());
}

TEST(MediaApiClientTest, HandlesSignaledAudioTrack) {
  webrtc::AudioTrackSinkInterface* audio_track_sink;
  // Audio track.
  rtc::scoped_refptr<webrtc::MockAudioTrack> mock_audio_track =
      webrtc::MockAudioTrack::Create();
  ON_CALL(*mock_audio_track, AddSink)
      .WillByDefault(
          [&audio_track_sink](webrtc::AudioTrackSinkInterface* sink) {
            audio_track_sink = sink;
          });
  // Receiver.
  auto mock_receiver = rtc::scoped_refptr<webrtc::MockRtpReceiver>(
      new webrtc::MockRtpReceiver());
  webrtc::RtpSource csrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/123, webrtc::RtpSourceType::CSRC,
      /*rtp_timestamp=*/1111111,
      {.audio_level = 100,
       .absolute_capture_time =
           webrtc::AbsoluteCaptureTime(1234567890, 1000000000)});
  webrtc::RtpSource ssrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/456, webrtc::RtpSourceType::SSRC,
      /*rtp_timestamp=*/2222222,
      {.audio_level = 100,
       .absolute_capture_time =
           webrtc::AbsoluteCaptureTime(1234567890, 1000000000)});
  ON_CALL(*mock_receiver, GetSources)
      .WillByDefault(Return(std::vector<webrtc::RtpSource>{
          std::move(csrc_rtp_source), std::move(ssrc_rtp_source)}));
  ON_CALL(*mock_receiver, media_type)
      .WillByDefault(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
  ON_CALL(*mock_receiver, track).WillByDefault(Return(mock_audio_track));
  // Transceiver.
  rtc::scoped_refptr<webrtc::MockRtpTransceiver> mock_transceiver =
      webrtc::MockRtpTransceiver::Create();
  ON_CALL(*mock_transceiver, mid).WillByDefault(Return("mid"));
  ON_CALL(*mock_transceiver, receiver).WillByDefault(Return(mock_receiver));
  // Observer.
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  AudioFrame received_frame;
  EXPECT_CALL(*observer, OnAudioFrame)
      .WillOnce([&received_frame](AudioFrame frame) {
        received_frame = std::move(frame);
      });
  // Peer connection.
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  ConferencePeerConnection::TrackSignaledCallback track_signaled_callback;
  EXPECT_CALL(*peer_connection, SetTrackSignaledCallback)
      .WillOnce([&](ConferencePeerConnection::TrackSignaledCallback callback) {
        track_signaled_callback = std::move(callback);
      });
  // Client.
  MediaApiClient client(CreateThread("client_thread"),
                        CreateThread("worker_thread"), std::move(observer),
                        std::move(peer_connection),
                        CreateConferenceDataChannels());
  track_signaled_callback(std::move(mock_transceiver));

  int16_t pcm_data[2 * 100];
  audio_track_sink->OnData(pcm_data,
                           /*bits_per_sample=*/16,
                           /*sample_rate=*/48000,
                           /*number_of_channels=*/2,
                           /*number_of_frames=*/100,
                           /*absolute_capture_timestamp_ms=*/std::nullopt);

  EXPECT_THAT(received_frame.pcm16, SizeIs(100 * 2));
  EXPECT_EQ(received_frame.bits_per_sample, 16);
  EXPECT_EQ(received_frame.sample_rate, 48000);
  EXPECT_EQ(received_frame.number_of_channels, 2);
  EXPECT_EQ(received_frame.number_of_frames, 100);
  EXPECT_EQ(received_frame.contributing_source, 123);
  EXPECT_EQ(received_frame.synchronization_source, 456);
}

TEST(MediaApiClientTest, HandlesSignaledVideoTrack) {
  rtc::VideoSinkInterface<webrtc::VideoFrame>* video_track_sink;
  // Video track.
  rtc::scoped_refptr<webrtc::MockVideoTrack> mock_video_track =
      webrtc::MockVideoTrack::Create();
  ON_CALL(*mock_video_track, AddOrUpdateSink)
      .WillByDefault(
          [&video_track_sink](
              rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
              const rtc::VideoSinkWants&) { video_track_sink = sink; });
  // Receiver.
  auto mock_receiver = rtc::scoped_refptr<webrtc::MockRtpReceiver>(
      new webrtc::MockRtpReceiver());
  webrtc::RtpSource csrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/123, webrtc::RtpSourceType::CSRC,
      /*rtp_timestamp=*/1111111,
      {.audio_level = 100,
       .absolute_capture_time =
           webrtc::AbsoluteCaptureTime(1234567890, 1000000000)});
  webrtc::RtpSource ssrc_rtp_source(
      webrtc::Timestamp::Micros(1234567890),
      /*source_id=*/456, webrtc::RtpSourceType::SSRC,
      /*rtp_timestamp=*/2222222,
      {.audio_level = 100,
       .absolute_capture_time =
           webrtc::AbsoluteCaptureTime(1234567890, 1000000000)});
  ON_CALL(*mock_receiver, GetSources)
      .WillByDefault(Return(std::vector<webrtc::RtpSource>{
          std::move(csrc_rtp_source), std::move(ssrc_rtp_source)}));
  ON_CALL(*mock_receiver, media_type)
      .WillByDefault(Return(cricket::MediaType::MEDIA_TYPE_VIDEO));
  ON_CALL(*mock_receiver, track).WillByDefault(Return(mock_video_track));
  // Transceiver.
  rtc::scoped_refptr<webrtc::MockRtpTransceiver> mock_transceiver =
      webrtc::MockRtpTransceiver::Create();
  ON_CALL(*mock_transceiver, mid).WillByDefault(Return("mid"));
  ON_CALL(*mock_transceiver, receiver).WillByDefault(Return(mock_receiver));
  // Observer.
  auto observer = webrtc::make_ref_counted<MockMediaApiClientObserver>();
  std::optional<VideoFrame> received_frame;
  EXPECT_CALL(*observer, OnVideoFrame)
      .WillOnce([&received_frame](VideoFrame frame) {
        received_frame.emplace(frame);
      });
  // Peer connection.
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  ConferencePeerConnection::TrackSignaledCallback track_signaled_callback;
  EXPECT_CALL(*peer_connection, SetTrackSignaledCallback)
      .WillOnce([&](ConferencePeerConnection::TrackSignaledCallback callback) {
        track_signaled_callback = std::move(callback);
      });
  // Client.
  MediaApiClient client(CreateThread("client_thread"),
                        CreateThread("worker_thread"), std::move(observer),
                        std::move(peer_connection),
                        CreateConferenceDataChannels());
  track_signaled_callback(std::move(mock_transceiver));

  webrtc::VideoFrame::Builder builder;
  webrtc::RtpPacketInfo packet_info;
  packet_info.set_csrcs({123});
  packet_info.set_ssrc(456);
  webrtc::RtpPacketInfos packet_infos({packet_info});
  builder.set_packet_infos(packet_infos);
  builder.set_video_frame_buffer(webrtc::I420Buffer::Create(42, 42));
  // The `OnFrame` callback returns a reference to the frame, so a local
  // variable must be maintained so the frame is not destroyed.
  webrtc::VideoFrame frame = builder.build();
  video_track_sink->OnFrame(frame);

  EXPECT_TRUE(received_frame.has_value());
  EXPECT_EQ(received_frame->frame.size(), 42 * 42);
  EXPECT_EQ(received_frame->contributing_source, 123);
  EXPECT_EQ(received_frame->synchronization_source, 456);
}

TEST(MediaApiClientTest, LogsWarningIfSignaledTrackIsUnsupported) {
  auto mock_receiver = rtc::scoped_refptr<webrtc::MockRtpReceiver>(
      new webrtc::MockRtpReceiver());
  // Non-audio and non-video tracks are unsupported.
  ON_CALL(*mock_receiver, media_type)
      .WillByDefault(Return(cricket::MediaType::MEDIA_TYPE_DATA));
  ON_CALL(*mock_receiver, track)
      .WillByDefault(Return(webrtc::MockAudioTrack::Create()));
  rtc::scoped_refptr<webrtc::MockRtpTransceiver> mock_transceiver =
      webrtc::MockRtpTransceiver::Create();
  ON_CALL(*mock_transceiver, mid).WillByDefault(Return("mid"));
  ON_CALL(*mock_transceiver, receiver).WillByDefault(Return(mock_receiver));
  auto peer_connection = std::make_unique<MockConferencePeerConnection>();
  ConferencePeerConnection::TrackSignaledCallback track_signaled_callback;
  EXPECT_CALL(*peer_connection, SetTrackSignaledCallback)
      .WillOnce([&](ConferencePeerConnection::TrackSignaledCallback callback) {
        track_signaled_callback = std::move(callback);
      });
  MediaApiClient client(
      CreateThread("client_thread"), CreateThread("worker_thread"),
      webrtc::make_ref_counted<MockMediaApiClientObserver>(),
      std::move(peer_connection), CreateConferenceDataChannels());
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([&message](int, const std::string&, const std::string& msg) {
        message = msg;
      });
  log.StartCapturingLogs();

  track_signaled_callback(std::move(mock_transceiver));

  EXPECT_THAT(message,
              HasSubstr("Received remote track of unsupported media type"));
}

}  // namespace
}  // namespace meet
