// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "open3d/t/pipelines/SolveTransformation.h"

#include <cmath>

#include "open3d/core/Tensor.h"
#include "open3d/t/pipelines/SolveTransformationImpl.h"

namespace open3d {
namespace t {
namespace pipelines {

core::Tensor ComputeTransformationFromRt(const core::Tensor &R,
                                         const core::Tensor &t) {
    core::Dtype dtype = core::Dtype::Float32;
    core::Device device = R.GetDevice();
    core::Tensor transformation = core::Tensor::Zeros({4, 4}, dtype, device);
    R.AssertShape({3, 3});
    R.AssertDtype(dtype);
    t.AssertShape({3});
    t.AssertDevice(device);
    t.AssertDtype(dtype);

    // Rotation
    transformation.SetItem(
            {core::TensorKey::Slice(0, 3, 1), core::TensorKey::Slice(0, 3, 1)},
            R);
    // Translation and Scale [Assumed to be 1]
    transformation.SetItem(
            {core::TensorKey::Slice(0, 3, 1), core::TensorKey::Slice(3, 4, 1)},
            t.Reshape({3, 1}));
    transformation[3][3] = 1;
    return transformation;
}

core::Tensor ComputeTransformationFromPose(const core::Tensor &X) {
    core::Dtype dtype = core::Dtype::Float32;
    X.AssertShape({6});
    X.AssertDtype(dtype);
    core::Device device = X.GetDevice();
    core::Tensor transformation = core::Tensor::Zeros({4, 4}, dtype, device);
    transformation = transformation.Contiguous();
    auto X_copy = X.Contiguous();
    float *transformation_ptr =
            static_cast<float *>(transformation.GetDataPtr());
    const float *X_ptr = static_cast<const float *>(X_copy.GetDataPtr());

    core::Device::DeviceType device_type = device.GetType();
    if (device_type == core::Device::DeviceType::CPU) {
        ComputeTransformationFromPoseImpl(transformation_ptr, X_ptr);
    } else if (device_type == core::Device::DeviceType::CUDA) {
#ifdef BUILD_CUDA_MODULE
        ComputeTransformationFromPoseImplCUDA(transformation_ptr, X_ptr);
#else
        utility::LogError("Not compiled with CUDA, but CUDA device is used.");
#endif
    } else {
        utility::LogError("Unimplemented device.");
    }

    // Translation from Pose X
    transformation.SetItem(
            {core::TensorKey::Slice(0, 3, 1), core::TensorKey::Slice(3, 4, 1)},
            X.GetItem({core::TensorKey::Slice(3, 6, 1)}).Reshape({3, 1}));
    // Current Implementation DOES NOT SUPPORT SCALE transfomation
    transformation[3][3] = 1;
    return transformation;
}

}  // namespace pipelines
}  // namespace t
}  // namespace open3d