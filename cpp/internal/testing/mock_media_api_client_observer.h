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

#ifndef CPP_INTERNAL_TESTING_MOCK_MEDIA_API_CLIENT_OBSERVER_H_
#define CPP_INTERNAL_TESTING_MOCK_MEDIA_API_CLIENT_OBSERVER_H_

#include "gmock/gmock.h"
#include "absl/status/status.h"
#include "meet_clients/api/media_api_client_interface.h"

namespace meet {

class MockMediaApiClientObserver : public MediaApiClientObserverInterface {
 public:
  MOCK_METHOD(void, OnJoined, (), (override));
  MOCK_METHOD(void, OnDisconnected, (absl::Status), (override));
  MOCK_METHOD(void, OnResourceUpdate, (ResourceUpdate), (override));
  MOCK_METHOD(void, OnAudioFrame, (AudioFrame), (override));
  MOCK_METHOD(void, OnVideoFrame, (VideoFrame), (override));
};

}  // namespace meet

#endif  // CPP_INTERNAL_TESTING_MOCK_MEDIA_API_CLIENT_OBSERVER_H_
