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
