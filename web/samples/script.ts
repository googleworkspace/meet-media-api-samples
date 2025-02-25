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

import {MeetMediaApiClientImpl} from '../internal/meetmediaapiclient_impl';
import {MeetConnectionState} from '../types/enums';
import {MeetStreamTrack} from '../types/mediatypes';
import {MeetSessionStatus} from '../types/meetmediaapiclient';

// Function maps session status to strings. If the session is joined, we go
// ahead and request a layout.
async function handleSessionChange(status: MeetSessionStatus) {
  let statusString;
  switch (status.connectionState) {
    case MeetConnectionState.WAITING:
      statusString = 'WAITING';
      break;
    case MeetConnectionState.JOINED:
      statusString = 'JOINED';
      // tslint:disable-next-line:no-any
      const client = (window as any).client;
      const mediaLayout = client.createMediaLayout({width: 500, height: 500});
      const response = await client.applyLayout([{mediaLayout}]);
      console.log(response);
      break;
    case MeetConnectionState.DISCONNECTED:
      statusString = 'DISCONNECTED';
      break;
    default:
      statusString = 'UNKNOWN';
      break;
  }
  // Update page with session status.
  document.getElementById('session-status')!.textContent =
    `Session Status: ${statusString}`;
}

const VIDEO_IDS = [1, 2, 3, 4, 5, 6];
const AUDIO_IDS = [1, 2, 3];

let availableVideoIds = [...VIDEO_IDS];
let availableAudioIds = [...AUDIO_IDS];
const trackIdToElementId = new Map<string, number>();

// Called when the Meet stream collection changes (when a Media track is added
// to or removed from the peer connection).
function handleStreamChange(meetStreamTracks: MeetStreamTrack[]) {
  // We create local sets of ids so that we don't have to add back ids when
  // tracks are removed.
  const localAvailableVideoIds = new Set(VIDEO_IDS);
  const localAvailableAudioIds = new Set(AUDIO_IDS);
  meetStreamTracks.forEach((meetStreamTrack: MeetStreamTrack) => {
    if (meetStreamTrack.mediaStreamTrack.kind === 'video') {
      const elementId = trackIdToElementId.get(
        meetStreamTrack.mediaStreamTrack.id,
      );
      if (elementId) {
        // If a track is already in the element then we remove it from the local
        // ids and continue.
        localAvailableVideoIds.delete(elementId);
        return;
      }
      // If this is a new track, then we create a MediaStream and add it to a
      // video element.
      const mediaStream = new MediaStream();
      mediaStream.addTrack(meetStreamTrack.mediaStreamTrack);

      // Update id collections. We do expect to run out of available ids, but
      // reassign to a valid id (1) in case we do.
      const videoId = availableVideoIds.pop() ?? 1;
      localAvailableVideoIds.delete(videoId);

      // Retrieve available video element and assign media stream to it.
      const videoIdString = `video-${videoId}`;
      const videoElement = document.getElementById(videoIdString);
      (videoElement! as HTMLVideoElement).srcObject = mediaStream;
      trackIdToElementId.set(meetStreamTrack.mediaStreamTrack.id, videoId);
    } else if (meetStreamTrack.mediaStreamTrack.kind === 'audio') {
      const elementId = trackIdToElementId.get(
        meetStreamTrack.mediaStreamTrack.id,
      );
      if (elementId) {
        // If a track is already in the element then we remove it from the local
        // ids and continue.
        localAvailableAudioIds.delete(elementId);
        return;
      }

      // If this is a new track, then we create a MediaStream and add it to a
      // audio element.
      const mediaStream = new MediaStream();
      mediaStream.addTrack(meetStreamTrack.mediaStreamTrack);

      // Update id collections. We do expect to run out of available ids, but
      // reassign to a valid id (1) in case we do.
      const audioId = availableAudioIds.pop() ?? 1;
      localAvailableAudioIds.delete(audioId);

      // Retrieve available audio element and assign media stream to it.
      const audioIdString = `audio-${audioId}`;
      const audioElement = document.getElementById(audioIdString);
      (audioElement! as HTMLAudioElement).srcObject = mediaStream;
      trackIdToElementId.set(meetStreamTrack.mediaStreamTrack.id, audioId);
    }
  });

  // Set local set of tracks to top level available id collections.
  availableVideoIds = [...localAvailableVideoIds];
  availableAudioIds = [...localAvailableAudioIds];
}

/**
 * Create Media API client and subscribe to session status and meet stream
 * changes.
 */
export function createClient(
  meetingSpaceId: string,
  numberOfVideoStreams: number,
  enableAudioStreams: boolean,
  accessToken: string,
) {
  const client = new MeetMediaApiClientImpl({
    meetingSpaceId,
    numberOfVideoStreams,
    enableAudioStreams,
    accessToken,
  });
  // tslint:disable-next-line:no-any
  (window as any).client = client;
  client.sessionStatus.subscribe(handleSessionChange);
  client.meetStreamTracks.subscribe(handleStreamChange);
  console.log('Media API Client created.');
}

/**
 * Join meeting if client exists
 */
export async function joinMeeting(): Promise<void> {
  // tslint:disable-next-line:no-any
  const client = (window as any).client;
  if (!client) return;
  console.log(await client.joinMeeting());
}

/**
 * Leave meeting if client exists
 */
export function leaveMeeting() {
  // tslint:disable-next-line:no-any
  console.log((window as any).client.leaveMeeting());
}
