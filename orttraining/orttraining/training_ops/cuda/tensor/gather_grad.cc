// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "orttraining/training_ops/cuda/tensor/gather_grad.h"

#include "core/common/safeint.h"
#include "core/providers/common.h"
#include "orttraining/training_ops/cuda/tensor/gather_grad_impl.h"

namespace onnxruntime {
namespace cuda {

#if defined(CUDA_VERSION) && CUDA_VERSION >= 11000
#define ALL_IEEE_FLOAT_TENSOR_TYPES           \
  { DataTypeImpl::GetTensorType<float>(),     \
    DataTypeImpl::GetTensorType<double>(),    \
    DataTypeImpl::GetTensorType<MLFloat16>(), \
    DataTypeImpl::GetTensorType<BFloat16>() }
#else
#define ALL_IEEE_FLOAT_TENSOR_TYPES DataTypeImpl::AllIEEEFloatTensorTypes()
#endif

ONNX_OPERATOR_KERNEL_EX(
    GatherGrad,
    kMSDomain,
    1,
    kCudaExecutionProvider,
    (*KernelDefBuilder::Create())
        .InputMemoryType(OrtMemTypeCPUInput, 0)
        .InputMemoryType(OrtMemTypeCPU, 3)
        .InputMemoryType(OrtMemTypeCPU, 5)
        .InputMemoryType(OrtMemTypeCPU, 6)
        .TypeConstraint("I", DataTypeImpl::GetTensorType<int64_t>())
        .TypeConstraint("Int32", DataTypeImpl::GetTensorType<int32_t>())
        .TypeConstraint("T", ALL_IEEE_FLOAT_TENSOR_TYPES)
        .TypeConstraint("Tind", std::vector<MLDataType>{
                                    DataTypeImpl::GetTensorType<int32_t>(),
                                    DataTypeImpl::GetTensorType<int64_t>()}),
    GatherGrad);

namespace {
template <typename T, typename TIndex>
Status CallGatherGradImpl(
    cudaStream_t stream,
    const CudaScratchBufferAllocator& allocator,
    int64_t num_gathered_per_index, int64_t gather_dimension_size, int64_t num_batches,
    const int32_t num_segments,
    const int32_t* segment_offsets,
    const int32_t last_segment_partial_segment_count,
    const int32_t last_segment_partial_segment_offset,
    const int32_t* per_segment_partial_segment_counts,
    const int32_t* per_segment_partial_segment_offsets,
    const Tensor& dX_indices_sorted, const Tensor& dY_indices_sorted,
    const Tensor& dY, const Tensor& gathered_indices,
    Tensor& dX) {
  using CudaT = typename ToCudaType<T>::MappedType;

  const T* dY_data = dY.template Data<T>();
  T* dX_data = dX.template MutableData<T>();
  const TIndex* indices_data = gathered_indices.template Data<TIndex>();

  const TIndex* dX_indices_sorted_data = dX_indices_sorted.template Data<TIndex>();
  const TIndex* dY_indices_sorted_data = dY_indices_sorted.template Data<TIndex>();

  const SafeInt<GatheredIndexIndex_t> num_gathered_indices{gathered_indices.Shape().Size()};

  GatherGradImpl(
      stream,
      allocator,
      reinterpret_cast<const CudaT*>(dY_data),
      indices_data,
      num_gathered_indices,
      gather_dimension_size,
      num_gathered_per_index,
      num_batches,
      num_segments,
      segment_offsets,
      last_segment_partial_segment_count,
      last_segment_partial_segment_offset,
      per_segment_partial_segment_counts,
      per_segment_partial_segment_offsets,
      dX_indices_sorted_data,
      dY_indices_sorted_data,
      reinterpret_cast<CudaT*>(dX_data));

  return Status::OK();
}

template <typename T>
Status DispatchToGatherGradImplByTindex(
    cudaStream_t stream,
    MLDataType tindex_data_type,
    const CudaScratchBufferAllocator& allocator,
    int64_t num_gathered_per_index, int64_t gather_dimension_size, int64_t num_batches,
    const int32_t num_segments,
    const int32_t* segment_offsets,
    const int32_t last_segment_partial_segment_count,
    const int32_t last_segment_partial_segment_offset,
    const int32_t* per_segment_partial_segment_counts,
    const int32_t* per_segment_partial_segment_offsets,
    const Tensor& dX_indices_sorted, const Tensor& dY_indices_sorted,
    const Tensor& dY, const Tensor& gathered_indices,
    Tensor& dX) {
  if (utils::IsPrimitiveDataType<int32_t>(tindex_data_type)) {
    return CallGatherGradImpl<T, int32_t>(
        stream, allocator, num_gathered_per_index, gather_dimension_size, num_batches,
        num_segments,
        segment_offsets,
        last_segment_partial_segment_count,
        last_segment_partial_segment_offset,
        per_segment_partial_segment_counts,
        per_segment_partial_segment_offsets,
        dX_indices_sorted,
        dY_indices_sorted,
        dY, gathered_indices, dX);
  } else if (utils::IsPrimitiveDataType<int64_t>(tindex_data_type)) {
    return CallGatherGradImpl<T, int64_t>(
        stream, allocator, num_gathered_per_index, gather_dimension_size, num_batches,
        num_segments,
        segment_offsets,
        last_segment_partial_segment_count,
        last_segment_partial_segment_offset,
        per_segment_partial_segment_counts,
        per_segment_partial_segment_offsets,
        dX_indices_sorted,
        dY_indices_sorted,
        dY, gathered_indices, dX);
  }

  return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "GatherGrad unsupported TIndex type: ", tindex_data_type);
}

Status DispatchToGatherGradImpl(
    cudaStream_t stream,
    MLDataType t_data_type, MLDataType tindex_data_type,
    const CudaScratchBufferAllocator& allocator,
    int64_t num_gathered_per_index, int64_t gather_dimension_size, int64_t num_batches,
    const int32_t num_segments,
    const int32_t* segment_offsets,
    const int32_t last_segment_partial_segment_count,
    const int32_t last_segment_partial_segment_offset,
    const int32_t* per_segment_partial_segment_counts,
    const int32_t* per_segment_partial_segment_offsets,
    const Tensor& dX_indices_sorted, const Tensor& dY_indices_sorted,
    const Tensor& dY, const Tensor& gathered_indices,
    Tensor& dX) {
  if (utils::IsPrimitiveDataType<float>(t_data_type)) {
    return DispatchToGatherGradImplByTindex<float>(
        stream, tindex_data_type, allocator, num_gathered_per_index, gather_dimension_size, num_batches,
        num_segments,
        segment_offsets,
        last_segment_partial_segment_count,
        last_segment_partial_segment_offset,
        per_segment_partial_segment_counts,
        per_segment_partial_segment_offsets,
        dX_indices_sorted, dY_indices_sorted,
        dY, gathered_indices, dX);
  } else if (utils::IsPrimitiveDataType<MLFloat16>(t_data_type)) {
    return DispatchToGatherGradImplByTindex<MLFloat16>(
        stream, tindex_data_type, allocator, num_gathered_per_index, gather_dimension_size, num_batches,
        num_segments,
        segment_offsets,
        last_segment_partial_segment_count,
        last_segment_partial_segment_offset,
        per_segment_partial_segment_counts,
        per_segment_partial_segment_offsets,
        dX_indices_sorted, dY_indices_sorted,
        dY, gathered_indices, dX);
#if defined(CUDA_VERSION) && CUDA_VERSION >= 11000
  } else if (utils::IsPrimitiveDataType<BFloat16>(t_data_type)) {
    return DispatchToGatherGradImplByTindex<BFloat16>(
        stream, tindex_data_type, allocator, num_gathered_per_index, gather_dimension_size, num_batches,
        num_segments,
        segment_offsets,
        last_segment_partial_segment_count,
        last_segment_partial_segment_offset,
        per_segment_partial_segment_counts,
        per_segment_partial_segment_offsets,
        dX_indices_sorted, dY_indices_sorted,
        dY, gathered_indices, dX);
#endif
  }

  return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "GatherGrad unsupported T type: ", t_data_type);
}
}  // namespace

