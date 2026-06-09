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
 * @fileoverview Handles the session control channel.
 */

import {
  LeaveRequest,
  SessionControlChannelFromClient,
  SessionControlChannelToClient,
} from '../../types/datachannels';
import {
  LogLevel,
  MeetConnectionState,
  MeetDisconnectReason,
} from '../../types/enums';
import {MeetSessionStatus} from '../../types/meetmediaapiclient';
import {SubscribableDelegate} from '../subscribable_impl';
import {ChannelLogger} from './channel_logger';

const DISCONNECT_REASON_MAP = new Map<string, MeetDisconnectReason>([
  ['REASON_CLIENT_LEFT', MeetDisconnectReason.CLIENT_LEFT],
  ['REASON_USER_STOPPED', MeetDisconnectReason.USER_STOPPED],
  ['REASON_CONFERENCE_ENDED', MeetDisconnectReason.CONFERENCE_ENDED],
  ['REASON_SESSION_UNHEALTHY', MeetDisconnectReason.SESSION_UNHEALTHY],
]);

/**
 * Helper class to handles the session control channel.
 */
export class SessionControlChannelHandler {
  private requestId = 1;
  private leaveSessionPromise: (() => void) | undefined;

  constructor(
    private readonly channel: RTCDataChannel,
    private readonly sessionStatusDelegate: SubscribableDelegate<MeetSessionStatus>,
    private readonly channelLogger?: ChannelLogger,
  ) {
    this.channel.onmessage = (event) => {
      this.onSessionControlMessage(event);
    };
    this.channel.onopen = () => {
      this.onSessionControlOpened();
    };
    this.channel.onclose = () => {
      this.onSessionControlClosed();
    };
  }

  private onSessionControlOpened() {
    this.channelLogger?.log(
      LogLevel.MESSAGES,
      'Session control channel: opened',
    );
    this.sessionStatusDelegate.set({
      connectionState: MeetConnectionState.WAITING,
    });
  }

  private onSessionControlMessage(event: MessageEvent) {
    const message = event.data;
    const json = JSON.parse(message) as SessionControlChannelToClient;
    if (json?.response) {
      this.channelLogger?.log(
        LogLevel.MESSAGES,
        'Session control channel: response recieved',
        json.response,
      );
      this.leaveSessionPromise?.();
    }
    if (json?.resources && json.resources.length > 0) {
      const sessionStatus = json.resources[0].sessionStatus;
      this.channelLogger?.log(
        LogLevel.RESOURCES,
        'Session control channel: resource recieved',
        json.resources[0],
      );
      if (sessionStatus.connectionState === 'STATE_WAITING') {
        this.sessionStatusDelegate.set({
          connectionState: MeetConnectionState.WAITING,
        });
      } else if (sessionStatus.connectionState === 'STATE_JOINED') {
        this.sessionStatusDelegate.set({
          connectionState: MeetConnectionState.JOINED,
        });
      } else if (sessionStatus.connectionState === 'STATE_DISCONNECTED') {
        this.sessionStatusDelegate.set({
          connectionState: MeetConnectionState.DISCONNECTED,
          disconnectReason:
            DISCONNECT_REASON_MAP.get(sessionStatus.disconnectReason || '') ??
            MeetDisconnectReason.SESSION_UNHEALTHY,
        });
      }
    }
  }
  private onSessionControlClosed() {
    // If the channel is closed, we should resolve the leave session promise.
    this.channelLogger?.log(
      LogLevel.MESSAGES,
      'Session control channel: closed',
    );
    this.leaveSessionPromise?.();
    if (
      this.sessionStatusDelegate.get().connectionState !==
      MeetConnectionState.DISCONNECTED
    ) {
      this.sessionStatusDelegate.set({
        connectionState: MeetConnectionState.DISCONNECTED,
        disconnectReason: MeetDisconnectReason.UNKNOWN,
      });
    }
  }

  leaveSession(): Promise<void> {
    this.channelLogger?.log(
      LogLevel.MESSAGES,
      'Session control channel: leave session request sent',
    );
    try {
      this.channel.send(
        JSON.stringify({
          request: {
            requestId: this.requestId++,
            leave: {},
          } as LeaveRequest,
        } as SessionControlChannelFromClient),
      );
    } catch (e) {
      this.channelLogger?.log(
        LogLevel.ERRORS,
        'Session control channel: Failed to send leave request with error',
        e as Error,
      );
      throw e;
    }
    return new Promise<void>((resolve) => {
      this.leaveSessionPromise = resolve;
    });
  }
}
