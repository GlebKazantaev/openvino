// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "transformations/common_optimizations/pruning.hpp"
#include "transformations/rt_info/mask_attribute.hpp"
#include "itt.hpp"

#include <ngraph/pattern/op/wrap_type.hpp>
#include <ngraph/opsets/opset6.hpp>

NGRAPH_RTTI_DEFINITION(ngraph::pass::PropagateMasks, "PropagateMasks", 0);

namespace ngraph {
namespace pass {
namespace mask_propagation {

class Convolution;
class Elementwise;
class Pooling;
class Reshape;
class MatMul;
class PassThrough;
class StopPropagation;

} // namespace mask_propagation
} // namespace pass
} // namespace ngraph

class ngraph::pass::mask_propagation::Convolution : public MatcherPass {
public:
    Convolution() {
        MATCHER_SCOPE(Convolution);
        auto input = pattern::any_input();
        auto weights = pattern::wrap_type<opset6::Constant>();
        auto conv = pattern::wrap_type<opset6::Convolution>({input, weights});

        ngraph::matcher_pass_callback callback = [=](ngraph::pattern::Matcher& m) {
            const auto & pattern_map = m.get_pattern_value_map();
            const auto & m_weights = pattern_map.at(weights);
            const auto & m_output = pattern_map.at(conv);
            const auto & m_input = pattern_map.at(input);

            InitConstMask({0}/* check only output channel */).apply(m_weights.get_node_shared_ptr());
            auto weights_mask = getMask(m_weights);
            if (weights_mask->all_dims_are_empty())
                std::cout << "MASK is empty for " << *m_output.get_node() << std::endl;

            if (auto input_mask = getMask(m_input)) {
                // Update weights mask to shrink by input channel
                auto weights_input_dim_mask = weights_mask->at(1/*input dim*/);
                auto data_input_mask = input_mask->at(1/*channel dim*/);
                *weights_input_dim_mask = *data_input_mask;
                weights_input_dim_mask->add_parent(data_input_mask);
            }

            // Create mask that describes which channel dimensions will be removed
            auto conv_mask = std::make_shared<Mask>(m_weights.get_shape().size());
            auto conv_channel_dim_mask = conv_mask->at(1/*channel dim*/);
            auto weights_output_dim_mask = weights_mask->at(0/*output dim*/);
            *conv_channel_dim_mask = *weights_output_dim_mask;
            conv_channel_dim_mask->add_parent(weights_output_dim_mask);
            setMask(m_output, conv_mask);

            return true;
        };

        auto m = std::make_shared<ngraph::pattern::Matcher>(conv, matcher_name);
        register_matcher(m, callback);
    }
};

class ngraph::pass::mask_propagation::Elementwise : public MatcherPass {
public:
    Elementwise() {
        MATCHER_SCOPE(Convolution);
        auto input = pattern::any_input();
        auto weights = pattern::any_input();
        auto eltwise = pattern::wrap_type<op::util::BinaryElementwiseArithmetic>({input, weights},
                                                                                 pattern::has_static_rank());

        ngraph::matcher_pass_callback callback = [=](ngraph::pattern::Matcher& m) {
            const auto & pattern_map = m.get_pattern_value_map();
            const auto & m_weights = pattern_map.at(weights);
            const auto & m_output = pattern_map.at(eltwise);
            const auto & m_input = pattern_map.at(input);

            if (m_weights.get_shape().size() < 2 || m_input.get_shape().size() < 2) return false;
            // In case if one of the inputs is constant
            // TODO: need to find channel dimension instead of hardcoded zero
            InitConstMask({0}).apply(m_input.get_node_shared_ptr());
            InitConstMask({0}).apply(m_weights.get_node_shared_ptr());

            auto weights_mask = getMask(m_weights);
            auto input_mask = getMask(m_input);

            if (input_mask && weights_mask) {
                // Merge masks from two inputs
                auto output_mask = std::make_shared<Mask>(m_output.get_partial_shape().rank().get_length());
                auto omask_iter = output_mask->rbegin();
                auto imask_iter = input_mask->rbegin();
                auto wmask_iter = weights_mask->rbegin();

                while (imask_iter != input_mask->rend() &&
                       wmask_iter != weights_mask->rend()) {
                    for (const auto & value : **imask_iter) {
                        if (wmask_iter->get()->count(value)) {
                            omask_iter->get()->insert(value);
                        }
                    }
                    omask_iter->get()->add_parent(*imask_iter);
                    omask_iter->get()->add_parent(*wmask_iter);
                    omask_iter->get()->update_dependencies();

                    omask_iter++;
                    imask_iter++;
                    wmask_iter++;
                }

                //output_mask->update_dependencies();
                setMask(m_output, output_mask);
                return true;
            }

            std::cout << "No mask for: " << m_output.get_node()->get_friendly_name() << std::endl;
            return false;
        };

        auto m = std::make_shared<ngraph::pattern::Matcher>(eltwise, matcher_name);
        register_matcher(m, callback);
    }
};

