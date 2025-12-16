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

#ifndef CPP_INTERNAL_TESTING_MOCK_CURL_API_WRAPPER_H_
#define CPP_INTERNAL_TESTING_MOCK_CURL_API_WRAPPER_H_

#include <cstdint>
#include <string>

#include "gmock/gmock.h"
#include <curl/curl.h>
#include "meet_clients/internal/curl_request.h"

namespace meet {

using ::testing::Return;

class MockCurlApiWrapper : public CurlApiWrapper {
 public:
  MockCurlApiWrapper() {
    ON_CALL(*this, EasyInit())
        .WillByDefault(Return(reinterpret_cast<CURL*>(0xdeadbeef)));

    ON_CALL(*this, SListAppend)
        .WillByDefault([this](struct curl_slist* l, const char* s) {
          return CurlApiWrapper::SListAppend(l, s);
        });

    ON_CALL(*this, SListFreeAll).WillByDefault([this](struct curl_slist* l) {
      return CurlApiWrapper::SListFreeAll(l);
    });

    ON_CALL(*this, EasySetOptCallback)
        .WillByDefault(Return(CURLcode::CURLE_OK));

    ON_CALL(*this, EasySetOptStr).WillByDefault(Return(CURLcode::CURLE_OK));

    ON_CALL(*this, EasySetOptInt).WillByDefault(Return(CURLcode::CURLE_OK));

    ON_CALL(*this, EasyPerform).WillByDefault(Return(CURLcode::CURLE_OK));
  }

  MOCK_METHOD(CURL*, EasyInit, (), (override));
  MOCK_METHOD(void, EasyCleanup, (CURL*), (override));
  MOCK_METHOD(CURLcode, EasyPerform, (CURL*), (override));
  MOCK_METHOD(CURLcode, EasySetOptInt, (CURL*, CURLoption, int), (override));
  MOCK_METHOD(CURLcode, EasySetOptStr, (CURL*, CURLoption, const std::string&),
              (override));
  MOCK_METHOD(CURLcode, EasySetOptPtr, (CURL*, CURLoption, void*), (override));
  MOCK_METHOD(CURLcode, EasySetOptCallback, (CURL*, CURLoption, intptr_t),
              (override));
  MOCK_METHOD(struct curl_slist*, SListAppend,
              (struct curl_slist*, const char*), (override));
  MOCK_METHOD(void, SListFreeAll, (struct curl_slist*), (override));
};

}  // namespace meet

#endif  // CPP_INTERNAL_TESTING_MOCK_CURL_API_WRAPPER_H_
