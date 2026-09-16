import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { meet } from '@googleworkspace/meet-addons';
import { MeetMediaApiClientImpl } from './internal/meetmediaapiclient_impl';
import { MeetConnectionState } from './types/enums';
import { GoogleGenAI, Modality, Session } from '@google/genai';

const CLOUD_PROJECT_NUMBER = process.env.CLOUD_PROJECT_NUMBER;
const CLIENT_ID = process.env.CLIENT_ID;

@customElement('gdm-live-audio')
export class GdmLiveAudio extends LitElement {
  @state() connected = false;
  @state() connecting = false;
  @state() initialized = false;
  @state() error = '';
  @state() volume = 0;
  @state() transcript = '';
  @state() outputTranscript = '';
  @state() sceneDescription = '';

  private meetClient: MeetMediaApiClientImpl | null = null;
  private isAddonInitialized = false;
  private accessToken = '';
  private activeTrackIds = new Set<string>();
  
  private audioContext: AudioContext | null = null;
  private outputAudioContext: AudioContext | null = null;
  private analyser: AnalyserNode | null = null;
  private dataArray: Uint8Array | null = null;
  private animationFrameId: number | null = null;
  
  private ai: GoogleGenAI | null = null;
  private session: Session | null = null;
  private workletNode: AudioWorkletNode | null = null;

  private accumulatedInputData: Float32Array | null = null;
  
  private accumulatedResponseChunks: Uint8Array[] = [];
  private responseTranscriptionTimer: number | null = null;
  
  private nextStartTime = 0;
  private sources = new Set<AudioBufferSourceNode>();

