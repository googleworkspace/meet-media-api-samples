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
 * @fileoverview Implementation of InternalMeetStreamTrack.
 */

import {MediaEntry, MeetStreamTrack} from '../types/mediatypes';
import {SubscribableDelegate} from './subscribable_impl';

import {InternalMediaEntry, InternalMeetStreamTrack} from './internal_types';

/**
 * Implementation of InternalMeetStreamTrack.
 */
export class InternalMeetStreamTrackImpl implements InternalMeetStreamTrack {
  private readonly reader: ReadableStreamDefaultReader;
  videoSsrc?: number;

  constructor(
    readonly receiver: RTCRtpReceiver,
    readonly mediaEntry: SubscribableDelegate<MediaEntry | undefined>,
    private readonly meetStreamTrack: MeetStreamTrack,
    private readonly internalMediaEntryMap: Map<MediaEntry, InternalMediaEntry>,
  ) {
    const mediaStreamTrack = meetStreamTrack.mediaStreamTrack;
    let mediaStreamTrackProcessor;
    if (mediaStreamTrack.kind === 'audio') {
      mediaStreamTrackProcessor = new MediaStreamTrackProcessor({
        track: mediaStreamTrack as MediaStreamAudioTrack,
      });
    } else {
      mediaStreamTrackProcessor = new MediaStreamTrackProcessor({
        track: mediaStreamTrack as MediaStreamVideoTrack,
      });
    }
    this.reader = mediaStreamTrackProcessor.readable.getReader();
  }

  async maybeAssignMediaEntryOnFrame(
    mediaEntry: MediaEntry,
    kind: 'audio' | 'video',
  ): Promise<void> {
    // Only want to check the media entry if it has the correct csrc type
    // for this meet stream track.
    if (
      !this.mediaStreamTrackSrcPresent(mediaEntry) ||
      this.meetStreamTrack.mediaStreamTrack.kind !== kind
    ) {
      return;
    }
    // Loop through the frames until media entry is assigned by either this
    // meet stream track or another meet stream track.
    while (!this.mediaEntryTrackAssigned(mediaEntry, kind)) {
      const frame = await this.reader.read();
      if (frame.done) break;
      if (kind === 'audio') {
        await this.onAudioFrame(mediaEntry);
      } else if (kind === 'video') {
        this.onVideoFrame(mediaEntry);
      }
      frame.value.close();
    }
    return;
  }

  private async onAudioFrame(mediaEntry: MediaEntry): Promise<void> {
    const internalMediaEntry = this.internalMediaEntryMap.get(mediaEntry);
    const contributingSources: RTCRtpContributingSource[] =
      this.receiver.getContributingSources();
    for (const contributingSource of contributingSources) {
      if (contributingSource.source === internalMediaEntry!.audioCsrc) {
        internalMediaEntry!.audioMeetStreamTrack.set(this.meetStreamTrack);
        this.mediaEntry.set(mediaEntry);
      }
    }
  }

  private onVideoFrame(mediaEntry: MediaEntry): void {
    const internalMediaEntry = this.internalMediaEntryMap.get(mediaEntry);
    const synchronizationSources: RTCRtpSynchronizationSource[] =
      this.receiver.getSynchronizationSources();
    for (const syncSource of synchronizationSources) {
      if (syncSource.source === internalMediaEntry!.videoSsrc) {
        this.videoSsrc = syncSource.source;
        internalMediaEntry!.videoMeetStreamTrack.set(this.meetStreamTrack);
        this.mediaEntry.set(mediaEntry);
      }
    }
    return;
  }

  private mediaEntryTrackAssigned(
    mediaEntry: MediaEntry,
    kind: 'audio' | 'video',
  ): boolean {
    if (
      (kind === 'audio' && mediaEntry.audioMeetStreamTrack.get()) ||
      (kind === 'video' && mediaEntry.videoMeetStreamTrack.get())
    ) {
      return true;
    }
    return false;
  }

  private mediaStreamTrackSrcPresent(mediaEntry: MediaEntry): boolean {
    const internalMediaEntry = this.internalMediaEntryMap.get(mediaEntry);
    if (this.meetStreamTrack.mediaStreamTrack.kind === 'audio') {
      return !!internalMediaEntry?.audioCsrc;
    } else if (this.meetStreamTrack.mediaStreamTrack.kind === 'video') {
      return !!internalMediaEntry?.videoSsrc;
    }
    return false;
  }
}
