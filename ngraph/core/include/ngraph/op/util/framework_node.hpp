// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <vector>

#include "ngraph/op/op.hpp"
#include "ngraph/partial_shape.hpp"

namespace ngraph
{
    namespace op
    {
        namespace util
        {
            class NGRAPH_API FrameworkNodeAttrs
            {
            public:
                void set_opset_name(const std::string& opset_name) { m_opset_name = opset_name; }

                void set_type_name(const std::string& type_name) { m_type_name = type_name; }

                const std::string& get_opset_name() const { return m_opset_name; }

                const std::string& get_type_name() const { return m_type_name; }

                const std::map<std::string, std::string>& get_attrs() const { return m_attrs; }

                void set_attrs(const std::map<std::string, std::string>& attrs) { m_attrs = attrs; }

            private:
                std::string m_type_name;
                std::string m_opset_name;

                std::map<std::string, std::string> m_attrs;
            };

            class NGRAPH_API FrameworkNode : public Op
            {
            public:
                NGRAPH_RTTI_DECLARATION;

                explicit FrameworkNode(const OutputVector& inputs);

                void validate_and_infer_types() override;

                bool visit_attributes(AttributeVisitor& visitor) override
                {
                    visitor.on_attribute("framework_node_attrs", m_attrs);
                    return true;
                }

                std::shared_ptr<Node>
                    clone_with_new_inputs(const OutputVector& new_args) const override;

            private:
                std::vector<std::tuple<ngraph::PartialShape, ngraph::element::Type>> m_inputs_desc;

                FrameworkNodeAttrs m_attrs;
            };
        } // namespace util
    }     // namespace op

    template <>
    class NGRAPH_API AttributeAdapter<op::util::FrameworkNodeAttrs>
        : public DirectValueAccessor<op::util::FrameworkNodeAttrs>
    {
    public:
        AttributeAdapter(op::util::FrameworkNodeAttrs& value);

        static constexpr DiscreteTypeInfo type_info{"AttributeAdapter<FrameworkNodeAttr>", 0};
        const DiscreteTypeInfo& get_type_info() const override { return type_info; }
    };
} // namespace ngraph
