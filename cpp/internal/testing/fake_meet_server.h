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

#ifndef CPP_INTERNAL_TESTING_FAKE_MEET_SERVER_H_
#define CPP_INTERNAL_TESTING_FAKE_MEET_SERVER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "meet_clients/internal/http_connector_interface.h"
#include "meet_clients/internal/media_api_client_factory.h"
#include "api/data_channel_interface.h"
#include "api/jsep.h"
#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/thread.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {

inline constexpr int kTestAudioSampleRateHz = 48000;
inline constexpr size_t kTestAudioChannels = 1;
inline constexpr int16_t kTestAudioSineAmplitude = 16000;
// This is selected so that the phase changes by pi/2 for each 10 ms frame,
// letting us detect missing frames as discontinuities.
inline constexpr int16_t kTestAudioSineFrequencyHz = 1025;
inline constexpr uint32_t kTestAudioCsrc = 12345;

// In-process fake Meet media server backed by a real WebRTC PeerConnection.
class FakeMeetServer : public webrtc::PeerConnectionObserver,
                       public webrtc::DataChannelObserver {
 public:
  FakeMeetServer();
  ~FakeMeetServer() override;

  absl::StatusOr<std::string> HandleOfferAndCreateAnswer(
      absl::string_view offer_sdp);

  void SendDisconnectedSessionStatus();

  // webrtc::PeerConnectionObserver implementation.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState /*new_state*/) override {}
  void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface>
                         data_channel) override;
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState /*new_state*/)
      override {}
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(
      const webrtc::IceCandidateInterface* /*candidate*/) override {}

  // webrtc::DataChannelObserver implementation.
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;

 private:
  absl::StatusOr<webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
  GetAudioTransceiver();
  void MaybeSendJoinedSessionStatus();

  absl::Notification ice_gathering_complete_;
  bool joined_sent_ = false;
  absl_nullable webrtc::scoped_refptr<webrtc::DataChannelInterface>
      session_control_channel_;

  std::unique_ptr<webrtc::Thread> network_thread_;
  std::unique_ptr<webrtc::Thread> signaling_thread_;
  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;
  webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  webrtc::scoped_refptr<webrtc::AudioSourceInterface> audio_source_;
  webrtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
};

class FakeHttpConnector : public HttpConnectorInterface {
 public:
  explicit FakeHttpConnector(FakeMeetServer& server) : server_(server) {}

  absl::StatusOr<std::string> ConnectActiveConference(
      absl::string_view /*join_endpoint*/, absl::string_view /*conference_id*/,
      absl::string_view /*access_token*/, absl::string_view sdp_offer,
      std::optional<int> /*connection_timeout_ms*/,
      std::optional<int> /*request_timeout_ms*/,
      std::optional<int> /*confirmation_timeout_ms*/) override {
    return server_.HandleOfferAndCreateAnswer(sdp_offer);
  }

 private:
  FakeMeetServer& server_;
};

MediaApiClientFactory CreateLoopbackMediaApiClientFactory(
    FakeMeetServer& server);

}  // namespace meet

#endif  // CPP_INTERNAL_TESTING_FAKE_MEET_SERVER_H_
