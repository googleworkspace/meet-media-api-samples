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
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/api/media_stats_resource.h"
#include "cpp/api/session_control_resource.h"
#include "cpp/api/video_assignment_resource.h"
#include "cpp/internal/conference_media_tracks.h"
#include "cpp/internal/stats_request_from_report.h"
#include "cpp/internal/variant_utils.h"
#include "webrtc/api/create_peerconnection_factory.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/media_stream_interface.h"
#include "webrtc/api/media_types.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtp_receiver_interface.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/stats/rtc_stats_collector_callback.h"
#include "webrtc/api/stats/rtc_stats_report.h"
#include "webrtc/api/task_queue/pending_task_safety_flag.h"
#include "webrtc/api/units/time_delta.h"
#include "webrtc/api/video/video_source_interface.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {
namespace {

// Lambda-based implementation of RTCStatsCollectorCallback.
class OnRTCStatsCollected : public webrtc::RTCStatsCollectorCallback {
 public:
  using Callback = absl::AnyInvocable<void(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport> &report)>;

  explicit OnRTCStatsCollected(Callback callback)
      : callback_(std::move(callback)) {}

  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport> &report)
      override {
    callback_(report);
  }

 private:
  Callback callback_;
};

}  // namespace

absl::Status MediaApiClient::ConnectActiveConference(
    absl::string_view join_endpoint, absl::string_view conference_id,
    absl::string_view access_token) {
  {
    absl::MutexLock lock(&mutex_);
    if (state_ != State::kReady) {
      return absl::FailedPreconditionError(absl::StrCat(
          "ConnectActiveConference called in ", StateToString(state_),
          " state instead of ready state."));
    }
    state_ = State::kConnecting;
  }
  VLOG(1) << "Client switched to connecting state.";

  client_thread_->PostTask(SafeTask(alive_flag_, [&,
                                                  join_endpoint = std::string(
                                                      join_endpoint),
                                                  conference_id = std::string(
                                                      conference_id),
                                                  access_token = std::string(
                                                      access_token)]() {
    absl::Status connect_status = conference_peer_connection_->Connect(
        join_endpoint, conference_id, access_token);
    if (!connect_status.ok()) {
      MaybeDisconnect(connect_status);
      return;
    }

    {
      absl::MutexLock lock(&mutex_);
      if (state_ != State::kConnecting) {
        LOG(WARNING)
            << "Client in " << StateToString(state_)
            << " state instead of connecting state after starting connection.";
        return;
      }
      state_ = State::kJoining;
    }
    VLOG(1) << "Client switched to joining state.";
  }));

  return absl::OkStatus();
};

absl::Status MediaApiClient::LeaveConference(int64_t request_id) {
  State state;
  {
    absl::MutexLock lock(&mutex_);

    if (state_ == State::kDisconnected) {
      return absl::InternalError(
          "LeaveConference called in disconnected state.");
    }
    state = state_;
  }

  absl::Status status = data_channels_.session_control->SendRequest(
      SessionControlChannelFromClient{
          .request = SessionControlRequest{.request_id = request_id,
                                           .leave_request = LeaveRequest()}});
  if (state != State::kJoined) {
    MaybeDisconnect(absl::InternalError(absl::StrCat(
        "LeaveConference called when in ", StateToString(state),
        " state instead of joined state. Requests are not guaranteed to be "
        "delivered unless the client is joined into the conference. Therefore, "
        "the client was disconnected immediately.")));
  }

  return status;
}

absl::Status MediaApiClient::SendRequest(const ResourceRequest &request) {
  {
    absl::MutexLock lock(&mutex_);
    if (state_ != State::kJoined) {
      LOG(WARNING)
          << "SendRequest called while client is in " << StateToString(state_)
          << " state instead of joined state. Requests are not guaranteed to "
             "be delivered if the client is not joined into the conference.";
    }
  }

  return std::visit(
      overloaded{
          [&](MediaStatsChannelFromClient) {
            return absl::InternalError(
                "Media stats requests should not be sent directly. This client "
                "implementation handles stats collection internally.");
          },
          [&](SessionControlChannelFromClient) {
            return data_channels_.session_control->SendRequest(request);
          },
          [&](VideoAssignmentChannelFromClient) {
            return data_channels_.video_assignment->SendRequest(request);
          }},
      request);
};

void MediaApiClient::HandleTrackSignaled(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
  // Tracks should only be signaled by the conference peer connection during its
  // connection flow. Therefore, no state check is needed.

  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver =
      transceiver->receiver();
  cricket::MediaType media_type = receiver->media_type();
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> receiver_track =
      receiver->track();
  // MID should always exist since Meet only supports BUNDLE srtp streams.
  std::string mid;
  if (transceiver->mid().has_value()) {
    mid = *transceiver->mid();
  } else {
    LOG(ERROR) << "MID is not set for transceiver";
    mid = "unset";
  }

  switch (media_type) {
    case cricket::MEDIA_TYPE_AUDIO: {
      auto conference_audio_track = std::make_unique<ConferenceAudioTrack>(
          mid, std::move(receiver),
          std::bind_front(&MediaApiClientObserverInterface::OnAudioFrame,
                          observer_));
      auto audio_track =
          static_cast<webrtc::AudioTrackInterface *>(receiver_track.get());
      audio_track->AddSink(conference_audio_track.get());
      media_tracks_.push_back(std::move(conference_audio_track));
    }
      return;
    case cricket::MEDIA_TYPE_VIDEO: {
      auto conference_video_track = std::make_unique<ConferenceVideoTrack>(
          mid, std::bind_front(&MediaApiClientObserverInterface::OnVideoFrame,
                               observer_));
      auto video_track =
          static_cast<webrtc::VideoTrackInterface *>(receiver_track.get());
      video_track->AddOrUpdateSink(conference_video_track.get(),
                                   rtc::VideoSinkWants());
      media_tracks_.push_back(std::move(conference_video_track));
    }
      return;
    default:
      LOG(WARNING) << "Received remote track of unsupported media type: "
                   << media_type;
      break;
  }
};

