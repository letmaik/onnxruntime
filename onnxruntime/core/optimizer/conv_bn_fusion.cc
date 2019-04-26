// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/graph/graph_utils.h"
#include "core/optimizer/initializer.h"
#include "core/optimizer/conv_bn_fusion.h"

using namespace ONNX_NAMESPACE;
using namespace ::onnxruntime::common;
namespace onnxruntime {

Status ConvBNFusion::Apply(Graph& graph, Node& node, bool& modified, bool& /*removed*/) {
  auto& conv_node = node;
  const Node& bn_node = *conv_node.OutputNodesBegin();

  // Get value of attribute group
  const onnxruntime::NodeAttributes& conv_attributes = conv_node.GetAttributes();
  const ONNX_NAMESPACE::AttributeProto* group_attr = &(conv_attributes.find("group")->second);
  if (group_attr != nullptr &&
      group_attr->type() == AttributeProto_AttributeType_INT &&
      group_attr->has_i() && group_attr->i() != 1) {
    return Status::OK();
  }

  // Get value of attribute epsilon
  const onnxruntime::NodeAttributes& attributes = bn_node.GetAttributes();
  const ONNX_NAMESPACE::AttributeProto* attr = &(attributes.find("epsilon")->second);
  if (attr == nullptr || attr->type() != AttributeProto_AttributeType_FLOAT) {
    return Status::OK();
  }
  float epsilon = static_cast<float>(attr->f());

  // Get initializers of BatchNormalization
  const auto& bn_inputs = bn_node.InputDefs();
  const ONNX_NAMESPACE::TensorProto* bn_scale_tensor_proto = nullptr;
  graph.GetInitializedTensor(bn_inputs[1]->Name(), bn_scale_tensor_proto);

  const ONNX_NAMESPACE::TensorProto* bn_B_tensor_proto = nullptr;
  graph.GetInitializedTensor(bn_inputs[2]->Name(), bn_B_tensor_proto);

  const ONNX_NAMESPACE::TensorProto* bn_mean_tensor_proto = nullptr;
  graph.GetInitializedTensor(bn_inputs[3]->Name(), bn_mean_tensor_proto);

  const ONNX_NAMESPACE::TensorProto* bn_var_tensor_proto = nullptr;
  graph.GetInitializedTensor(bn_inputs[4]->Name(), bn_var_tensor_proto);

  const auto& conv_inputs = conv_node.InputDefs();
  const ONNX_NAMESPACE::TensorProto* conv_W_tensor_proto = nullptr;
  graph.GetInitializedTensor(conv_inputs[1]->Name(), conv_W_tensor_proto);

  // Currently, fusion is only supported for float or double data type.
  if (!Initializer::IsSupportedDataType(bn_scale_tensor_proto) ||
      !Initializer::IsSupportedDataType(bn_B_tensor_proto) ||
      !Initializer::IsSupportedDataType(bn_mean_tensor_proto) ||
      !Initializer::IsSupportedDataType(bn_var_tensor_proto) ||
      !Initializer::IsSupportedDataType(conv_W_tensor_proto) ||
      bn_scale_tensor_proto->dims_size() != 1 ||
      bn_B_tensor_proto->dims_size() != 1 ||
      bn_mean_tensor_proto->dims_size() != 1 ||
      bn_var_tensor_proto->dims_size() != 1 ||
      bn_scale_tensor_proto->dims(0) != bn_B_tensor_proto->dims(0) ||
      bn_B_tensor_proto->dims(0) != bn_mean_tensor_proto->dims(0) ||
      bn_mean_tensor_proto->dims(0) != bn_var_tensor_proto->dims(0) ||
      bn_scale_tensor_proto->data_type() != bn_B_tensor_proto->data_type() ||
      bn_B_tensor_proto->data_type() != bn_mean_tensor_proto->data_type() ||
      bn_mean_tensor_proto->data_type() != bn_var_tensor_proto->data_type() ||
      conv_W_tensor_proto->data_type() != bn_scale_tensor_proto->data_type() ||
      !(conv_W_tensor_proto->dims_size() > 2 && conv_W_tensor_proto->dims(0) == bn_scale_tensor_proto->dims(0))) {
    return Status::OK();
  }

  auto bn_scale = std::make_unique<Initializer>(bn_scale_tensor_proto);
  auto bn_B = std::make_unique<Initializer>(bn_B_tensor_proto);
  auto bn_mean = std::make_unique<Initializer>(bn_mean_tensor_proto);
  auto bn_var = std::make_unique<Initializer>(bn_var_tensor_proto);
  auto conv_W = std::make_unique<Initializer>(conv_W_tensor_proto);

  const ONNX_NAMESPACE::TensorProto* conv_B_tensor_proto = nullptr;
  std::unique_ptr<Initializer> conv_B = nullptr;
  if (conv_inputs.size() == 3) {
    if (!graph.GetInitializedTensor(conv_inputs[2]->Name(), conv_B_tensor_proto)) {
      return Status::OK();
    }

    if (!Initializer::IsSupportedDataType(conv_B_tensor_proto) ||
        conv_B_tensor_proto->dims_size() != 1 ||
        conv_B_tensor_proto->dims(0) != bn_B_tensor_proto->dims(0) ||
        conv_B_tensor_proto->data_type() != bn_B_tensor_proto->data_type()) {
      return Status::OK();
    }
    conv_B = std::make_unique<Initializer>(conv_B_tensor_proto);
  }

  // Calculate new value of initializers of conv node
  bn_var->add(epsilon);
  bn_var->sqrt();
  bn_scale->div(*bn_var);
  conv_W->scale_by_axis(*bn_scale, 1);

  if (conv_inputs.size() == 3) {
    conv_B->sub(*bn_mean);
    conv_B->mul(*bn_scale);
    conv_B->add(*bn_B);
  } else {
    bn_mean->mul(*bn_scale);
    bn_B->sub(*bn_mean);
  }

  // Create new initializers of conv
  ONNX_NAMESPACE::TensorProto new_conv_W_tensor_proto(*conv_W_tensor_proto);
  conv_W->ToProto(&new_conv_W_tensor_proto);

  ONNX_NAMESPACE::TensorProto new_conv_B_tensor_proto;
  NodeArg* bn_B_node_arg = nullptr;
  if (conv_inputs.size() == 3) {
    conv_B->ToProto(&new_conv_B_tensor_proto);
  } else {
    bn_B->ToProto(&new_conv_B_tensor_proto);
    bn_B_node_arg = graph.GetNodeArg(bn_B_tensor_proto->name());
    if (bn_B_node_arg == nullptr) {
      return Status::OK();
    }
  }

  // Replace initializers of conv node
  graph.RemoveInitializedTensor(conv_W_tensor_proto->name());
  if (conv_inputs.size() == 3) {
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6011)  // Not deferencing null pointer. conv_B_tensor_proto is set on line 93
#endif
    graph.RemoveInitializedTensor(conv_B_tensor_proto->name());
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  } else {
    graph.RemoveInitializedTensor(bn_B_tensor_proto->name());
    conv_node.MutableInputDefs().push_back(bn_B_node_arg);
    conv_node.MutableInputArgsCount()[2] = 1;
  }
  graph.AddInitializedTensor(new_conv_W_tensor_proto);
  graph.AddInitializedTensor(new_conv_B_tensor_proto);

  // Remove BN node.
  auto* bn_node_to_remove = graph.GetNode(bn_node.Index());
  if (graph_utils::RemoveNode(graph, *bn_node_to_remove)) {
    modified = true;
  }

  return Status::OK();
}

bool ConvBNFusion::SatisfyCondition(const Graph& graph, const Node& node) {
  if (!graph_utils::IsSupportedOptypeVersionAndDomain(node, "Conv", 1) ||
      node.GetOutputEdgesCount() != 1) {
    return false;
  }

  const auto& next_node = *node.OutputNodesBegin();
  if (!graph_utils::IsSupportedOptypeVersionAndDomain(next_node, "BatchNormalization", 7) ||
      next_node.GetInputEdgesCount() != 1 ||
      graph.IsNodeOutputsInGraphOutputs(next_node) ||
      next_node.GetExecutionProviderType() != node.GetExecutionProviderType()) {
    return false;
  }

  return true;
}

}  // namespace onnxruntime
