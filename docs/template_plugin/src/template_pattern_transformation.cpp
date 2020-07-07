// Copyright (C) 2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "template_pattern_transformation.hpp"

#include <ngraph/opsets/opset3.hpp>
#include <ngraph/rt_info.hpp>
#include <ngraph/pass/manager.hpp>

using namespace ngraph;

// ! [graph_rewrite:template_transformation_cpp]
// template_pattern_transformation.cpp
ngraph::pass::DecomposeDivideMatcher::DecomposeDivideMatcher() {
    // Pattern example
    auto input0 = std::make_shared<pattern::op::Label>(element::f32, Shape{});
    auto input1 = std::make_shared<pattern::op::Label>(element::f32, Shape{});
    auto div = std::make_shared<ngraph::opset3::Divide>(input0, input1);

    ngraph::graph_rewrite_callback callback = [](pattern::Matcher& m) {
        auto div = std::dynamic_pointer_cast<ngraph::opset3::Divide> (m.get_match_root());
        // We can not apply this transformation in case with integer input data type
        if (!div || div->input(0).get_element_type().is_integral()) {
            return false;
        }

        // Decompose Divide into Multiply with Power operations
        auto pow = std::make_shared<ngraph::opset3::Power>(div->input_value(1),
                                                           opset3::Constant::create(div->get_input_element_type(1), Shape{1}, {-1}));

        auto mul = std::make_shared<ngraph::opset3::Multiply>(div->input_value(0), pow);

        // Save original name to last operation in replacement sub-graph
        mul->set_friendly_name(div->get_friendly_name());

        // Copy runtime info attributes to newly created operation
        ngraph::copy_runtime_info(div, {pow, mul});

        // Replace Divide operation with Multiply
        ngraph::replace_node(div, mul);

        // Return true as the root node was changed
        return true;
    };

    // Register pattern with Divide operation as a pattern root node
    auto m = std::make_shared<ngraph::pattern::Matcher>(div, "ConvertDivide");
    // Register Matcher
    this->register_matcher(m, callback);
}
// ! [graph_rewrite:template_transformation_cpp]

// ! [matcher_pass:relu_fusion]
ngraph::pass::ReluReluFusionMatcher::ReluReluFusionMatcher() {
    auto m_relu1 = std::make_shared<pattern::op::Label>(element::f32, Shape{}, [](std::shared_ptr<Node> node) -> bool {
            // Check that node has type opset3::Relu and has only one consumer
            return std::dynamic_pointer_cast<opset3::Relu>(node) && node->output(0).get_target_inputs().size() == 1;
        });
    auto m_relu2 = std::make_shared<ngraph::opset3::Relu>(m_relu1);

    ngraph::graph_rewrite_callback callback = [=](pattern::Matcher& m) {
        // Map that helps to connect labels with matched outputs
        auto & label_to_output = m.get_pattern_value_map();

        // Create new Relu operation and add register it for additional execution (pattern matching) using register_new_node
        auto new_relu = register_new_node<ngraph::opset3::Relu>(label_to_output.at(m_relu1));

        // Copy runtime info attributes to newly created operation
        ngraph::copy_runtime_info(m.get_matched_nodes(), new_relu);

        // Save last Relu name to new Relu operation
        new_relu->set_friendly_name(m.get_match_root()->get_friendly_name());

        // Replace Relu->Relu with Relu
        ngraph::replace_node(m.get_match_root(), new_relu);

        // Return true as the root node was changed
        return true;
    };

    // Register pattern with Relu operation as a pattern root node
    auto m = std::make_shared<ngraph::pattern::Matcher>(m_relu2, "ReluReluFusion");
    // Register Matcher
    this->register_matcher(m, callback);
}
// ! [matcher_pass:relu_fusion]

void run_matcher_on_node(std::shared_ptr<ngraph::Node> node) {
// ! [matcher_pass:run_on_node]
if (ngraph::pass::DecomposeDivideMatcher().apply(node)) {
    // successful execution (root node was replaced)
}
// ! [matcher_pass:run_on_node]
}

void run_matcher_with_manager(std::shared_ptr<ngraph::Function> f) {
// ! [matcher_pass:manager]
// Two matchers will run independently (two independent graph traversals)
pass::Manager manager;
manager.register_pass<ngraph::pass::DecomposeDivideMatcher>();
manager.register_pass<ngraph::pass::ReluReluFusionMatcher>();
manager.run_passes(f);
// ! [matcher_pass:manager]
}

void run_matcher_with_gr(std::shared_ptr<ngraph::Function> f) {
// ! [matcher_pass:graph_rewrite]
// Two matcher passes will run simultaneously in a single graph traversal
ngraph::pass::GraphRewrite pass;
pass.add_matcher<ngraph::pass::DecomposeDivideMatcher>();
pass.add_matcher<ngraph::pass::ReluReluFusionMatcher>();
pass.run_on_function(f);
// ! [matcher_pass:graph_rewrite]
}