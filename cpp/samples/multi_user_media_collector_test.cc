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

#include "meet_clients/samples/multi_user_media_collector.h"

#include <sys/types.h>

#include <cstdint>
#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "meet_clients/samples/output_writer_interface.h"
#include "meet_clients/samples/testing/media_data.h"
#include "meet_clients/samples/testing/mock_output_writer.h"
#include "meet_clients/samples/testing/mock_resource_manager.h"
#include "api/make_ref_counted.h"
#include "rtc_base/thread.h"

namespace media_api_samples {
namespace {

using ::base_logging::INFO;
using ::testing::_;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::MatchesRegex;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ScopedMockLog;

TEST(MultiUserMediaCollectorTest, WaitForJoinedTimesOutBeforeJoining) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", absl::Seconds(1), std::move(thread));

  EXPECT_EQ(collector->WaitForJoined(absl::Seconds(1)).code(),
            absl::StatusCode::kDeadlineExceeded);
}

TEST(MultiUserMediaCollectorTest, WaitForJoinedSucceedsAfterJoining) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", absl::Seconds(1), std::move(thread));
  collector->OnJoined();
  EXPECT_EQ(collector->WaitForJoined(absl::Seconds(1)), absl::OkStatus());
}

TEST(MultiUserMediaCollectorTest,
     WaitForDisconnectedTimesOutBeforeDisconnecting) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", absl::Seconds(1), std::move(thread));

  EXPECT_EQ(collector->WaitForDisconnected(absl::Seconds(1)).code(),
            absl::StatusCode::kDeadlineExceeded);
}

TEST(MultiUserMediaCollectorTest,
     WaitForDisconnectedSucceedsAfterDisconnecting) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", absl::Seconds(1), std::move(thread));
  collector->OnDisconnected(absl::OkStatus());
  EXPECT_EQ(collector->WaitForDisconnected(absl::Seconds(1)), absl::OkStatus());
}

TEST(MultiUserMediaCollectorTest,
     WaitForJoinedSucceedsAfterDisconnectingWithNonOkStatus) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", absl::Seconds(1), std::move(thread));
  collector->OnDisconnected(absl::InternalError("test"));
  EXPECT_EQ(collector->WaitForJoined(absl::Seconds(1)),
            absl::InternalError("test"));
}

TEST(MultiUserMediaCollectorTest,
     WaitForDisconnectedSucceedsAfterDisconnectingWithNonOkStatus) {
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", absl::Seconds(1), std::move(thread));
  collector->OnDisconnected(absl::InternalError("test"));
  EXPECT_EQ(collector->WaitForDisconnected(absl::Seconds(1)),
            absl::InternalError("test"));
}

