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

#ifndef CPP_INTERNAL_STATS_REQUEST_FROM_REPORT_H_
#define CPP_INTERNAL_STATS_REQUEST_FROM_REPORT_H_

#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "cpp/api/media_stats_resource.h"
#include "webrtc/api/scoped_refptr.h"
// TODO: Remove once build has updated to a recent WebRTC version.
#include "cpp/internal/webrtc_forward_decls.h"
#include "webrtc/api/stats/rtc_stats_report.h"

namespace meet {

// Convenience function to convert a stats report to a media stats request.
//
// The request id will be used when constructing the request.
//
// The allowlist is a map from section type to a set of allowed attributes.
// Sections and attributes that are not in the allowlist are not included in the
// request.
MediaStatsChannelFromClient StatsRequestFromReport(
    const webrtc::scoped_refptr<const webrtc::RTCStatsReport>& report,
    int64_t request_id,
    const absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>>&
        allowlist);

}  // namespace meet

#endif  // CPP_INTERNAL_STATS_REQUEST_FROM_REPORT_H_
