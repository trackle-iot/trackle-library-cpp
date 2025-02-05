/**
 * @file register_get_function.c
 * Contains auxiliary functions to register GET callbacks.
 */

#include <stdbool.h>

#include <trackle_interface.h>

bool TestAuxFun_trackleGetBool(Trackle *trackle_s, const char *funName, bool (*fun)(const char *, const char *))
{
    return trackleGet(trackle_s, funName, (void *(*)(const char *, const char *))fun, VAR_BOOLEAN);
}

bool TestAuxFun_trackleGetInt32(Trackle *trackle_s, const char *funName, int32_t (*fun)(const char *, const char *))
{
    return trackleGet(trackle_s, funName, (void *(*)(const char *, const char *))fun, VAR_INT);
}

bool TestAuxFun_trackleGetDouble(Trackle *trackle_s, const char *funName, double (*fun)(const char *, const char *))
{
    return trackleGet(trackle_s, funName, (void *(*)(const char *, const char *))fun, VAR_DOUBLE);
}

bool TestAuxFun_trackleGetString(Trackle *trackle_s, const char *funName, char *(*fun)(const char *, const char *))
{
    return trackleGet(trackle_s, funName, (void *(*)(const char *, const char *))fun, VAR_STRING);
}

bool TestAuxFun_trackleGetJson(Trackle *trackle_s, const char *funName, char *(*fun)(const char *, const char *))
{
    return trackleGet(trackle_s, funName, (void *(*)(const char *, const char *))fun, VAR_JSON);
}

#ifdef TRACKLE_USE_EXTERNAL_BUFFER
    #define BLOCK_SIZE 1024  // Maximum block size
    #define EXT_SIZE (TRACKLE_CONCURRENT_MESSAGES * BLOCK_SIZE * (TRACKLE_BLOCKS_NUMBER - 1))
    uint8_t ext_buffer[EXT_SIZE];
#endif

bool TestAuxFun_tracklSetExternalBuffer(Trackle *trackle_s)
{
#ifdef TRACKLE_USE_EXTERNAL_BUFFER
    return trackleSetExternalBuffer(trackle_s, ext_buffer, EXT_SIZE);
#else 
    return true;
#endif
}