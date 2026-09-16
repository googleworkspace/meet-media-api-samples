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
 * @fileoverview Utility functions for the MeetMediaApiClient.
 */

import {
  MediaEntry,
  MediaLayout,
  MeetStreamTrack,
  Participant,
} from '../types/mediatypes';

import {InternalMediaEntry} from './internal_types';
import {SubscribableDelegate} from './subscribable_impl';

interface InternalMediaEntryElement {
  mediaEntry: MediaEntry;
  internalMediaEntry: InternalMediaEntry;
}

/**
 * Creates a new media entry.
 * @return The new media entry and its internal representation.
 */
export function createMediaEntry({
  audioMuted = false,
  videoMuted = false,
  screenShare = false,
  isPresenter = false,
  participant,
  mediaLayout,
  videoMeetStreamTrack,
  audioMeetStreamTrack,
  audioCsrc,
  videoCsrc,
  videoSsrc,
  id,
  session = '',
  sessionName = '',
}: {
  id: number;
  audioMuted?: boolean;
  videoMuted?: boolean;
  screenShare?: boolean;
  isPresenter?: boolean;
  participant?: Participant;
  mediaLayout?: MediaLayout;
  audioMeetStreamTrack?: MeetStreamTrack;
  videoMeetStreamTrack?: MeetStreamTrack;
  videoCsrc?: number;
  audioCsrc?: number;
  videoSsrc?: number;
  session?: string;
  sessionName?: string;
}): InternalMediaEntryElement {
  const participantDelegate = new SubscribableDelegate<Participant | undefined>(
    participant,
  );
  const audioMutedDelegate = new SubscribableDelegate<boolean>(audioMuted);
  const videoMutedDelegate = new SubscribableDelegate<boolean>(videoMuted);
  const screenShareDelegate = new SubscribableDelegate<boolean>(screenShare);
  const isPresenterDelegate = new SubscribableDelegate<boolean>(isPresenter);
  const mediaLayoutDelegate = new SubscribableDelegate<MediaLayout | undefined>(
    mediaLayout,
  );
  const audioMeetStreamTrackDelegate = new SubscribableDelegate<
    MeetStreamTrack | undefined
  >(audioMeetStreamTrack);
  const videoMeetStreamTrackDelegate = new SubscribableDelegate<
    MeetStreamTrack | undefined
  >(videoMeetStreamTrack);

  const mediaEntry: MediaEntry = {
    participant: participantDelegate.getSubscribable(),
    audioMuted: audioMutedDelegate.getSubscribable(),
    videoMuted: videoMutedDelegate.getSubscribable(),
    screenShare: screenShareDelegate.getSubscribable(),
    isPresenter: isPresenterDelegate.getSubscribable(),
    mediaLayout: mediaLayoutDelegate.getSubscribable(),
    audioMeetStreamTrack: audioMeetStreamTrackDelegate.getSubscribable(),
    videoMeetStreamTrack: videoMeetStreamTrackDelegate.getSubscribable(),
    sessionName,
    session,
  };
  const internalMediaEntry: InternalMediaEntry = {
    id,
    audioMuted: audioMutedDelegate,
    videoMuted: videoMutedDelegate,
    screenShare: screenShareDelegate,
    isPresenter: isPresenterDelegate,
    mediaLayout: mediaLayoutDelegate,
    audioMeetStreamTrack: audioMeetStreamTrackDelegate,
    videoMeetStreamTrack: videoMeetStreamTrackDelegate,
    participant: participantDelegate,
    videoSsrc,
    audioCsrc,
    videoCsrc,
  };
  return {mediaEntry, internalMediaEntry};
}
