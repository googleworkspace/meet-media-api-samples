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

#include "cpp/internal/conference_peer_connection.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/base/log_severity.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "cpp/internal/http_connector_interface.h"
#include "cpp/internal/testing/sdp_constants.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/set_local_description_observer_interface.h"
#include "webrtc/api/set_remote_description_observer_interface.h"
#include "webrtc/api/test/mock_peerconnectioninterface.h"
#include "webrtc/api/test/mock_rtp_transceiver.h"
#include "webrtc/api/test/mock_session_description_interface.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {
namespace {

using ::base_logging::WARNING;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ScopedMockLog;
using ::testing::status::StatusIs;

// Add missing mock method to the WebRTC MockPeerConnectionInterface.
//
// ConferencePeerConnection uses different SetLocalDescription and
// SetRemoteDescription methods than the WebRTC MockPeerConnectionInterface
// provides mocks for.
class MockPeerConnection : public webrtc::MockPeerConnectionInterface {
 public:
  using webrtc::MockPeerConnectionInterface::SetLocalDescription;
  MOCK_METHOD(
      void, SetLocalDescription,
      (rtc::scoped_refptr<webrtc::SetLocalDescriptionObserverInterface>),
      (override));

  MOCK_METHOD(
      void, SetRemoteDescription,
      (std::unique_ptr<webrtc::SessionDescriptionInterface>,
       rtc::scoped_refptr<webrtc::SetRemoteDescriptionObserverInterface>),
      (override));
};

class MockHttpConnector : public HttpConnectorInterface {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, ConnectActiveConference,
              (absl::string_view join_endpoint, absl::string_view conference_id,
               absl::string_view access_token,
               absl::string_view local_description),
              (override));
};

std::unique_ptr<rtc::Thread> CreateSignalingThread() {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->SetName("signaling_thread", nullptr);
  EXPECT_TRUE(thread->Start());
  return thread;
}

TEST(ConferencePeerConnectionTest, ConnectSucceeds) {
  auto peer_connection = webrtc::make_ref_counted<MockPeerConnection>();
  EXPECT_CALL(*peer_connection, SetLocalDescription(_))
      .WillOnce([&](rtc::scoped_refptr<
                    webrtc::SetLocalDescriptionObserverInterface>
                        observer) {
        observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
      });
  auto answer_description =
      std::make_unique<webrtc::MockSessionDescriptionInterface>();
  EXPECT_CALL(*answer_description, ToString(_)).WillOnce([](std::string* str) {
    *str = kWebRtcOffer;
    return true;
  });
  EXPECT_CALL(*peer_connection, local_description())
      .WillOnce(Return(answer_description.get()));
  auto http_connector = std::make_unique<MockHttpConnector>();
  EXPECT_CALL(*http_connector,
              ConnectActiveConference("join-endpoint", "conference-id",
                                      "access-token", kWebRtcOffer))
      .WillOnce(Return(kWebRtcAnswer));
  EXPECT_CALL(*peer_connection, SetRemoteDescription(_, _))
      .WillOnce(
          [&](std::unique_ptr<webrtc::SessionDescriptionInterface> description,
              rtc::scoped_refptr<
                  webrtc::SetRemoteDescriptionObserverInterface>
                  observer) {
            std::string remote_description;
            description->ToString(&remote_description);
            EXPECT_EQ(remote_description, kWebRtcAnswer);

            observer->OnSetRemoteDescriptionComplete(webrtc::RTCError::OK());
          });
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::move(http_connector));
  conference_peer_connection.SetPeerConnection(std::move(peer_connection));

  absl::Status connect_status = conference_peer_connection.Connect(
      "join-endpoint", "conference-id", "access-token");

  EXPECT_TRUE(connect_status.ok());
}

TEST(ConferencePeerConnectionTest, ConnectFailsWithNullPeerConnection) {
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());

  absl::Status connect_status = conference_peer_connection.Connect(
      "join-endpoint", "conference-id", "access-token");

  EXPECT_THAT(connect_status, StatusIs(absl::StatusCode::kInternal,
                                       "Peer connection is null."));
}

