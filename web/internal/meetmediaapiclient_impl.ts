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

import {
  MediaApiCommunicationProtocol,
  MediaApiCommunicationResponse,
} from '../types/communication_protocol';
import {MediaApiResponseStatus} from '../types/datachannels';
import {MeetConnectionState} from '../types/enums';
import {
  CanvasDimensions,
  MediaEntry,
  MediaLayout,
  MediaLayoutRequest,
  MeetMediaClientRequiredConfiguration,
  MeetStreamTrack,
  Participant,
} from '../types/mediatypes';
import {
  MeetMediaApiClient,
  MeetSessionStatus,
} from '../types/meetmediaapiclient';
import {Subscribable} from '../types/subscribable';
import {ChannelLogger} from './channel_handlers/channel_logger';
import {MediaEntriesChannelHandler} from './channel_handlers/media_entries_channel_handler';
import {MediaStatsChannelHandler} from './channel_handlers/media_stats_channel_handler';
import {ParticipantsChannelHandler} from './channel_handlers/participants_channel_handler';
import {SessionControlChannelHandler} from './channel_handlers/session_control_channel_handler';
import {VideoAssignmentChannelHandler} from './channel_handlers/video_assignment_channel_handler';
import {DefaultCommunicationProtocolImpl} from './communication_protocols/default_communication_protocol_impl';
import {InternalMeetStreamTrackImpl} from './internal_meet_stream_track_impl';
import {
  InternalMediaEntry,
  InternalMediaLayout,
  InternalMeetStreamTrack,
  InternalParticipant,
} from './internal_types';
import {MeetStreamTrackImpl} from './meet_stream_track_impl';
import {SubscribableDelegate, SubscribableImpl} from './subscribable_impl';

// Meet only supports 3 audio virtual ssrcs. If disabled, there will be no
// audio.
const NUMBER_OF_AUDIO_VIRTUAL_SSRC = 3;

const MINIMUM_VIDEO_STREAMS = 0;
const MAXIMUM_VIDEO_STREAMS = 3;

/**
 * Implementation of MeetMediaApiClient.
 */
export class MeetMediaApiClientImpl implements MeetMediaApiClient {
  // Public properties
  readonly sessionStatus: Subscribable<MeetSessionStatus>;
  readonly meetStreamTracks: Subscribable<MeetStreamTrack[]>;
  readonly mediaEntries: Subscribable<MediaEntry[]>;
  readonly participants: Subscribable<Participant[]>;
  readonly presenter: Subscribable<MediaEntry | undefined>;
  readonly screenshare: Subscribable<MediaEntry | undefined>;

  // Private properties
  private readonly sessionStatusDelegate: SubscribableDelegate<MeetSessionStatus>;
  private readonly meetStreamTracksDelegate: SubscribableDelegate<
    MeetStreamTrack[]
  >;
  private readonly mediaEntriesDelegate: SubscribableDelegate<MediaEntry[]>;
  private readonly participantsDelegate: SubscribableDelegate<Participant[]>;
  private readonly presenterDelegate: SubscribableDelegate<
    MediaEntry | undefined
  >;
  private readonly screenshareDelegate: SubscribableDelegate<
    MediaEntry | undefined
  >;

  private readonly peerConnection: RTCPeerConnection;

  private sessionControlChannel: RTCDataChannel | undefined;
  private sessionControlChannelHandler:
    | SessionControlChannelHandler
    | undefined;

  private videoAssignmentChannel: RTCDataChannel | undefined;
  private videoAssignmentChannelHandler:
    | VideoAssignmentChannelHandler
    | undefined;

  private mediaEntriesChannel: RTCDataChannel | undefined;
  private mediaStatsChannel: RTCDataChannel | undefined;
  private participantsChannel: RTCDataChannel | undefined;

  /* tslint:disable:no-unused-variable */
  // This is unused because it is receive only.
  // @ts-ignore
  private mediaEntriesChannelHandler: MediaEntriesChannelHandler | undefined;

  // @ts-ignore
  private mediaStatsChannelHandler: MediaStatsChannelHandler | undefined;

  // @ts-ignore
  private participantsChannelHandler: ParticipantsChannelHandler | undefined;
  /* tslint:enable:no-unused-variable */

