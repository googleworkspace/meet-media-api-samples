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
 * @fileoverview interface for MeetMediaApiClient.
 */

import {MediaApiCommunicationProtocol} from './communication_protocol';
import {MediaApiResponseStatus} from './datachannels';
import {MeetConnectionState, MeetDisconnectReason} from './enums';
import {
  CanvasDimensions,
  MediaEntry,
  MediaLayout,
  MediaLayoutRequest,
  MeetStreamTrack,
  Participant,
} from './mediatypes';
import {Subscribable} from './subscribable';

/**
 * The status of the Meet Media API session.
 */
export interface MeetSessionStatus {
  connectionState: MeetConnectionState;
  disconnectReason?: MeetDisconnectReason;
}

/**
 * Interface for the MeetMediaApiClient. Takes a required configuration
 * and provides a set of subscribables to the client.
 * Takes a {@link https://developers.google.com/meet/media-api/reference/web/media_api_web.meetmediaclientrequiredconfiguration | MeetMediaClientRequiredConfiguration} as a constructor
 * parameter.
 */
export interface MeetMediaApiClient {
  /**
   * The status of the session. Subscribable to changes in the session status.
   */
  readonly sessionStatus: Subscribable<MeetSessionStatus>;
  /**
   * The meet stream tracks in the meeting. Subscribable to changes in the meet
   * stream track collection.
   */
  readonly meetStreamTracks: Subscribable<MeetStreamTrack[]>;
  /**
   * The media entries in the meeting. Subscribable to changes in the media
   * entry collection.
   */
  readonly mediaEntries: Subscribable<MediaEntry[]>;
  /**
   * The participants in the meeting. Subscribable to changes in the
   * participant collection.
   */
  readonly participants: Subscribable<Participant[]>;
  /**
   * The presenter in the meeting. Subscribable to changes in the presenter.
   */
  readonly presenter: Subscribable<MediaEntry | undefined>;
  /**
   * The screenshare in the meeting. Subscribable to changes in the screenshare.
   */
  readonly screenshare: Subscribable<MediaEntry | undefined>;

  /**
   * Joins the meeting.
   * @param communicationProtocol The communication protocol to use. If not
   *     provided, a default {@link https://developers.google.com/meet/media-api/reference/web/media_api_web.mediaapicommunicationprotocol | MediaApiCommunicationProtocol} will be used.
   */
  joinMeeting(
    communicationProtocol?: MediaApiCommunicationProtocol,
  ): Promise<void>;

  /**
   * Leaves the meeting.
   */
  leaveMeeting(): Promise<void>;

  /**
   * Applies the given media layout requests. This is required to be able to
   * request a video stream. Only accepts media layouts that have been
   * created with the {@link https://developers.google.com/meet/media-api/reference/web/media_api_web.meetmediaapiclient.createmedialayout | createMediaLayout} function.
   * @param requests The requests to apply.
   * @return A promise that resolves when the request has been accepted. NOTE:
   *     The promise resolving on the request does not mean the layout has been
   *     applied. It means that the request has been accepted and you may need
   *     to wait a short amount of time for these layouts to be applied.
   */
  applyLayout(requests: MediaLayoutRequest[]): Promise<MediaApiResponseStatus>;

  /**
   * Creates a new media layout. Only media layouts that are created with this
   * function can be applied. Otherwise, the {@link https://developers.google.com/meet/media-api/reference/web/media_api_web.meetmediaapiclient.applylayout.md | applyLayout} function will
   * throw an error. Once the media layout has been created, you can construct a
   * request and apply it with the {@link https://developers.google.com/meet/media-api/reference/web/media_api_web.meetmediaapiclient.applylayout.md | applyLayout} function. These media
   * layout objects are meant to be reused (can be reassigned to a different
   * request) but are distinct per stream (need to be created for each stream).
   * @param canvasDimensions The dimensions of the canvas to render the layout
   *     on.
   * @return The new media layout.
   */
  createMediaLayout(canvasDimensions: CanvasDimensions): MediaLayout;
}
