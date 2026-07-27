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
 * @fileoverview A helper class that implements generic getter and subscriber
 * functions. Only the owner of the class can update the value.
 */

/**
 * A helper class that can be used to get and subscribe to updates on a
 * value.
 */
export interface Subscribable<T> {
  /**
   * @return the current value.
   */
  get(): T;

  /**
   * Allows a callback to be added. This callback will be called whenever the
   * value is updated.
   * @return An unsubscribe function.
   */
  subscribe(callback: (value: T) => void): () => void;

  /**
   * Removes the callback from the list of subscribers. The original callback
   * instance must be passed in as an argument.
   * @return True if the callback was removed, false if it was not found.
   */
  unsubscribe(callback: (value: T) => void): boolean;
}
