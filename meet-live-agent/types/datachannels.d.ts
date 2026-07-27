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

/**
 * @fileoverview Data channel interfaces for the Google Meet Media API.
 */

// COMMON

/**
 * Base interface for all requests.
 */
export declare interface MediaApiRequest {
  /**
   * A unique client-generated identifier for this request. Different requests
   * must never have the same request ID in the scope of one data channel.
   */
  requestId: number;
}

/**
 * Base status for a response.
 */
export declare interface MediaApiResponseStatus {
  /** Status code for the response. */
  code: number;
  /** Message for the response. */
  message: string;
  /** Additional details for the response. */
  // tslint:disable-next-line:no-any
  details: any[];
}

/**
 * Base interface for all responses.
 */
export declare interface MediaApiResponse {
  /**
   * ID of the associated request.
   */
  requestId: number;
  /**
   * Response status for the request.
   */
  status: MediaApiResponseStatus;
}

/**
 * Base interface for all resource snapshots provided by the server.
 */
export declare interface ResourceSnapshot {
  /**
   * The resource ID of the resource being updated. For singleton resources,
   * this is unset.
   */
  id?: number;
}

/**
 * Base interface for all deleted resources.
 */
export declare interface DeletedResource {
  /**
   * ID of the deleted resource.
   */
  id: number;
}

// SESSION CONTROL

/**
 * Session control data channel message from the client to the server.
 */
export declare interface SessionControlChannelFromClient {
  /** Request to leave the session. */
  request: LeaveRequest;
}

/**
 * Session control data channel message from the server to the client.
 */
export declare interface SessionControlChannelToClient {
  /** An optional response to an incoming request. */
  response?: LeaveResponse;
  /**
   * List of resource snapshots managed by the server, with no implied order.
   */
  resources?: SessionStatusResource[];
}

/**
 * Tells the server the client is about to disconnect. After receiving the
 * response, the client should not expect to receive any other messages or media
 * RTP.
 */
export declare interface LeaveRequest extends MediaApiRequest {
  /**
   * Leave field, always empty.
   */
  leave: {};
}

/**
 * Response to a leave request from the server.
 */
export declare interface LeaveResponse extends MediaApiResponse {
  /**
   * Leave field, always empty.
   */
  leave: {};
}

/**
 * Singleton resource containing the status of the media session.
 */
export declare interface SessionStatusResource extends ResourceSnapshot {
  sessionStatus: SessionStatus;
}

/**
 * Session status.
 */
export declare interface SessionStatus {
  /**
   * The connection state of the session.
   *
   * - `STATE_WAITING`: Session is waiting to be admitted into the meeting.
   *   The client may never observe this state if it was admitted or rejected
   *   quickly.
   *
   * - `STATE_JOINED`: Session has fully joined the meeting.
   *
   * - `STATE_DISCONNECTED`: Session is not connected to the meeting.
   */
  connectionState: 'STATE_WAITING' | 'STATE_JOINED' | 'STATE_DISCONNECTED';

  /**
   * The reason for the disconnection from the meeting. Only set if the
   * `connectionState` is `STATE_DISCONNECTED`.
   *
   * - `REASON_CLIENT_LEFT`: The Media API client sent a leave request.
   *
   * - `REASON_USER_STOPPED`: A user explicitly stopped the Media API session.
   *
   * - `REASON_CONFERENCE_ENDED`: The conference ended.
   *
   * - `REASON_SESSION_UNHEALTHY`: Something else went wrong with the session.
   */
  disconnectReason?:
    | 'REASON_CLIENT_LEFT'
    | 'REASON_USER_STOPPED'
    | 'REASON_CONFERENCE_ENDED'
    | 'REASON_SESSION_UNHEALTHY';
}

// PARTICIPANTS

/**
 * Participants data channel message from the server to the client.
 */
export declare interface ParticipantsChannelToClient {
  /**
   * List of resource snapshots managed by the server, with no implied order.
   */
  resources?: ParticipantResource[];
  /** List of deleted resources with no implied order. */
  deletedResources?: DeletedParticipant[];
}

