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
 * @fileoverview A class to handle the media stats channel.
 */

import {
  MediaApiResponseStatus,
  MediaStatsChannelFromClient,
  MediaStatsChannelToClient,
  MediaStatsResource,
  StatsSectionData,
  UploadMediaStatsRequest,
  UploadMediaStatsResponse,
} from '../../types/datachannels';
import {LogLevel} from '../../types/enums';
import {ChannelLogger} from './channel_logger';

type SupportedMediaStatsTypes =
  | 'codec'
  | 'candidate-pair'
  | 'media-playout'
  | 'transport'
  | 'local-candidate'
  | 'remote-candidate'
  | 'inbound-rtp';

const STATS_TYPE_CONVERTER: {[key: string]: string} = {
  'codec': 'codec',
  'candidate-pair': 'candidate_pair',
  'media-playout': 'media_playout',
  'transport': 'transport',
  'local-candidate': 'local_candidate',
  'remote-candidate': 'remote_candidate',
  'inbound-rtp': 'inbound_rtp',
};

/**
 * Helper class to handle the media stats channel. This class is responsible
 * for sending media stats to the backend and receiving configuration updates
 * from the backend. For realtime metrics when debugging manually, use
 * chrome://webrtc-internals.
 */
export class MediaStatsChannelHandler {
  /**
   * A map of allowlisted sections. The key is the section type, and the value
   * is the keys that are allowlisted for that section.
   */
  private readonly allowlist = new Map<string, string[]>();
  private requestId = 1;
  private readonly pendingRequestResolveMap = new Map<
    number,
    (value: MediaApiResponseStatus) => void
  >();
  /** Id for the interval to send media stats. */
  private intervalId = 0;

  constructor(
    private readonly channel: RTCDataChannel,
    private readonly peerConnection: RTCPeerConnection,
    private readonly channelLogger?: ChannelLogger,
  ) {
    this.channel.onmessage = (event) => {
      this.onMediaStatsMessage(event);
    };
    this.channel.onclose = () => {
      clearInterval(this.intervalId);
      this.intervalId = 0;
      this.channelLogger?.log(LogLevel.MESSAGES, 'Media stats channel: closed');
      // Resolve all pending requests with an error.
      for (const [, resolve] of this.pendingRequestResolveMap) {
        resolve({code: 400, message: 'Channel closed', details: []});
      }
      this.pendingRequestResolveMap.clear();
    };
    this.channel.onopen = () => {
      this.channelLogger?.log(LogLevel.MESSAGES, 'Media stats channel: opened');
    };
  }

  private onMediaStatsMessage(message: MessageEvent) {
    const data = JSON.parse(message.data) as MediaStatsChannelToClient;
    if (data.response) {
      this.onMediaStatsResponse(data.response);
    }
    if (data.resources) {
      this.onMediaStatsResources(data.resources);
    }
  }

  private onMediaStatsResponse(response: UploadMediaStatsResponse) {
    this.channelLogger?.log(
      LogLevel.MESSAGES,
      'Media stats channel: response received',
      response,
    );
    const resolve = this.pendingRequestResolveMap.get(response.requestId);
    if (resolve) {
      resolve(response.status);
      this.pendingRequestResolveMap.delete(response.requestId);
    }
  }

  private onMediaStatsResources(resources: MediaStatsResource[]) {
    // We expect only one resource to be sent.
    if (resources.length > 1) {
      resources.forEach((resource) => {
        this.channelLogger?.log(
          LogLevel.ERRORS,
          'Media stats channel: more than one resource received',
          resource,
        );
      });
    }
    const resource = resources[0];
    this.channelLogger?.log(
      LogLevel.MESSAGES,
      'Media stats channel: resource received',
      resource,
    );
    if (resource.configuration) {
      for (const [key, value] of Object.entries(
        resource.configuration.allowlist,
      )) {
        this.allowlist.set(key, value.keys);
      }
      // We want to stop the interval if the upload interval is zero
      if (
        this.intervalId &&
        resource.configuration.uploadIntervalSeconds === 0
      ) {
        clearInterval(this.intervalId);
        this.intervalId = 0;
      }
      // We want to start the interval if the upload interval is not zero.
      if (resource.configuration.uploadIntervalSeconds) {
        // We want to reset the interval if the upload interval has changed.
        if (this.intervalId) {
          clearInterval(this.intervalId);
        }
        this.intervalId = setInterval(
          this.sendMediaStats.bind(this),
          resource.configuration.uploadIntervalSeconds * 1000,
        );
      }
    } else {
      this.channelLogger?.log(
        LogLevel.ERRORS,
        'Media stats channel: resource received without configuration',
      );
    }
  }

  async sendMediaStats(): Promise<MediaApiResponseStatus> {
    const stats: RTCStatsReport = await this.peerConnection.getStats();
    const requestStats: StatsSectionData[] = [];

    stats.forEach(
      (
        report:
          | RTCTransportStats
          | RTCIceCandidatePairStats
          | RTCOutboundRtpStreamStats
          | RTCInboundRtpStreamStats,
      ) => {
        const statsType = report.type as SupportedMediaStatsTypes;
        if (statsType && this.allowlist.has(report.type)) {
          const filteredMediaStats: {[key: string]: string | number} = {};
          Object.entries(report).forEach((entry) => {
            // id is not accepted with other stats. It is populated in the top
            // level section.
            if (
              this.allowlist.get(report.type)?.includes(entry[0]) &&
              entry[0] !== 'id'
            ) {
              // We want to convert the camel case to underscore.
              filteredMediaStats[this.camelToUnderscore(entry[0])] = entry[1];
            }
          });
          const filteredMediaStatsDictionary = {
            'id': report.id,
            [STATS_TYPE_CONVERTER[report.type as string]]: filteredMediaStats,
          };
          const filteredStatsSectionData =
            filteredMediaStatsDictionary as StatsSectionData;

          requestStats.push(filteredStatsSectionData);
        }
      },
    );

    if (!requestStats.length) {
      this.channelLogger?.log(
        LogLevel.ERRORS,
        'Media stats channel: no media stats to send',
      );
      return {code: 400, message: 'No media stats to send', details: []};
    }

    if (this.channel.readyState === 'open') {
      const mediaStatsRequest: UploadMediaStatsRequest = {
        requestId: this.requestId,
        uploadMediaStats: {sections: requestStats},
      };

      const request: MediaStatsChannelFromClient = {
        request: mediaStatsRequest,
      };
      this.channelLogger?.log(
        LogLevel.MESSAGES,
        'Media stats channel: sending request',
        mediaStatsRequest,
      );
      try {
        this.channel.send(JSON.stringify(request));
      } catch (e) {
        this.channelLogger?.log(
          LogLevel.ERRORS,
          'Media stats channel: Failed to send request with error',
          e as Error,
        );
        throw e;
      }

      this.requestId++;
      const requestPromise = new Promise<MediaApiResponseStatus>((resolve) => {
        this.pendingRequestResolveMap.set(mediaStatsRequest.requestId, resolve);
      });
      return requestPromise;
    } else {
      clearInterval(this.intervalId);
      this.intervalId = 0;
      this.channelLogger?.log(
        LogLevel.ERRORS,
        'Media stats channel: handler tried to send message when channel was closed',
      );
      return {code: 400, message: 'Channel is not open', details: []};
    }
  }

  private camelToUnderscore(text: string): string {
    return text.replace(/([A-Z])/g, '_$1').toLowerCase();
  }
}
