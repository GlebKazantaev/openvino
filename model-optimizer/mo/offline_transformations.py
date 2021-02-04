#!/usr/bin/env python3

"""
 Copyright (C) 2021 Intel Corporation

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
"""

import os
import sys


if __name__ == "__main__":
    from openvino.inference_engine import IECore
    from openvino.offline_transformations import ApplyMOCTransformations

    orig_model_name = sys.argv[1]
    ie = IECore()
    net = ie.read_network(model=orig_model_name + ".xml", weights=orig_model_name + ".bin")
    ApplyMOCTransformations(net, True)

    # remove old IR version to avoid collisions in case if serialize fails
    os.remove(orig_model_name + ".xml")
    os.remove(orig_model_name + ".bin")
    net.serialize(orig_model_name + ".xml", orig_model_name + ".bin")
