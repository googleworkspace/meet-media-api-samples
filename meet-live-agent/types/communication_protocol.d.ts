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
 * @fileoverview An abstract communication protocol.
 */

/**
 * An abstract communication protocol.
 */
export interface MediaApiCommunicationProtocol {
  /**
   * Connects to the active conference with the given SDP offer.
   * @param sdpOffer The SDP offer to connect to the active conference.
   * @return A promise that resolves to the communication response.
   */
  connectActiveConference(
    sdpOffer: string,
  ): Promise<MediaApiCommunicationResponse>;
}

/**
 * The response from the communication protocol.
 */
export declare interface MediaApiCommunicationResponse {
  /**
   * The WebRTC answer to the offer. Format is SDP.
   */
  answer: string;
}
