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
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/internal/conference_data_channel.h"
#include "cpp/internal/conference_peer_connection.h"
#include "cpp/internal/curl_connector.h"
#include "cpp/internal/curl_request.h"
#include "cpp/internal/http_connector_interface.h"
#include "cpp/internal/media_api_audio_device_module.h"
#include "cpp/internal/media_api_client.h"
#include "cpp/internal/media_entries_resource_handler.h"
#include "cpp/internal/media_stats_resource_handler.h"
#include "cpp/internal/participants_resource_handler.h"
#include "cpp/internal/session_control_resource_handler.h"
#include "cpp/internal/video_assignment_resource_handler.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/api/audio_codecs/opus_audio_decoder_factory.h"
#include "webrtc/api/create_peerconnection_factory.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/media_types.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/rtp_transceiver_direction.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "webrtc/api/video_codecs/video_encoder_factory_template.h"
#include "webrtc/api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {
namespace {

// The number of audio streams to create if audio streams should be enabled.
//
// There may be exactly three audio streams or none at all.
constexpr int kReceivingAudioStreamCount = 3;
// The maximum number of video streams that may be created.
//
// There may be 0, 1, 2, or 3 video streams.
constexpr int kMaxReceivingVideoStreamCount = 3;

webrtc::PeerConnectionInterface::RTCConfiguration GetRtcConfiguration() {
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

  config.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyBalanced;
  config.rtcp_mux_policy =
      webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
  webrtc::PeerConnectionInterface::IceServer stun_server;
  stun_server.urls.push_back("stun:stun.l.google.com:19302");
  config.servers.push_back(stun_server);
  return config;
}

absl::Status ConfigureTransceivers(
    webrtc::PeerConnectionInterface& peer_connection, bool enable_audio_streams,
    int receiving_video_stream_count) {
  if (enable_audio_streams) {
    for (int i = 0; i < kReceivingAudioStreamCount; i++) {
      webrtc::RtpTransceiverInit audio_init;
      audio_init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
      audio_init.stream_ids = {absl::StrCat("audio_stream_", i)};

      // The transceiver objects are not used here, but they do need to be added
      // so the proper media descriptions will be included in the SDP offer. The
      // receiver tracks of the transceivers will be exposed in the `OnTrack`
      // callback of the `webrtc::PeerConnectionObserver` when connecting
      // starts.
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
          audio_result = peer_connection.AddTransceiver(
              cricket::MediaType::MEDIA_TYPE_AUDIO, audio_init);

      if (!audio_result.ok()) {
        return absl::InternalError(
            absl::StrCat("Failed to add audio transceiver: ",
                         audio_result.error().message()));
      }
    }
  }

  for (int i = 0; i < receiving_video_stream_count; i++) {
    webrtc::RtpTransceiverInit video_init;
    video_init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
    video_init.stream_ids = {absl::StrCat("video_stream_", i)};

    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
        video_result = peer_connection.AddTransceiver(
            cricket::MediaType::MEDIA_TYPE_VIDEO, video_init);

    if (!video_result.ok()) {
      return absl::InternalError(absl::StrCat(
          "Failed to add video transceiver: ", video_result.error().message()));
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<MediaApiClient::ConferenceDataChannels> CreateDataChannels(
    webrtc::PeerConnectionInterface& peer_connection) {
  const webrtc::DataChannelInit kDataChannelConfig = {.reliable = true,
                                                      .ordered = true};

  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
      media_entries_data_channel_status =
          peer_connection.CreateDataChannelOrError("media-entries",
                                                   &kDataChannelConfig);
  if (!media_entries_data_channel_status.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to create media entries data channel: ",
                     media_entries_data_channel_status.error().message()));
  }

  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
      media_stats_data_channel_status =
          peer_connection.CreateDataChannelOrError("media-stats",
                                                   &kDataChannelConfig);
  if (!media_stats_data_channel_status.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to create media stats data channel: ",
                     media_stats_data_channel_status.error().message()));
  }

  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
      participants_data_channel_status =
          peer_connection.CreateDataChannelOrError("participants",
                                                   &kDataChannelConfig);
  if (!participants_data_channel_status.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to create participants data channel: ",
                     participants_data_channel_status.error().message()));
  }

  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
      session_control_data_channel_status =
          peer_connection.CreateDataChannelOrError("session-control",
                                                   &kDataChannelConfig);
  if (!session_control_data_channel_status.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to create session control data channel: ",
                     session_control_data_channel_status.error().message()));
  }

  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::DataChannelInterface>>
      video_assignment_data_channel_status =
          peer_connection.CreateDataChannelOrError("video-assignment",
                                                   &kDataChannelConfig);
  if (!video_assignment_data_channel_status.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to create video assignment data channel: ",
                     video_assignment_data_channel_status.error().message()));
  }

  return MediaApiClient::ConferenceDataChannels{
      .media_entries = std::make_unique<ConferenceDataChannel>(
          std::make_unique<MediaEntriesResourceHandler>(),
          std::move(media_entries_data_channel_status).value()),
      .media_stats = std::make_unique<ConferenceDataChannel>(
          std::make_unique<MediaStatsResourceHandler>(),
          std::move(media_stats_data_channel_status).value()),
      .participants = std::make_unique<ConferenceDataChannel>(
          std::make_unique<ParticipantsResourceHandler>(),
          std::move(participants_data_channel_status).value()),
      .session_control = std::make_unique<ConferenceDataChannel>(
          std::make_unique<SessionControlResourceHandler>(),
          std::move(session_control_data_channel_status).value()),
      .video_assignment = std::make_unique<ConferenceDataChannel>(
          std::make_unique<VideoAssignmentResourceHandler>(),
          std::move(video_assignment_data_channel_status).value())};
}

}  // namespace