class ngraph::pass::mask_propagation::Pooling : public MatcherPass {
public:
    Pooling() {
        MATCHER_SCOPE(Pooling);
        auto input = pattern::any_input();
        auto pool = pattern::wrap_type<opset6::AvgPool, opset6::MaxPool>({input});

        ngraph::matcher_pass_callback callback = [=](ngraph::pattern::Matcher& m) {
            const auto & pattern_map = m.get_pattern_value_map();
            const auto & m_output = pattern_map.at(pool);
            const auto & m_input = pattern_map.at(input);

            if (auto input_mask = getMask(m_input)) {
                auto pool_mask = std::make_shared<Mask>(input_mask->size());
                auto channel_input_dim_mask = input_mask->at(1/*input channel dim*/);
                auto channel_output_dim_mask = pool_mask->at(1/*input channel dim*/);
                *channel_output_dim_mask = *channel_input_dim_mask;
                for (size_t dim = 0; dim < pool_mask->size(); ++dim) {
                    pool_mask->at(dim)->add_parent(input_mask->at(dim));
                }
                pool_mask->update_dependencies();
                setMask(m_output, pool_mask);
                return true;
            }
            return false;
        };

        auto m = std::make_shared<ngraph::pattern::Matcher>(pool, matcher_name);
        register_matcher(m, callback);
    }
};

class ngraph::pass::mask_propagation::Reshape : public MatcherPass {
public:
    Reshape() {
        MATCHER_SCOPE(Convolution);
        auto input = pattern::any_input();
        auto shape_pattern = pattern::any_input();
        auto reshape = pattern::wrap_type<opset6::Reshape>({input, shape_pattern});

        ngraph::matcher_pass_callback callback = [=](ngraph::pattern::Matcher& m) {
            const auto & pattern_map = m.get_pattern_value_map();
            const auto & m_output = pattern_map.at(reshape);
            const auto & m_input = pattern_map.at(input);
            const auto & m_shape = pattern_map.at(shape_pattern);

            if (auto input_mask = getMask(m_input)) {
                const auto & output_shape = m_output.get_shape();
                auto reshape_mask = std::make_shared<Mask>(output_shape.size());

                auto reshape_constant_mask = std::make_shared<Mask>(output_shape.size());
                reshape_constant_mask->set_shape_like(true);

                auto channel_input_dim_mask = input_mask->at(1/*input channel dim*/);

                // TODO: implement normal check
                auto channel_output_dim_mask = reshape_mask->at(1/*input channel dim*/);
                auto const_channel_output_dims_mask = reshape_constant_mask->at(1);
                *channel_output_dim_mask = *channel_input_dim_mask;
                *const_channel_output_dims_mask = *channel_input_dim_mask;
                channel_output_dim_mask->add_parent(channel_input_dim_mask);
                setMask(m_output, reshape_mask);
                setMask(m_shape, reshape_constant_mask);
                return true;
            }
            return false;
        };

        auto m = std::make_shared<ngraph::pattern::Matcher>(reshape, matcher_name);
        register_matcher(m, callback);
    }
};

