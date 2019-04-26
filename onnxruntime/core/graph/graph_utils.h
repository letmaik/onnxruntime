// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/graph/onnx_protobuf.h"
#include "core/graph/graph.h"

namespace onnxruntime {

namespace graph_utils {

bool IsSupportedOptypeVersionAndDomain(const Node& node,
                                       const std::string& op_type,
                                       ONNX_NAMESPACE::OperatorSetVersion version,
                                       const std::string& domain = kOnnxDomainAlias);

/* Returns true if the execution provider assigned to current node is present in the compatible providers list
 * or if the compatible_providers list is empty
 */
bool IsSupportedProvider(const Node& node,
                         const std::unordered_set<std::string>& compatible_providers);

/** Check whether the node has a single input and a single output. The single input can be either the output of
    another node or an initializer, but not an implicit input from a parent subgraph. The single output can be 
    fed to multiple downstream operators, i.e., it can have multiple output edges. */
bool IsSingleInSingleOutNode(const Node& node);

/** Returns true if the graph has the given input.*/
bool IsGraphInput(const Graph& graph, const NodeArg* input);

/** Checks if the given node has only constant inputs (initializers). */
bool AllNodeInputsAreConstant(const Graph& graph, const Node& node);

/** Get the name of the incoming NodeArg with the specified index for the given node. */
const std::string& GetNodeInputName(const Node& node, int index);

/** Get the name of the outgoing NodeArg with the specified index for the given node. */
const std::string& GetNodeOutputName(const Node& node, int index);

/** Return the attribute of a Node with a given name. */
const ONNX_NAMESPACE::AttributeProto* GetNodeAttribute(const Node& node, const std::string& attr_name);

/** Retrieve the values for a repeated attribute of a node and place them to the values vector. */
template <typename T>
bool GetRepeatedNodeAttributeValues(const Node& node,
                                    const std::string& attr_name,
                                    std::vector<T>& values) {
  const auto* attr = graph_utils::GetNodeAttribute(node, attr_name);
  if (attr) {
    values = ONNX_NAMESPACE::RetrieveValues<T>(*attr);
    return true;
  } else {
    return false;
  }
}

Status ForAllMutableSubgraphs(Graph& main_graph, std::function<Status(Graph&)> func);
Status ForAllSubgraphs(const Graph& main_graph, std::function<Status(const Graph&)> func);

/** Remove the given Node from the Graph and keep Graph consistent by rebuilding needed connections.
    We support the removal of the Node if it has no implicit inputs and a single output (but it can have multiple
    output edges). Then we support two cases based on the Node's inputs:
    - If the Node has a single incoming node (and possibly multiple initializers), we can remove the Node and
      connect its incoming node to its outgoing nodes.
    - If the Node has a single initializer as input, we remove the Node and feed the initializer as input to its
      output nodes. */
bool RemoveNode(Graph& graph, Node& node);

/** Remove all output edges from the given Node of the Graph. 
    This should probably be elevated to the Graph API eventually. */
size_t RemoveNodeOutputEdges(Graph& graph, Node& node);

}  // namespace graph_utils

}  // namespace onnxruntime
