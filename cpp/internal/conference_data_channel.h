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

#ifndef CPP_INTERNAL_CONFERENCE_DATA_CHANNEL_H_
#define CPP_INTERNAL_CONFERENCE_DATA_CHANNEL_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "cpp/api/media_api_client_interface.h"
#include "cpp/internal/conference_data_channel_interface.h"
#include "cpp/internal/resource_handler_interface.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/scoped_refptr.h"

namespace meet {

// A wrapper around a `webrtc::DataChannelInterface` that provides a simplified
// interface for sending resource requests and receiving resource updates.
//
// This class closes the underlying data channel when it is destroyed.
class ConferenceDataChannel : public ConferenceDataChannelInterface,
                              public webrtc::DataChannelObserver {
 public:
  ConferenceDataChannel(
      std::unique_ptr<ResourceHandlerInterface> resource_handler,
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
      : resource_handler_(std::move(resource_handler)),
        data_channel_(std::move(data_channel)) {
    data_channel_->RegisterObserver(this);
  };

  ~ConferenceDataChannel() override {
    data_channel_->UnregisterObserver();
    data_channel_->Close();
  }

  // ConferenceDataChannel is neither copyable nor movable.
  ConferenceDataChannel(const ConferenceDataChannel&) = delete;
  ConferenceDataChannel& operator=(const ConferenceDataChannel&) = delete;

  void OnStateChange() override {
    VLOG(1) << "ConferenceDataChannel::OnStateChange: "
            << data_channel_->state();
  };

  void OnMessage(const webrtc::DataBuffer& buffer) override;

  // Future WebRTC updates will force this to always be true. Ensure that
  // current behavior reflects desired future behavior.
  bool IsOkToCallOnTheNetworkThread() override { return true; }

  // Sets the callback for receiving resource updates from the resource data
  // channel.
  //
  // The callback is called on the associated peer connection's network thread.
  //
  // Resource data channels can only have one callback at a time, and the
  // callback must outlive the resource data channel if one is set.
  //
  // Setting the callback is not thread-safe, so it should only be called before
  // the resource data channel is used (i.e. before the peer connection is
  // started).
  void SetCallback(ResourceUpdateCallback callback) override {
    callback_ = std::move(callback);
  }

  absl::Status SendRequest(ResourceRequest request) override;

 private:
  std::string label() const { return data_channel_->label(); }

  ResourceUpdateCallback callback_;
  std::unique_ptr<ResourceHandlerInterface> resource_handler_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
};

}  // namespace meet

#endif  // CPP_INTERNAL_CONFERENCE_DATA_CHANNEL_H_
