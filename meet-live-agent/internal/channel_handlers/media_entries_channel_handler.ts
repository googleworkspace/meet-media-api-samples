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
 * @fileoverview Handles Media entries
 */

import {
  DeletedMediaEntry,
  MediaEntriesChannelToClient,
  MediaEntryResource,
} from '../../types/datachannels';
import {LogLevel} from '../../types/enums';
import {
  MediaEntry,
  MediaLayout,
  MeetStreamTrack,
  Participant,
} from '../../types/mediatypes';
import {
  InternalMediaEntry,
  InternalMediaLayout,
  InternalMeetStreamTrack,
  InternalParticipant,
} from '../internal_types';
import {SubscribableDelegate} from '../subscribable_impl';
import {createMediaEntry} from '../utils';
import {ChannelLogger} from './channel_logger';

/**
 * Helper class to handle the media entries channel.
 */
export class MediaEntriesChannelHandler {
  constructor(
    private readonly channel: RTCDataChannel,
    private readonly mediaEntriesDelegate: SubscribableDelegate<MediaEntry[]>,
    private readonly idMediaEntryMap: Map<number, MediaEntry>,
    private readonly internalMediaEntryMap = new Map<
      MediaEntry,
      InternalMediaEntry
    >(),
    private readonly internalMeetStreamTrackMap = new Map<
      MeetStreamTrack,
      InternalMeetStreamTrack
    >(),
    private readonly internalMediaLayoutMap = new Map<
      MediaLayout,
      InternalMediaLayout
    >(),
    private readonly participantsDelegate: SubscribableDelegate<Participant[]>,
    private readonly nameParticipantMap: Map<string, Participant>,
    private readonly idParticipantMap: Map<number, Participant>,
    private readonly internalParticipantMap: Map<
      Participant,
      InternalParticipant
    >,
    private readonly presenterDelegate: SubscribableDelegate<
      MediaEntry | undefined
    >,
    private readonly screenshareDelegate: SubscribableDelegate<
      MediaEntry | undefined
    >,
    private readonly channelLogger?: ChannelLogger,
  ) {
    this.channel.onmessage = (event) => {
      this.onMediaEntriesMessage(event);
    };
    this.channel.onopen = () => {
      this.channelLogger?.log(
        LogLevel.MESSAGES,
        'Media entries channel: opened',
      );
    };
    this.channel.onclose = () => {
      this.channelLogger?.log(
        LogLevel.MESSAGES,
        'Media entries channel: closed',
      );
    };
  }

