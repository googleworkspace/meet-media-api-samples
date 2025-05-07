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

#ifndef CPP_API_MEDIA_API_CLIENT_INTERFACE_H_
#define CPP_API_MEDIA_API_CLIENT_INTERFACE_H_

/// This file contains `MediaApiClientInterface`. It is designed to
/// utilize the
/// [PeerConnection](https://w3c.github.io/webrtc-pc/#peer-to-peer-connections)
/// interface.
///
/// It demonstrates "how-to", and establishes the required configurations and
/// SCTP/SRTP connections with Meet servers. These connections enable the
/// streaming of conference metadata, video, and audio streams from Google Meet
/// conferences to the client.
///
/// @note This is a reference client. It is not intended to be a complete SDK
/// with full support, customization, nor optimizations of WebRTC and real time
/// communication.
///
/// All conference media streams are "receive-only". Currently, the Meet Media
/// API does not support sending of media from `MediaApiClientInterface` into a
/// conference.
///
/// API requests from the client intended to affect application state of a
/// conference or received media (e.g. change video resolution), are transmitted
/// via SCTP data channels. This is in contrast to typical API requests over
/// HTTP or RPC.
///
/// The following steps are needed to set up a typical Meet Media API session:
///
/// 1. Create an implementation of the `MediaApiClientObserverInterface`.
///
/// 2. Create a `MediaApiClientInterface` using an implementation of the
/// `MediaApiClientFactoryInterface`.
///
/// 3. Call `MediaApiClientInterface::ConnectActiveConference` with the
/// appropriate parameters. This initiates the connection with Meet servers.
///
/// 4. Wait for the `MediaApiClientObserverInterface::OnJoined` callback to be
/// invoked.
///
/// 5. If video was enabled, send a `meet::SetVideoAssignmentRequest` via
/// `MediaApiClientInterface::SendRequest`. No video will be
/// transmitted from Meet servers to the client until a successful request has
/// been sent. Check `video_assignment_resource.h` for more information.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "cpp/api/media_entries_resource.h"
#include "cpp/api/media_stats_resource.h"
#include "cpp/api/participants_resource.h"
#include "cpp/api/session_control_resource.h"
#include "cpp/api/video_assignment_resource.h"
#include "webrtc/api/ref_count.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/video_frame.h"