Status GatherGrad::ComputeInternal(OpKernelContext* context) const {
  const Tensor* X_shape_tensor = context->Input<Tensor>(0);
  const TensorShape X_shape(X_shape_tensor->template Data<int64_t>(), X_shape_tensor->Shape().Size());
  const Tensor* gathered_indices = context->Input<Tensor>(1);
  const Tensor* dY = context->Input<Tensor>(2);

  const Tensor* num_segments_tensor = context->Input<Tensor>(3);
  const int32_t num_segments = *num_segments_tensor->template Data<int32_t>();

  const Tensor* segment_offsets_tensor = context->Input<Tensor>(4);
  const int32_t* segment_offsets = segment_offsets_tensor->template Data<int32_t>();

  const Tensor* last_segment_partial_segment_count_tensor = context->Input<Tensor>(5);
  const int32_t last_segment_partial_segment_count = *last_segment_partial_segment_count_tensor->template Data<int32_t>();

  const Tensor* last_segment_partial_segment_offset_tensor = context->Input<Tensor>(6);
  const int32_t last_segment_partial_segment_offset = *last_segment_partial_segment_offset_tensor->template Data<int32_t>();

  const Tensor* per_segment_partial_segment_counts_tensor = context->Input<Tensor>(7);
  const int32_t* per_segment_partial_segment_counts = per_segment_partial_segment_counts_tensor->template Data<int32_t>();

  const Tensor* per_segment_partial_segment_offsets_tensor = context->Input<Tensor>(8);
  const int32_t* per_segment_partial_segment_offsets = per_segment_partial_segment_offsets_tensor->template Data<int32_t>();

  const Tensor* dX_indices_sorted_tensor = context->Input<Tensor>(9);

  const Tensor* dY_indices_sorted_tensor = context->Input<Tensor>(10);

  Tensor* dX = context->Output(0, X_shape);
  CUDA_RETURN_IF_ERROR(cudaMemsetAsync(dX->MutableDataRaw(), 0, dX->SizeInBytes(), Stream()));

  if (gathered_indices->Shape().Size() == 0) {
    // nothing else to do
    return Status::OK();
  }

  MLDataType t_type = dY->DataType();
  MLDataType tindex_type = gathered_indices->DataType();

  const auto axis = HandleNegativeAxis(axis_, X_shape.NumDimensions());
  const int64_t num_gathered_per_index = X_shape.SizeFromDimension(axis + 1);
  const int64_t gather_dimension_size = X_shape[axis];
  const int64_t num_batches = X_shape.SizeToDimension(axis);

  auto out = DispatchToGatherGradImpl(
      Stream(), t_type, tindex_type, CudaScratchBufferAllocator{*this},
      num_gathered_per_index, gather_dimension_size, num_batches,
      num_segments,
      segment_offsets,
      last_segment_partial_segment_count,
      last_segment_partial_segment_offset,
      per_segment_partial_segment_counts,
      per_segment_partial_segment_offsets,
      *dX_indices_sorted_tensor,
      *dY_indices_sorted_tensor,
      *dY, *gathered_indices, *dX);

  return out;
}

}  // namespace cuda
}  // namespace onnxruntime