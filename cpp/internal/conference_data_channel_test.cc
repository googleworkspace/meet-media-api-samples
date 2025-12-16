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

#include "meet_clients/internal/conference_data_channel.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/base/log_severity.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "meet_clients/api/media_api_client_interface.h"
#include "meet_clients/api/session_control_resource.h"
#include "meet_clients/internal/resource_handler_interface.h"
#include "api/data_channel_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_data_channel.h"

namespace meet {
namespace {

using ::base_logging::ERROR;
using ::base_logging::WARNING;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::MockFunction;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ScopedMockLog;
using ::testing::StrEq;
using ::testing::status::StatusIs;

class MockResourceHandler : public ResourceHandlerInterface {
 public:
  MOCK_METHOD(absl::StatusOr<ResourceUpdate>, ParseUpdate,
              (absl::string_view update), (override));
  MOCK_METHOD(absl::StatusOr<std::string>, StringifyRequest,
              (const ResourceRequest &request), (override));
};

TEST(ConferenceDataChannelTest, SendRequestSucceeds) {
  auto resource_handler = std::make_unique<MockResourceHandler>();
  EXPECT_CALL(*resource_handler, StringifyRequest(_))
      .WillOnce(Return("test-request"));
  auto data_channel = webrtc::MockDataChannelInterface::Create();
  std::string sent_message;
  EXPECT_CALL(*data_channel, SendAsync)
      .WillOnce([&sent_message](
                    webrtc::DataBuffer buffer,
                    absl::AnyInvocable<void(webrtc::RTCError) &&> on_complete) {
        sent_message = std::string(buffer.data.cdata<char>(), buffer.size());
        std::move(on_complete)(webrtc::RTCError::OK());
      });
  ConferenceDataChannel conference_data_channel(std::move(resource_handler),
                                                std::move(data_channel));

  absl::Status status =
      conference_data_channel.SendRequest(SessionControlChannelFromClient());

  EXPECT_OK(status);
  EXPECT_THAT(sent_message, StrEq("test-request"));
}

TEST(ConferenceDataChannelTest, SendRequestFailsWhenParsingFails) {
  auto resource_handler = std::make_unique<MockResourceHandler>();
  EXPECT_CALL(*resource_handler, StringifyRequest(_))
      .WillOnce(Return(absl::InternalError("test-error")));
  auto data_channel = webrtc::MockDataChannelInterface::Create();
  EXPECT_CALL(*data_channel, SendAsync).Times(0);
  ConferenceDataChannel conference_data_channel(std::move(resource_handler),
                                                std::move(data_channel));

  absl::Status status =
      conference_data_channel.SendRequest(SessionControlChannelFromClient());

  EXPECT_THAT(status, StatusIs(absl::StatusCode::kInternal, "test-error"));
}

TEST(ConferenceDataChannelTest, SendRequestLogsErrorWhenTransmissionFails) {
  auto resource_handler = std::make_unique<MockResourceHandler>();
  EXPECT_CALL(*resource_handler, StringifyRequest(_))
      .WillOnce(Return("test-request"));
  auto data_channel = webrtc::MockDataChannelInterface::Create();
  EXPECT_CALL(*data_channel, SendAsync)
      .WillOnce([](webrtc::DataBuffer /*buffer*/,
                   absl::AnyInvocable<void(webrtc::RTCError) &&> on_complete) {
        std::move(on_complete)(webrtc::RTCError(
            webrtc::RTCErrorType::INTERNAL_ERROR, "test-error"));
      });
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(ERROR, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();
  ConferenceDataChannel conference_data_channel(std::move(resource_handler),
                                                std::move(data_channel));

  absl::Status status =
      conference_data_channel.SendRequest(SessionControlChannelFromClient());

  EXPECT_OK(status);
  EXPECT_THAT(message, HasSubstr("test-error"));
}

TEST(ConferenceDataChannelTest,
     CreatingConferenceDataChannelRegistersObserver) {
  auto data_channel = webrtc::MockDataChannelInterface::Create();
  webrtc::DataChannelObserver *observer;
  EXPECT_CALL(*data_channel, RegisterObserver(_))
      .WillOnce([&observer](webrtc::DataChannelObserver *inner_observer) {
        observer = inner_observer;
      });

  ConferenceDataChannel conference_data_channel(
      std::make_unique<MockResourceHandler>(), std::move(data_channel));

  EXPECT_THAT(observer, NotNull());
}

TEST(ConferenceDataChannelTest, ReceivingUpdateSucceeds) {
  auto resource_handler = std::make_unique<MockResourceHandler>();
  EXPECT_CALL(*resource_handler, ParseUpdate(_))
      .WillOnce(Return(SessionControlChannelToClient()));
  auto data_channel = webrtc::MockDataChannelInterface::Create();
  webrtc::DataChannelObserver *observer;
  EXPECT_CALL(*data_channel, RegisterObserver(_))
      .WillOnce([&observer](webrtc::DataChannelObserver *inner_observer) {
        observer = inner_observer;
      });
  ConferenceDataChannel conference_data_channel(std::move(resource_handler),
                                                std::move(data_channel));
  ResourceUpdate received_update;
  MockFunction<void(ResourceUpdate)> mock_function;
  EXPECT_CALL(mock_function, Call)
      .WillOnce([&received_update](ResourceUpdate update) {
        received_update = std::move(update);
      });
  conference_data_channel.SetCallback(mock_function.AsStdFunction());

  observer->OnMessage(webrtc::DataBuffer("test-update"));

  EXPECT_TRUE(
      std::holds_alternative<SessionControlChannelToClient>(received_update));
}

TEST(ConferenceDataChannelTest, ReceivingUpdateWithoutCallbackDoesNothing) {
  auto data_channel = webrtc::MockDataChannelInterface::Create();
  webrtc::DataChannelObserver *observer;
  EXPECT_CALL(*data_channel, RegisterObserver(_))
      .WillOnce([&observer](webrtc::DataChannelObserver *inner_observer) {
        observer = inner_observer;
      });
  ConferenceDataChannel conference_data_channel(
      std::make_unique<MockResourceHandler>(), std::move(data_channel));
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(WARNING, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();

  observer->OnMessage(webrtc::DataBuffer("test-update"));

  EXPECT_THAT(message,
              HasSubstr("data channel received message but has no callback."));
}

TEST(ConferenceDataChannelTest, ReceivingUpdateFailsWhenReceivingBinaryData) {
  auto data_channel = webrtc::MockDataChannelInterface::Create();
  webrtc::DataChannelObserver *observer;
  EXPECT_CALL(*data_channel, RegisterObserver(_))
      .WillOnce([&observer](webrtc::DataChannelObserver *inner_observer) {
        observer = inner_observer;
      });
  ConferenceDataChannel conference_data_channel(
      std::make_unique<MockResourceHandler>(), std::move(data_channel));
  conference_data_channel.SetCallback([](ResourceUpdate /*update*/) {});
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(ERROR, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();

  observer->OnMessage(webrtc::DataBuffer("test-update", true));

  EXPECT_THAT(message,
              HasSubstr("data channel received unexpected binary update."));
}

TEST(ConferenceDataChannelTest, ReceivingUpdateFailsWhenParsingFails) {
  auto resource_handler = std::make_unique<MockResourceHandler>();
  EXPECT_CALL(*resource_handler, ParseUpdate(_))
      .WillOnce(Return(absl::InternalError("parsing-error")));
  auto data_channel = webrtc::MockDataChannelInterface::Create();
  webrtc::DataChannelObserver *observer;
  EXPECT_CALL(*data_channel, RegisterObserver(_))
      .WillOnce([&observer](webrtc::DataChannelObserver *inner_observer) {
        observer = inner_observer;
      });
  ConferenceDataChannel conference_data_channel(std::move(resource_handler),
                                                std::move(data_channel));
  conference_data_channel.SetCallback([](ResourceUpdate /*update*/) {});
  ScopedMockLog log(kDoNotCaptureLogsYet);
  std::string message;
  EXPECT_CALL(log, Log(ERROR, _, _))
      .WillOnce([&message](int, const std::string &, const std::string &msg) {
        message = msg;
      });
  log.StartCapturingLogs();

  observer->OnMessage(webrtc::DataBuffer("test-update"));

  EXPECT_THAT(message, HasSubstr("parsing-error"));
}

TEST(ConferenceDataChannelTest,
     DestroyingConferenceDataChannelTearsDownChannel) {
  auto data_channel = webrtc::MockDataChannelInterface::Create();
  EXPECT_CALL(*data_channel, UnregisterObserver);
  EXPECT_CALL(*data_channel, Close);

  {
    ConferenceDataChannel conference_data_channel(
        std::make_unique<MockResourceHandler>(), std::move(data_channel));
  }
}

}  // namespace
}  // namespace meet