  private mediaLayoutId = 1;

  // Media layout retrieval by id. Needed by the video assignment channel handler
  // to update the media layout.
  private readonly idMediaLayoutMap = new Map<number, MediaLayout>();

  // Used to update media layouts.
  private readonly internalMediaLayoutMap = new Map<
    MediaLayout,
    InternalMediaLayout
  >();

  // Media entry retrieval by id. Needed by the video assignment channel handler
  // to update the media entry.
  private readonly idMediaEntryMap = new Map<number, MediaEntry>();

  // Used to update media entries.
  private readonly internalMediaEntryMap = new Map<
    MediaEntry,
    InternalMediaEntry
  >();

  // Used to update meet stream tracks.
  private readonly internalMeetStreamTrackMap = new Map<
    MeetStreamTrack,
    InternalMeetStreamTrack
  >();

  private readonly idParticipantMap = new Map<number, Participant>();
  private readonly nameParticipantMap = new Map<string, Participant>();
  private readonly internalParticipantMap = new Map<
    Participant,
    InternalParticipant
  >();

  constructor(
    private readonly requiredConfiguration: MeetMediaClientRequiredConfiguration,
  ) {
    this.validateConfiguration();

    this.sessionStatusDelegate = new SubscribableDelegate<MeetSessionStatus>({
      connectionState: MeetConnectionState.UNKNOWN,
    });
    this.sessionStatus = this.sessionStatusDelegate.getSubscribable();
    this.meetStreamTracksDelegate = new SubscribableDelegate<MeetStreamTrack[]>(
      [],
    );
    this.meetStreamTracks = this.meetStreamTracksDelegate.getSubscribable();
    this.mediaEntriesDelegate = new SubscribableDelegate<MediaEntry[]>([]);
    this.mediaEntries = this.mediaEntriesDelegate.getSubscribable();
    this.participantsDelegate = new SubscribableDelegate<Participant[]>([]);
    this.participants = this.participantsDelegate.getSubscribable();
    this.presenterDelegate = new SubscribableDelegate<MediaEntry | undefined>(
      undefined,
    );
    this.presenter = this.presenterDelegate.getSubscribable();
    this.screenshareDelegate = new SubscribableDelegate<MediaEntry | undefined>(
      undefined,
    );
    this.screenshare = this.screenshareDelegate.getSubscribable();

    const configuration = {
      sdpSemantics: 'unified-plan',
      bundlePolicy: 'max-bundle' as RTCBundlePolicy,
      iceServers: [{urls: 'stun:stun.l.google.com:19302'}],
    };

    // Create peer connection
    this.peerConnection = new RTCPeerConnection(configuration);
    this.peerConnection.ontrack = (e) => {
      if (e.track) {
        this.createMeetStreamTrack(e.track, e.receiver);
      }
    };
  }

  private validateConfiguration(): void {
    if (
      this.requiredConfiguration.numberOfVideoStreams < MINIMUM_VIDEO_STREAMS ||
      this.requiredConfiguration.numberOfVideoStreams > MAXIMUM_VIDEO_STREAMS
    ) {
      throw new Error(
        `Unsupported number of video streams, must be between ${MINIMUM_VIDEO_STREAMS} and ${MAXIMUM_VIDEO_STREAMS}`,
      );
    }
  }

  private createMeetStreamTrack(
    mediaStreamTrack: MediaStreamTrack,
    receiver: RTCRtpReceiver,
  ): void {
    const meetStreamTracks = this.meetStreamTracks.get();
    const mediaEntryDelegate = new SubscribableDelegate<MediaEntry | undefined>(
      undefined,
    );
    const meetStreamTrack = new MeetStreamTrackImpl(
      mediaStreamTrack,
      mediaEntryDelegate,
    );

    const internalMeetStreamTrack = new InternalMeetStreamTrackImpl(
      receiver,
      mediaEntryDelegate,
      meetStreamTrack,
      this.internalMediaEntryMap,
    );

    const newStreamTrackArray = [...meetStreamTracks, meetStreamTrack];
    this.internalMeetStreamTrackMap.set(
      meetStreamTrack,
      internalMeetStreamTrack,
    );
    this.meetStreamTracksDelegate.set(newStreamTrackArray);
  }

