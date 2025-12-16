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

#ifndef CPP_SAMPLES_OUTPUT_FILE_H_
#define CPP_SAMPLES_OUTPUT_FILE_H_

#include <fstream>
#include <ios>
#include <utility>

#include "meet_clients/samples/output_writer_interface.h"

namespace media_api_samples {

// An output writer that writes to a local file.
class OutputFile : public OutputWriterInterface {
 public:
  explicit OutputFile(std::ofstream file) : file_(std::move(file)) {}
  void Write(const char* content, std::streamsize size) override;
  void Close() override;

 private:
  std::ofstream file_;
};

}  // namespace media_api_samples

#endif  // CPP_SAMPLES_OUTPUT_FILE_H_