// (-- LINT.IfChange --)

/**
 * Base participant resource type
 */
export declare interface ParticipantResource extends ResourceSnapshot {
  participant: BaseParticipant;
}

/**
 * Singleton resource containing participant information.
 * There will be exactly one of signedInUser, anonymousUser, or phoneUser fields
 * set to determine the type of participant.
 */
export declare interface BaseParticipant extends ResourceSnapshot {
  /**
   * Resource name of the participant.
   * Format: `conferenceRecords/{conferenceRecord}/participants/{participant}`
   *
   * You can use this to retrieve additional information about the participant
   * from the {@link https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants | Meet REST API - Participants} resource.
   *
   * Unused for now. Use participantKey instead.
   */
  name?: string;
  /**
   * Participant key of associated participant.
   * Format is `participants/{participant}`.
   *
   * You can use this to retrieve additional information about the participant
   * from the {@link https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants | Meet REST API - Participants} resource.
   *
   * `Note`: This has to be in the format of `conferenceRecords/{conference_record}/participants/{participant}`.
   *
   *  You can retrieve the conference record from the {@link https://developers.google.com/meet/api/guides/conferences | Meet REST API - Conferences} resource.
   *
   */
  participantKey?: string;
  /**
   * Participant id for internal usage.
   */
  participantId: number;
  /**
   * If set, the participant is a signed in user. Provides a unique ID and
   * display name.
   */
  signedInUser?: SignedInUser;
  /**
   * If set, the participant is an anonymous user. Provides a display name.
   */
  anonymousUser?: AnonymousUser;
  /**
   * If set, the participant is a dial-in user. Provides a partially redacted
   * phone number.
   */
  phoneUser?: PhoneUser;
}

/**
 * Signed in user type, always has a unique id and display name.
 */
export declare interface SignedInUser {
  /**
   * Unique ID for the user. Interoperable with {@link https://developers.google.com/admin-sdk/directory/reference/rest/v1/users | Admin SDK API } and {@link https://developers.google.com/people/api/rest/v1/people | People API}.
   * Format: `users/{user}`
   */
  user: string;

  /**
   * For a personal device, it's the user's first name and last name.
   * For a robot account, it's the administrator-specified device name. For
   * example, "Altostrat Room".
   */
  displayName: string;
}

/**
 * Anonymous user type, requires display name to be set.
 */
export declare interface AnonymousUser {
  /** User provided name when they join a conference anonymously. */
  displayName: string;
}

/**
 * Phone user type, always has a display name. User dialing in from a phone
 * where the user's identity is unknown because they haven't signed in with a
 * Google Account.
 */
export declare interface PhoneUser {
  /** Partially redacted user's phone number. */
  displayName: string;
}
// (--
// LINT.ThenChange(//depot/google3/google/apps/meet/v2main/resource.proto)
// --)

/**
 * Deleted resource for a participant.
 */
export declare interface DeletedParticipant extends DeletedResource {
  /**
   * Set to true if the participant is successfully deleted.
   */
  participant: boolean;
}

// MEDIA ENTRIES

/**
 * Media entries data channel message from the server to the client.
 */
export declare interface MediaEntriesChannelToClient {
  /**
   * List of resource snapshots managed by the server, with no implied order.
   */
  resources?: MediaEntryResource[];
  /** List of deleted resources with no implied order. */
  deletedResources?: DeletedMediaEntry[];
}

/**
 * Resource snapshot for a media entry.
 */
export declare interface MediaEntryResource extends ResourceSnapshot {
  /**
   * Media entry resource.
   */
  mediaEntry: MediaEntry;
}

/**
 * Media Entry interface.
 */
export declare interface MediaEntry {
  /**
   * Participant ID for the media entry.
   * @deprecated Use participant key instead.
   */
  participantId: number;