void MediaApiClient::HandleResourceUpdate(ResourceUpdate update) {
  observer_->OnResourceUpdate(update);

  if (std::holds_alternative<SessionControlChannelToClient>(update)) {
    SessionControlChannelToClient session_control_update =
        std::move(std::get<SessionControlChannelToClient>(update));
    if (session_control_update.resources.size() != 1) {
      LOG(ERROR) << "Unexpected number of resources in session control update. "
                 << "Expected 1, got: "
                 << session_control_update.resources.size();
      return;
    }

    SessionControlResourceSnapshot session_control_resource =
        std::move(session_control_update.resources[0]);
    if (session_control_resource.session_status.connection_state ==
        SessionStatus::ConferenceConnectionState::kJoined) {
      {
        absl::MutexLock lock(&mutex_);
        if (state_ != State::kJoining) {
          LOG(WARNING) << "Received joined session status while in "
                       << StateToString(state_)
                       << " state instead of joining state.";
          return;
        }

        state_ = State::kJoined;
        observer_->OnJoined();
      }
      VLOG(1) << "Client switched to joined state.";
    } else if (session_control_resource.session_status.connection_state ==
               SessionStatus::ConferenceConnectionState::kDisconnected) {
      VLOG(1) << "Received disconnected session status.";
      // Disconnections triggered by session control updates are considered OK,
      // as they are not actionable by the client. Session control
      // disconnections can occur for a variety of reasons, including:
      // - The server has disconnected the client from the conference.
      // - The conference has ended.
      // - The client has left the conference.
      MaybeDisconnect(absl::OkStatus());
      return;
    }
  } else if (std::holds_alternative<MediaStatsChannelToClient>(update)) {
    MediaStatsChannelToClient media_stats_update =
        std::move(std::get<MediaStatsChannelToClient>(update));
    if (!media_stats_update.resources.has_value()) {
      return;
    }

    if (media_stats_update.resources->size() != 1) {
      LOG(ERROR) << "Unexpected number of resources in media stats update.";
      return;
    }

    MediaStatsConfiguration configuration =
        std::move(media_stats_update.resources.value()[0].configuration);
    // Only 1 MediaStatsChannelToClient update is expected from the server, so
    // the stats config will only be set once. The client then starts collecting
    // stats which reads `stats_config_`. Therefore, no mutex is needed.
    stats_config_ =
        // Request IDs must be non-zero, so the initial value is 1.
        StatsConfig({.stats_request_id = 1,
                     .upload_interval = configuration.upload_interval_seconds,
                     .allowlist = std::move(configuration.allowlist)});

    // Move stats collection off of the network thread to the client thread.
    client_thread_->PostTask(SafeTask(alive_flag_, [&]() {
      // Collect stats regardless of the client's state; if the client is
      // disconnected, stats collection will be a no-op. If the client is not
      // joined into the conference, the server will handle the stats
      // appropriately.
      CollectStats();
    }));
  }
};

void MediaApiClient::MaybeDisconnect(absl::Status status) {
  // This method closes the peer connection if the client has not already been
  // disconnected. Closing the peer connection makes a blocking call on the
  // signaling thread and the network thread. Because disconnection may be
  // triggered on the networking thread (by receiving a session control update),
  // this method must be posted to a different thread to avoid deadlocking.
  //
  // This has the added benefit of not blocking threads that call into the
  // client API as well.
  if (!client_thread_->IsCurrent()) {
    client_thread_->PostTask(SafeTask(
        alive_flag_,
        [this, status = std::move(status)]() { MaybeDisconnect(status); }));
    return;
  }

  {
    absl::MutexLock lock(&mutex_);
    if (state_ == State::kDisconnected) {
      LOG(WARNING) << "Client attempted to disconnect with status: "
                   << status.message()
                   << " while already in disconnected state.";
      return;
    }
    state_ = State::kDisconnected;
  }
  VLOG(1) << "Client switched to disconnected state: " << status.message();

  conference_peer_connection_->Close();
  observer_->OnDisconnected(status);
};

void MediaApiClient::CollectStats() {
  if (stats_config_.upload_interval == 0) {
    LOG(WARNING) << "Stats initiated with 0 upload interval.";
    return;
  }

  auto callback = webrtc::make_ref_counted<OnRTCStatsCollected>(
      [this](
          const rtc::scoped_refptr<const webrtc::RTCStatsReport> &report) {
        MediaStatsChannelFromClient request = StatsRequestFromReport(
            report, stats_config_.stats_request_id, stats_config_.allowlist);
        stats_config_.stats_request_id++;
        absl::Status send_status =
            data_channels_.media_stats->SendRequest(std::move(request));
        if (!send_status.ok()) {
          LOG(ERROR) << "Failed to send stats request: " << send_status;
        }

        // Periodically collect stats by repeatedly posting a delayed task after
        // collecting stats.
        //
        // Closing the peer connection will cancel any pending and future tasks,
        // stopping stats collection.
        client_thread_->PostDelayedTask(
            [&]() { CollectStats(); },
            webrtc::TimeDelta::Seconds(stats_config_.upload_interval));
      });
  conference_peer_connection_->GetStats(callback.get());
}

}  // namespace meet
