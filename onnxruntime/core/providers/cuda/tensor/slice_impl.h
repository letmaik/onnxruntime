// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
#include <stdint.h>
#include "core/providers/cuda/shared_inc/cuda_utils.h"
#include "core/common/common.h"

namespace onnxruntime {
namespace cuda {

Status SliceImpl(const size_t element_size,
                 const int32_t dimension_count,
                 const int64_t* starts,
                 const int64_t* input_strides,
                 const fast_divmod* output_div_strides,
                 const void* input_data,
                 void* output_data,
                 const size_t N);

}  // namespace cuda
}  // namespace onnxruntime