  /**
   * Resource name of the participant.
   * Format: `conferenceRecords/{conferenceRecord}/participants/{participant}`
   *
   * You can use this to retrieve additional information about the participant
   * from the {@link https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants | Meet REST API - Participants} resource.
   *
   * Unused for now. Use participantKey instead.
   */
  participant?: string;

  /**
   * Participant key of associated participant.
   * Format is `participants/{participant}`.
   *
   * You can use this to retrieve additional information about the participant
   * from the {@link https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants | Meet REST API - Participants} resource.
   *
   * `Note`: This has to be in the format of `conferenceRecords/{conference_record}/participants/{participant}`.
   *
   *  You can retrieve the conference record from the {@link https://developers.google.com/meet/api/guides/conferences | Meet REST API - Conferences} resource.
   *
   */
  participantKey?: string;

  /**
   * Participant session name. There should be a one to one mapping of session
   * to Media Entry. You can use this to retrieve additional information about
   * the participant session from the
   * {@link https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants.participantSessions | Meet REST API - ParticipantSessions} resource
   *
   * Format is
   * `conferenceRecords/{conference_record}/participants/{participant}/participantSessions/{participant_session}`
   * Unused for now. Use sessionName instead.
   */
  session?: string;

  /**
   * The session ID of the media entry.
   *
   * Format is
   * `participants/{participant}/participantSessions/{participant_session}`
   *
   * You can use this to retrieve additional information about
   * the participant session from the
   * {@link https://developers.google.com/meet/api/reference/rest/v2/conferenceRecords.participants.participantSessions | Meet REST API - ParticipantSessions} resource.
   *
   * `Note`: This has to be in the format of `conferenceRecords/{conference_record}/participants/{participant}/participantSessions/{participant_session}`.
   *
   *  You can retrieve the conference record from the {@link https://developers.google.com/meet/api/guides/conferences | Meet REST API - Conferences} resource.
   */
  sessionName?: string;
  /**
   * CSRC for any audio stream contributed by this participant.
   */
  audioCsrc?: number;
  /**
   * CSRCs for any video streams contributed by this participant.
   */
  videoCsrcs?: number[];
  /**
   * Whether the current entry is presentating.
   */
  presenter: boolean;
  /**
   * Whether the current entry is a screenshare.
   */
  screenshare: boolean;
  /**
   * Whether this participant muted their audio stream.
   */
  audioMuted: boolean;
  /**
   * Whether this participant muted their video stream.
   */
  videoMuted: boolean;
}

/**
 * Deleted resource for a media entry.
 */
export declare interface DeletedMediaEntry extends DeletedResource {
  /**
   * Set to true if the media entry is successfully deleted.
   */
  mediaEntry: boolean;
}

// VIDEO ASSIGNMENT

/**
 * Video assignment data channel message from the server to the client.
 */
export declare interface VideoAssignmentChannelToClient {
  /** An optional response to an incoming request. */
  response?: SetVideoAssignmentResponse;
  /**
   * Resource snapshots managed by the server.
   */
  resources?: VideoAssignmentResource[];
}

/**
 * Video assignment data channel message from the client to the server.
 */
export declare interface VideoAssignmentChannelFromClient {
  /**
   * Request to set video assignment.
   */
  request: SetVideoAssignmentRequest;
}

/**
 * Dimensions of a canvas.
 */
export declare interface CanvasDimensions {
  /**
   * Height in square pixels. For cameras that can change orientation,
   * height refers to the measurement on the vertical axis.
   */
  height: number;
  /**
   * Width in square pixels. For cameras that can change orientation,
   * width refers to the measurement on the horizontal axis.
   */
  width: number;
}

/**
 * Video canvas for video assignment.
 */
export declare interface MediaApiCanvas {
  /**
   * ID for the video canvas. This is required and must be unique within
   * the containing layout model. Clients should prudently reuse these
   * IDs, as this allows the backend to keep assigning video streams to
   * the same canvas as much as possible.
   */
  id: number;
  /**
   * Dimensions of the canvas.
   */
  dimensions: CanvasDimensions;
  /**
   * Tells the server to choose the best video stream for this canvas.
   * This is the only supported mode for now.
   */
  relevant: {};
}

