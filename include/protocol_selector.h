/*
 * Trackle Library - Source-Available IoT Client Library
 * Copyright (c) 2022 IOTREADY S.r.l. All rights reserved.
 * Copyright (c) 2015 Particle Industries, Inc.
 *
 * This source code is licensed under the Trackle Source-Available License
 * Agreement found in the LICENSE file in the root directory of this source tree.
 * Commercial deployment requires one paid Device License Key per device.
 */

#pragma once

#include "hal_platform.h"

#ifdef __cplusplus
namespace trackle
{
    namespace protocol
    {
        class Protocol;
    }
}
typedef trackle::protocol::Protocol ProtocolFacade;
#else
typedef void *ProtocolFacade;
#endif
