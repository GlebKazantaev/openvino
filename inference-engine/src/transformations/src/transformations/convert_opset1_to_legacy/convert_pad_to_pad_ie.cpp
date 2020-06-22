// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "transformations/convert_opset1_to_legacy/convert_pad_to_pad_ie.hpp"

#include <memory>
#include <vector>

#include <ngraph/opsets/opset1.hpp>
#include <ngraph/rt_info.hpp>

void ngraph::pass::ConvertPadToPadIE::convert_pad() {
    auto pad = std::make_shared<pattern::op::Label>(element::f32, Shape{}, pattern::has_class<ngraph::opset1::Pad>());

    ngraph::graph_rewrite_callback callback = [](pattern::Matcher& m) {
        auto pad = std::dynamic_pointer_cast<ngraph::opset1::Pad> (m.get_match_root());
        if (!pad) {
            return false;
        }

        auto pad_ie = std::make_shared<ngraph::op::PadIE>(pad);
        if (pad_ie == nullptr)
            return false;
        pad_ie->set_friendly_name(pad->get_friendly_name());
        ngraph::copy_runtime_info(pad, pad_ie);
        ngraph::replace_node(pad, pad_ie);
        return true;
    };

    auto m = std::make_shared<ngraph::pattern::Matcher>(pad, "ConvertPadToPadIE");
    this->add_matcher(m, callback, PassProperty::CHANGE_DYNAMIC_STATE);
}