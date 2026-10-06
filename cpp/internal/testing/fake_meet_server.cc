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

#include "meet_clients/internal/testing/fake_meet_server.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "third_party/icu/source/tools/toolutil/json-json.hpp"
#include "meet_clients/internal/media_api_client_factory.h"
#include "api/audio/audio_device.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/data_channel_interface.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials_view.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/set_local_description_observer_interface.h"
#include "api/set_remote_description_observer_interface.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "rtc_base/buffer.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/thread.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {
namespace {

constexpr absl::Duration kDefaultTimeout = absl::Seconds(10);
constexpr absl::string_view kSessionStatusUpdate =
    R"({"resources":[{"sessionStatus":{"connectionState":"$0"}}]})";

class TestFieldTrials : public webrtc::FieldTrialsView {
 public:
  std::string Lookup(absl::string_view key) const override { return ""; }
  bool IsTest() const override { return true; }
  std::unique_ptr<webrtc::FieldTrialsView> CreateCopy() const override {
    return std::make_unique<TestFieldTrials>();
  }
};

// Capturer that generates a continuous sine wave at 48 kHz sample rate.
class SineWaveCapturer : public webrtc::TestAudioDeviceModule::Capturer {
 public:
  int SamplingFrequency() const override { return kTestAudioSampleRateHz; }

  int NumChannels() const override { return kTestAudioChannels; }

  bool Capture(webrtc::BufferT<int16_t>* buffer) override {
    const size_t num_samples =
        webrtc::TestAudioDeviceModule::SamplesPerFrame(kTestAudioSampleRateHz);
    buffer->SetSize(num_samples);
    const double angle_delta =
        2.0 * M_PI * kTestAudioSineFrequencyHz / kTestAudioSampleRateHz;
    for (int16_t& sample : *buffer) {
      sample = static_cast<int16_t>(kTestAudioSineAmplitude *
                                    std::sin(angle_delta * sample_index_++));
    }
    return true;
  }

 private:
  int64_t sample_index_ = 0;
};

class SetLocalDescriptionObserver
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override {
    status_ =
        error.ok() ? absl::OkStatus() : absl::InternalError(error.message());
    notification_.Notify();
  }

  absl::Status Wait(absl::Duration timeout) {
    if (!notification_.WaitForNotificationWithTimeout(timeout)) {
      return absl::DeadlineExceededError(
          "Timed out waiting for SetLocalDescription.");
    }
    return status_;
  }

 private:
  absl::Notification notification_;
  absl::Status status_ = absl::InternalError("Not completed.");
};

class SetRemoteDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    status_ =
        error.ok() ? absl::OkStatus() : absl::InternalError(error.message());
    notification_.Notify();
  }

  absl::Status Wait(absl::Duration timeout) {
    if (!notification_.WaitForNotificationWithTimeout(timeout)) {
      return absl::DeadlineExceededError(
          "Timed out waiting for SetRemoteDescription.");
    }
    return status_;
  }

 private:
  absl::Notification notification_;
  absl::Status status_ = absl::InternalError("Not completed.");
};

}  // namespace

