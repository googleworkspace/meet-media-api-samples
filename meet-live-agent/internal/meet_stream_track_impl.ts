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
 * @fileoverview Implementation of MeetStreamTrack.
 */

import {MediaEntry, MeetStreamTrack} from '../types/mediatypes';
import {Subscribable} from '../types/subscribable';

import {SubscribableDelegate} from './subscribable_impl';

/**
 * The implementation of MeetStreamTrack.
 */
export class MeetStreamTrackImpl implements MeetStreamTrack {
  readonly mediaEntry: Subscribable<MediaEntry | undefined>;

  constructor(
    readonly mediaStreamTrack: MediaStreamTrack,
    private readonly mediaEntryDelegate: SubscribableDelegate<
      MediaEntry | undefined
    >,
  ) {
    this.mediaEntry = this.mediaEntryDelegate.getSubscribable();
  }
}
