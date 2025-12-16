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

#ifndef CPP_INTERNAL_CURL_REQUEST_H_
#define CPP_INTERNAL_CURL_REQUEST_H_

// This file contains the implementation of a generic CurlRequest class with
// supporting wrappers and factories.
//
// It is implemented using the libcurl library. No implementations
// contained herein are representative of any logic that must be implemented to
// satisfy Meet Media API requirements.
//
// It's just for making requests.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <curl/curl.h>
#include <curl/curl.h>

namespace meet {

class CurlApiWrapper {
 public:
  CurlApiWrapper();
  virtual ~CurlApiWrapper();

  // CurlApiWrapper is neither copyable nor movable.
  CurlApiWrapper(const CurlApiWrapper&) = delete;
  CurlApiWrapper& operator=(const CurlApiWrapper&) = delete;

  virtual CURL* EasyInit();
  virtual void EasyCleanup(CURL* curl);
  virtual CURLcode EasySetOptInt(CURL* curl, CURLoption option, int value);
  virtual CURLcode EasySetOptStr(CURL* curl, CURLoption option,
                                 const std::string& value);
  virtual CURLcode EasySetOptPtr(CURL* curl, CURLoption option, void* value);
  virtual CURLcode EasySetOptCallback(CURL* curl, CURLoption option,
                                      intptr_t address);

  // A type-safe wrapper around function callback options.
  template <typename R, typename... Args>
  inline CURLcode EasySetOptCallback(CURL* curl, CURLoption option,
                                     R (*callback)(Args...)) {
    return EasySetOptCallback(curl, option,
                              reinterpret_cast<intptr_t>(callback));
  }

  virtual CURLcode EasyPerform(CURL* curl);
  virtual struct curl_slist* SListAppend(struct curl_slist* list,
                                         const char* value);
  virtual void SListFreeAll(struct curl_slist* list);
};

// Generic CurlRequest implementation for making requests to servers.
//
// Example usage:
//
// std::unique_ptr<CurlRequest> request = curl_request_factory->Create();
// request->SetRequestUrl("https://example.com");
// request->SetRequestMethod(CurlRequest::Method::kPost);
// request->SetRequestHeader("Authorization", "Bearer <token>");
// request->SetRequestHeader("Content-Type", "application/json");
// request->SetRequestBody("{\"offer\": \"<offer>\"}");
// absl::Status status = request->Send();
//
// if (!status.ok()) {
//   LOG(ERROR) << "Failed to send request: " << status;
//   return;
// }
//
// std::string response_data = request->GetResponseData();
//
class CurlRequest {
 public:
  enum class Method {
    kPost,
    kGet,
    kPut,
  };

  explicit CurlRequest(CurlApiWrapper& curl_api) : curl_api_(curl_api) {}

  absl::Status Send();

  std::string GetResponseData() const { return std::string(response_data_); }
  void SetRequestUrl(std::string url) {
    request_parameters_.url = std::move(url);
  }
  void SetRequestHeader(std::string key, std::string value) {
    request_parameters_.headers[std::move(key)] = std::move(value);
  }
  void SetRequestBody(std::string body) {
    request_parameters_.body = std::move(body);
  }
  void SetRequestMethod(Method method) {
    request_parameters_.request_method = RequestMethodToCurlOption(method);
  }
  void SetCaCertPath(absl::string_view ca_cert_path) {
    ca_cert_path_ = std::string(ca_cert_path);
  }

 private:
  static CURLoption RequestMethodToCurlOption(Method method) {
    switch (method) {
      case Method::kPost:
        return CURLOPT_POST;
      case Method::kPut:
        return CURLOPT_UPLOAD;
      case Method::kGet:
      default:
        return CURLOPT_HTTPGET;
    }
  }

  struct RequestParameters {
    std::string url;
    std::string body;
    absl::flat_hash_map<std::string, std::string> headers;
    CURLoption request_method = CURLOPT_POST;
  };

  absl::Cord response_data_;
  RequestParameters request_parameters_;
  std::string error_message_;
  CurlApiWrapper& curl_api_;
  std::optional<std::string> ca_cert_path_;
};

}  // namespace meet

#endif  // CPP_INTERNAL_CURL_REQUEST_H_