FakeMeetServer::FakeMeetServer()
    : network_thread_(webrtc::Thread::CreateWithSocketServer()),
      signaling_thread_(webrtc::Thread::Create()) {
  CHECK(network_thread_->Start());
  CHECK(signaling_thread_->Start());

  webrtc::scoped_refptr<webrtc::AudioDeviceModule> adm =
      webrtc::TestAudioDeviceModule::Create(
          webrtc::CreateEnvironment(std::make_unique<TestFieldTrials>()),
          std::make_unique<SineWaveCapturer>(),
          webrtc::TestAudioDeviceModule::CreateDiscardRenderer(
              kTestAudioSampleRateHz));
  factory_ = webrtc::CreatePeerConnectionFactory(
      network_thread_.get(), signaling_thread_.get(), adm,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      /*video_encoder_factory=*/nullptr,
      /*video_decoder_factory=*/nullptr,
      /*audio_mixer=*/nullptr,
      /*audio_processing=*/nullptr,
      /*audio_frame_processor=*/nullptr, std::make_unique<TestFieldTrials>());
  CHECK(factory_ != nullptr);

  webrtc::PeerConnectionFactoryInterface::Options options;
  options.network_ignore_mask &= ~webrtc::ADAPTER_TYPE_LOOPBACK;
  factory_->SetOptions(options);

  webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::PeerConnectionInterface>>
      pc_or_error = factory_->CreatePeerConnectionOrError(
          webrtc::PeerConnectionInterface::RTCConfiguration(),
          webrtc::PeerConnectionDependencies(this));
  CHECK(pc_or_error.ok()) << pc_or_error.error().message();
  peer_connection_ = pc_or_error.MoveValue();

  // WebRTC's CreateDefaultAudioOptions() enables echo_cancellation,
  // auto_gain_control, noise_suppression, and highpass_filter by default.
  // Disable them so the audio processor does not treat the stationary test
  // sine wave as background noise and attenuate it.
  webrtc::AudioOptions audio_options;
  audio_options.echo_cancellation = false;
  audio_options.auto_gain_control = false;
  audio_options.noise_suppression = false;
  audio_options.highpass_filter = false;
  audio_source_ = factory_->CreateAudioSource(audio_options);
  audio_track_ =
      factory_->CreateAudioTrack("server_audio", audio_source_.get());
}

FakeMeetServer::~FakeMeetServer() {
  signaling_thread_->BlockingCall([this]() {
    if (session_control_channel_ != nullptr) {
      session_control_channel_->UnregisterObserver();
      session_control_channel_ = nullptr;
    }
    peer_connection_->Close();
  });
}

absl::StatusOr<webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
FakeMeetServer::GetAudioTransceiver() {
  for (const auto& transceiver : peer_connection_->GetTransceivers()) {
    if (transceiver->media_type() == webrtc::MediaType::AUDIO) {
      return transceiver;
    }
  }
  return absl::InternalError("No audio transceiver found on server.");
}

absl::StatusOr<std::string> FakeMeetServer::HandleOfferAndCreateAnswer(
    absl::string_view offer_sdp) {
  webrtc::SdpParseError parse_error;
  std::unique_ptr<webrtc::SessionDescriptionInterface> offer =
      webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, offer_sdp,
                                       &parse_error);
  if (offer == nullptr) {
    return absl::InternalError(absl::StrCat("Failed to parse client offer: ",
                                            parse_error.description));
  }

  auto set_remote_observer =
      webrtc::make_ref_counted<SetRemoteDescriptionObserver>();
  peer_connection_->SetRemoteDescription(std::move(offer), set_remote_observer);
  if (absl::Status status = set_remote_observer->Wait(kDefaultTimeout);
      !status.ok()) {
    return status;
  }

  // Configure the first audio transceiver to send audio.
  if (absl::Status status =
          signaling_thread_->BlockingCall([this]() -> absl::Status {
            absl::StatusOr<
                webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
                audio_transceiver = GetAudioTransceiver();
            if (!audio_transceiver.ok()) {
              LOG(INFO) << "Skipping audio transceiver setup: "
                        << audio_transceiver.status();
              return absl::OkStatus();
            }
            if (!(*audio_transceiver)->sender()->SetTrack(audio_track_.get())) {
              return absl::InternalError(
                  "Failed to set audio track on sender.");
            }
            webrtc::RTCError direction_error =
                (*audio_transceiver)
                    ->SetDirectionWithError(
                        webrtc::RtpTransceiverDirection::kSendOnly);
            if (!direction_error.ok()) {
              return absl::InternalError(
                  absl::StrCat("Failed to set transceiver direction: ",
                               direction_error.message()));
            }
            return absl::OkStatus();
          });
      !status.ok()) {
    return status;
  }

  auto set_local_observer =
      webrtc::make_ref_counted<SetLocalDescriptionObserver>();
  peer_connection_->SetLocalDescription(set_local_observer);
  if (absl::Status status = set_local_observer->Wait(kDefaultTimeout);
      !status.ok()) {
    return status;
  }

  // Set CSRC on the active audio sender after SetLocalDescription has
  // created the underlying AudioSendStream.
  if (absl::Status status =
          signaling_thread_->BlockingCall([this]() -> absl::Status {
            absl::StatusOr<
                webrtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
                audio_transceiver = GetAudioTransceiver();
            if (!audio_transceiver.ok()) {
              LOG(INFO) << "Skipping audio sender setup: "
                        << audio_transceiver.status();
              return absl::OkStatus();
            }
            webrtc::RtpParameters params =
                (*audio_transceiver)->sender()->GetParameters();
            if (params.encodings.empty()) {
              return absl::InternalError("Audio sender has no RTP encodings.");
            }
            params.encodings[0].csrcs = std::vector<uint32_t>{kTestAudioCsrc};
            webrtc::RTCError err =
                (*audio_transceiver)->sender()->SetParameters(params);
            if (!err.ok()) {
              return absl::InternalError(
                  absl::StrCat("Failed to set CSRC: ", err.message()));
            }
            return absl::OkStatus();
          });
      !status.ok()) {
    return status;
  }

  if (!ice_gathering_complete_.WaitForNotificationWithTimeout(
          kDefaultTimeout)) {
    return absl::DeadlineExceededError(
        "Timed out waiting for server ICE gathering to complete.");
  }

  return signaling_thread_->BlockingCall([this]() {
    std::string answer_sdp;
    peer_connection_->local_description()->ToString(&answer_sdp);
    return answer_sdp;
  });
}