/**
 * Request to set video assignment. In order to get video streams, the client
 * must set a video assignment.
 */
export declare interface SetVideoAssignmentRequest extends MediaApiRequest {
  /**
   * Set video assignment.
   */
  setAssignment: {
    /** Layout model for the video assignment. */
    layoutModel: LayoutModel;
    /**
     * Maximum video resolution the client wants to receive for any video
     * feeds.
     */
    maxVideoResolution: VideoAssignmentMaxResolution;
  };
}

/**
 * Maximum video resolution the client wants to receive for any video feeds.
 */
export declare interface VideoAssignmentMaxResolution {
  /**
   * Height in square pixels. For cameras that can change orientation,
   * height refers to the measurement on the vertical axis.
   */
  height: number;
  /**
   * Width in square pixels. For cameras that can change orientation,
   * width refers to the measurement on the horizontal axis.
   */
  width: number;
  /** Frames per second. */
  frameRate: number;
}

/** Layout model for the video assignment. */
export declare interface LayoutModel {
  /**
   * Label of the layout model. This is used to identify the layout model
   * when requesting video assignment.
   */
  label: string;
  /**
   * Canvases to assign videos to virtual SSRCs. Providing more
   * canvases than exists virtual streams will result in an error status.
   * Virtual video SSRCs are allocated during initialization of the client
   * and the number of virtual SSRCs is fixed to the initial number of requested video streams.
   */
  canvases: MediaApiCanvas[];
}

/**
 * Response to a set video assignment request from the server.
 */
export declare interface SetVideoAssignmentResponse extends MediaApiResponse {
  /**
   * Set video assignment. This is always empty.
   */
  setAssignment: {};
}

/**
 * Singleton resource describing how video streams are assigned to video
 * canvases specified in the client's video layout model.
 */
export declare interface VideoAssignmentResource extends ResourceSnapshot {
  videoAssignment: VideoAssignmentLayoutModel;
}

/**
 * Video assignment for a layout model.
 */
export declare interface VideoAssignmentLayoutModel {
  /** Label of the layout model. */
  label: string;
  /** Canvas assignments, with no implied order. */
  canvases: CanvasAssignment[];
}

/**
 * Video assignment for a single canvas.
 */
export declare interface CanvasAssignment {
  /**
   * The video canvas the video should be shown in.
   */
  canvasId: number;
  /**
   * The virtual video SSRC that the video will be sent over, or
   * unset if no video from the participant.
   */
  ssrc?: number;
  /**
   * ID of the media entry associated with the video stream.
   */
  mediaEntryId: number;
}

// MEDIA STATS

/**
 * Media stats data channel message from the server to the client.
 */
export declare interface MediaStatsChannelToClient {
  /** An optional response to an incoming request. */
  response?: UploadMediaStatsResponse;
  /**
   * Resource snapshots managed by the server.
   */
  resources?: MediaStatsResource[];
}

/**
 * Resource snapshot for media stats. Managed by the server.
 */
export declare interface MediaStatsResource extends ResourceSnapshot {
  /**
   * Configuration for media stats provided by the server and has to be used by
   * the client to upload media stats.
   */
  configuration: MediaStatsConfiguration;
}

/**
 * Configuration for media stats. Provided by the server and has to be used by
 * the client to upload media stats.
 */
export declare interface MediaStatsConfiguration {
  /**
   * The interval between each upload of media stats. If this is zero, the
   * client should not upload any media stats.
   */
  uploadIntervalSeconds: number;
  /**
   * A map of allow listed sections. The key is the section type, and the value
   * is the keys that are allow listed for that section. Fields can be found in
   * {@link https://developer.mozilla.org/en-US/docs/Web/API/RTCStatsReport | RTCStatsReport}
   */
  allowlist: Map<string, {key: string[]}>;
}

/**
 * Media stats data channel message from the client to the server.
 */
