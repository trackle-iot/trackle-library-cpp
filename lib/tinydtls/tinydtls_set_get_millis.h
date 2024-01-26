#ifndef TINYDTLS_SET_GET_MILLIS_H_
#define TINYDTLS_SET_GET_MILLIS_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief  Set get millis function for Tiny DTLS
     *
     * @param newGetMillis New function to use to get milliseconds elapsed.
     */
    void TinyDtls_set_get_millis(void (*newGetMillis)(uint32_t *));

#ifdef __cplusplus
}
#endif

#endif