  async joinMeeting(
    communicationProtocol?: MediaApiCommunicationProtocol,
  ): Promise<void> {
    // The offer must be in the order of audio, datachannels, video.

    // Create audio transceivers based on initial config.
    if (this.requiredConfiguration.enableAudioStreams) {
      for (let i = 0; i < NUMBER_OF_AUDIO_VIRTUAL_SSRC; i++) {
        // Integrating clients must support and negotiate the OPUS codec in
        // the SDP offer.
        // This is the default for WebRTC.
        // https://developer.mozilla.org/en-US/docs/Web/Media/Formats/WebRTC_codecs.
        this.peerConnection.addTransceiver('audio', {direction: 'recvonly'});
      }
    }

    // ---- UTILITY DATA CHANNELS -----

    // All data channels must be reliable and ordered.
    const dataChannelConfig = {
      ordered: true,
      reliable: true,
    };

    // Always create the session and media stats control channel.
    this.sessionControlChannel = this.peerConnection.createDataChannel(
      'session-control',
      dataChannelConfig,
    );
    let sessionControlchannelLogger;
    if (this.requiredConfiguration?.logsCallback) {
      sessionControlchannelLogger = new ChannelLogger(
        'session-control',
        this.requiredConfiguration.logsCallback,
      );
    }
    this.sessionControlChannelHandler = new SessionControlChannelHandler(
      this.sessionControlChannel,
      this.sessionStatusDelegate,
      sessionControlchannelLogger,
    );

    this.mediaStatsChannel = this.peerConnection.createDataChannel(
      'media-stats',
      dataChannelConfig,
    );
    let mediaStatsChannelLogger;
    if (this.requiredConfiguration?.logsCallback) {
      mediaStatsChannelLogger = new ChannelLogger(
        'media-stats',
        this.requiredConfiguration.logsCallback,
      );
    }
    this.mediaStatsChannelHandler = new MediaStatsChannelHandler(
      this.mediaStatsChannel,
      this.peerConnection,
      mediaStatsChannelLogger,
    );

    // ---- CONDITIONAL DATA CHANNELS -----

    // We only need the video assignment channel if we are requesting video.
    if (this.requiredConfiguration.numberOfVideoStreams > 0) {
      this.videoAssignmentChannel = this.peerConnection.createDataChannel(
        'video-assignment',
        dataChannelConfig,
      );
      let videoAssignmentChannelLogger;
      if (this.requiredConfiguration?.logsCallback) {
        videoAssignmentChannelLogger = new ChannelLogger(
          'video-assignment',
          this.requiredConfiguration.logsCallback,
        );
      }
      this.videoAssignmentChannelHandler = new VideoAssignmentChannelHandler(
        this.videoAssignmentChannel,
        this.idMediaEntryMap,
        this.internalMediaEntryMap,
        this.idMediaLayoutMap,
        this.internalMediaLayoutMap,
        this.mediaEntriesDelegate,
        this.internalMeetStreamTrackMap,
        videoAssignmentChannelLogger,
      );
    }

    if (
      this.requiredConfiguration.numberOfVideoStreams > 0 ||
      this.requiredConfiguration.enableAudioStreams
    ) {
      this.mediaEntriesChannel = this.peerConnection.createDataChannel(
        'media-entries',
        dataChannelConfig,
      );
      let mediaEntriesChannelLogger;
      if (this.requiredConfiguration?.logsCallback) {
        mediaEntriesChannelLogger = new ChannelLogger(
          'media-entries',
          this.requiredConfiguration.logsCallback,
        );
      }
      this.mediaEntriesChannelHandler = new MediaEntriesChannelHandler(
        this.mediaEntriesChannel,
        this.mediaEntriesDelegate,
        this.idMediaEntryMap,
        this.internalMediaEntryMap,
        this.internalMeetStreamTrackMap,
        this.internalMediaLayoutMap,
        this.participantsDelegate,
        this.nameParticipantMap,
        this.idParticipantMap,
        this.internalParticipantMap,
        this.presenterDelegate,
        this.screenshareDelegate,
        mediaEntriesChannelLogger,
      );

      this.participantsChannel =
        this.peerConnection.createDataChannel('participants');
      let participantsChannelLogger;
      if (this.requiredConfiguration?.logsCallback) {
        participantsChannelLogger = new ChannelLogger(
          'participants',
          this.requiredConfiguration.logsCallback,
        );
      }

      this.participantsChannelHandler = new ParticipantsChannelHandler(
        this.participantsChannel,
        this.participantsDelegate,
        this.idParticipantMap,
        this.nameParticipantMap,
        this.internalParticipantMap,
        this.internalMediaEntryMap,
        participantsChannelLogger,
      );
    }

    this.sessionStatusDelegate.subscribe((status) => {
      if (status.connectionState === MeetConnectionState.DISCONNECTED) {
        this.mediaStatsChannel?.close();
        this.videoAssignmentChannel?.close();
        this.mediaEntriesChannel?.close();
      }
    });

    // Local description has to be set before adding video transceivers to
    // preserve the order of audio, datachannels, video.
    let pcOffer = await this.peerConnection.createOffer();
    await this.peerConnection.setLocalDescription(pcOffer);

    for (let i = 0; i < this.requiredConfiguration.numberOfVideoStreams; i++) {
      // Integrating clients must support and negotiate AV1, VP9, and VP8 codecs
      // in the SDP offer.
      // The default for WebRTC is VP8.
      // https://developer.mozilla.org/en-US/docs/Web/Media/Formats/WebRTC_codecs.
      this.peerConnection.addTransceiver('video', {direction: 'recvonly'});
    }

    pcOffer = await this.peerConnection.createOffer();
    await this.peerConnection.setLocalDescription(pcOffer);
    let response: MediaApiCommunicationResponse;
    try {
      const protocol: MediaApiCommunicationProtocol =
        communicationProtocol ??
        new DefaultCommunicationProtocolImpl(this.requiredConfiguration);
      response = await protocol.connectActiveConference(pcOffer.sdp ?? '');
    } catch (e) {
      throw new Error(
        'Internal error, call to connectActiveConference failed, Exception: ' +
          (e as Error).name +
          ' ' +
          (e as Error).message,
      );
    }
    if (response?.answer) {
      await this.peerConnection.setRemoteDescription({
        type: 'answer',
        sdp: response?.answer,
      });
    } else {
      // We do not expect this to happen and therefore it is an internal
      // error.
      throw new Error('Internal error, no answer in response');
    }
    return;
  }