export declare interface MediaStatsChannelFromClient {
  /**
   * Request to upload media stats.
   */
  request: UploadMediaStatsRequest;
}

/**
 * Uploads media stats from the client to the server. The stats are
 * retrieved from WebRTC by calling {@link https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/getStats | RTCPeerConnection.getStats()}. The
 * returned {@link https://developer.mozilla.org/en-US/docs/Web/API/RTCStatsReport | RTCStatsReport} can be mapped to the sections below.
 */
export declare interface UploadMediaStatsRequest extends MediaApiRequest {
  /**
   * Upload media stats.
   */
  uploadMediaStats: UploadMediaStats;
}

/**
 * Upload media stats.
 */
export declare interface UploadMediaStats {
  /**
   * Represents the entries in
   * {@link https://developer.mozilla.org/en-US/docs/Web/API/RTCStatsReport | RTCStatsReport}.
   * Formatted as an array of objects with an id and a type.
   * The value of the id is a string WebRTC id of the section.
   * The value of the type is the section.
   */
  sections: StatsSectionData[];
}

/**
 * A base section of media stats. All sections have an id.
 */
export declare interface StatsSection {
  id: string;
}

// STATS SECTION TYPES
// Mapped Types are being used instead of interfaces to allow for objects to be
// indexed by string.

/**
 * Stats section types. There are defined by the {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype | WebRTC spec.}
 */
export declare interface StatTypes {
  // Need to use underscore naming to conform to expected structure for data
  // channel.
  // tslint:disable: enforce-name-casing
  /**
   * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-candidate-pair | ICE candidate pair stats } related to RTCIceTransport.
   */
  candidate_pair: CandidatePairSection;
  /**
   * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-codec | Codec stats } that is currently being used by RTP streams being received by RTCPeerConnection.
   */
  codec: CodecSection;
  /**
   * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-inbound-rtp | RTP stats } for inbound stream that is currently received by RTCPeerConnection.
   */
  inbound_rtp: InboundRtpSection;
  /**
   * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-media-playout | Media playout stats } related to RTCPeerConnection.
   */
  media_playout: MediaPlayoutSection;
  /**
   * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-transport | Transport stats } related to RTCPeerConnection.
   */
  transport: TransportSection;
  /**
   * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-local-candidate | ICE candidate stats } for the local candidate related to RTCPeerConnection.
   */
  local_candidate: IceCandidateSection;
  /**
   * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-remote-candidate | ICE candidate stats } for the remote candidate related to RTCPeerConnection.
   */
  remote_candidate: IceCandidateSection;
  // tslint:enable: enforce-name-casing
}

/**
 * A section of media stats. Used to map the {@link https://developer.mozilla.org/en-US/docs/Web/API/RTCStatsReport | RTCStatsReport} to the expected
 * structure for the data channel. All sections have an id and a type.
 * For fields in a specific type, please see the StatTypes interface.
 */
export declare type StatsSectionData = StatsSection & {
  [key in keyof StatTypes]?: StatTypes[key];
};

/**
 * Code section string fields.
 * @ignore
 */
type CodeSectionStringFields = 'mime_type';
/**
 * Codec section numeric fields.
 * @ignore
 */
type CodecSectionNumberFields = 'payload_type';

/**
 * Codec fields.
 * @ignore
 */
export declare type CodecSectionFields =
  | CodeSectionStringFields
  | CodecSectionNumberFields;

/**
 * Codec field mapping.
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-codec | WebRTC spec.}
 * @ignore
 */
export declare type CodecSection = {
  [key in CodecSectionFields]?: key extends CodeSectionStringFields
    ? string
    : number;
};

/**
 * Inbound RTP section string fields.
 * @ignore
 */
type InboundRtpSectionStringFields = 'codec_id' | 'kind';

/**
 * Inbound RTP section number fields.
 * @ignore
 */
