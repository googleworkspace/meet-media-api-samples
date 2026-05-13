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

#ifndef CPP_SAMPLES_OUTPUT_WRITER_INTERFACE_H_
#define CPP_SAMPLES_OUTPUT_WRITER_INTERFACE_H_

#include <ios>
#include <memory>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace media_api_samples {

// Interface for writing data.
class OutputWriterInterface {
 public:
  virtual ~OutputWriterInterface() = default;
  virtual void Write(const char* content, std::streamsize size) = 0;
  virtual void Close() = 0;
};

// Interface for providing output writers.
using OutputWriterProvider =
    absl::AnyInvocable<std::unique_ptr<OutputWriterInterface>(
        absl::string_view file_name)>;

}  // namespace media_api_samples

#endif  // CPP_SAMPLES_OUTPUT_WRITER_INTERFACE_H_