MediaApiClientFactory::MediaApiClientFactory() {
  peer_connection_factory_provider_ = [](rtc::Thread* signaling_thread,
                                         rtc::Thread* worker_thread)
      -> rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> {
    return webrtc::CreatePeerConnectionFactory(
        /*network_thread=*/nullptr, worker_thread, signaling_thread,
        webrtc::make_ref_counted<MediaApiAudioDeviceModule>(*worker_thread),
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateOpusAudioDecoderFactory(),
        std::make_unique<webrtc::VideoEncoderFactoryTemplate<
            webrtc::LibvpxVp9EncoderTemplateAdapter>>(),
        std::make_unique<webrtc::VideoDecoderFactoryTemplate<
            webrtc::LibvpxVp8DecoderTemplateAdapter,
            webrtc::LibvpxVp9DecoderTemplateAdapter,
            webrtc::Dav1dDecoderTemplateAdapter>>(),
        /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr);
  };
  http_connector_provider_ = []() {
    return std::make_unique<CurlConnector>(std::make_unique<CurlApiWrapper>());
  };
}

absl::StatusOr<std::unique_ptr<MediaApiClientInterface>>
MediaApiClientFactory::CreateMediaApiClient(
    const MediaApiClientConfiguration& api_config,
    rtc::scoped_refptr<MediaApiClientObserverInterface> observer) {
  if (api_config.receiving_video_stream_count > kMaxReceivingVideoStreamCount) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Receiving video stream count must be less than or equal to ",
        kMaxReceivingVideoStreamCount, "; got ",
        api_config.receiving_video_stream_count));
  }
  std::unique_ptr<rtc::Thread> client_thread = rtc::Thread::Create();
  client_thread->SetName("media_api_client_internal_thread", nullptr);
  if (!client_thread->Start()) {
    return absl::InternalError("Failed to start client thread");
  }
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("media_api_client_signaling_thread", nullptr);
  if (!signaling_thread->Start()) {
    return absl::InternalError("Failed to start signaling thread");
  }
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->SetName("media_api_client_worker_thread", nullptr);
  if (!worker_thread->Start()) {
    return absl::InternalError("Failed to start worker thread");
  }

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory = peer_connection_factory_provider_(
          signaling_thread.get(), worker_thread.get());

  std::unique_ptr<HttpConnectorInterface> curl_connector =
      http_connector_provider_();
  auto conference_peer_connection = std::make_unique<ConferencePeerConnection>(
      std::move(signaling_thread), std::move(curl_connector));
  auto peer_connection_status =
      peer_connection_factory->CreatePeerConnectionOrError(
          GetRtcConfiguration(),
          webrtc::PeerConnectionDependencies(conference_peer_connection.get()));
  if (!peer_connection_status.ok()) {
    return absl::InternalError(
        absl::StrCat("Failed to create peer connection: ",
                     peer_connection_status.error().message()));
  }

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection =
      std::move(peer_connection_status).value();

  absl::Status configure_transceivers_status =
      ConfigureTransceivers(*peer_connection, api_config.enable_audio_streams,
                            api_config.receiving_video_stream_count);
  if (!configure_transceivers_status.ok()) {
    return configure_transceivers_status;
  }

  absl::StatusOr<MediaApiClient::ConferenceDataChannels>
      conference_data_channels = CreateDataChannels(*peer_connection);
  if (!conference_data_channels.ok()) {
    return conference_data_channels.status();
  }

  conference_peer_connection->SetPeerConnection(std::move(peer_connection));

  return std::make_unique<MediaApiClient>(
      std::move(client_thread), std::move(worker_thread), std::move(observer),
      std::move(conference_peer_connection),
      std::move(conference_data_channels).value());
}

}  // namespace meet