class ngraph::pass::mask_propagation::MatMul : public MatcherPass {
public:
    MatMul() {
        MATCHER_SCOPE(MatMul);
        auto input = pattern::any_input();
        auto weights = pattern::any_input();
        auto matmul = pattern::wrap_type<opset6::MatMul>({input, weights});

        ngraph::matcher_pass_callback callback = [=](ngraph::pattern::Matcher& m) {
            const auto & pattern_map = m.get_pattern_value_map();
            const auto & m_output = pattern_map.at(matmul);
            const auto & m_input = pattern_map.at(input);
            const auto & m_weights = pattern_map.at(weights);
            const auto & m_matmul = std::dynamic_pointer_cast<opset6::MatMul>(m_output.get_node_shared_ptr());
            if (!m_matmul) return false;


            if (auto input_mask = getMask(m_input)) {
                auto weights_mask = std::make_shared<Mask>(m_weights.get_shape().size());

                auto channel_input_dim_mask = input_mask->at(1/*input channel dim*/);

                // TODO: implement normal check
                int64_t input_channel_dim = m_matmul->get_transpose_b() ? 1 : 0;
                auto channel_output_dim_mask = weights_mask->at(input_channel_dim);
                *channel_output_dim_mask = *channel_input_dim_mask;
                channel_output_dim_mask->add_parent(channel_input_dim_mask);
                setMask(m_weights, weights_mask);
                return true;
            }
            return false;
        };

        auto m = std::make_shared<ngraph::pattern::Matcher>(matmul, matcher_name);
        register_matcher(m, callback);
    }
};

class ngraph::pass::mask_propagation::PassThrough : public MatcherPass {
public:
    PassThrough() {
        MATCHER_SCOPE(PassThrough);
        auto input = pattern::any_input();
        auto unary_op = pattern::wrap_type<op::util::UnaryElementwiseArithmetic>({input});

        ngraph::matcher_pass_callback callback = [=](ngraph::pattern::Matcher& m) {
            const auto & pattern_map = m.get_pattern_value_map();
            const auto & m_input = pattern_map.at(input);
            const auto & m_output = pattern_map.at(unary_op);

            if (auto input_mask = getMask(m_input)) {
                setMask(m_output, input_mask);
            }

            return true;
        };

        auto m = std::make_shared<ngraph::pattern::Matcher>(unary_op, matcher_name);
        register_matcher(m, callback);
    }
};

class ngraph::pass::mask_propagation::StopPropagation : public MatcherPass {
public:
    StopPropagation() {
        MATCHER_SCOPE(StopPropagation);
        auto any_node = pattern::any_input();

        ngraph::matcher_pass_callback callback = [](ngraph::pattern::Matcher& m) {
            const auto & node = m.get_match_root();
            for (const auto & input : node->input_values()) {
                if (auto mask = getMask(input)) {
                    // Invalidate current mask and its parent masks
                    mask->invalidate();
                    std::cout << "Invalidate masks for " << *input.get_node() << " because " << node << " is unknown\n";
                }
            }
            return true;
        };

        auto m = std::make_shared<ngraph::pattern::Matcher>(any_node, matcher_name);
        register_matcher(m, callback);
    }
};

ngraph::pass::PropagateMasks::PropagateMasks() {
    add_matcher<mask_propagation::Convolution>();
    add_matcher<mask_propagation::Elementwise>();
    add_matcher<mask_propagation::Pooling>();
    add_matcher<mask_propagation::Reshape>();
    add_matcher<mask_propagation::MatMul>();
    add_matcher<mask_propagation::PassThrough>();
    add_matcher<mask_propagation::StopPropagation>();
}
