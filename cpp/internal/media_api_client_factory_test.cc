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

#include "cpp/internal/media_api_client_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/internal/http_connector_interface.h"
#include "cpp/internal/testing/mock_media_api_client_observer.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/media_types.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/test/mock_data_channel.h"
#include "webrtc/api/test/mock_peer_connection_factory_interface.h"
#include "webrtc/api/test/mock_peerconnectioninterface.h"
#include "webrtc/api/test/mock_rtp_transceiver.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {
namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::status::StatusIs;

class MockHttpConnector : public HttpConnectorInterface {
 public:
  MOCK_METHOD(absl::StatusOr<std::string>, ConnectActiveConference,
              (absl::string_view join_endpoint, absl::string_view conference_id,
               absl::string_view access_token, absl::string_view sdp_offer),
              (override));
};

TEST(MediaApiClientFactoryTest,
     SuccessfullyCreatesMediaApiClientWithAudioAndVideoStreams) {
  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  rtc::scoped_refptr<webrtc::MockPeerConnectionInterface> peer_connection =
      webrtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError(_, _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>(
              peer_connection)));
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-entries", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-stats", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("participants", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("session-control", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("video-assignment", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  MediaApiClientFactory::PeerConnectionFactoryProvider
      peer_connection_factory_provider =
          [&](rtc::Thread* signaling_thread, rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return peer_connection_factory;
  };
  MediaApiClientFactory::HttpConnectorProvider http_connector_provider = []() {
    return std::make_unique<MockHttpConnector>();
  };
  MediaApiClientFactory factory(std::move(peer_connection_factory_provider),
                                std::move(http_connector_provider));
  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 3,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_TRUE(media_api_client_status.ok());
}

TEST(MediaApiClientFactoryTest, FailsIfReceivingVideoStreamCountIsTooHigh) {
  MediaApiClientFactory factory;

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 4,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_THAT(media_api_client_status,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "Receiving video stream count must be less than or "
                       "equal to 3; got 4"));
}

TEST(MediaApiClientFactoryTest, FailsIfPeerConnectionFactoryFailsToCreate) {
  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError(_, _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                        "test error")));
  MediaApiClientFactory::PeerConnectionFactoryProvider
      peer_connection_factory_provider =
          [&](rtc::Thread* signaling_thread, rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return peer_connection_factory;
  };
  MediaApiClientFactory::HttpConnectorProvider http_connector_provider = []() {
    return std::make_unique<MockHttpConnector>();
  };
  MediaApiClientFactory factory(std::move(peer_connection_factory_provider),
                                std::move(http_connector_provider));

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 3,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_THAT(media_api_client_status,
              StatusIs(absl::StatusCode::kInternal,
                       "Failed to create peer connection: test error"));
}

TEST(MediaApiClientFactoryTest, FailsIfAudioTransceiverFailsToBeCreated) {
  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  rtc::scoped_refptr<webrtc::MockPeerConnectionInterface> peer_connection =
      webrtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError(_, _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>(
              peer_connection)));
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, _))
      .WillOnce([](cricket::MediaType media_type,
                   const webrtc::RtpTransceiverInit& init) {
        return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                "test error");
      });
  MediaApiClientFactory::PeerConnectionFactoryProvider
      peer_connection_factory_provider =
          [&](rtc::Thread* signaling_thread, rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return peer_connection_factory;
  };
  MediaApiClientFactory::HttpConnectorProvider http_connector_provider = []() {
    return std::make_unique<MockHttpConnector>();
  };
  MediaApiClientFactory factory(std::move(peer_connection_factory_provider),
                                std::move(http_connector_provider));

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 3,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_THAT(media_api_client_status,
              StatusIs(absl::StatusCode::kInternal,
                       "Failed to add audio transceiver: test error"));
}

TEST(MediaApiClientFactoryTest, FailsIfVideoTransceiverFailsToBeCreated) {
  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  rtc::scoped_refptr<webrtc::MockPeerConnectionInterface> peer_connection =
      webrtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError(_, _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>(
              peer_connection)));
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, _))
      .WillOnce([](cricket::MediaType media_type,
                   const webrtc::RtpTransceiverInit& init) {
        return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                "test error");
      });
  ON_CALL(*peer_connection, CreateDataChannelOrError)
      .WillByDefault([](const std::string&, const webrtc::DataChannelInit*) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      });
  MediaApiClientFactory::PeerConnectionFactoryProvider
      peer_connection_factory_provider =
          [&](rtc::Thread* signaling_thread, rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return peer_connection_factory;
  };
  MediaApiClientFactory::HttpConnectorProvider http_connector_provider = []() {
    return std::make_unique<MockHttpConnector>();
  };
  MediaApiClientFactory factory(std::move(peer_connection_factory_provider),
                                std::move(http_connector_provider));

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 3,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_THAT(media_api_client_status,
              StatusIs(absl::StatusCode::kInternal,
                       "Failed to add video transceiver: test error"));
}

