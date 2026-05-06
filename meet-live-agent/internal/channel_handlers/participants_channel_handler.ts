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
 * @fileoverview Handles participants data channel updates
 */

import {
  DeletedParticipant,
  ParticipantResource,
  ParticipantsChannelToClient,
} from '../../types/datachannels';
import {LogLevel} from '../../types/enums';
import {
  Participant as LocalParticipant,
  MediaEntry,
} from '../../types/mediatypes';
import {InternalMediaEntry, InternalParticipant} from '../internal_types';
import {SubscribableDelegate} from '../subscribable_impl';
import {ChannelLogger} from './channel_logger';

/**
 * Handler for participants channel
 */
export class ParticipantsChannelHandler {
  constructor(
    private readonly channel: RTCDataChannel,
    private readonly participantsDelegate: SubscribableDelegate<
      LocalParticipant[]
    >,
    private readonly idParticipantMap = new Map<number, LocalParticipant>(),
    private readonly nameParticipantMap = new Map<string, LocalParticipant>(),
    private readonly internalParticipantMap = new Map<
      LocalParticipant,
      InternalParticipant
    >(),
    private readonly internalMediaEntryMap = new Map<
      MediaEntry,
      InternalMediaEntry
    >(),
    private readonly channelLogger?: ChannelLogger,
  ) {
    this.channel.onmessage = (event) => {
      this.onParticipantsMessage(event);
    };
    this.channel.onopen = () => {
      this.onParticipantsOpened();
    };
    this.channel.onclose = () => {
      this.onParticipantsClosed();
    };
  }

  private onParticipantsOpened() {
    this.channelLogger?.log(LogLevel.MESSAGES, 'Participants channel: opened');
  }

  private onParticipantsMessage(event: MessageEvent) {
    const data = JSON.parse(event.data) as ParticipantsChannelToClient;
    let participants = this.participantsDelegate.get();
    data.deletedResources?.forEach((deletedResource: DeletedParticipant) => {
      this.channelLogger?.log(
        LogLevel.RESOURCES,
        'Participants channel: deleted resource',
        deletedResource,
      );
      const participant = this.idParticipantMap.get(deletedResource.id);
      if (!participant) {
        return;
      }
      this.idParticipantMap.delete(deletedResource.id);
      const deletedParticipant = this.internalParticipantMap.get(participant);
      if (!deletedParticipant) {
        return;
      }
      deletedParticipant.ids.delete(deletedResource.id);
      if (deletedParticipant.ids.size !== 0) {
        return;
      }
      if (participant.participant.name) {
        this.nameParticipantMap.delete(participant.participant.name);
      }
      participants = participants.filter((p) => p !== participant);
      this.internalParticipantMap.delete(participant);
      deletedParticipant.mediaEntries.get().forEach((mediaEntry) => {
        const internalMediaEntry = this.internalMediaEntryMap.get(mediaEntry);
        if (internalMediaEntry) {
          internalMediaEntry.participant.set(undefined);
        }
      });
    });

    const addedParticipants: LocalParticipant[] = [];
    data.resources?.forEach((resource: ParticipantResource) => {
      this.channelLogger?.log(
        LogLevel.RESOURCES,
        'Participants channel: added resource',
        resource,
      );
      if (!resource.id) {
        // We expect all participants to have an id. If not, we log an error
        // and ignore the participant.
        this.channelLogger?.log(
          LogLevel.ERRORS,
          'Participants channel: participant resource has no id',
          resource,
        );
        return;
      }
      // We do not expect that the participant resource already exists.
      // However, it is possible that the media entries channel references it
      // before we receive the participant resource. In this case, we update
      // the participant resource with the type and maintain the media entry
      // relationship.
      let existingMediaEntriesDelegate:
        | SubscribableDelegate<MediaEntry[]>
        | undefined;
      let existingParticipant: LocalParticipant | undefined;
      let existingIds: Set<number> | undefined;
      if (this.idParticipantMap.has(resource.id)) {
        existingParticipant = this.idParticipantMap.get(resource.id);
      } else if (
        resource.participant.name &&
        this.nameParticipantMap.has(resource.participant.name)
      ) {
        existingParticipant = this.nameParticipantMap.get(
          resource.participant.name,
        );
      } else if (resource.participant.participantKey) {
        existingParticipant = Array.from(
          this.internalParticipantMap.entries(),
        ).find(
          ([participant, _]) =>
            participant.participant.participantKey ===
            resource.participant.participantKey,
        )?.[0];
      }

      if (existingParticipant) {
        const internalParticipant =
          this.internalParticipantMap.get(existingParticipant);
        if (internalParticipant) {
          existingMediaEntriesDelegate = internalParticipant.mediaEntries;
          // (TODO: Remove this once we are using participant
          // names as identifiers. Right now, it is possible for a participant to
          // have multiple ids due to updates being treated as new resources.
          existingIds = internalParticipant.ids;
          existingIds.forEach((id) => {
            this.idParticipantMap.delete(id);
          });
        }
        if (existingParticipant.participant.name) {
          this.nameParticipantMap.delete(existingParticipant.participant.name);
        }
        this.internalParticipantMap.delete(existingParticipant);
        participants = participants.filter((p) => p !== existingParticipant);
        this.channelLogger?.log(
          LogLevel.ERRORS,
          'Participants channel: participant resource already exists',
          resource,
        );
      }

      const participantElement = createParticipant(
        resource,
        existingMediaEntriesDelegate,
        existingIds,
      );
      const participant = participantElement.participant;
      const internalParticipant = participantElement.internalParticipant;
      participantElement.internalParticipant.ids.forEach((id) => {
        this.idParticipantMap.set(id, participant);
      });
      if (resource.participant.name) {
        this.nameParticipantMap.set(resource.participant.name, participant);
      }

      this.internalParticipantMap.set(participant, internalParticipant);
      addedParticipants.push(participant);
    });

    // Update participant collection.
    if (data.resources?.length || data.deletedResources?.length) {
      const newParticipants = [...participants, ...addedParticipants];
      this.participantsDelegate.set(newParticipants);
    }
  }

  private onParticipantsClosed() {
    this.channelLogger?.log(LogLevel.MESSAGES, 'Participants channel: closed');
  }
}

interface InternalParticipantElement {
  participant: LocalParticipant;
  internalParticipant: InternalParticipant;
}

/**
 * Creates a new participant.
 * @return The new participant and its internal representation.
 */
function createParticipant(
  resource: ParticipantResource,
  mediaEntriesDelegate = new SubscribableDelegate<MediaEntry[]>([]),
  existingIds = new Set<number>(),
): InternalParticipantElement {
  if (!resource.id) {
    throw new Error('Participant resource must have an id');
  }

  const participant: LocalParticipant = {
    participant: resource.participant,
    mediaEntries: mediaEntriesDelegate.getSubscribable(),
  };

  existingIds.add(resource.id);

  const internalParticipant: InternalParticipant = {
    name: resource.participant.name ?? '',
    ids: existingIds,
    mediaEntries: mediaEntriesDelegate,
  };
  return {
    participant,
    internalParticipant,
  };
}
