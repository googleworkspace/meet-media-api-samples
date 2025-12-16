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

#include "meet_clients/samples/single_user_media_collector.h"

#include <cstdint>
#include <ios>
#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "meet_clients/api/media_api_client_interface.h"
#include "meet_clients/samples/output_writer_interface.h"
#include "meet_clients/samples/testing/media_data.h"
#include "meet_clients/samples/testing/mock_output_writer.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "rtc_base/thread.h"

namespace media_api_samples {
namespace {

using ::testing::_;
using ::testing::MockFunction;
using ::testing::Return;

TEST(SingleUserMediaCollectorTest, WaitForJoinedTimesOutBeforeJoining) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread));

  EXPECT_EQ(collector->WaitForJoined(absl::Seconds(1)).code(),
            absl::StatusCode::kDeadlineExceeded);
}

TEST(SingleUserMediaCollectorTest, WaitForJoinedSucceedsAfterJoining) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread));
  collector->OnJoined();
  EXPECT_EQ(collector->WaitForJoined(absl::Seconds(1)), absl::OkStatus());
}

TEST(SingleUserMediaCollectorTest,
     WaitForDisconnectedTimesOutBeforeDisconnecting) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread));

  EXPECT_EQ(collector->WaitForDisconnected(absl::Seconds(1)).code(),
            absl::StatusCode::kDeadlineExceeded);
}

TEST(SingleUserMediaCollectorTest,
     WaitForDisconnectedSucceedsAfterDisconnecting) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread));
  collector->OnDisconnected(absl::OkStatus());
  EXPECT_EQ(collector->WaitForDisconnected(absl::Seconds(1)), absl::OkStatus());
}

TEST(SingleUserMediaCollectorTest,
     WaitForJoinedSucceedsAfterDisconnectingWithNonOkStatus) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread));
  collector->OnDisconnected(absl::InternalError("test"));
  EXPECT_EQ(collector->WaitForJoined(absl::Seconds(1)),
            absl::InternalError("test"));
}

TEST(SingleUserMediaCollectorTest,
     WaitForDisconnectedSucceedsAfterDisconnectingWithNonOkStatus) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread));
  collector->OnDisconnected(absl::InternalError("test"));
  EXPECT_EQ(collector->WaitForDisconnected(absl::Seconds(1)),
            absl::InternalError("test"));
}

TEST(SingleUserMediaCollectorTest, ReceivesAudioFrameAndWritesToAudioFile) {
  AudioTestData test_data = CreateAudioTestData(/*num_samples=*/10);
  std::vector<int16_t> pcm16 = std::move(test_data.pcm16);

  auto mock_output_file = std::make_unique<MockOutputWriter>();
  std::vector<int16_t> written_pcm16;
  absl::Notification write_notification;
  EXPECT_CALL(*mock_output_file, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        EXPECT_EQ(size, sizeof(int16_t));
        written_pcm16.push_back(reinterpret_cast<const int16_t*>(content)[0]);
        if (written_pcm16.size() == pcm16.size()) {
          write_notification.Notify();
        }
      });

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider, Call("test_audio.pcm"))
      .WillOnce(Return(std::move(mock_output_file)));
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread),
      std::move(mock_output_file_provider).AsStdFunction());

  collector->OnAudioFrame(test_data.frame);

  EXPECT_TRUE(
      write_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(written_pcm16, pcm16);
}