void FakeMeetServer::SendDisconnectedSessionStatus() {
  signaling_thread_->BlockingCall([this]() {
    if (session_control_channel_ != nullptr &&
        session_control_channel_->state() ==
            webrtc::DataChannelInterface::kOpen) {
      session_control_channel_->Send(webrtc::DataBuffer(
          absl::Substitute(kSessionStatusUpdate, "STATE_DISCONNECTED")));
    }
  });
}

void FakeMeetServer::OnDataChannel(
    webrtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  if (data_channel->label() == "session-control") {
    session_control_channel_ = data_channel;
    session_control_channel_->RegisterObserver(this);
    MaybeSendJoinedSessionStatus();
  }
}

void FakeMeetServer::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  if (new_state == webrtc::PeerConnectionInterface::kIceGatheringComplete) {
    if (!ice_gathering_complete_.HasBeenNotified()) {
      ice_gathering_complete_.Notify();
    }
  }
}

void FakeMeetServer::OnStateChange() { MaybeSendJoinedSessionStatus(); }

void FakeMeetServer::OnMessage(const webrtc::DataBuffer& buffer) {
  absl::string_view message(buffer.data.cdata<char>(), buffer.data.size());
  const nlohmann::json parsed =
      nlohmann::json::parse(message, /*cb=*/nullptr,
                            /*allow_exceptions=*/false);
  if (parsed.is_object() && parsed.contains("request") &&
      parsed["request"].is_object() && parsed["request"].contains("leave")) {
    SendDisconnectedSessionStatus();
  } else {
    LOG(WARNING) << "FakeMeetServer received unexpected session-control "
                    "message: "
                 << message;
  }
}

void FakeMeetServer::MaybeSendJoinedSessionStatus() {
  if (!joined_sent_ && session_control_channel_ != nullptr &&
      session_control_channel_->state() ==
          webrtc::DataChannelInterface::kOpen) {
    joined_sent_ = true;
    session_control_channel_->Send(webrtc::DataBuffer(
        absl::Substitute(kSessionStatusUpdate, "STATE_JOINED")));
  }
}

MediaApiClientFactory CreateLoopbackMediaApiClientFactory(
    FakeMeetServer& server) {
  return MediaApiClientFactory(
      [](webrtc::Thread* signaling_thread) {
        webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
            MediaApiClientFactory::CreateDefaultPeerConnectionFactory(
                signaling_thread, std::make_unique<TestFieldTrials>());
        webrtc::PeerConnectionFactoryInterface::Options options;
        options.network_ignore_mask &= ~webrtc::ADAPTER_TYPE_LOOPBACK;
        factory->SetOptions(options);
        return factory;
      },
      [&server]() { return std::make_unique<FakeHttpConnector>(server); });
}

}  // namespace meet
