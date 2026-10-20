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
 * @fileoverview Video assignment channel handler.
 */

import {
  MediaApiCanvas,
  MediaApiResponseStatus,
  SetVideoAssignmentRequest,
  SetVideoAssignmentResponse,
  VideoAssignmentChannelFromClient,
  VideoAssignmentChannelToClient,
  VideoAssignmentResource,
} from '../../types/datachannels';
import {LogLevel} from '../../types/enums';
import {
  MediaEntry,
  MediaLayout,
  MediaLayoutRequest,
  MeetStreamTrack,
} from '../../types/mediatypes';
import {
  InternalMediaEntry,
  InternalMediaLayout,
  InternalMeetStreamTrack,
} from '../internal_types';
import {SubscribableDelegate} from '../subscribable_impl';
import {createMediaEntry} from '../utils';
import {ChannelLogger} from './channel_logger';

// We request the highest possible resolution by default.
const MAX_RESOLUTION = {
  height: 1080,
  width: 1920,
  frameRate: 30,
};

/**
 * Helper class to handle the video assignment channel.
 */
export class VideoAssignmentChannelHandler {
  private requestId = 1;
  private readonly mediaLayoutLabelMap = new Map<MediaLayout, string>();
  private readonly pendingRequestResolveMap = new Map<
    number,
    (value: MediaApiResponseStatus) => void
  >();

  constructor(
    private readonly channel: RTCDataChannel,
    private readonly idMediaEntryMap: Map<number, MediaEntry>,
    private readonly internalMediaEntryMap = new Map<
      MediaEntry,
      InternalMediaEntry
    >(),
    private readonly idMediaLayoutMap = new Map<number, MediaLayout>(),
    private readonly internalMediaLayoutMap = new Map<
      MediaLayout,
      InternalMediaLayout
    >(),
    private readonly mediaEntriesDelegate: SubscribableDelegate<MediaEntry[]>,
    private readonly internalMeetStreamTrackMap = new Map<
      MeetStreamTrack,
      InternalMeetStreamTrack
    >(),
    private readonly channelLogger?: ChannelLogger,
  ) {
    this.channel.onmessage = (event) => {
      this.onVideoAssignmentMessage(event);
    };
    this.channel.onclose = () => {
      // Resolve all pending requests with an error.
      this.channelLogger?.log(
        LogLevel.MESSAGES,
        'Video assignment channel: closed',
      );
      for (const [, resolve] of this.pendingRequestResolveMap) {
        resolve({code: 400, message: 'Channel closed', details: []});
      }
      this.pendingRequestResolveMap.clear();
    };
    this.channel.onopen = () => {
      this.channelLogger?.log(
        LogLevel.MESSAGES,
        'Video assignment channel: opened',
      );
    };
  }

  private onVideoAssignmentMessage(message: MessageEvent) {
    const data = JSON.parse(message.data) as VideoAssignmentChannelToClient;
    if (data.response) {
      this.onVideoAssignmentResponse(data.response);
    }
    if (data.resources) {
      this.onVideoAssignmentResources(data.resources);
    }
  }

  private onVideoAssignmentResponse(response: SetVideoAssignmentResponse) {
    // Users should listen on the video assignment channel for actual video
    // assignments. These responses signify that the request was expected.
    this.channelLogger?.log(
      LogLevel.MESSAGES,
      'Video assignment channel: recieved response',
      response,
    );
    this.pendingRequestResolveMap.get(response.requestId)?.(response.status);
  }

  private onVideoAssignmentResources(resources: VideoAssignmentResource[]) {
    resources.forEach((resource) => {
      this.channelLogger?.log(
        LogLevel.RESOURCES,
        'Video assignment channel: resource added',
        resource,
      );
      if (resource.videoAssignment.canvases) {
        this.onVideoAssignment(resource);
      }
    });
  }

