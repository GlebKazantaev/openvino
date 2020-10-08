//*****************************************************************************
// Copyright 2017-2020 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#pragma once

#include <list>
#include <memory>
#include <vector>

#include "ngraph/deprecated.hpp"
#include "ngraph/function.hpp"
#include "ngraph/node.hpp"
#include "ngraph/util.hpp"

namespace ngraph
{
    namespace pass
    {
        enum class PassProperty : uint32_t
        {
            // Pass requires node shapes to be static
            REQUIRE_STATIC_SHAPE = 0x1,
            // Pass transformation will change the function's dynamic state
            CHANGE_DYNAMIC_STATE = 1 << 1,
        };

        typedef EnumMask<PassProperty> PassPropertyMask;
        const PassPropertyMask all_pass_property_off;
        using param_callback = std::function<bool(const std::shared_ptr<const ::ngraph::Node>)>;
        using param_callback_map = std::map<ngraph::DiscreteTypeInfo, param_callback>;

        class NGRAPH_API PassConfig
        {
        public:
            void disable(const DiscreteTypeInfo& type_info) { m_disabled.insert(type_info); }
            template <typename T>
            void disable()
            {
                disable(T::type_info);
            }

            void enable(const DiscreteTypeInfo& type_info) { m_disabled.erase(type_info); }
            template <typename T>
            void enable()
            {
                enable(T::type_info);
            }

            void set_callback(const param_callback& callback) { m_callback = callback; }
            template <typename... Args>
            typename std::enable_if<sizeof...(Args) == 0>::type
                set_callback(const param_callback& callback)
            {
            }

            template <typename T, class... Args>
            void set_callback(const param_callback& callback)
            {
                m_callback_map[T::type_info] = callback;
                set_callback<Args...>(callback);
            }

            param_callback get_callback(const DiscreteTypeInfo& type_info) const
            {
                const auto& it = m_callback_map.find(type_info);
                if (it != m_callback_map.end())
                {
                    return it->second;
                }
                else
                {
                    return m_callback;
                }
            }

            template <typename T>
            param_callback get_callback() const
            {
                return get_callback(T::type_info);
            }

            bool is_disabled(const DiscreteTypeInfo& type_info) const
            {
                return m_disabled.count(type_info);
            }

            template <typename T>
            bool is_disabled() const
            {
                return is_disabled(T::type_info);
            }

        private:
            param_callback m_callback = [](const std::shared_ptr<const ::ngraph::Node>&) {
                return false;
            };
            param_callback_map m_callback_map;
            std::unordered_set<DiscreteTypeInfo> m_disabled;
        };

        class NGRAPH_API PassBase
        {
            friend class Manager;

        public:
            PassBase();
            virtual ~PassBase() {}
            /// Check if this pass has all the pass properties.
            bool get_property(const PassPropertyMask& prop_mask) const;

            void set_name(const std::string& name) { m_name = name; }
            std::string get_name() const;

            void set_callback(const param_callback& callback);

            void set_pass_config(const std::shared_ptr<PassConfig>& pass_config)
            {
                m_pass_config = pass_config;
            }

            std::shared_ptr<PassConfig> get_pass_config() { return m_pass_config; }
            NGRAPH_DEPRECATED("Please use transformation_callback method instead")
            bool m_transformation_callback(const std::shared_ptr<const Node>& node)
            {
                return m_pass_config->get_callback(get_type_info())(node);
            }

            bool transformation_callback(const std::shared_ptr<const Node>& node)
            {
                return m_pass_config->get_callback(get_type_info())(node);
            }

            using type_info_t = DiscreteTypeInfo;

            virtual const type_info_t& get_type_info() const = 0;

        protected:
            void set_property(const PassPropertyMask& prop, bool value);

        private:
            PassPropertyMask m_property;

            std::string m_name;
            std::shared_ptr<PassConfig> m_pass_config;
        };

        class NGRAPH_API FunctionPass : public PassBase
        {
        public:
            NGRAPH_RTTI_DECLARATION;
            virtual ~FunctionPass();
            virtual bool run_on_function(std::shared_ptr<ngraph::Function>) = 0;
        };

        class NGRAPH_DEPRECATED("Use MatcherPass or FunctionPass instead.") NGRAPH_API NodePass
            : public PassBase
        {
        public:
            NGRAPH_RTTI_DECLARATION;
            virtual ~NodePass();
            virtual bool run_on_node(std::shared_ptr<ngraph::Node>) = 0;
        };

        class Manager;
        enum class FusionType : uint32_t
        {
            //`DIFFERENTIABLE_FUSIONS` produce ops that support autodiff
            // i.e. implement `generate_adjoints`
            DIFFERENTIABLE_FUSIONS = 0x1,
            REGULAR_FUSIONS = 0x2,
            //`FOP_FUSIONS` produce ops in the FusedOps category that might
            // not be supported by all backends
            FOP_FUSIONS = 0x4,
            ALL_FUSIONS = 0xFFFFFFFF
        };
        typedef EnumMask<FusionType> FusionTypeMask;
    }
}
