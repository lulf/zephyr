/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define CA_CERTIFICATE_TAG 1

/* This is the same cert as what is found in net-tools/mqtts-cert.pem file
 */
static const unsigned char ca_certificate[] = {
#include "mqtts-cert.der.inc"
};