  private onMediaEntriesMessage(message: MessageEvent) {
    const data = JSON.parse(message.data) as MediaEntriesChannelToClient;
    let mediaEntryArray = this.mediaEntriesDelegate.get();

    // Delete media entries.
    data.deletedResources?.forEach((deletedResource: DeletedMediaEntry) => {
      this.channelLogger?.log(
        LogLevel.RESOURCES,
        'Media entries channel: resource deleted',
        deletedResource,
      );
      const deletedMediaEntry = this.idMediaEntryMap.get(deletedResource.id);
      if (deletedMediaEntry) {
        mediaEntryArray = mediaEntryArray.filter(
          (mediaEntry) => mediaEntry !== deletedMediaEntry,
        );
        // If we find the media entry in the id map, it should exist in the
        // internal map.
        const internalMediaEntry =
          this.internalMediaEntryMap.get(deletedMediaEntry);
        // Remove relationship between media entry and media layout.
        const mediaLayout: MediaLayout | undefined =
          internalMediaEntry!.mediaLayout.get();
        if (mediaLayout) {
          const internalMediaLayout =
            this.internalMediaLayoutMap.get(mediaLayout);
          if (internalMediaLayout) {
            internalMediaLayout.mediaEntry.set(undefined);
          }
        }

        // Remove relationship between media entry and meet stream tracks.
        const videoMeetStreamTrack =
          internalMediaEntry!.videoMeetStreamTrack.get();
        if (videoMeetStreamTrack) {
          const internalVideoStreamTrack =
            this.internalMeetStreamTrackMap.get(videoMeetStreamTrack);
          internalVideoStreamTrack!.mediaEntry.set(undefined);
        }

        const audioMeetStreamTrack =
          internalMediaEntry!.audioMeetStreamTrack.get();
        if (audioMeetStreamTrack) {
          const internalAudioStreamTrack =
            this.internalMeetStreamTrackMap.get(audioMeetStreamTrack);
          internalAudioStreamTrack!.mediaEntry.set(undefined);
        }

        // Remove relationship between media entry and participant.
        const participant = internalMediaEntry!.participant.get();
        if (participant) {
          const internalParticipant =
            this.internalParticipantMap.get(participant);
          const newMediaEntries: MediaEntry[] =
            internalParticipant!.mediaEntries
              .get()
              .filter((mediaEntry) => mediaEntry !== deletedMediaEntry);
          internalParticipant!.mediaEntries.set(newMediaEntries);
          internalMediaEntry!.participant.set(undefined);
        }

        // Remove from maps
        this.idMediaEntryMap.delete(deletedResource.id);
        this.internalMediaEntryMap.delete(deletedMediaEntry);

        if (this.screenshareDelegate.get() === deletedMediaEntry) {
          this.screenshareDelegate.set(undefined);
        }
        if (this.presenterDelegate.get() === deletedMediaEntry) {
          this.presenterDelegate.set(undefined);
        }
      }
    });

    // Update or add media entries.
    const addedMediaEntries: MediaEntry[] = [];
    data.resources?.forEach((resource: MediaEntryResource) => {
      this.channelLogger?.log(
        LogLevel.RESOURCES,
        'Media entries channel: resource added',
        resource,
      );

      let internalMediaEntry: InternalMediaEntry | undefined;
      let mediaEntry: MediaEntry | undefined;
      let videoCsrc = 0;
      if (
        resource.mediaEntry.videoCsrcs &&
        resource.mediaEntry.videoCsrcs.length > 0
      ) {
        // We expect there to only be one video Csrcs. There is possibility
        // for this to be more than value in WebRTC but unlikely in Meet.
        // TODO : Explore making video csrcs field singluar.
        videoCsrc = resource.mediaEntry.videoCsrcs[0];
      } else {
        this.channelLogger?.log(
          LogLevel.ERRORS,
          'Media entries channel: more than one video Csrc in media entry',
          resource,
        );
      }

      if (this.idMediaEntryMap.has(resource.id!)) {
        // Update media entry if it already exists.
        mediaEntry = this.idMediaEntryMap.get(resource.id!);
        mediaEntry!.sessionName = resource.mediaEntry.sessionName;
        mediaEntry!.session = resource.mediaEntry.session;
        internalMediaEntry = this.internalMediaEntryMap.get(mediaEntry!);
        internalMediaEntry!.audioMuted.set(resource.mediaEntry.audioMuted);
        internalMediaEntry!.videoMuted.set(resource.mediaEntry.videoMuted);
        internalMediaEntry!.screenShare.set(resource.mediaEntry.screenshare);
        internalMediaEntry!.isPresenter.set(resource.mediaEntry.presenter);
        internalMediaEntry!.audioCsrc = resource.mediaEntry.audioCsrc;
        internalMediaEntry!.videoCsrc = videoCsrc;
      } else {
        // Create new media entry if it does not exist.
        const mediaEntryElement = createMediaEntry({
          audioMuted: resource.mediaEntry.audioMuted,
          videoMuted: resource.mediaEntry.videoMuted,
          screenShare: resource.mediaEntry.screenshare,
          isPresenter: resource.mediaEntry.presenter,
          id: resource.id!,
          audioCsrc: resource.mediaEntry.audioCsrc,
          videoCsrc,
          sessionName: resource.mediaEntry.sessionName,
          session: resource.mediaEntry.session,
        });
        internalMediaEntry = mediaEntryElement.internalMediaEntry;
        mediaEntry = mediaEntryElement.mediaEntry;
        this.internalMediaEntryMap.set(mediaEntry, internalMediaEntry);
        this.idMediaEntryMap.set(internalMediaEntry.id, mediaEntry);
        addedMediaEntries.push(mediaEntry);
      }

      // Assign meet streams to media entry if they are not already assigned
      // correctly.
      if (
        !mediaEntry!.audioMuted.get() &&
        internalMediaEntry!.audioCsrc &&
        !this.isMediaEntryAssignedToMeetStreamTrack(internalMediaEntry!)
      ) {
        this.assignAudioMeetStreamTrack(mediaEntry!, internalMediaEntry!);
      }

      // Assign participant to media entry
      let existingParticipant: Participant | undefined;
      if (resource.mediaEntry.participant) {
        existingParticipant = this.nameParticipantMap.get(
          resource.mediaEntry.participant,
        );
      } else if (resource.mediaEntry.participantKey) {
        existingParticipant = Array.from(
          this.internalParticipantMap.entries(),
        ).find(
          ([participant, _]) =>
            participant.participant.participantKey ===
            resource.mediaEntry.participantKey,
        )?.[0];
      }

      if (existingParticipant) {
        const internalParticipant =
          this.internalParticipantMap.get(existingParticipant);
        if (internalParticipant) {
          const newMediaEntries: MediaEntry[] = [
            ...internalParticipant.mediaEntries.get(),
            mediaEntry!,
          ];
          internalParticipant.mediaEntries.set(newMediaEntries);
        }
        internalMediaEntry!.participant.set(existingParticipant);
      } else if (
        resource.mediaEntry.participant ||
        resource.mediaEntry.participantKey
      ) {
        // This is unexpected behavior, but technically possible. We expect
        // that the participants are received from the participants channel
        // before the media entries channel but this is not guaranteed.
        this.channelLogger?.log(
          LogLevel.RESOURCES,
          'Media entries channel: participant not found in name participant map' +
            ' creating participant',
        );
        const subscribableDelegate = new SubscribableDelegate<MediaEntry[]>([
          mediaEntry!,
        ]);
        const newParticipant: Participant = {
          participant: {
            name: resource.mediaEntry.participant,
            anonymousUser: {},
            participantKey: resource.mediaEntry.participantKey,
          },
          mediaEntries: subscribableDelegate.getSubscribable(),
        };
        // TODO: Use participant resource name instead of id.
        // tslint:disable-next-line:deprecation
        const ids: Set<number> = resource.mediaEntry.participantId
          ? // tslint:disable-next-line:deprecation
            new Set([resource.mediaEntry.participantId])
          : new Set();
        const internalParticipant: InternalParticipant = {
          name: resource.mediaEntry.participant ?? '',
          ids,
          mediaEntries: subscribableDelegate,
        };
        if (resource.mediaEntry.participant) {
          this.nameParticipantMap.set(
            resource.mediaEntry.participant,
            newParticipant,
          );
        }
        this.internalParticipantMap.set(newParticipant, internalParticipant);
        // TODO: Use participant resource name instead of id.
        // tslint:disable-next-line:deprecation
        if (resource.mediaEntry.participantId) {
          this.idParticipantMap.set(
            // TODO: Use participant resource name instead of id.
            // tslint:disable-next-line:deprecation
            resource.mediaEntry.participantId,
            newParticipant,
          );
        }
        const participantArray = this.participantsDelegate.get();
        this.participantsDelegate.set([...participantArray, newParticipant]);
        internalMediaEntry!.participant.set(newParticipant);
      }
      if (resource.mediaEntry.presenter) {
        this.presenterDelegate.set(mediaEntry);
      } else if (
        !resource.mediaEntry.presenter &&
        this.presenterDelegate.get() === mediaEntry
      ) {
        this.presenterDelegate.set(undefined);
      }
      if (resource.mediaEntry.screenshare) {
        this.screenshareDelegate.set(mediaEntry);
      } else if (
        !resource.mediaEntry.screenshare &&
        this.screenshareDelegate.get() === mediaEntry
      ) {
        this.screenshareDelegate.set(undefined);
      }
    });

    // Update media entry collection.
    if (
      (data.resources && data.resources.length > 0) ||
      (data.deletedResources && data.deletedResources.length > 0)
    ) {
      const newMediaEntryArray = [...mediaEntryArray, ...addedMediaEntries];
      this.mediaEntriesDelegate.set(newMediaEntryArray);
    }
  }

