// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/optimizer/rewrite_rule.h"

namespace onnxruntime {

/**
@Class ConvAddFusion

Rewrite rule that fuses two Conv+Add nodes to a single Conv node.

It is attempted to be triggered only on nodes with op type "Conv".
*/
class ConvAddFusion : public RewriteRule {
 public:
  ConvAddFusion() noexcept : RewriteRule("ConvAddFusion"){}

 private:
  /** Apply rule when op type is the following. */
  const std::string included_op_type_ = "Conv";

  bool SatisfyCondition(const Graph& graph, const Node& node) override;

  Status Apply(Graph& graph, Node& node, bool& modified, bool& deleted) override;
};

}  // namespace onnxruntime
