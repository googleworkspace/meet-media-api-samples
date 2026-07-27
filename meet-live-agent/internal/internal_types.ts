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
 * @fileoverview types are for use in the internal client and should not be
 * used outside of the client.
 */

import {
  MediaEntry,
  MediaLayout,
  MeetStreamTrack,
  Participant,
} from '../types/mediatypes';

import {SubscribableDelegate} from './subscribable_impl';

/**
 * Internal representation of a media entry. Has handles on the
 * subscribable delegates.
 */
export interface InternalMediaEntry {
  audioCsrc?: number;
  videoCsrc?: number;
  videoSsrc?: number;
  readonly id: number;
  readonly audioMuted: SubscribableDelegate<boolean>;
  readonly videoMuted: SubscribableDelegate<boolean>;
  readonly screenShare: SubscribableDelegate<boolean>;
  readonly isPresenter: SubscribableDelegate<boolean>;
  readonly mediaLayout: SubscribableDelegate<MediaLayout | undefined>;
  readonly videoMeetStreamTrack: SubscribableDelegate<
    MeetStreamTrack | undefined
  >;
  readonly audioMeetStreamTrack: SubscribableDelegate<
    MeetStreamTrack | undefined
  >;
  readonly participant: SubscribableDelegate<Participant | undefined>;
}

/**
 * Internal representation of a media layout. Has handles on the
 * subscribable delegates. This only relates to video.
 */
export interface InternalMediaLayout {
  videoSsrc?: number;
  readonly id: number;
  readonly mediaEntry: SubscribableDelegate<MediaEntry | undefined>;
}

/**
 * Internal representation of a meet stream track. Has handles on the
 * subscribable delegates.
 */
export interface InternalMeetStreamTrack {
  readonly mediaEntry: SubscribableDelegate<MediaEntry | undefined>;
  readonly receiver: RTCRtpReceiver;
  videoSsrc?: number;
  maybeAssignMediaEntryOnFrame: (
    mediaEntry: MediaEntry,
    kind: 'audio' | 'video',
  ) => Promise<void>;
}

/**
 * Internal representation of a participant. Has handles on the
 * subscribable delegates.
 */
export interface InternalParticipant {
  // TODO - Remove this once we are using participant names as
  // identifiers. Right now, it is possible for a participant to have multiple
  // ids due to updates being treated as new resources. This occurance should
  // become less frequent once we ignore child participants but can will still
  // be possible until participant names are used as identifiers.
  readonly ids: Set<number>;
  readonly name: string;
  readonly mediaEntries: SubscribableDelegate<MediaEntry[]>;
}