  private isMediaEntryAssignedToMeetStreamTrack(
    internalMediaEntry: InternalMediaEntry,
  ): boolean {
    const audioStreamTrack = internalMediaEntry.audioMeetStreamTrack.get();
    if (!audioStreamTrack) return false;
    const internalAudioMeetStreamTrack =
      this.internalMeetStreamTrackMap.get(audioStreamTrack);
    // This is not expected. Map should be comprehensive of all meet stream
    // tracks.
    if (!internalAudioMeetStreamTrack) return false;
    // The Audio CRSCs changed and therefore need to be checked if the current
    // audio csrc is in the contributing sources.
    const contributingSources: RTCRtpContributingSource[] =
      internalAudioMeetStreamTrack.receiver.getContributingSources();

    for (const contributingSource of contributingSources) {
      if (contributingSource.source === internalMediaEntry.audioCsrc) {
        // Audio Csrc found in contributing sources.
        return true;
      }
    }
    // Audio Csrc not found in contributing sources, unassign audio meet stream
    // track.
    internalMediaEntry.audioMeetStreamTrack.set(undefined);
    return false;
  }

  private assignAudioMeetStreamTrack(
    mediaEntry: MediaEntry,
    internalMediaEntry: InternalMediaEntry,
  ) {
    for (const [
      meetStreamTrack,
      internalMeetStreamTrack,
    ] of this.internalMeetStreamTrackMap.entries()) {
      // Only audio tracks are assigned here.
      if (meetStreamTrack.mediaStreamTrack.kind !== 'audio') continue;
      const receiver = internalMeetStreamTrack.receiver;
      const contributingSources: RTCRtpContributingSource[] =
        receiver.getContributingSources();
      for (const contributingSource of contributingSources) {
        if (contributingSource.source === internalMediaEntry.audioCsrc) {
          internalMediaEntry.audioMeetStreamTrack.set(meetStreamTrack);
          internalMeetStreamTrack.mediaEntry.set(mediaEntry);
          return;
        }
      }
      // If Audio Csrc is not found in contributing sources, fall back to
      // polling frames for assignment.
      internalMeetStreamTrack.maybeAssignMediaEntryOnFrame(mediaEntry, 'audio');
    }
  }
}
