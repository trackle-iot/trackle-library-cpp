#include "protocol_defs.h"

system_error_t trackle::protocol::toSystemError(ProtocolError error)
{
    switch (error)
    {
    case NO_ERROR:
        return SYSTEM_ERROR_NONE;
    case PING_TIMEOUT:
    case MESSAGE_TIMEOUT:
        return SYSTEM_ERROR_TIMEOUT;
    case IO_ERROR:
    case IO_ERROR_FORWARD_MESSAGE_CHANNEL:
    case IO_ERROR_SET_DATA_MAX_EXCEEDED:
    case IO_ERROR_PARSING_SERVER_PUBLIC_KEY:
    case IO_ERROR_GENERIC_ESTABLISH:
    case IO_ERROR_GENERIC_RECEIVE:
    case IO_ERROR_GENERIC_SEND:
    case IO_ERROR_GENERIC_MBEDTLS_SSL_WRITE:
    case IO_ERROR_DISCARD_SESSION:
    case IO_ERROR_LIGHTSSL_BLOCKING_SEND:
    case IO_ERROR_LIGHTSSL_BLOCKING_RECEIVE:
    case IO_ERROR_LIGHTSSL_RECEIVE:
    case IO_ERROR_LIGHTSSL_HANDSHAKE_NONCE:
    case IO_ERROR_LIGHTSSL_HANDSHAKE_RECV_KEY:
        return SYSTEM_ERROR_IO;
    case INVALID_STATE:
        return SYSTEM_ERROR_INVALID_STATE;
    case AUTHENTICATION_ERROR:
        return SYSTEM_ERROR_NOT_ALLOWED;
    case BANDWIDTH_EXCEEDED:
        return SYSTEM_ERROR_LIMIT_EXCEEDED;
    case INSUFFICIENT_STORAGE:
        return SYSTEM_ERROR_TOO_LARGE;
    case NOT_IMPLEMENTED:
        return SYSTEM_ERROR_NOT_SUPPORTED;
    default:
        return SYSTEM_ERROR_PROTOCOL; // Generic protocol error
    }
}