TEST(MultiUserMediaCollectorTest, ClosesAudioAndVideoSegmentsOnDisconnect) {
  // Output file 1.
  AudioTestData test_data1 = CreateAudioTestData(/*num_samples=*/10);
  test_data1.frame.contributing_source = 1;
  auto mock_audio_output_file = std::make_unique<MockOutputWriter>();
  absl::Notification audio_close_notification;
  EXPECT_CALL(*mock_audio_output_file, Close);

  // Output file 2.
  VideoTestData test_data2 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data2.meet_frame.contributing_source = 2;
  auto mock_video_output_file = std::make_unique<MockOutputWriter>();
  absl::Notification video_close_notification;
  EXPECT_CALL(*mock_video_output_file, Close);

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_audio_identifier_1_tmp.pcm"))
      .WillOnce(Return(std::move(mock_audio_output_file)));
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_2_tmp_10x5.yuv"))
      .WillOnce(Return(std::move(mock_video_output_file)));
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return("identifier_1"));
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(2))
      .WillOnce(Return("identifier_2"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnAudioFrame(std::move(test_data1.frame));
  collector->OnVideoFrame(std::move(test_data2.meet_frame));
  collector->OnDisconnected(absl::OkStatus());

  EXPECT_EQ(collector->WaitForDisconnected(absl::Seconds(1)), absl::OkStatus());
}

TEST(MultiUserMediaCollectorTest, ClosingSegmentsRenamesFiles) {
  // Output file 1.
  AudioTestData test_data1 = CreateAudioTestData(/*num_samples=*/10);
  test_data1.frame.contributing_source = 1;
  auto mock_audio_output_file = std::make_unique<MockOutputWriter>();
  absl::Notification audio_close_notification;
  EXPECT_CALL(*mock_audio_output_file, Close);

  // Output file 2.
  VideoTestData test_data2 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data2.meet_frame.contributing_source = 2;
  auto mock_video_output_file = std::make_unique<MockOutputWriter>();
  absl::Notification video_close_notification;
  EXPECT_CALL(*mock_video_output_file, Close);

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_audio_identifier_1_tmp.pcm"))
      .WillOnce(Return(std::move(mock_audio_output_file)));
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_2_tmp_10x5.yuv"))
      .WillOnce(Return(std::move(mock_video_output_file)));
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return("identifier_1"));
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(2))
      .WillOnce(Return("identifier_2"));
  MockFunction<void(absl::string_view, absl::string_view)> mock_renamer;
  // Final segment names are generated using the current time, so just check
  // that the final segment name matches the expected format.
  EXPECT_CALL(mock_renamer,
              Call("test_audio_identifier_1_tmp.pcm",
                   MatchesRegex("test_audio_identifier_1_.*_.*\\.pcm")));
  EXPECT_CALL(mock_renamer,
              Call("test_video_identifier_2_tmp_10x5.yuv",
                   MatchesRegex("test_video_identifier_2_.*_.*_10x5\\.yuv")));
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      std::move(mock_renamer).AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnAudioFrame(std::move(test_data1.frame));
  collector->OnVideoFrame(std::move(test_data2.meet_frame));
  collector->OnDisconnected(absl::OkStatus());
}