namespace meet {

struct MediaApiClientConfiguration {
  /// For values greater than zero, the Meet Media API client will establish
  /// that many video SRTP streams. After the session is initialized, no other
  /// streams will be created nor intentionally terminated. All connections will
  /// be cleaned up after the session is complete. Up to three streams are
  /// supported and they are "receive-only". Attempts to set a value greater
  /// than three will result in an error.
  uint32_t receiving_video_stream_count = 0;
  /// If audio is enabled, three "receive-only" audio SRTP streams will be
  /// created, always. After the session is initialized, no other streams will
  /// be created nor intentionally terminated. All connections will be cleaned
  /// up after the session is complete.
  bool enable_audio_streams = false;
};

/// Requests that can be sent to Meet servers.
///
/// Requests can expect a corresponding response via the
/// `MediaApiClientObserverInterface`.
///
/// @note In this client implementation, stats requests are sent automatically
/// by the client. They do not need to be sent using
/// `MediaApiClientInterface::SendRequest`.
using ResourceRequest =
    std::variant<SessionControlChannelFromClient,
                 VideoAssignmentChannelFromClient,
                 MediaStatsChannelFromClient>;

/// Updates that can be received from Meet servers.
///
/// Updates can be received in response to a request sent via
/// `MediaApiClientInterface::SendRequest` or from a push from Meet servers.
using ResourceUpdate =
    std::variant<SessionControlChannelToClient, VideoAssignmentChannelToClient,
                 MediaEntriesChannelToClient, ParticipantsChannelToClient,
                 MediaStatsChannelToClient>;

struct AudioFrame {
  absl::Span<const int16_t> pcm16;
  int bits_per_sample;
  int sample_rate;
  size_t number_of_channels;
  size_t number_of_frames;
  bool is_from_loudest_speaker;
  /// Contributing source (CSRC) of the current audio frame. This ID is used to
  /// identify which participant in the conference generated the frame.
  /// Integrators can cross reference this value with values pushed from Meet
  /// servers to the client via `MediaEntriesToClient` resource updates.
  ///
  /// @see [WebRTC Contributing
  /// Source](https://www.w3.org/TR/webrtc/#dom-rtcrtpcontributingsource)
  uint32_t contributing_source;
  /// Synchronization source (SSRC) of the audio frame. This ID identifies which
  /// media stream the audio frame originated from. The SSRC is for debugging
  /// purposes only.
  ///
  /// @see [WebRTC Synchronization
  /// Source](https://www.w3.org/TR/webrtc/#dom-rtcrtpsynchronizationsource)
  uint32_t synchronization_source;
};

struct VideoFrame {
  const webrtc::VideoFrame& frame;
  /// Contributing source (CSRC) of the current audio frame. This ID is used to
  /// identify which participant in the conference generated the frame.
  /// Integrators can cross reference this value with values pushed from Meet
  /// servers to the client via `MediaEntriesToClient` resource updates.
  ///
  /// @see [WebRTC Contributing
  /// Source](https://www.w3.org/TR/webrtc/#dom-rtcrtpcontributingsource)
  uint32_t contributing_source;
  /// Synchronization source (SSRC) of the video frame. This ID identifies which
  /// media stream the video frame originated from. The ssrc is for debugging
  /// purposes only.
  ///
  /// @see [WebRTC Synchronization
  /// Source](https://www.w3.org/TR/webrtc/#dom-rtcrtpsynchronizationsource)
  uint32_t synchronization_source;
};

/// Interface for observing client events.
///
/// Methods are invoked on internal threads, and therefore observer
/// implementations must offload non-trivial work to other threads. Otherwise,
/// they risk blocking the client.
class MediaApiClientObserverInterface : public webrtc::RefCountInterface {
 public:
  ~MediaApiClientObserverInterface() override = default;

  /// Invoked when the client has entered the
  /// `meet::SessionStatus::ConferenceConnectionState::kJoined` state.
  ///
  /// Once this is invoked, the client is fully operational and will remain in
  /// this state until `MediaApiClientObserverInterface::OnDisconnected` is
  /// invoked.
  virtual void OnJoined() = 0;

  /// Invoked when the client disconnects for whatever reason.
  ///
  /// - This will only be called after
  /// `MediaApiClientInterface::ConnectActiveConference` is called.
  ///
  /// - This will be called once and only once, either before or after
  /// `MediaApiClientObserverInterface::OnJoined` is called.
  ///
  /// - Once this is invoked, no other callbacks will be invoked.
  ///
  /// Disconnections are either graceful or ungraceful. Disconnections are
  /// considered graceful if the client receives a
  /// `SessionControlChannelToClient` resource update with a session status of
  /// `meet::SessionStatus::ConferenceConnectionState::kDisconnected`, or if
  /// `MediaApiClientInterface::LeaveConference` is called while the client is
  /// joining the conference. All other disconnections are considered ungraceful
  /// (`PeerConnection` closed, Meet servers unreachable, etc).
  ///
  /// This client implementation passes an OK status for graceful disconnections
  /// and an error status for ungraceful disconnections. Graceful disconnections
  /// can be analyzed by checking the `SessionControlChannelToClient` resource
  /// update received via `MediaApiClientObserverInterface::OnResourceUpdate`.
  virtual void OnDisconnected(absl::Status status) = 0;

  /// Invoked when a resource update is received from Meet servers.
  ///
  /// This can be in response to a request sent via
  /// `MediaApiClientInterface::SendRequest` or a push from Meet servers.
  ///
  /// This will only be invoked while in the
  /// `meet::SessionStatus::ConferenceConnectionState::kJoined` state.
  virtual void OnResourceUpdate(ResourceUpdate update) = 0;

