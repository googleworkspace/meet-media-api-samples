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

#include "meet_clients/internal/curl_request.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/cleanup/cleanup.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include <curl/curl.h>
#include <curl/curl.h>
#include <curl/easy.h>

namespace meet {
namespace {

size_t OnCurlWrite(void* buffer, size_t size, size_t nitems, void* userdata) {
  if (buffer == nullptr) {
    LOG(DFATAL) << "Curl response buffer is nullptr";
    return 1;
  };
  if (userdata == nullptr) {
    LOG(DFATAL) << "Curl response userdata is nullptr";
    return 1;
  };
  const size_t content_length = size * nitems;
  auto context = static_cast<absl::Cord*>(userdata);
  context->Append(
      absl::string_view(static_cast<const char*>(buffer), size * nitems));
  return content_length;
}

absl::Status CheckOk(CURLcode code, absl::string_view option) {
  if (code != CURLE_OK) {
    return absl::InternalError(absl::StrCat("Failed to set curl ", option, ": ",
                                            curl_easy_strerror(code)));
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status CurlRequest::Send() {
  if (!response_data_.empty()) {
    return absl::InternalError(
        "Request object has already been used for another curl request");
  }

  CURL* curl = curl_api_.EasyInit();
  if (curl == nullptr) {
    return absl::InternalError("Failed to initialize curl");
  }

  absl::Cleanup cleanup_curl = [&] { curl_api_.EasyCleanup(curl); };

  if (request_parameters_.headers.empty()) {
    return absl::InvalidArgumentError("Request headers are empty");
  }

  struct curl_slist* headers = nullptr;
  for (const auto& header : request_parameters_.headers) {
    std::string formatted_request_header = header.first + ": " + header.second;
    headers = curl_api_.SListAppend(headers, formatted_request_header.c_str());
  }

  absl::Cleanup cleanup_headers = [&] { curl_api_.SListFreeAll(headers); };

  if (absl::Status opt_status =
          CheckOk(curl_api_.EasySetOptPtr(curl, CURLOPT_HTTPHEADER, headers),
                  "http header");
      !opt_status.ok()) {
    return opt_status;
  }

  if (absl::Status opt_status = CheckOk(
          curl_api_.EasySetOptInt(curl, request_parameters_.request_method, 1),
          "http method");
      !opt_status.ok()) {
    return opt_status;
  }

  if (request_parameters_.url.empty()) {
    return absl::InvalidArgumentError("Request URL is empty");
  }

  if (absl::Status opt_status = CheckOk(
          curl_api_.EasySetOptStr(curl, CURLOPT_URL, request_parameters_.url),
          "url");
      !opt_status.ok()) {
    return opt_status;
  }

  if (request_parameters_.body.empty()) {
    return absl::InvalidArgumentError("Request body is empty");
  }

  if (absl::Status opt_status =
          CheckOk(curl_api_.EasySetOptStr(curl, CURLOPT_POSTFIELDS,
                                          request_parameters_.body),
                  "request body");
      !opt_status.ok()) {
    return opt_status;
  }

  if (absl::Status opt_status =
          CheckOk(curl_api_.EasySetOptCallback(curl, CURLOPT_WRITEFUNCTION,
                                               &OnCurlWrite),
                  "write function");
      !opt_status.ok()) {
    return opt_status;
  }

  if (absl::Status opt_status = CheckOk(
          curl_api_.EasySetOptPtr(curl, CURLOPT_WRITEDATA, &response_data_),
          "write data");
      !opt_status.ok()) {
    return opt_status;
  }

  if (ca_cert_path_.has_value()) {
    if (absl::Status opt_status = CheckOk(
            curl_api_.EasySetOptStr(curl, CURLOPT_CAINFO, *ca_cert_path_),
            "ca cert path");
        !opt_status.ok()) {
      return opt_status;
    }
  }

  CURLcode send_result = curl_api_.EasyPerform(curl);
  if (send_result != CURLE_OK) {
    return absl::InternalError(absl::StrCat("Curl failed easy perform: ",
                                            curl_easy_strerror(send_result)));
  }
  return absl::OkStatus();
}

CurlApiWrapper::CurlApiWrapper() { curl_global_init(CURL_GLOBAL_ALL); }

CurlApiWrapper::~CurlApiWrapper() { curl_global_cleanup(); }

CURL* CurlApiWrapper::EasyInit() { return curl_easy_init(); }

void CurlApiWrapper::EasyCleanup(CURL* curl) { curl_easy_cleanup(curl); }

CURLcode CurlApiWrapper::EasySetOptInt(CURL* curl, CURLoption option,
                                       int value) {
  return curl_easy_setopt(curl, option, static_cast<long>(value));
}

CURLcode CurlApiWrapper::EasySetOptStr(CURL* curl, CURLoption option,
                                       const std::string& value) {
  return curl_easy_setopt(curl, option, value.c_str());
}

CURLcode CurlApiWrapper::EasySetOptPtr(CURL* curl, CURLoption option,
                                       void* value) {
  return curl_easy_setopt(curl, option, value);
}

CURLcode CurlApiWrapper::EasySetOptCallback(CURL* curl, CURLoption option,
                                            intptr_t address) {
  return curl_easy_setopt(curl, option, address);
}

CURLcode CurlApiWrapper::EasyPerform(CURL* curl) {
  return curl_easy_perform(curl);
}

struct curl_slist* CurlApiWrapper::SListAppend(struct curl_slist* list,
                                               const char* value) {
  return curl_slist_append(list, value);
}

void CurlApiWrapper::SListFreeAll(struct curl_slist* list) {
  curl_slist_free_all(list);
}

}  // namespace meet