  private onVideoAssignment(videoAssignment: VideoAssignmentResource) {
    const canvases = videoAssignment.videoAssignment.canvases;
    canvases.forEach(
      (canvas: {canvasId: number; ssrc?: number; mediaEntryId: number}) => {
        const mediaLayout = this.idMediaLayoutMap.get(canvas.canvasId);
        // We expect that the media layout is already created.
        let internalMediaEntry;
        if (mediaLayout) {
          const assignedMediaEntry = mediaLayout.mediaEntry.get();
          let mediaEntry;
          // if association already exists, we need to either update the video
          // ssrc or remove the association if the ids don't match.
          if (
            assignedMediaEntry &&
            this.internalMediaEntryMap.get(assignedMediaEntry)?.id ===
              canvas.mediaEntryId
          ) {
            // We expect the internal media entry to be already created if the media entry exists.
            internalMediaEntry =
              this.internalMediaEntryMap.get(assignedMediaEntry);
            // If the media canvas is already associated with a media entry, we
            // need to update the video ssrc.
            // Expect the media entry to be created, without assertion, TS
            // complains it can be undefined.
            // tslint:disable:no-unnecessary-type-assertion
            internalMediaEntry!.videoSsrc = canvas.ssrc;
            mediaEntry = assignedMediaEntry;
          } else {
            // If asssocation does not exist, we will attempt to retreive the
            // media entry from the map.
            const existingMediaEntry = this.idMediaEntryMap.get(
              canvas.mediaEntryId,
            );
            // Clear existing association if it exists.
            if (assignedMediaEntry) {
              this.internalMediaEntryMap
                .get(assignedMediaEntry)
                ?.mediaLayout.set(undefined);
              this.internalMediaLayoutMap
                .get(mediaLayout)
                ?.mediaEntry.set(undefined);
            }
            if (existingMediaEntry) {
              // If the media entry exists, need to create the media canvas association.
              internalMediaEntry =
                this.internalMediaEntryMap.get(existingMediaEntry);
              internalMediaEntry!.videoSsrc = canvas.ssrc;
              internalMediaEntry!.mediaLayout.set(mediaLayout);
              mediaEntry = existingMediaEntry;
            } else {
              // If the media entry doewsn't exist, we need to create it and
              // then create the media canvas association.
              // We don't expect to hit this expression, but since data channels
              // don't guarantee order, we do this to be safe.
              const mediaEntryElement = createMediaEntry({
                id: canvas.mediaEntryId,
                mediaLayout,
                videoSsrc: canvas.ssrc,
              });
              this.internalMediaEntryMap.set(
                mediaEntryElement.mediaEntry,
                mediaEntryElement.internalMediaEntry,
              );
              internalMediaEntry = mediaEntryElement.internalMediaEntry;
              const newMediaEntry = mediaEntryElement.mediaEntry;
              this.idMediaEntryMap.set(canvas.mediaEntryId, newMediaEntry);
              const newMediaEntries = [
                ...this.mediaEntriesDelegate.get(),
                newMediaEntry,
              ];
              this.mediaEntriesDelegate.set(newMediaEntries);
              mediaEntry = newMediaEntry;
            }
            this.internalMediaLayoutMap
              .get(mediaLayout)
              ?.mediaEntry.set(mediaEntry);
            this.internalMediaEntryMap

              .get(mediaEntry!)
              ?.mediaLayout.set(mediaLayout);
          }
          if (
            !this.isMediaEntryAssignedToMeetStreamTrack(
              mediaEntry!,
              internalMediaEntry!,
            )
          ) {
            this.assignVideoMeetStreamTrack(mediaEntry!);
          }
        }
        // tslint:enable:no-unnecessary-type-assertion
        this.channelLogger?.log(
          LogLevel.ERRORS,
          'Video assignment channel: server sent a canvas that was not created by the client',
        );
      },
    );
  }

  sendRequests(
    mediaLayoutRequests: MediaLayoutRequest[],
  ): Promise<MediaApiResponseStatus> {
    const label = Date.now().toString();
    const canvases: MediaApiCanvas[] = [];
    mediaLayoutRequests.forEach((request) => {
      this.mediaLayoutLabelMap.set(request.mediaLayout, label);
      canvases.push({
        id: this.internalMediaLayoutMap.get(request.mediaLayout)!.id,
        dimensions: request.mediaLayout.canvasDimensions,
        relevant: {},
      });
    });
    const request: SetVideoAssignmentRequest = {
      requestId: this.requestId++,
      setAssignment: {
        layoutModel: {
          label,
          canvases,
        },
        maxVideoResolution: MAX_RESOLUTION,
      },
    };
    this.channelLogger?.log(
      LogLevel.MESSAGES,
      'Video Assignment channel: Sending request',
      request,
    );
    try {
      this.channel.send(
        JSON.stringify({
          request,
        } as VideoAssignmentChannelFromClient),
      );
    } catch (e) {
      this.channelLogger?.log(
        LogLevel.ERRORS,
        'Video Assignment channel: Failed to send request with error',
        e as Error,
      );
      throw e;
    }

    const requestPromise = new Promise<MediaApiResponseStatus>((resolve) => {
      this.pendingRequestResolveMap.set(request.requestId, resolve);
    });
    return requestPromise;
  }

  private isMediaEntryAssignedToMeetStreamTrack(
    mediaEntry: MediaEntry,
    internalMediaEntry: InternalMediaEntry,
  ): boolean {
    const videoMeetStreamTrack = mediaEntry.videoMeetStreamTrack.get();
    if (!videoMeetStreamTrack) return false;
    const internalMeetStreamTrack =
      this.internalMeetStreamTrackMap.get(videoMeetStreamTrack);

    if (internalMeetStreamTrack!.videoSsrc === internalMediaEntry.videoSsrc) {
      return true;
    } else {
      // ssrcs can change, if the video ssrc is not the same, we need to remove
      // the relationship between the media entry and the meet stream track.
      internalMediaEntry.videoMeetStreamTrack.set(undefined);
      internalMeetStreamTrack?.mediaEntry.set(undefined);
      return false;
    }
  }

  private assignVideoMeetStreamTrack(mediaEntry: MediaEntry) {
    for (const [meetStreamTrack, internalMeetStreamTrack] of this
      .internalMeetStreamTrackMap) {
      if (meetStreamTrack.mediaStreamTrack.kind === 'video') {
        internalMeetStreamTrack.maybeAssignMediaEntryOnFrame(
          mediaEntry,
          'video',
        );
      }
    }
  }
}
