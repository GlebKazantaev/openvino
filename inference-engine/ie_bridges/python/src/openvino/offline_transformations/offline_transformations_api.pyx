# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

from .cimport offline_transformations_api_impl_defs as C
from ..inference_engine.ie_api cimport IENetwork

from libcpp cimport bool
from libcpp.string cimport string
from libcpp.pair cimport pair

def ApplyMOCTransformations(IENetwork network, bool cf):
    C.ApplyMOCTransformations(network.impl, cf)

def ApplyLowLatencyTransformation(IENetwork network):
    C.ApplyLowLatencyTransformation(network.impl)

def ApplyPruningTransformation(IENetwork network):
    C.ApplyPruningTransformation(network.impl)

def GenerateMappingFile(IENetwork network, string path):
    C.GenerateMappingFile(network.impl, path)

def CheckAPI():
    C.CheckAPI()

def CompareNetworks(IENetwork lhs, IENetwork rhs):
    cdef pair[bool, string] c_pair
    c_pair = C.CompareNetworks(lhs.impl, rhs.impl)
    return c_pair