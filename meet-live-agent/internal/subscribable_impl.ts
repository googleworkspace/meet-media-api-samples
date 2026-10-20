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
 * @fileoverview Implementation of the Subscribable interface.
 */

import {Subscribable} from '../types/subscribable';

/**
 * Implementation of the Subscribable interface.
 */
export class SubscribableImpl<T> implements Subscribable<T> {
  constructor(private readonly subscribableDelegate: SubscribableDelegate<T>) {}

  get(): T {
    return this.subscribableDelegate.get();
  }

  subscribe(callback: (value: T) => void): () => void {
    this.subscribableDelegate.subscribe(callback);
    return () => {
      this.subscribableDelegate.unsubscribe(callback);
    };
  }

  unsubscribe(callback: (value: T) => void): boolean {
    return this.subscribableDelegate.unsubscribe(callback);
  }
}

/**
 * Helper class to update a subscribable value.
 */
export class SubscribableDelegate<T> {
  private readonly subscribers = new Set<(value: T) => void>();
  private readonly subscribable: Subscribable<T> = new SubscribableImpl<T>(
    this,
  );

  constructor(private value: T) {}

  set(newValue: T) {
    if (this.value !== newValue) {
      this.value = newValue;
      for (const callback of this.subscribers) {
        callback(newValue);
      }
    }
  }

  get(): T {
    return this.value;
  }

  subscribe(callback: (value: T) => void): void {
    this.subscribers.add(callback);
  }

  unsubscribe(callback: (value: T) => void): boolean {
    return this.subscribers.delete(callback);
  }

  getSubscribable(): Subscribable<T> {
    return this.subscribable;
  }
}