  /// Callback for receiving audio frames.
  ///
  /// Audio frames will not be received for muted participants.
  ///
  /// This will only be invoked while in the
  /// `meet::SessionStatus::ConferenceConnectionState::kJoined` state.
  virtual void OnAudioFrame(AudioFrame frame) = 0;

  /// Callbacks for receiving video frames.
  ///
  /// Video frames will not be received for participants with their video
  /// disabled (i.e. their video is muted).
  ///
  /// This will only be invoked while in the
  /// `meet::SessionStatus::ConferenceConnectionState::kJoined` state.
  virtual void OnVideoFrame(VideoFrame frame) = 0;
};

/// Interface for the Meet Media API client.
///
/// This client implementation is meant to be used for one connection lifetime
/// and then thrown away; if integrators need a new connection, they should
/// create a new instance of `MediaApiClientInterface`.
class MediaApiClientInterface {
 public:
  virtual ~MediaApiClientInterface() = default;

  /// Attempts to connect with Meet servers. This process involves
  /// communicating the intent to join an active Meet conference. It establishes
  /// the signaled SRTP and SCTP connections with the backend.
  ///
  /// - If the client successfully joins the conference,
  /// `MediaApiClientObserverInterface::OnJoined` will be called.
  /// - If this method returns OK and joining fails,
  /// `MediaApiClientObserverInterface::OnDisconnected` method will be called.
  /// - If the client successfully joins,
  /// `MediaApiClientObserverInterface::OnDisconnected` will be invoked when the
  /// client leaves the conference for whatever reason.
  ///
  /// Once fully joined, if audio was enabled, the client will begin receiving
  /// any available streams immediately. If video was enabled, the client will
  /// not receive any video streams until a `meet::SetVideoAssignmentRequest`
  /// is successfully sent to Meet servers and applied.
  ///
  /// @param join_endpoint Must be a valid URL, including the protocol and
  /// host. There aren't very robust checks performed on the provided URL. It is
  /// expected that the URL is well-formed.
  virtual absl::Status ConnectActiveConference(
      absl::string_view join_endpoint, absl::string_view conference_id,
      absl::string_view access_token) = 0;

  /// Convenience method for sending a `SessionControlChannelFromClient` request
  /// with a `LeaveRequest` to Meet servers. This tells the server that the
  /// client should be disconnected from the conference. The request will use
  /// the provided request ID. See `MediaApiClientInterface::SendRequest` for
  /// more information.
  ///
  /// If successful, the client will receive a `SessionControlChannelToClient`
  /// resource update with the same request ID, a session status of
  /// `meet::SessionStatus::ConferenceConnectionState::kDisconnected`, and a
  /// `meet::LeaveResponse`.
  ///
  /// If this is called before the client is fully joined, the client will
  /// immediately transition to the disconnected state, as the Meet servers will
  /// not necessarily respond to the request until the client is fully joined.
  virtual absl::Status LeaveConference(int64_t request_id) = 0;

  /// Sends a resource request to Meet servers.
  ///
  /// @param request This request must have a non-zero, unique
  /// `request_id`. For
  /// example, a `SessionControlRequest`'s request ID must be non-zero and
  /// unique to other requests' IDs. The request ID can be used to
  /// associate the request to the response or error in the
  /// `MediaApiClientObserverInterface`.
  virtual absl::Status SendRequest(const ResourceRequest& request) = 0;

  /// Creates a new instance of `MediaApiClientInterface`.
  ///
  /// It is configured with the required codecs to support streaming media from
  /// Meet conferences. Required SCTP data channels will be opened and the
  /// proper number of SRTP streams will be signaled with Meet servers.
  ///
  /// @param api_session_observer This observer will be retained by the client
  /// until the client is destroyed.
  static absl::StatusOr<std::unique_ptr<MediaApiClientInterface>> Create(
      const MediaApiClientConfiguration& api_config,
      rtc::scoped_refptr<MediaApiClientObserverInterface>
          api_session_observer);
};

}  // namespace meet

#endif  // CPP_API_MEDIA_API_CLIENT_INTERFACE_H_
