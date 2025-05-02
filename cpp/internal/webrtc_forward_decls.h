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

#ifndef CPP_INTERNAL_WEBRTC_FORWARD_DECLS_H_
#define CPP_INTERNAL_WEBRTC_FORWARD_DECLS_H_

// TODO: Remove once build has updated to a recent WebRTC version.
#ifdef FORCE_ADD_WEBRTC_NAMESPACE_EXPORTS
#include "webrtc/api/candidate.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/media_types.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/video_sink_interface.h"
#include "webrtc/api/video/video_source_interface.h"
#include "webrtc/p2p/base/port.h"
#include "webrtc/rtc_base/thread.h"
#include "webrtc/rtc_base/time_utils.h"

namespace webrtc {
using ::cricket::Candidate;
using ::cricket::CandidatePairChangeEvent;
using ::cricket::MediaType;
using ::rtc::make_ref_counted;
using ::rtc::scoped_refptr;
using ::rtc::Thread;
using ::rtc::TimeMillis;
using ::rtc::VideoSinkInterface;
using ::rtc::VideoSinkWants;
}  // namespace webrtc
#endif  // FORCE_ADD_WEBRTC_NAMESPACE_EXPORTS

#endif  // CPP_INTERNAL_WEBRTC_FORWARD_DECLS_H_
