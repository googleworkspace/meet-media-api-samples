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

#ifndef CPP_INTERNAL_VARIANT_UTILS_H_
#define CPP_INTERNAL_VARIANT_UTILS_H_
#include "absl/base/nullability.h"

ABSL_POINTERS_DEFAULT_NONNULL

namespace meet {

// Utility template for switching over a variant type.
//
// See https://en.cppreference.com/w/cpp/utility/variant/visit example #4.
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};

}  // namespace meet

#endif  // CPP_INTERNAL_VARIANT_UTILS_H_
