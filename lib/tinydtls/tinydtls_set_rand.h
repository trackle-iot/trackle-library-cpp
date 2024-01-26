#ifndef TINYDTLS_SET_RAND_H_
#define TINYDTLS_SET_RAND_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief  Set PRNG function for Tiny DTLS
     *
     * @param newCustomRand New function to use as random generator (with same signature as rand)
     */
    void TinyDtls_set_rand(uint32_t (*newCustomRand)());

#ifdef __cplusplus
}
#endif

#endif