  leaveMeeting(): Promise<void> {
    if (this.sessionControlChannelHandler) {
      return this.sessionControlChannelHandler?.leaveSession();
    } else {
      throw new Error('You must connect to a meeting before leaving it');
    }
  }

  // The promise resolving on the request does not mean the layout has been
  // applied. It means that the request has been accepted and you may need to
  // wait a short amount of time for these layouts to be applied.
  applyLayout(requests: MediaLayoutRequest[]): Promise<MediaApiResponseStatus> {
    if (!this.videoAssignmentChannelHandler) {
      throw new Error(
        'You must connect to a meeting with video before applying a layout',
      );
    }
    requests.forEach((request) => {
      if (!request.mediaLayout) {
        throw new Error('The request must include a media layout');
      }
      if (!this.internalMediaLayoutMap.has(request.mediaLayout)) {
        throw new Error(
          'The media layout must be created using the client before it can be applied',
        );
      }
    });
    return this.videoAssignmentChannelHandler.sendRequests(requests);
  }

  createMediaLayout(canvasDimensions: CanvasDimensions): MediaLayout {
    const mediaEntryDelegate = new SubscribableDelegate<MediaEntry | undefined>(
      undefined,
    );
    const mediaEntry = new SubscribableImpl<MediaEntry | undefined>(
      mediaEntryDelegate,
    );
    const mediaLayout: MediaLayout = {canvasDimensions, mediaEntry};
    this.internalMediaLayoutMap.set(mediaLayout, {
      id: this.mediaLayoutId,
      mediaEntry: mediaEntryDelegate,
    });
    this.idMediaLayoutMap.set(this.mediaLayoutId, mediaLayout);
    this.mediaLayoutId++;
    return mediaLayout;
  }
}
