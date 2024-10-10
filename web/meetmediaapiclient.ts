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

import {MediaApiCommunicationProtocol} from './internal/communication_protocols/communication_protocol';
import {MediaApiResponseStatus} from './types/datachannels';
import {
  CanvasDimensions,
  MediaEntry,
  MediaLayout,
  MediaLayoutRequest,
  MeetSessionStatus,
  MeetStreamTrack,
  Participant,
} from './types/mediatypes';
import {Subscribable} from './types/subscribable';

/**
 * TESTING
 * Interface for the MeetMediaApiClient.
 */
export interface MeetMediaApiClient {
  readonly sessionStatus: Subscribable<MeetSessionStatus>;
  readonly meetStreamTracks: Subscribable<MeetStreamTrack[]>;
  readonly mediaEntries: Subscribable<MediaEntry[]>;
  readonly participants: Subscribable<Participant[]>;
  readonly presenter: Subscribable<MediaEntry | undefined>;
  readonly screenshare: Subscribable<MediaEntry[] | undefined>;

  joinMeeting(
    communicationProtocol?: MediaApiCommunicationProtocol,
  ): Promise<void>;
  leaveMeeting(): Promise<void>;

  /**
   * @param requests The requests to apply.
   * @return A promise that resolves when the request has been accepted. NOTE:
   *     The promise resolving on the request does not mean the layout has been
   *     applied. It means that the request has been accepted and you may need
   *     to wait a short amount of time for these layouts to be applied.
   */
  applyLayout(requests: MediaLayoutRequest[]): Promise<MediaApiResponseStatus>;

  /**
   * Creates a new media layout. Only media layouts that are created with this
   * function can be applied. Otherwise, the applyLayout function will throw an
   * error. Once the media layout has been created, you can construct a request
   * and apply it with the applyLayout function. These media layout objects are
   * meant to be reused (can be reassigned to a different request) but are
   * distinct per stream (need to be created for each stream).
   * @param canvasDimensions The dimensions of the canvas to render the layout
   *     on.
   * @return The new media layout.
   */
  createMediaLayout(canvasDimensions: CanvasDimensions): MediaLayout;
}
