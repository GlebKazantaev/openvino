# Copyright (C) 2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--path_to_xml")
    parser.add_argument("--path_to_bin")
    parser.add_argument("--path_to_ref_xml")
    parser.add_argument("--path_to_ref_bin")
    args = parser.parse_args()

    try:
        from openvino.inference_engine import read_network_without_extensions # pylint: disable=import-error
        from openvino.offline_transformations import CompareNetworks # pylint: disable=import-error
    except Exception as e:
        print("[ WARNING ] {}".format(e))
        exit(1)

    net = read_network_without_extensions(args.path_to_xml, args.path_to_bin)
    net_ref = read_network_without_extensions(args.path_to_ref_xml, args.path_to_ref_bin)

    resp, msg = CompareNetworks(net, net_ref)

    print((resp, msg))
    if not resp:
        print(msg)
        exit(1)
