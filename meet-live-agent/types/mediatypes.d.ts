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
 * @fileoverview Meet Media Api types and interfaces.
 */

import {
  DeletedResource,
  MediaApiRequest,
  MediaApiResponse,
  ResourceSnapshot,
} from './datachannels';
import {LogLevel} from './enums';
import {Subscribable} from './subscribable';

/**
 * Serves as the central relational object between the
 * participant, media canvas and meet stream. This object represents media in
 * a Meet call and holds metadata for the media.
 */
export interface MediaEntry {
  /**
   * Participant abstraction associated with this media entry.
   * participant is immutable.
   */
  readonly participant: Subscribable<Participant | undefined>;
  /**
   * Whether this participant muted their audio stream.
   */
  readonly audioMuted: Subscribable<boolean>;
  /**
   * Whether this participant muted their video stream.
   */
  readonly videoMuted: Subscribable<boolean>;
  /**
   * Whether the current entry is a screenshare.
   */
  readonly screenShare: Subscribable<boolean>;
  /**
   * Whether the current entry is a presenter self-view.
   */
  readonly isPresenter: Subscribable<boolean>;
  /**
   * The media layout associated with this media entry.
   */
  readonly mediaLayout: Subscribable<MediaLayout | undefined>;
  /**
   * The video meet stream track associated with this media entry. Contains the
   * webrtc media stream track.
   */
  readonly videoMeetStreamTrack: Subscribable<MeetStreamTrack | undefined>;
  /**
   * The audio meet stream track associated with this media entry. Contains the
   * webrtc media stream track.
   */
  readonly audioMeetStreamTrack: Subscribable<MeetStreamTrack | undefined>;
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
}

/**
 * An abstraction that represents a participant in a Meet call. Contains
 * the participant object and the media entries associated with this participant.
 */
export interface Participant {
  /**
   * Participant abstraction associated with this participant.
   */
  participant: BaseParticipant;
  /**
   * The media entries associated with this participant. These can be
   * transient. There is one participant to many media entries relationship.
   */
  readonly mediaEntries: Subscribable<MediaEntry[]>;
}

/**
 * Base participant type. Only one of signedInUser, anonymousUser, or phoneUser
 * fields will be set to determine the type of participant.
 */
export interface BaseParticipant {
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
 * A signed in user in a Meet call.
 */
export interface SignedInUser {
  /**
   * Unique ID for the user. Interoperable with {@link https://developers.google.com/admin-sdk/directory/reference/rest/v1/users | Admin SDK API} and {@link https://developers.google.com/people/api/rest/v1/people | People API.}
   * Format: `users/{user}`
   */
  readonly user: string;
  /**
   * Display name of the user. First and last name for a personal device. Admin
   * defined name for a robot account.
   */
  readonly displayName: string;
}

/**
 * An anonymous user in a Meet call.
 */
export interface AnonymousUser {
  /** User specified display name. */
  readonly displayName?: string;
}

/**
 * A dial-in user in a Meet call.
 */
export interface PhoneUser {
  /** Partially redacted user's phone number. */
  readonly displayName: string;
}

/**
 * An abstraction of a track in a Meet stream. This is used to represent
 * both audio and video tracks and their relationship to Media Entries.
 */
export interface MeetStreamTrack {
  /**
   * The {@link https://developer.mozilla.org/en-US/docs/Web/API/MediaStreamTrack | WebRTC MediaStreamTrack} interface of the Media Capture and Streams API represents a single audio or video media track within a stream.
   */
  readonly mediaStreamTrack: MediaStreamTrack;
  /**
   * The media entry associated with this track. This a one to one
   * relationship.
   */
  readonly mediaEntry: Subscribable<MediaEntry | undefined>;
}

/**
 * The dimensions of the canvas for video streams.
 */
export interface CanvasDimensions {
  /**
   * Width measured in pixels. This can be changed by the user.
   */
  width: number;
  /**
   * Height measured in pixels. This can be changed by the user.
   */
  height: number;
}

/**
 * A Media layout for the Media API Web client. This must be created by the
 * Media API client to be valid. This is used to request a video stream.
 */
export interface MediaLayout {
  /**
   * The dimensions of the layout.
   */
  readonly canvasDimensions: CanvasDimensions;
  /**
   * The media entry associated with this layout.
   */
  readonly mediaEntry: Subscribable<MediaEntry | undefined>;
}

/**
 * A request for a {@link https://developers.google.com/meet/media-api/reference/web/media_api_web.medialayout | MediaLayout}. This is required to be able to request a video
 * stream.
 */
export interface MediaLayoutRequest {
  /**
   * The {@link https://developers.google.com/meet/media-api/reference/web/media_api_web.medialayout | MediaLayout} to request.
   */
  mediaLayout: MediaLayout;
}

/**
 * Required configuration for the {@link https://developers.google.com/meet/media-api/reference/web/media_api_web.meetmediaapiclient | MeetMediaApiClient}.
 */
export interface MeetMediaClientRequiredConfiguration {
  /**
   * The meeting space ID to connect to.
   */
  meetingSpaceId: string;
  /**
   * The number of video streams to request.
   */
  numberOfVideoStreams: number;
  /**
   * Number of audio streams is not configurable. False maps to 0 and True maps
   * to 3.
   */
  enableAudioStreams: boolean;
  /**
   * The access token to use for authentication.
   */
  accessToken: string;
  /**
   * The callback to use for logging events.
   */
  logsCallback?: (logEvent: LogEvent) => void;
}

/**
 * List of log source types.
 */
export type LogSourceType =
  | 'session-control'
  | 'participants'
  | 'media-entries'
  | 'video-assignment'
  | 'media-stats';

/**
 * Log event that is propagated to the callback.
 */
export interface LogEvent {
  /**
   * The level of the log event.
   */
  level: LogLevel;
  /**
   * The log string of the event.
   */
  logString: string;
  /**
   * The source type of the event.
   */
  sourceType: LogSourceType;
  /**
   * The relevant object of the event.
   */
  relevantObject?:
    | Error
    | DeletedResource
    | ResourceSnapshot
    | MediaApiResponse
    | MediaApiRequest;
}