TEST(MediaApiClientFactoryTest,
     FailsIfMediaEntriesDataChannelFailsToBeCreated) {
  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  rtc::scoped_refptr<webrtc::MockPeerConnectionInterface> peer_connection =
      webrtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError(_, _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>(
              peer_connection)));
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-entries", _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                        "test error")));
  MediaApiClientFactory::PeerConnectionFactoryProvider
      peer_connection_factory_provider =
          [&](rtc::Thread* signaling_thread, rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return peer_connection_factory;
  };
  MediaApiClientFactory::HttpConnectorProvider http_connector_provider = []() {
    return std::make_unique<MockHttpConnector>();
  };
  MediaApiClientFactory factory(std::move(peer_connection_factory_provider),
                                std::move(http_connector_provider));

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 3,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_THAT(media_api_client_status,
              StatusIs(absl::StatusCode::kInternal,
                       "Failed to create media entries data channel: test "
                       "error"));
}

TEST(MediaApiClientFactoryTest, FailsIfMediaStatsDataChannelFailsToBeCreated) {
  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  rtc::scoped_refptr<webrtc::MockPeerConnectionInterface> peer_connection =
      webrtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError(_, _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>(
              peer_connection)));
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-entries", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-stats", _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                        "test error")));
  MediaApiClientFactory::PeerConnectionFactoryProvider
      peer_connection_factory_provider =
          [&](rtc::Thread* signaling_thread, rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return peer_connection_factory;
  };

  MediaApiClientFactory::HttpConnectorProvider http_connector_provider = []() {
    return std::make_unique<MockHttpConnector>();
  };
  MediaApiClientFactory factory(std::move(peer_connection_factory_provider),
                                std::move(http_connector_provider));

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 3,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_THAT(media_api_client_status,
              StatusIs(absl::StatusCode::kInternal,
                       "Failed to create media stats data channel: test "
                       "error"));
}

TEST(MediaApiClientFactoryTest,
     FailsIfParticipantsDataChannelFailsToBeCreated) {
  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  rtc::scoped_refptr<webrtc::MockPeerConnectionInterface> peer_connection =
      webrtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError(_, _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>(
              peer_connection)));
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-entries", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-stats", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("participants", _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                        "test error")));
  MediaApiClientFactory::PeerConnectionFactoryProvider
      peer_connection_factory_provider =
          [&](rtc::Thread* signaling_thread, rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return peer_connection_factory;
  };
  MediaApiClientFactory::HttpConnectorProvider http_connector_provider = []() {
    return std::make_unique<MockHttpConnector>();
  };
  MediaApiClientFactory factory(std::move(peer_connection_factory_provider),
                                std::move(http_connector_provider));

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 3,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_THAT(media_api_client_status,
              StatusIs(absl::StatusCode::kInternal,
                       "Failed to create participants data channel: test "
                       "error"));
}

TEST(MediaApiClientFactoryTest,
     FailsIfSessionControlDataChannelFailsToBeCreated) {
  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  rtc::scoped_refptr<webrtc::MockPeerConnectionInterface> peer_connection =
      webrtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError(_, _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>(
              peer_connection)));
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-entries", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-stats", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("participants", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("session-control", _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                        "test error")));
  MediaApiClientFactory::PeerConnectionFactoryProvider
      peer_connection_factory_provider =
          [&](rtc::Thread* signaling_thread, rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return peer_connection_factory;
  };
  MediaApiClientFactory::HttpConnectorProvider http_connector_provider = []() {
    return std::make_unique<MockHttpConnector>();
  };
  MediaApiClientFactory factory(std::move(peer_connection_factory_provider),
                                std::move(http_connector_provider));

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 3,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_THAT(media_api_client_status,
              StatusIs(absl::StatusCode::kInternal,
                       "Failed to create session control data channel: test "
                       "error"));
}

TEST(MediaApiClientFactoryTest,
     FailsIfVideoAssignmentDataChannelFailsToBeCreated) {
  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  rtc::scoped_refptr<webrtc::MockPeerConnectionInterface> peer_connection =
      webrtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError(_, _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>(
              peer_connection)));
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, _))
      .Times(3)
      .WillRepeatedly([](cricket::MediaType media_type,
                         const webrtc::RtpTransceiverInit& init) {
        return static_cast<
            rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      });
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-entries", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("media-stats", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("participants", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("session-control", _))
      .WillOnce(Return(
          static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
              webrtc::MockDataChannelInterface::Create())));
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError("video-assignment", _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR,
                                        "test error")));
  MediaApiClientFactory::PeerConnectionFactoryProvider
      peer_connection_factory_provider =
          [&](rtc::Thread* signaling_thread, rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return peer_connection_factory;
  };
  MediaApiClientFactory::HttpConnectorProvider http_connector_provider = []() {
    return std::make_unique<MockHttpConnector>();
  };
  MediaApiClientFactory factory(std::move(peer_connection_factory_provider),
                                std::move(http_connector_provider));

  absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
      media_api_client_status = factory.CreateMediaApiClient(
          MediaApiClientConfiguration{
              .receiving_video_stream_count = 3,
              .enable_audio_streams = true,
          },
          webrtc::make_ref_counted<MockMediaApiClientObserver>());

  EXPECT_THAT(media_api_client_status,
              StatusIs(absl::StatusCode::kInternal,
                       "Failed to create video assignment data channel: test "
                       "error"));
}

}  // namespace
}  // namespace meet
