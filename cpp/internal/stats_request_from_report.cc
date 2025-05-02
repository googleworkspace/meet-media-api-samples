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

#include "cpp/internal/stats_request_from_report.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "cpp/api/media_stats_resource.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/stats/attribute.h"
// TODO: Remove once build has updated to a recent WebRTC version.
#include "cpp/internal/webrtc_forward_decls.h"
#include "webrtc/api/stats/rtc_stats.h"
#include "webrtc/api/stats/rtc_stats_report.h"

namespace meet {

MediaStatsChannelFromClient StatsRequestFromReport(
    const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& report,
    int64_t stats_request_id,
    const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>&
        allowlist) {
  VLOG(1) << "StatsRequestFromReport: " << report->ToJson();

  MediaStatsChannelFromClient request;
  request.request.request_id = stats_request_id;
  request.request.upload_media_stats = UploadMediaStatsRequest();

  for (const webrtc::RTCStats& report_section : *report) {
    auto it = allowlist.find(report_section.type());
    if (it == allowlist.end()) {
      continue;
    }
    absl::flat_hash_set<std::string> allowed_attributes = it->second;

    MediaStatsSection request_section;
    request_section.id = report_section.id();
    request_section.type = report_section.type();
    for (const webrtc::Attribute& attribute : report_section.Attributes()) {
      if (attribute.has_value() &&
          allowed_attributes.contains(attribute.name())) {
        request_section.values[attribute.name()] = attribute.ToString();
      }
    }

    if (!request_section.values.empty()) {
      request.request.upload_media_stats->sections.push_back(
          std::move(request_section));
    }
  }

  return request;
}

}  // namespace meet
