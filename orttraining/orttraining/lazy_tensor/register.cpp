// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
// Instead of torch/torch.h, include torch torch/extension.h
// for extra Python headers.
#include <torch/extension.h>
#include <torch/csrc/jit/passes/pass_manager.h>
#include <torch/csrc/jit/passes/graph_fuser.h>
#include <torch/csrc/jit/runtime/custom_operator.h>
#include "accelerator.h"
#include "core/common/logging/logging.h"

namespace onnxruntime {
namespace lazytensor {
void register_ort_as_torch_jit_executor() {
  // Pytorch's JIT symbol to be execute by ORT.
  const auto accelerator_symbol =
      torch::jit::Symbol::fromQualString("pw::CompilationGroup");

  // First, register a pass that will coalesce supported consecutive operators
  // into a single symbol (it contains a subgraph). Encountering an unsupported
  // operator will result two seperated symbols (i.e., two independent sub-graphs).
  //
  // TODO: Allow single-op fusion in Pytorch so ORT can receive single-op sub-graph.
  torch::jit::RegisterPass pass([accelerator_symbol](std::shared_ptr<torch::jit::Graph>& g) {
    CustomFuseGraph(g, Accelerator::Supported, accelerator_symbol);
  });

  // Define a function to generate actual computation code for a
  // symbol (type: torch::jit::Node).
  //
  // TODO: Test (and enable) aliasing and in-place operators. Ideally, LazyTensor
  //       converts sub-graphs to SSA before passing them to JIT, so we should
  //       get them for free.
  torch::jit::OperationCreator op_creator =
      [](const torch::jit::Node* node) -> torch::jit::Operation {
    // Construct an accelerator instance. It's responsible
    // for executing the "node". Note that the "node" is a sub-graph.
    auto accelerator = std::make_shared<Accelerator>(node);
    return [accelerator](torch::jit::Stack& stack) {
      accelerator->Run(stack);
    };
  };

  // Tell Pytorch to use "op_creator" to execute "accelerator_symbol"
  // when executing JIT graph.
  torch::jit::RegisterOperators op({torch::jit::Operator(
      accelerator_symbol, op_creator,
      c10::AliasAnalysisKind::PURE_FUNCTION)});
}
}  // namespace lazytensor
}  // namespace onnxruntime

namespace onnxruntime{
namespace python{

void addObjectMethodsForLazyTensor(pybind11::module& m) {
  LOGS_DEFAULT(INFO) << "pybind11 module init for lazy tensor";
  m.def(
      "register_ort_as_torch_jit_executor",
      []() {
        onnxruntime::lazytensor::register_ort_as_torch_jit_executor();
      });
}

} // namespace python
} // namespace onnxruntime