  private videoEl: HTMLVideoElement | null = null;
  private canvasEl: HTMLCanvasElement | null = null;
  private videoIntervalId: number | null = null;

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: flex-start;
      height: 100%;
      box-sizing: border-box;
      font-family: sans-serif;
      background: #121212;
      color: white;
      padding: 10px;
    }
    button {
      padding: 15px 30px;
      font-size: 18px;
      cursor: pointer;
      background-color: #007bff;
      color: white;
      border: none;
      border-radius: 5px;
      transition: background-color 0.3s;
    }
    button:hover {
      background-color: #0056b3;
    }
    button[disabled] {
      background-color: #555;
      cursor: not-allowed;
    }
    .message {
      font-size: 20px;
      color: #4caf50;
    }
    .error {
      color: #f44336;
      margin-top: 10px;
    }
    .volume-bar {
      width: 200px;
      height: 20px;
      background-color: #333;
      border-radius: 10px;
      overflow: hidden;
      margin-top: 20px;
    }
    .volume-level {
      height: 100%;
      background-color: #4caf50;
      transition: width 0.1s ease;
    }
    .transcript-area {
      width: 95%;
      flex-grow: 1;
      margin-top: 10px;
      background-color: #222;
      color: #ccc;
      border: 1px solid #444;
      border-radius: 5px;
      padding: 10px;
      font-family: monospace;
      resize: none;
    }
    .label {
      align-self: flex-start;
      margin-left: 5%;
      margin-top: 15px;
      font-weight: bold;
      color: #aaa;
    }
    .hidden-video {
      display: none;
    }
  `;

  constructor() {
    super();
    this.unloadHandler = this.unloadHandler.bind(this);
  }

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('unload', this.unloadHandler);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('unload', this.unloadHandler);
    this.disconnect();
  }

  private unloadHandler() {
    this.disconnect();
  }

  firstUpdated() {
    this.initializeSession();
  }

  private initializeSession() {
    const google = (window as any).google;
    if (!google) {
      this.error = "Google Identity Services not loaded";
      return;
    }

    const client = google.accounts.oauth2.initTokenClient({
      client_id: CLIENT_ID,
      scope: 'https://www.googleapis.com/auth/meetings.space.created https://www.googleapis.com/auth/meetings.conference.media.readonly https://www.googleapis.com/auth/meetings.space.readonly',
      callback: async (tokenResponse: any) => {
        this.accessToken = tokenResponse.access_token;
        await this.initializeAddon();
        const meetingId = (window as any).meetingId;
        if (!meetingId) {
          this.error = "Meeting ID not found";
          return;
        }
        this.initialized = true;
      },
      error_callback: (errorResponse: any) => {
        this.error = "Authentication failed";
      },
    });

    client.requestAccessToken();
  }

  private async initializeAddon() {
    if (this.isAddonInitialized) return;
    const session = await meet.addon.createAddonSession({
      cloudProjectNumber: CLOUD_PROJECT_NUMBER,
    });
    const sidePanelClient = await session.createSidePanelClient();
    const meetingInfo = await sidePanelClient.getMeetingInfo();
    (window as any).meetingId = meetingInfo.meetingId;
    this.isAddonInitialized = true;
  }

  private async connect() {
    if (!this.initialized || !this.accessToken) return;

    this.connecting = true;
    this.error = '';
    const meetingId = (window as any).meetingId;

    try {
      // Initialize AudioContexts. Gemini expects 16kHz input and returns 24kHz output.
      this.audioContext = new AudioContext({ sampleRate: 16000 });
      this.outputAudioContext = new AudioContext({ sampleRate: 24000 });
      this.analyser = this.audioContext.createAnalyser();
      this.analyser.fftSize = 256;
      this.dataArray = new Uint8Array(this.analyser.frequencyBinCount);
      
      this.nextStartTime = this.outputAudioContext.currentTime;

      // Load the AudioWorklet that captures raw PCM audio data.
      await this.audioContext.audioWorklet.addModule('/pcm-recorder-processor.js');
      this.workletNode = new AudioWorkletNode(this.audioContext, 'pcm-recorder-processor');

      this.workletNode.port.onmessage = (e) => {
        const inputData = e.data; // Float32Array
        
        // Accumulate for transcription (approx 5 seconds at 16kHz = 80000 samples)
        if (!this.accumulatedInputData) {
            this.accumulatedInputData = inputData;
        } else {
            const newArray = new Float32Array(this.accumulatedInputData.length + inputData.length);
            newArray.set(this.accumulatedInputData);
            newArray.set(inputData, this.accumulatedInputData.length);
            this.accumulatedInputData = newArray;
        }

        if (this.accumulatedInputData.length >= 80000) {
            const dataToTranscribe = this.accumulatedInputData;
            this.accumulatedInputData = null; // Reset buffer
            this.transcribeInputAudio(dataToTranscribe);
        }

        const pcmBuffer = this.floatTo16BitPCM(inputData);
        const base64Data = this.arrayBufferToBase64(pcmBuffer);
        
        if (this.session) {
          try {
            this.session.sendRealtimeInput({
              audio: {
                mimeType: "audio/pcm;rate=16000",
                data: base64Data
              }
            });
            if (Math.random() < 0.01) {
              console.log("Sent audio chunk to Gemini");
            }
          } catch (err) {
            console.error("Error sending audio to Gemini:", err);
          }
        }
      };

      // Initialize Gemini Live session.
      this.ai = new GoogleGenAI({ apiKey: process.env.GEMINI_API_KEY });
      const model = 'gemini-3.1-flash-live-preview'; // Use the live preview model

      this.session = await this.ai.live.connect({
        model: model,
        config: {
          responseModalities: [Modality.AUDIO],
        },
        callbacks: {
          onopen: () => {
            console.log("Gemini Live: Session opened.");
          },
          onmessage: (message) => {
            const parts = message.serverContent?.modelTurn?.parts;
            if (parts) {
              for (const part of parts) {
                if (part.inlineData) {
                  const audio = part.inlineData;
                  const pcmBytes = this.base64ToUint8Array(audio.data);

                  this.accumulatedResponseChunks.push(pcmBytes);
                  
                  if (this.responseTranscriptionTimer) {
                    clearTimeout(this.responseTranscriptionTimer);
                  }
                  this.responseTranscriptionTimer = window.setTimeout(() => {
                    this.transcribeResponseAudio();
                  }, 1000);

                  this.nextStartTime = Math.max(
                    this.nextStartTime,
                    this.outputAudioContext!.currentTime,
                  );

                  const audioBuffer = this.outputAudioContext!.createBuffer(1, pcmBytes.length / 2, 24000);
                  const channelData = audioBuffer.getChannelData(0);
                  const view = new DataView(pcmBytes.buffer);
                  for (let i = 0; i < channelData.length; i++) {
                    channelData[i] = view.getInt16(i * 2, true) / 0x7FFF;
                  }

                  const source = this.outputAudioContext!.createBufferSource();
                  source.buffer = audioBuffer;
                  source.connect(this.outputAudioContext!.destination);
                  source.addEventListener('ended', () => {
                    this.sources.delete(source);
                  });

                  source.start(this.nextStartTime);
                  this.nextStartTime = this.nextStartTime + audioBuffer.duration;
                  this.sources.add(source);
                }
              }
            }
            if (Math.random() < 0.01) {
              console.log("Received message from Gemini:", message);
            }
          },
          onerror: (e) => {
            console.error("Gemini Live error:", e);
          },
          onclose: (e) => {
            console.log("Gemini Live closed:", e.reason);
            this.session = null;
          }
        }
      });

      // Initialize the Meet Media API client.
      this.meetClient = new MeetMediaApiClientImpl({
        meetingSpaceId: meetingId,
        numberOfVideoStreams: 1,
        enableAudioStreams: true,
        accessToken: this.accessToken,
        logsCallback: (event) => console.log(`Meet Media API [${event.sourceType}]:`, event.logString),
      });

      this.meetClient.sessionStatus.subscribe((status) => {
        if (status.connectionState === MeetConnectionState.JOINED) {
          this.connected = true;
          this.connecting = false;
          this.startVolumeAnalysis();
          
          console.log("Applying layout to receive video...");
          const mediaLayout = this.meetClient!.createMediaLayout({ width: 768, height: 768 });
          this.meetClient!.applyLayout([{ mediaLayout }]).catch(e => console.error("Error applying layout:", e));
        }
      });

      this.meetClient.meetStreamTracks.subscribe((tracks) => {
        tracks.forEach((meetTrack) => {
          const track = meetTrack.mediaStreamTrack;
          if (track.kind === 'audio' && !this.activeTrackIds.has(track.id)) {
            console.log("Connecting audio track:", track.id);

            const audioEl = document.createElement('audio');
            audioEl.muted = true;
            audioEl.srcObject = new MediaStream([track]);
            audioEl.play().catch(e => console.error("Error playing wakeup audio:", e));
            (this as any)[`wakeupAudio_${track.id}`] = audioEl;

            const source = this.audioContext!.createMediaStreamSource(new MediaStream([track]));
            source.connect(this.analyser!);
            source.connect(this.workletNode!);
            this.activeTrackIds.add(track.id);
          } else if (track.kind === 'video') {
            console.log("Connecting video track:", track.id);
            this.videoEl = document.createElement('video');
            this.videoEl.srcObject = new MediaStream([track]);
            this.videoEl.muted = true;
            this.videoEl.setAttribute('playsinline', 'true');
            
            // Make it invisible but in the DOM
            this.videoEl.style.position = 'absolute';
            this.videoEl.style.width = '0';
            this.videoEl.style.height = '0';
            this.videoEl.style.opacity = '0';
            this.videoEl.style.pointerEvents = 'none';
            
            this.shadowRoot!.appendChild(this.videoEl);
            
            this.videoEl.play().catch(e => console.error("Error playing video:", e));

            this.canvasEl = document.createElement('canvas');
            this.canvasEl.width = 768;
            this.canvasEl.height = 768;

            this.startVideoProcessing();
          }
        });
      });

      await this.meetClient.joinMeeting();
    } catch (e: any) {
      this.error = `Failed to connect: ${e.message || e}`;
      this.connecting = false;
    }
  }

  private startVideoProcessing() {
    this.videoIntervalId = window.setInterval(() => {
      this.captureAndProcessFrame();
    }, 5000); // Every 5 seconds
  }

  private async captureAndProcessFrame() {
    if (!this.videoEl || !this.canvasEl || !this.session) return;

    const ctx = this.canvasEl.getContext('2d');
    if (!ctx) return;

    // Draw the video frame to the canvas (resizing to 768x768)
    ctx.drawImage(this.videoEl, 0, 0, this.canvasEl.width, this.canvasEl.height);

    // Get base64 JPEG
    const base64Data = this.canvasEl.toDataURL('image/jpeg', 0.8).split(',')[1];

    // Send to Gemini Live
    try {
      this.session.sendRealtimeInput({
        video: {
          mimeType: "image/jpeg",
          data: base64Data
        }
      });
      console.log("Sent video frame to Gemini Live");
    } catch (e) {
      console.error("Error sending video to Gemini Live:", e);
    }

    // Send to Description Model
    if (!this.ai) return;
    try {
      const response = await this.ai.models.generateContent({
        model: 'gemini-2.5-flash',
        contents: [
          {
            inlineData: {
              mimeType: 'image/jpeg',
              data: base64Data
            }
          },
          "Describe what is seen in this image in one or two sentences."
        ]
      });
      this.sceneDescription = response.text || 'No description available';
      console.log("Updated scene description.");
    } catch (e) {
      console.error("Failed to get scene description:", e);
    }
  }

  private startVolumeAnalysis() {
    const updateVolume = () => {
      if (this.analyser && this.dataArray) {
        this.analyser.getByteFrequencyData(this.dataArray as any);
        let sum = 0;
        for (let i = 0; i < this.dataArray.length; i++) {
          sum += this.dataArray[i];
        }
        const average = sum / this.dataArray.length;
        this.volume = average;
        this.animationFrameId = requestAnimationFrame(updateVolume);
      }
    };
    updateVolume();
  }

  private async transcribeInputAudio(float32Array: Float32Array) {
    console.log("Transcribing accumulated input audio...");
    try {
      const pcmBuffer = this.floatTo16BitPCM(float32Array);
      const wavBytes = this.addWavHeader(new Uint8Array(pcmBuffer), 16000);
      const base64Wav = this.arrayBufferToBase64(wavBytes.buffer);
      
      if (!this.ai) return;

      const response = await this.ai.models.generateContent({
        model: 'gemini-2.5-flash',
        contents: [
          {
            inlineData: {
              mimeType: 'audio/wav',
              data: base64Wav
            }
          },
          "Please provide a transcript of this audio. If it is only noise, say 'Noise'."
        ]
      });

      const text = response.text || '';
      this.transcript = (text.trim().toLowerCase() === 'noise' || text.trim().toLowerCase() === 'noise.') ? '' : text;
    } catch (e) {
      console.error("Failed to transcribe input audio:", e);
      this.transcript = 'Failed to transcribe audio.';
    }
  }

  private concatenateUint8Arrays(arrays: Uint8Array[]): Uint8Array {
    let totalLength = 0;
    for (const arr of arrays) {
      totalLength += arr.length;
    }
    const result = new Uint8Array(totalLength);
    let offset = 0;
    for (const arr of arrays) {
      result.set(arr, offset);
      offset += arr.length;
    }
    return result;
  }

  private async transcribeResponseAudio() {
    if (this.accumulatedResponseChunks.length === 0) return;
    
    const chunks = this.accumulatedResponseChunks;
    this.accumulatedResponseChunks = [];
    this.responseTranscriptionTimer = null;
    
    console.log("Transcribing accumulated response audio...");
    try {
      const pcmBytes = this.concatenateUint8Arrays(chunks);
      const wavBytes = this.addWavHeader(pcmBytes, 24000);
      const base64Wav = this.arrayBufferToBase64(wavBytes.buffer);
      
      if (!this.ai) return;

      const response = await this.ai.models.generateContent({
        model: 'gemini-2.5-flash',
        contents: [
          {
            inlineData: {
              mimeType: 'audio/wav',
              data: base64Wav
            }
          },
          "Please provide a transcript of this audio. If you cannot hear anything, say 'Silence'."
        ]
      });

      const text = response.text || '';
      this.outputTranscript = (text.trim().toLowerCase() === 'silence' || text.trim().toLowerCase() === 'silence.') ? '' : text;
    } catch (e) {
      console.error("Failed to transcribe response audio:", e);
      this.outputTranscript = 'Failed to transcribe response.';
    }
  }

  private addWavHeader(pcmData: Uint8Array, sampleRate: number): Uint8Array {
    const header = new ArrayBuffer(44);
    const view = new DataView(header);

    const writeString = (offset: number, string: string) => {
      for (let i = 0; i < string.length; i++) {
        view.setUint8(offset + i, string.charCodeAt(i));
      }
    };

    writeString(0, 'RIFF');
    view.setUint32(4, 36 + pcmData.length, true);
    writeString(8, 'WAVE');
    writeString(12, 'fmt ');
    view.setUint32(16, 16, true);
    view.setUint16(20, 1, true);
    view.setUint16(22, 1, true);
    view.setUint32(24, sampleRate, true);
    view.setUint32(28, sampleRate * 2, true);
    view.setUint16(32, 2, true);
    view.setUint16(34, 16, true);
    writeString(36, 'data');
    view.setUint32(40, pcmData.length, true);

    const wav = new Uint8Array(44 + pcmData.length);
    wav.set(new Uint8Array(header), 0);
    wav.set(pcmData, 44);

    return wav;
  }

  private async disconnect() {
    if (this.animationFrameId) {
      cancelAnimationFrame(this.animationFrameId);
      this.animationFrameId = null;
    }
    if (this.videoIntervalId) {
      clearInterval(this.videoIntervalId);
      this.videoIntervalId = null;
    }
    if (this.audioContext) {
      await this.audioContext.close();
      this.audioContext = null;
    }
    if (this.outputAudioContext) {
      await this.outputAudioContext.close();
      this.outputAudioContext = null;
    }
    if (this.session) {
      this.session.close();
      this.session = null;
    }
    if (this.meetClient) {
      try {
        await this.meetClient.leaveMeeting();
      } catch (e) {
        console.error("Error leaving meeting:", e);
      }
      this.meetClient = null;
    }

    this.activeTrackIds.forEach(id => {
      const audioEl = (this as any)[`wakeupAudio_${id}`];
      if (audioEl) {
        audioEl.srcObject = null;
        delete (this as any)[`wakeupAudio_${id}`];
      }
    });
    this.activeTrackIds.clear();

    if (this.videoEl) {
      this.videoEl.srcObject = null;
      this.videoEl.remove();
      this.videoEl = null;
    }
    this.canvasEl = null;

    this.connected = false;
    this.connecting = false;
    this.volume = 0;
    this.transcript = '';
    this.outputTranscript = '';
    this.sceneDescription = '';
    this.accumulatedResponseChunks = [];

    this.sources.forEach(source => source.stop());
    this.sources.clear();
    this.nextStartTime = 0;
  }

  private floatTo16BitPCM(float32Array: Float32Array): ArrayBuffer {
    const buffer = new ArrayBuffer(float32Array.length * 2);
    const view = new DataView(buffer);
    let offset = 0;
    for (let i = 0; i < float32Array.length; i++, offset += 2) {
      let s = Math.max(-1, Math.min(1, float32Array[i]));
      view.setInt16(offset, s < 0 ? s * 0x8000 : s * 0x7FFF, true);
    }
    return buffer;
  }

  private arrayBufferToBase64(buffer: ArrayBufferLike): string {
    let binary = '';
    const bytes = new Uint8Array(buffer);
    const len = bytes.byteLength;
    for (let i = 0; i < len; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    return btoa(binary);
  }

  private base64ToUint8Array(base64: string): Uint8Array {
    const binaryString = atob(base64);
    const len = binaryString.length;
    const bytes = new Uint8Array(len);
    for (let i = 0; i < len; i++) {
      bytes[i] = binaryString.charCodeAt(i);
    }
    return bytes;
  }

  render() {
    const volumePercentage = (this.volume / 255) * 100;
    return html`
      ${!this.initialized ? html`<div>Initializing...</div>` : ''}

      ${this.initialized && !this.connected && !this.connecting ? html`
        <button @click=${this.connect}>Connect to Meet Media API</button>
      ` : ''}
      
      ${this.connecting ? html`<div>Connecting...</div>` : ''}
      
      ${this.connected ? html`
        <div class="message">Connected successfully!</div>
        <div>Volume: ${Math.round(volumePercentage)}%</div>
        <div class="volume-bar">
          <div class="volume-level" style="width: ${volumePercentage}%"></div>
        </div>
        
        <div class="label">Input Transcript:</div>
        <textarea class="transcript-area" .value=${this.transcript} readonly placeholder="Input transcription will appear here..."></textarea>
        
        <div class="label">Output Transcript:</div>
        <textarea class="transcript-area" .value=${this.outputTranscript} readonly placeholder="Output transcription will appear here..."></textarea>

        <div class="label">Scene Description:</div>
        <textarea class="transcript-area" .value=${this.sceneDescription} readonly placeholder="Scene description will appear here..."></textarea>
      ` : ''}
      
      ${this.error ? html`<div class="error">${this.error}</div>` : ''}
    `;
  }
}