type InboundRtpSectionNumberFields =
  | 'ssrc'
  | 'jitter'
  | 'packets_lost'
  | 'packets_received'
  | 'bytes_received'
  | 'jitter_buffer_delay'
  | 'jitter_buffer_emitted_count'
  | 'jitter_buffer_minimum_delay'
  | 'jitter_buffer_target_delay'
  | 'total_audio_energy'
  | 'fir_count'
  | 'frame_height'
  | 'frame_width'
  | 'frames_decoded'
  | 'frames_dropped'
  | 'frames_per_second'
  | 'frames_received'
  | 'freeze_count'
  | 'key_frames_decoded'
  | 'nack_count'
  | 'pli_count'
  | 'retransmitted_packets_received'
  | 'total_freezes_duration'
  | 'total_pauses_duration'
  | 'audio_level'
  | 'concealed_samples'
  | 'total_samples_received'
  | 'total_samples_duration';

/**
 * Inbound Rtp fields.
 * @ignore
 */
export type InboundRtpSectionFields =
  | InboundRtpSectionStringFields
  | InboundRtpSectionNumberFields;

/**
 * Inbound RTP field mapping.
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-inbound-rtp | WebRTC spec.}
 * @ignore
 */
export declare type InboundRtpSection = {
  [key in InboundRtpSectionFields]?: key extends InboundRtpSectionStringFields
    ? string
    : number;
};

/**
 * Candidate pair fields.
 * @ignore
 */
export type CandidatePairSectionFields =
  | 'available_outgoing_bitrate'
  | 'bytes_received'
  | 'bytes_sent'
  | 'current_round_trip_time'
  | 'packets_received'
  | 'packets_sent'
  | 'total_round_trip_time';

/**
 * Candidate pair field mapping.
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-candidate-pair | WebRTC spec.}
 * @ignore
 */
export declare type CandidatePairSection = {
  [key in CandidatePairSectionFields]: number;
};

/**
 * Media playout fields.
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcaudioplayoutstats | WebRTC spec.}
 * @ignore
 */
export type MediaPlayoutSectionFields =
  | 'synthesized_samples_duration'
  | 'synthesized_samples_events'
  | 'total_samples_duration'
  | 'total_playout_delay'
  | 'total_samples_count';

/**
 * Media playout field mapping.
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-media-playout | WebRTC spec.}
 * @ignore
 */
export declare type MediaPlayoutSection = {
  [key in MediaPlayoutSectionFields]?: number;
};

/**
 * Transport fields.
 * {@link https://www.w3.org/TR/webrtc-stats/#transportstats-dict | WebRTC spec.}
 * @ignore
 */
export type TransportSectionFields = 'selected_candidate_pair_id';

/**
 * Transport field mapping.
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-transport. | WebRTC spec.}
 * @ignore
 */
export declare type TransportSection = {
  [key in TransportSectionFields]?: string;
};

/**
 * Ice candidate string fields.
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcicecandidatestats | WebRTC spec.}
 * @ignore
 */
type IceCandidateSectionStringFields =
  | 'address'
  | 'candidate_type'
  | 'protocol'
  | 'network_type';

/**
 * Ice candidate number fields.
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcicecandidatestats | WebRTC spec.}
 * @ignore
 */
type IceCandidateSectionNumberFields = 'port';

/**
 *  Ice candidate fields.
 *  {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcicecandidatestats | WebRTC spec.}
 * @ignore
 */
export declare type IceCandidateSectionFields =
  | IceCandidateSectionStringFields
  | IceCandidateSectionNumberFields;

/**
 * Ice candidate field mapping.
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-local-candidate | Local candidate WebRTC spec.} or
 * {@link https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-remote-candidate | Remote candidate WebRTC spec.}.
 * @ignore
 */
export declare type IceCandidateSection = {
  [key in IceCandidateSectionFields]?: key extends IceCandidateSectionStringFields
    ? string
    : number;
};

/**
 * Response to a media stats upload request.
 */
export declare interface UploadMediaStatsResponse extends MediaApiResponse {
  uploadMediaStats: {};
}
