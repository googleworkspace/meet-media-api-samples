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

#ifndef CPP_INTERNAL_CONFERENCE_PEER_CONNECTION_H_
#define CPP_INTERNAL_CONFERENCE_PEER_CONNECTION_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "cpp/internal/conference_media_tracks.h"
#include "cpp/internal/conference_peer_connection_interface.h"
#include "cpp/internal/http_connector_interface.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/media_stream_interface.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtp_receiver_interface.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/stats/rtc_stats_collector_callback.h"

namespace meet {

// A wrapper around a `webrtc::PeerConnectionInterface` that provides a
// simplified interface for connecting to a conference.
//
// This class closes the underlying peer connection when it is destroyed if it
// is not already closed. Note that closing the peer connection also closes all
// data channels and media tracks.
class ConferencePeerConnection : public ConferencePeerConnectionInterface,
                                 public webrtc::PeerConnectionObserver {
 public:
  ConferencePeerConnection(
      std::unique_ptr<rtc::Thread> signaling_thread,
      std::unique_ptr<HttpConnectorInterface> http_connector)
      : signaling_thread_(std::move(signaling_thread)),
        http_connector_(std::move(http_connector)) {};

  ~ConferencePeerConnection() override {
    VLOG(1) << "ConferencePeerConnection::~ConferencePeerConnection called.";

    Close();
  }

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {
    VLOG(1) << "OnSignalingChange: " << new_state;
  };

  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> /* stream */)
      override {
    VLOG(1) << "OnAddStream called.";
  }

  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> /* stream */)
      override {
    VLOG(1) << "OnRemoveStream called.";
  }

  void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface>
                         data_channel) override {
    LOG(ERROR) << "OnDataChannel opened from server: " << data_channel->label();
    // The Meet servers should never open a data channel; all data channels are
    // opened by the client.
    data_channel->Close();
  };

  void OnRenegotiationNeeded() override {
    VLOG(1) << "OnRenegotiationNeeded called.";
  }

  void OnNegotiationNeededEvent(uint32_t /* event_id */) override {
    VLOG(1) << "OnNegotiationNeededEvent called.";
  }

  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState /* new_state */)
      override {
    VLOG(1) << "OnIceConnectionChange called.";
  }

  void OnStandardizedIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState /* new_state */)
      override {
    VLOG(1) << "OnStandardizedIceConnectionChange called.";
  }

  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
    VLOG(1) << "OnIceGatheringChange: " << new_state;
  };

  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
    VLOG(1) << "OnIceCandidate: " << candidate->sdp_mline_index();
  };

  void OnIceCandidateError(const std::string& /* address */, int /* port */,
                           const std::string& /* url */, int /* error_code */,
                           const std::string& /* error_text */) override {
    VLOG(1) << "OnIceCandidateError called.";
  }

  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) override {
    VLOG(1) << "OnIceCandidatesRemoved: " << candidates.size();
  }

  void OnIceConnectionReceivingChange(bool /* receiving */) override {
    VLOG(1) << "OnIceConnectionReceivingChange called.";
  }

  void OnIceSelectedCandidatePairChanged(
      const cricket::CandidatePairChangeEvent& /* event */) override {
    VLOG(1) << "OnIceSelectedCandidatePairChanged called.";
  }

  void OnAddTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> /* receiver */,
      const std::vector<
          rtc::scoped_refptr<webrtc::MediaStreamInterface>>& /* streams */)
      override {
    VLOG(1) << "OnAddTrack called.";
  }

  void OnRemoveTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override {
    VLOG(1) << "OnRemoveTrack called.";
  };

  void OnInterestingUsage(int /* usage_pattern */) override {
    VLOG(1) << "OnInterestingUsage called.";
  }

  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;

  void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface>
                   transceiver) override;

  // Sets the disconnect callback for the conference peer connection. Conference
  // peer connections can only have one disconnect callback at a time, and the
  // disconnect callback must outlive the conference peer connection if one is
  // set.
  //
  // Calling this is not thread-safe, so it should only be called before the
  // conference peer connection is used.
  void SetDisconnectCallback(DisconnectCallback disconnect_callback) override {
    disconnect_callback_ = std::move(disconnect_callback);
  }

  // Sets the track signaled callback for the conference peer connection.
  // Conference peer connections can only have one track signaled callback at a
  // time, and the track signaled callback must outlive the conference peer
  // connection if one is set.
  //
  // Tracks will be signaled during the `Connect` call, before it returns.
  //
  // Calling this is not thread-safe, so it should only be called before the
  // conference peer connection is used.
  void SetTrackSignaledCallback(
      TrackSignaledCallback track_signaled_callback) override {
    track_signaled_callback_ = std::move(track_signaled_callback);
  }

  // Connects to the conference with the given arguments and blocks until the
  // peer connection connects or fails to connect.
  //
  // Note that `disconnected_callback_` will not be called if this method
  // returns an error; `disconnected_callback_` will only be called if the
  // connection is disconnected after this method returns OK.
  absl::Status Connect(absl::string_view join_endpoint,
                       absl::string_view conference_id,
                       absl::string_view access_token) override;

  void Close() override {
    VLOG(1) << "ConferencePeerConnection::Close called.";

    if (peer_connection_ == nullptr) {
      LOG(WARNING) << "ConferencePeerConnection::Close called with a null peer "
                      "connection.";
      return;
    }
    peer_connection_->Close();
  }

  void GetStats(webrtc::RTCStatsCollectorCallback* callback) override {
    VLOG(1) << "ConferencePeerConnection::GetStats called.";

    if (peer_connection_ == nullptr) {
      LOG(WARNING) << "ConferencePeerConnection::GetStats called with a null "
                      "peer connection.";
      return;
    }
    peer_connection_->GetStats(callback);
  }

  // Sets the underlying peer connection that this class wraps.
  //
  // Calling this is not thread-safe, so it should only be called before the
  // conference peer connection is used.
  void SetPeerConnection(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
    peer_connection_ = std::move(peer_connection);
  }

 private:
  // Media tracks created while the peer connection's remote description is
  // being set. This will be null after `Connect` returns.
  std::vector<ConferenceMediaTrack> media_tracks_;

  DisconnectCallback disconnect_callback_;
  TrackSignaledCallback track_signaled_callback_;
  std::unique_ptr<rtc::Thread> signaling_thread_;
  std::unique_ptr<HttpConnectorInterface> http_connector_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
};

}  // namespace meet

#endif  // CPP_INTERNAL_CONFERENCE_PEER_CONNECTION_H_