TEST(ConferencePeerConnectionTest,
     ConnectFailsWhenSettingLocalDescriptionFails) {
  auto peer_connection = webrtc::make_ref_counted<MockPeerConnection>();
  EXPECT_CALL(*peer_connection, SetLocalDescription(_))
      .WillOnce(
          [&](rtc::scoped_refptr<
              webrtc::SetLocalDescriptionObserverInterface>
                  observer) {
            observer->OnSetLocalDescriptionComplete(
                webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                 "local-description-error"));
          });
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());
  conference_peer_connection.SetPeerConnection(std::move(peer_connection));

  absl::Status connect_status = conference_peer_connection.Connect(
      "join-endpoint", "conference-id", "access-token");

  EXPECT_THAT(connect_status, StatusIs(absl::StatusCode::kInternal,
                                       HasSubstr("local-description-error")));
}

TEST(ConferencePeerConnectionTest, ConnectFailsWhenHttpConnectorFails) {
  auto peer_connection = webrtc::make_ref_counted<MockPeerConnection>();
  EXPECT_CALL(*peer_connection, SetLocalDescription(_))
      .WillOnce([&](rtc::scoped_refptr<
                    webrtc::SetLocalDescriptionObserverInterface>
                        observer) {
        observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
      });
  auto answer_description =
      std::make_unique<webrtc::MockSessionDescriptionInterface>();
  EXPECT_CALL(*answer_description, ToString(_)).WillOnce([](std::string* str) {
    *str = kWebRtcOffer;
    return true;
  });
  EXPECT_CALL(*peer_connection, local_description())
      .WillOnce(Return(answer_description.get()));
  auto http_connector = std::make_unique<MockHttpConnector>();
  EXPECT_CALL(*http_connector, ConnectActiveConference(_, _, _, _))
      .WillOnce(Return(absl::InternalError("http-connector-error")));
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::move(http_connector));
  conference_peer_connection.SetPeerConnection(std::move(peer_connection));

  absl::Status connect_status = conference_peer_connection.Connect(
      "join-endpoint", "conference-id", "access-token");

  EXPECT_THAT(connect_status, StatusIs(absl::StatusCode::kInternal,
                                       HasSubstr("http-connector-error")));
}

TEST(ConferencePeerConnectionTest,
     ConnectFailsWhenSettingRemoteDescriptionFails) {
  auto peer_connection = webrtc::make_ref_counted<MockPeerConnection>();
  EXPECT_CALL(*peer_connection, SetLocalDescription(_))
      .WillOnce([&](rtc::scoped_refptr<
                    webrtc::SetLocalDescriptionObserverInterface>
                        observer) {
        observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
      });
  auto answer_description =
      std::make_unique<webrtc::MockSessionDescriptionInterface>();
  EXPECT_CALL(*answer_description, ToString(_)).WillOnce([](std::string* str) {
    *str = kWebRtcOffer;
    return true;
  });
  EXPECT_CALL(*peer_connection, local_description())
      .WillOnce(Return(answer_description.get()));
  auto http_connector = std::make_unique<MockHttpConnector>();
  EXPECT_CALL(*http_connector, ConnectActiveConference(_, _, _, _))
      .WillOnce(Return(kWebRtcAnswer));
  EXPECT_CALL(*peer_connection, SetRemoteDescription(_, _))
      .WillOnce(
          [&](std::unique_ptr<webrtc::SessionDescriptionInterface> description,
              rtc::scoped_refptr<
                  webrtc::SetRemoteDescriptionObserverInterface>
                  observer) {
            observer->OnSetRemoteDescriptionComplete(
                webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                 "remote-description-error"));
          });
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::move(http_connector));
  conference_peer_connection.SetPeerConnection(std::move(peer_connection));

  absl::Status connect_status = conference_peer_connection.Connect(
      "join-endpoint", "conference-id", "access-token");

  EXPECT_THAT(connect_status, StatusIs(absl::StatusCode::kInternal,
                                       HasSubstr("remote-description-error")));
}

TEST(ConferencePeerConnectionTest,
     ClosesPeerConnectionWhenConferencePeerConnectionIsClosed) {
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());
  auto peer_connection = webrtc::make_ref_counted<MockPeerConnection>();
  absl::Notification notification;
  EXPECT_CALL(*peer_connection, Close())
      .Times(2)
      .WillRepeatedly([&notification]() {
        // The peer connection will be closed again once the conference peer
        // connection is destroyed. Therefore, only target the `Close` call that
        // happens when `ConferencePeerConnection::Close` is called.
        if (!notification.HasBeenNotified()) {
          notification.Notify();
        }
      });
  conference_peer_connection.SetPeerConnection(std::move(peer_connection));

  conference_peer_connection.Close();

  EXPECT_TRUE(notification.HasBeenNotified());
}

