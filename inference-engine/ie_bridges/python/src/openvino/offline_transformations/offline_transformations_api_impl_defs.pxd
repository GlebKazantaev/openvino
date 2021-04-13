# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

from libcpp cimport bool
from libcpp.string cimport string
from libcpp.pair cimport pair

from ..inference_engine.ie_api_impl_defs cimport IENetwork

cdef extern from "offline_transformations_api_impl.hpp" namespace "InferenceEnginePython":
    cdef void ApplyMOCTransformations(IENetwork network, bool cf)

    cdef void ApplyLowLatencyTransformation(IENetwork network)

    cdef void ApplyPruningTransformation(IENetwork network)

    cdef void GenerateMappingFile(IENetwork network, string path)

    cdef void CheckAPI()

    cdef pair[bool, string] CompareNetworks(IENetwork lhs, IENetwork rhs)