TEST(MultiUserMediaCollectorTest, ReceivesAudioFrameAndWritesToFile) {
  AudioTestData test_data = CreateAudioTestData(/*num_samples=*/10);
  test_data.frame.contributing_source = 1;
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
  EXPECT_CALL(mock_output_file_provider,
              Call("test_audio_identifier_1_tmp.pcm"))
      .WillOnce(Return(std::move(mock_output_file)));
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return("identifier_1"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnAudioFrame(std::move(test_data.frame));

  EXPECT_TRUE(
      write_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(written_pcm16, pcm16);
}

TEST(MultiUserMediaCollectorTest,
     ReceivesAudioFramesWithSameSourceCreatesOneFile) {
  AudioTestData test_data1 = CreateAudioTestData(/*num_samples=*/10);
  test_data1.frame.contributing_source = 1;
  AudioTestData test_data2 = CreateAudioTestData(/*num_samples=*/20);
  test_data2.frame.contributing_source = 1;

  auto mock_output_file = std::make_unique<MockOutputWriter>();
  uint16_t written_pcm16_count = 0;
  absl::Notification write_notification;
  EXPECT_CALL(*mock_output_file, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        EXPECT_EQ(size, sizeof(int16_t));
        written_pcm16_count++;
        if (written_pcm16_count ==
            test_data1.pcm16.size() + test_data2.pcm16.size()) {
          write_notification.Notify();
        }
      });
  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_audio_identifier_1_tmp.pcm"))
      .WillOnce(Return(std::move(mock_output_file)));
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return("identifier_1"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnAudioFrame(std::move(test_data1.frame));
  collector->OnAudioFrame(std::move(test_data2.frame));

  EXPECT_TRUE(
      write_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiUserMediaCollectorTest,
     ReceivesAudioFramesWithDifferentSourcesCreatesMultipleFiles) {
  // Output file 1.
  AudioTestData test_data1 = CreateAudioTestData(/*num_samples=*/10);
  test_data1.frame.contributing_source = 1;
  auto mock_output_file1 = std::make_unique<MockOutputWriter>();
  uint16_t written_pcm16_count1 = 0;
  absl::Notification write_notification1;
  EXPECT_CALL(*mock_output_file1, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        EXPECT_EQ(size, sizeof(int16_t));
        written_pcm16_count1++;
        if (written_pcm16_count1 == test_data1.pcm16.size()) {
          write_notification1.Notify();
        }
      });

  // Output file 2.
  AudioTestData test_data2 = CreateAudioTestData(/*num_samples=*/20);
  test_data2.frame.contributing_source = 2;
  auto mock_output_file2 = std::make_unique<MockOutputWriter>();
  uint16_t written_pcm16_count2 = 0;
  absl::Notification write_notification2;
  EXPECT_CALL(*mock_output_file2, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        EXPECT_EQ(size, sizeof(int16_t));
        written_pcm16_count2++;
        if (written_pcm16_count2 == test_data2.pcm16.size()) {
          write_notification2.Notify();
        }
      });

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_audio_identifier_1_tmp.pcm"))
      .WillOnce(Return(std::move(mock_output_file1)));
  EXPECT_CALL(mock_output_file_provider,
              Call("test_audio_identifier_2_tmp.pcm"))
      .WillOnce(Return(std::move(mock_output_file2)));
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return("identifier_1"));
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(2))
      .WillOnce(Return("identifier_2"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnAudioFrame(std::move(test_data1.frame));
  collector->OnAudioFrame(std::move(test_data2.frame));

  EXPECT_TRUE(
      write_notification1.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_TRUE(
      write_notification2.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiUserMediaCollectorTest,
     ReceivesAudioFramesAfterSegmentGapCreatesMultipleFiles) {
  // Output file 1.
  AudioTestData test_data1 = CreateAudioTestData(/*num_samples=*/10);
  test_data1.frame.contributing_source = 1;
  auto mock_output_file1 = std::make_unique<MockOutputWriter>();
  uint16_t written_pcm16_count1 = 0;
  absl::Notification write_notification1;
  EXPECT_CALL(*mock_output_file1, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        EXPECT_EQ(size, sizeof(int16_t));
        written_pcm16_count1++;
        if (written_pcm16_count1 == test_data1.pcm16.size()) {
          write_notification1.Notify();
        }
      });
  // Expect that the first output file is closed when the second frame is
  // received.
  EXPECT_CALL(*mock_output_file1, Close);

  // Output file 2.
  AudioTestData test_data2 = CreateAudioTestData(/*num_samples=*/20);
  // Use the same contributing source as the first frame.
  test_data2.frame.contributing_source = 1;
  auto mock_output_file2 = std::make_unique<MockOutputWriter>();
  uint16_t written_pcm16_count2 = 0;
  absl::Notification write_notification2;
  EXPECT_CALL(*mock_output_file2, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        EXPECT_EQ(size, sizeof(int16_t));
        written_pcm16_count2++;
        if (written_pcm16_count2 == test_data2.pcm16.size()) {
          write_notification2.Notify();
        }
      });

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  int file_count = 0;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_audio_identifier_1_tmp.pcm"))
      .Times(2)
      .WillRepeatedly([&] {
        file_count++;
        if (file_count == 1) {
          return std::move(mock_output_file1);
        } else {
          return std::move(mock_output_file2);
        }
      });
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillRepeatedly(Return("identifier_1"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnAudioFrame(std::move(test_data1.frame));
  // Wait long enough for the segment gap to be exceeded.
  absl::SleepFor(absl::Seconds(2));
  collector->OnAudioFrame(std::move(test_data2.frame));

  EXPECT_TRUE(
      write_notification1.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_TRUE(
      write_notification2.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiUserMediaCollectorTest, StartingNewAudioSegmentReleasesOldSegment) {
  AudioTestData test_data1 = CreateAudioTestData(/*num_samples=*/10);
  test_data1.frame.contributing_source = 1;
  auto mock_output_file1 = std::make_unique<MockOutputWriter>();
  // Expect that the first output file is closed when the second frame is
  // received.
  EXPECT_CALL(*mock_output_file1, Close);

  // Output file 2.
  AudioTestData test_data2 = CreateAudioTestData(/*num_samples=*/20);
  // Use the same contributing source as the first frame.
  test_data2.frame.contributing_source = 1;
  auto mock_output_file2 = std::make_unique<MockOutputWriter>();
  absl::Notification close_notification;
  EXPECT_CALL(*mock_output_file2, Close).WillOnce([&] {
    close_notification.Notify();
  });

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  int file_count = 0;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_audio_identifier_1_tmp.pcm"))
      .Times(2)
      .WillRepeatedly([&] {
        file_count++;
        if (file_count == 1) {
          return std::move(mock_output_file1);
        } else {
          return std::move(mock_output_file2);
        }
      });
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillRepeatedly(Return("identifier_1"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnAudioFrame(std::move(test_data1.frame));
  // Wait long enough for the segment gap to be exceeded.
  absl::SleepFor(absl::Seconds(2));
  collector->OnAudioFrame(std::move(test_data2.frame));
  collector->OnDisconnected(absl::OkStatus());

  // Wait for the second output file to be closed, expecting that the first
  // output file is not closed a second time.
  EXPECT_TRUE(
      close_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiUserMediaCollectorTest,
     StartsNewAudioSegmentWithResourceManagerErrorLogsError) {
  AudioTestData test_data = CreateAudioTestData(/*num_samples=*/10);
  test_data.frame.contributing_source = 1;

  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return(absl::InternalError("test error")));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_",
      MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>()
          .AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));
  ScopedMockLog log(kDoNotCaptureLogsYet);
  absl::SetVLogLevel("multi_user_media_collector", 1);
  absl::Notification log_notification;
  EXPECT_CALL(log, Log(INFO, _,
                       "No audio file identifier found for contributing source "
                       "1: test error"))
      .WillOnce([&](int, const std::string&, const std::string& msg) {
        log_notification.Notify();
      });
  log.StartCapturingLogs();

  collector->OnAudioFrame(std::move(test_data.frame));

  EXPECT_TRUE(
      log_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiUserMediaCollectorTest, ReceivesVideoFrameAndWritesToFile) {
  VideoTestData test_data = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data.meet_frame.contributing_source = 1;
  std::vector<char> yuv_data = std::move(test_data.yuv_data);

  auto mock_output_file = std::make_unique<MockOutputWriter>();
  std::vector<char> written_yuv_data;
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
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_1_tmp_10x5.yuv"))
      .WillOnce(Return(std::move(mock_output_file)));
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return("identifier_1"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnVideoFrame(std::move(test_data.meet_frame));

  EXPECT_TRUE(
      write_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(written_yuv_data, yuv_data);
}

TEST(MultiUserMediaCollectorTest,
     ReceivingVideoFramesWithSameSourceAndSizeCreatesOneFile) {
  VideoTestData test_data1 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data1.meet_frame.contributing_source = 1;
  VideoTestData test_data2 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data2.meet_frame.contributing_source = 1;

  auto mock_output_file = std::make_unique<MockOutputWriter>();
  uint16_t written_yuv_count = 0;
  absl::Notification write_notification;
  EXPECT_CALL(*mock_output_file, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        written_yuv_count += size;
        if (written_yuv_count ==
            test_data1.yuv_data.size() + test_data2.yuv_data.size()) {
          write_notification.Notify();
        }
      });
  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_1_tmp_10x5.yuv"))
      .WillOnce(Return(std::move(mock_output_file)));
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return("identifier_1"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnVideoFrame(std::move(test_data1.meet_frame));
  collector->OnVideoFrame(std::move(test_data2.meet_frame));

  EXPECT_TRUE(
      write_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiUserMediaCollectorTest,
     ReceivingVideoFramesWithDifferentSourcesCreatesMultipleFiles) {
  // Output file 1.
  VideoTestData test_data1 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data1.meet_frame.contributing_source = 1;
  auto mock_output_file1 = std::make_unique<MockOutputWriter>();
  uint16_t written_yuv_count1 = 0;
  absl::Notification write_notification1;
  EXPECT_CALL(*mock_output_file1, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        written_yuv_count1 += size;
        if (written_yuv_count1 == test_data1.yuv_data.size()) {
          write_notification1.Notify();
        }
      });

  // Output file 2.
  VideoTestData test_data2 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data2.meet_frame.contributing_source = 2;
  auto mock_output_file2 = std::make_unique<MockOutputWriter>();
  uint16_t written_yuv_count2 = 0;
  absl::Notification write_notification2;
  EXPECT_CALL(*mock_output_file2, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        written_yuv_count2 += size;
        if (written_yuv_count2 == test_data2.yuv_data.size()) {
          write_notification2.Notify();
        }
      });

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_1_tmp_10x5.yuv"))
      .WillOnce(Return(std::move(mock_output_file1)));
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_2_tmp_10x5.yuv"))
      .WillOnce(Return(std::move(mock_output_file2)));
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return("identifier_1"));
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(2))
      .WillOnce(Return("identifier_2"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnVideoFrame(std::move(test_data1.meet_frame));
  collector->OnVideoFrame(std::move(test_data2.meet_frame));

  EXPECT_TRUE(
      write_notification1.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_TRUE(
      write_notification2.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiUserMediaCollectorTest,
     ReceivingVideoFramesWithDifferentResolutionsCreatesMultipleFiles) {
  // Output file 1.
  VideoTestData test_data1 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data1.meet_frame.contributing_source = 1;
  auto mock_output_file1 = std::make_unique<MockOutputWriter>();
  uint16_t written_yuv_count1 = 0;
  absl::Notification write_notification1;
  EXPECT_CALL(*mock_output_file1, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        written_yuv_count1 += size;
        if (written_yuv_count1 == test_data1.yuv_data.size()) {
          write_notification1.Notify();
        }
      });

  // Output file 2, only change the width.
  VideoTestData test_data2 = CreateVideoTestData(/*width=*/20, /*height=*/5);
  test_data2.meet_frame.contributing_source = 1;
  auto mock_output_file2 = std::make_unique<MockOutputWriter>();
  uint16_t written_yuv_count2 = 0;
  absl::Notification write_notification2;
  EXPECT_CALL(*mock_output_file2, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        written_yuv_count2 += size;
        if (written_yuv_count2 == test_data2.yuv_data.size()) {
          write_notification2.Notify();
        }
      });

  // Output file 3, only change the height.
  VideoTestData test_data3 = CreateVideoTestData(/*width=*/20, /*height=*/20);
  test_data3.meet_frame.contributing_source = 1;
  auto mock_output_file3 = std::make_unique<MockOutputWriter>();
  uint16_t written_yuv_count3 = 0;
  absl::Notification write_notification3;
  EXPECT_CALL(*mock_output_file3, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        written_yuv_count3 += size;
        if (written_yuv_count3 == test_data3.yuv_data.size()) {
          write_notification3.Notify();
        }
      });

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_1_tmp_10x5.yuv"))
      .WillOnce(Return(std::move(mock_output_file1)));
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_1_tmp_20x5.yuv"))
      .WillOnce(Return(std::move(mock_output_file2)));
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_1_tmp_20x20.yuv"))
      .WillOnce(Return(std::move(mock_output_file3)));
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .Times(3)
      .WillRepeatedly(Return("identifier_1"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

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

TEST(MultiUserMediaCollectorTest,
     ReceivesVideoFramesAfterSegmentGapCreatesMultipleFiles) {
  // Output file 1.
  VideoTestData test_data1 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data1.meet_frame.contributing_source = 1;
  auto mock_output_file1 = std::make_unique<MockOutputWriter>();
  uint16_t written_yuv_count1 = 0;
  absl::Notification write_notification1;
  EXPECT_CALL(*mock_output_file1, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        written_yuv_count1 += size;
        if (written_yuv_count1 == test_data1.yuv_data.size()) {
          write_notification1.Notify();
        }
      });
  // Expect that the first output file is closed when the second frame is
  // received.
  EXPECT_CALL(*mock_output_file1, Close);

  // Output file 2.
  VideoTestData test_data2 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  // Use the same contributing source as the first frame.
  test_data2.meet_frame.contributing_source = 1;
  auto mock_output_file2 = std::make_unique<MockOutputWriter>();
  uint16_t written_yuv_count2 = 0;
  absl::Notification write_notification2;
  EXPECT_CALL(*mock_output_file2, Write(_, _))
      .WillRepeatedly([&](const char* content, std::streamsize size) {
        written_yuv_count2 += size;
        if (written_yuv_count2 == test_data2.yuv_data.size()) {
          write_notification2.Notify();
        }
      });

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  int file_count = 0;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_1_tmp_10x5.yuv"))
      .Times(2)
      .WillRepeatedly([&] {
        file_count++;
        if (file_count == 1) {
          return std::move(mock_output_file1);
        } else {
          return std::move(mock_output_file2);
        }
      });
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillRepeatedly(Return("identifier_1"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnVideoFrame(std::move(test_data1.meet_frame));
  // Wait long enough for the segment gap to be exceeded.
  absl::SleepFor(absl::Seconds(2));
  collector->OnVideoFrame(std::move(test_data2.meet_frame));

  EXPECT_TRUE(
      write_notification1.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_TRUE(
      write_notification2.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiUserMediaCollectorTest, StartingNewVideoSegmentReleasesOldSegment) {
  VideoTestData test_data1 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data1.meet_frame.contributing_source = 1;
  auto mock_output_file1 = std::make_unique<MockOutputWriter>();
  // Expect that the first output file is closed when the second frame is
  // received.
  EXPECT_CALL(*mock_output_file1, Close);

  VideoTestData test_data2 = CreateVideoTestData(/*width=*/10, /*height=*/5);
  // Use the same contributing source as the first frame.
  test_data2.meet_frame.contributing_source = 1;
  auto mock_output_file2 = std::make_unique<MockOutputWriter>();
  absl::Notification close_notification;
  EXPECT_CALL(*mock_output_file2, Close).WillOnce([&] {
    close_notification.Notify();
  });

  MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>
      mock_output_file_provider;
  int file_count = 0;
  EXPECT_CALL(mock_output_file_provider,
              Call("test_video_identifier_1_tmp_10x5.yuv"))
      .Times(2)
      .WillRepeatedly([&] {
        file_count++;
        if (file_count == 1) {
          return std::move(mock_output_file1);
        } else {
          return std::move(mock_output_file2);
        }
      });
  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillRepeatedly(Return("identifier_1"));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_", std::move(mock_output_file_provider).AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));

  collector->OnVideoFrame(std::move(test_data1.meet_frame));
  // Wait long enough for the segment gap to be exceeded.
  absl::SleepFor(absl::Seconds(2));
  collector->OnVideoFrame(std::move(test_data2.meet_frame));
  collector->OnDisconnected(absl::OkStatus());

  // Wait for the second output file to be closed, expecting that the first
  // output file is not closed a second time.
  EXPECT_TRUE(
      close_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

TEST(MultiUserMediaCollectorTest,
     StartsNewVideoSegmentWithResourceManagerErrorLogsError) {
  VideoTestData test_data = CreateVideoTestData(/*width=*/10, /*height=*/5);
  test_data.meet_frame.contributing_source = 1;

  auto mock_resource_manager = std::make_unique<MockResourceManager>();
  EXPECT_CALL(*mock_resource_manager, GetOutputFileIdentifier(1))
      .WillOnce(Return(absl::InternalError("test error")));
  auto renamer = MockFunction<void(absl::string_view, absl::string_view)>();
  auto thread = webrtc::Thread::Create();
  thread->Start();
  auto collector = webrtc::make_ref_counted<MultiUserMediaCollector>(
      "test_",
      MockFunction<std::unique_ptr<OutputWriterInterface>(absl::string_view)>()
          .AsStdFunction(),
      renamer.AsStdFunction(), absl::Seconds(1),
      std::move(mock_resource_manager), std::move(thread));
  ScopedMockLog log(kDoNotCaptureLogsYet);
  absl::SetVLogLevel("multi_user_media_collector", 1);
  absl::Notification log_notification;
  EXPECT_CALL(log, Log(INFO, _,
                       "No video file identifier found for contributing source "
                       "1: test error"))
      .WillOnce([&](int, const std::string&, const std::string& msg) {
        log_notification.Notify();
      });
  log.StartCapturingLogs();

  collector->OnVideoFrame(std::move(test_data.meet_frame));

  EXPECT_TRUE(
      log_notification.WaitForNotificationWithTimeout(absl::Seconds(1)));
}

}  // namespace
}  // namespace media_api_samples