TEST(SingleUserMediaCollectorTest, ReceivesAudioFrameOnlyCreatesOneAudioFile) {
  AudioTestData test_data1 = CreateAudioTestData(/*num_samples=*/10);
  AudioTestData test_data2 = CreateAudioTestData(/*num_samples=*/20);

  auto mock_output_file = std::make_unique<MockOutputWriter>();
  int write_count = 0;
  absl::Notification write_notification;
  EXPECT_CALL(*mock_output_file, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        write_count++;
        if (write_count == test_data1.pcm16.size() + test_data2.pcm16.size()) {
          write_notification.Notify();
        }
      });
  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider, Call(_))
      .Times(1)
      .WillOnce(Return(std::move(mock_output_file)));
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread),
      std::move(mock_output_file_provider).AsStdFunction());

  collector->OnAudioFrame(std::move(test_data1.frame));
  collector->OnAudioFrame(std::move(test_data2.frame));

  EXPECT_TRUE(
      write_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(SingleUserMediaCollectorTest, ReceivesVideoFrameAndWritesToVideoFile) {
  VideoTestData test_data = CreateVideoTestData(/*width=*/10, /*height=*/5);
  std::vector<char> yuv_data = std::move(test_data.yuv_data);

  auto mock_output_file = std::make_unique<MockOutputWriter>();
  std::vector<char> written_yuv_data;
  written_yuv_data.reserve(yuv_data.size());
  absl::Notification write_notification;
  EXPECT_CALL(*mock_output_file, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        for (int i = 0; i < size; ++i) {
          written_yuv_data.push_back(content[i]);
        }
        if (written_yuv_data.size() == yuv_data.size()) {
          write_notification.Notify();
        }
      });
  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider, Call("test_video_0_10x5.yuv"))
      .WillOnce(Return(std::move(mock_output_file)));
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread),
      std::move(mock_output_file_provider).AsStdFunction());

  collector->OnVideoFrame(std::move(test_data.meet_frame));

  EXPECT_TRUE(
      write_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(written_yuv_data, yuv_data);
}

TEST(SingleUserMediaCollectorTest,
     ReceivingVideoFramesWithSameSizesOnlyCreatesOneVideoFile) {
  VideoTestData test_data1 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  VideoTestData test_data2 = CreateVideoTestData(/*width=*/10, /*height=*/5);

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  absl::Notification write_notification;
  EXPECT_CALL(mock_output_file_provider, Call("test_video_0_10x5.yuv"))
      .WillOnce([&] {
        write_notification.Notify();
        return std::make_unique<MockOutputWriter>();
      });
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread),
      std::move(mock_output_file_provider).AsStdFunction());

  collector->OnVideoFrame(std::move(test_data1.meet_frame));
  collector->OnVideoFrame(std::move(test_data2.meet_frame));

  EXPECT_TRUE(
      write_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(SingleUserMediaCollectorTest,
     ReceivingVideoFramesWithDifferentSizesCreatesMultipleVideoFiles) {
  VideoTestData test_data1 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  VideoTestData test_data2 = CreateVideoTestData(/*width=*/20, /*height=*/5);
  VideoTestData test_data3 = CreateVideoTestData(/*width=*/10, /*height=*/5);

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  absl::Notification write_notification1;
  absl::Notification write_notification2;
  absl::Notification write_notification3;
  EXPECT_CALL(mock_output_file_provider, Call("test_video_0_10x5.yuv"))
      .WillOnce([&] {
        write_notification1.Notify();
        return std::make_unique<MockOutputWriter>();
      });
  EXPECT_CALL(mock_output_file_provider, Call("test_video_1_20x5.yuv"))
      .WillOnce([&] {
        write_notification2.Notify();
        return std::make_unique<MockOutputWriter>();
      });
  EXPECT_CALL(mock_output_file_provider, Call("test_video_2_10x5.yuv"))
      .WillOnce([&] {
        write_notification3.Notify();
        return std::make_unique<MockOutputWriter>();
      });
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<SingleUserMediaCollector>(
      "test_", std::move(thread),
      std::move(mock_output_file_provider).AsStdFunction());

  collector->OnVideoFrame(std::move(test_data1.meet_frame));
  collector->OnVideoFrame(std::move(test_data2.meet_frame));
  collector->OnVideoFrame(std::move(test_data3.meet_frame));

  EXPECT_TRUE(
      write_notification1.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_TRUE(
      write_notification2.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_TRUE(
      write_notification3.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

}  // namespace
}  // namespace media_api_samples