TEST(ConferencePeerConnectionTest,
     ClosesPeerConnectionWhenConferencePeerConnectionIsDestroyed) {
  absl::Notification notification;
  // Use a unique pointer to explicitly invoke the destructor.
  auto conference_peer_connection = std::make_unique<ConferencePeerConnection>(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());
  auto peer_connection = webrtc::make_ref_counted<MockPeerConnection>();
  EXPECT_CALL(*peer_connection, Close()).WillOnce([&notification]() {
    notification.Notify();
  });
  conference_peer_connection->SetPeerConnection(std::move(peer_connection));

  conference_peer_connection = nullptr;

  EXPECT_TRUE(notification.HasBeenNotified());
}

TEST(ConferencePeerConnectionTest, LogsWarningWhenClosedWithoutPeerConnection) {
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([&message](int, const std::string&, const std::string& msg) {
        message = msg;
      });
  log.StartCapturingLogs();

  conference_peer_connection.Close();

  EXPECT_EQ(message,
            "ConferencePeerConnection::Close called with a null peer "
            "connection.");
}

TEST(ConferencePeerConnectionTest,
     CallsDisconnectedCallbackWhenPeerConnectionIsClosed) {
  MockFunction<void(absl::Status)> mock_function;
  absl::Status status;
  EXPECT_CALL(mock_function, Call).WillOnce([&status](absl::Status s) {
    status = s;
  });
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());
  conference_peer_connection.SetDisconnectCallback(
      mock_function.AsStdFunction());

  conference_peer_connection.OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState::kClosed);

  EXPECT_THAT(status, StatusIs(absl::StatusCode::kInternal,
                               HasSubstr("Peer connection closed.")));
}

TEST(ConferencePeerConnectionTest,
     DoesNothingWhenPeerConnectionChangesToNonClosedState) {
  MockFunction<void(absl::Status)> mock_function;
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());
  conference_peer_connection.SetDisconnectCallback(
      mock_function.AsStdFunction());
  absl::Notification notification;
  ON_CALL(mock_function, Call).WillByDefault([&notification](absl::Status) {
    notification.Notify();
  });

  conference_peer_connection.OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState::kConnected);

  EXPECT_FALSE(notification.HasBeenNotified());
}

TEST(ConferencePeerConnectionTest,
     LogsWarningWhenPeerConnectionIsClosedWithoutCallback) {
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([&message](int, const std::string&, const std::string& msg) {
        message = msg;
      });
  log.StartCapturingLogs();

  conference_peer_connection.OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState::kClosed);

  EXPECT_EQ(message, "PeerConnection closed without disconnect callback.");
}

TEST(ConferencePeerConnectionTest,
     CallsTrackSignaledCallbackWhenTrackIsSignaled) {
  MockFunction<void(rtc::scoped_refptr<webrtc::RtpTransceiverInterface>)>
      mock_function;
  absl::Notification notification;
  EXPECT_CALL(mock_function, Call(_)).WillOnce([&notification](auto) {
    notification.Notify();
  });
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());
  conference_peer_connection.SetTrackSignaledCallback(
      mock_function.AsStdFunction());

  conference_peer_connection.OnTrack(
      webrtc::make_ref_counted<webrtc::MockRtpTransceiver>());

  EXPECT_TRUE(notification.HasBeenNotified());
}

TEST(ConferencePeerConnectionTest,
     LogsWarningWhenTrackIsSignaledWithoutCallback) {
  ConferencePeerConnection conference_peer_connection(
      CreateSignalingThread(), std::make_unique<MockHttpConnector>());
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([&message](int, const std::string&, const std::string& msg) {
        message = msg;
      });
  log.StartCapturingLogs();

  conference_peer_connection.OnTrack(
      webrtc::make_ref_counted<webrtc::MockRtpTransceiver>());

  EXPECT_EQ(message,
            "ConferencePeerConnection::OnTrack called without callback.");
}

}  // namespace
}  // namespace meet
