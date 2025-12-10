#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include <trackle_interface.h>

// GET functions

bool CloudFun_getEchoBool(const char *args, const char *key)
{
    return atoi(args);
}

int32_t CloudFun_getEchoInt(const char *args, const char *key)
{
    return atoi(args);
}

double CloudFun_getEchoDouble(const char *args, const char *key)
{
    return atof(args);
}

static char stringVal[64];
char *CloudFun_getEchoString(const char *args, const char *key)
{
    strncpy(stringVal, args, 64);
    stringVal[63] = '\0';
    return stringVal;
}

static char jsonVal[64];
char *CloudFun_getEchoJson(const char *args, const char *key)
{
    strncpy(jsonVal, args, 64);
    jsonVal[63] = '\0';
    return jsonVal;
}

// GET long string function - returns a string that requires blockwise transfer (over 1024 bytes)
static char longStringVal[4000];
char *CloudFun_getLongString(const char *args, const char *key)
{
    // Generate a long string (around 3800 characters to ensure blockwise transfer)
    int size = atoi(args);
    if (size <= 0 || size > 3800)
    {
        size = 3800; // Default to 3800 characters
    }
    
    // Fill with pattern based on args
    for (int i = 0; i < size && i < 3999; i++)
    {
        longStringVal[i] = 'A' + (i % 26);
    }
    longStringVal[size < 3999 ? size : 3999] = '\0';
    return longStringVal;
}

// GET too long string function - returns a string that exceeds maximum allowed size
static char tooLongStringVal[50000];
char *CloudFun_getTooLongString(const char *args, const char *key)
{
    // Generate a very long string (over 50000 characters to exceed limits)
    int size = atoi(args);
    if (size <= 0 || size > 49999)
    {
        size = 49999; // Default to 49999 characters
    }
    
    // Fill with pattern
    for (int i = 0; i < size && i < 49999; i++)
    {
        tooLongStringVal[i] = 'B' + (i % 26);
    }
    tooLongStringVal[size < 49999 ? size : 49999] = '\0';
    return tooLongStringVal;
}

// POST functions

int CloudFun_failingPost(const char *args, const char *key)
{
    return -10;
}

int CloudFun_successPost(const char *args, const char *key)
{
    return 10;
}

bool privatePostExecuted = false;
int CloudFun_privatePost(const char *args, const char *key)
{
    privatePostExecuted = true;
    return 16;
}

static bool sentPublish[10] = {false};
static int publishPublished[10] = {-1}; // Here we use an int so that if it stays to -1 due to a bug it can be detected
void CloudFun_sendPublish(const char *eventName, const char *data, uint32_t msgKey, bool published)
{
    if (msgKey >= 0 && msgKey <= 9)
    {
        sentPublish[msgKey] = true;
        publishPublished[msgKey] = published;
    }
}

static bool completePublish[10] = {false};
static int publishError[10] = {-1};
void CloudFun_completePublish(int error, const void *data, void *callbackData, void *reserved)
{
    uint32_t msgKey = (uint32_t)callbackData;
    if (msgKey >= 0 && msgKey <= 9)
    {
        completePublish[msgKey] = true;
        publishError[msgKey] = error;
    }
}

bool signalCalled = false;
void CloudFun_signalCallback(bool on, unsigned int params, void *reserved)
{
    signalCalled = true;
}

bool rebootCalled = false;
void CloudFun_rebootCallback(const char *data)
{
    rebootCalled = true;
}

bool setTimeCalled = false;
void CloudFun_setTimeCallback(time_t time, unsigned int param, void *reserved)
{
    setTimeCalled = true;
}

bool CloudFun_getPublishComplete(size_t idx)
{
    if (idx < 0 || idx > 9)
        return false;
    return completePublish[idx];
}

int CloudFun_getPublishError(size_t idx)
{
    if (idx < 0 || idx > 9)
        return -1;
    return publishError[idx];
}

bool CloudFun_getPublishSend(size_t idx)
{
    if (idx < 0 || idx > 9)
        return false;
    return sentPublish[idx];
}

int CloudFun_getPublishPublished(size_t idx)
{
    if (idx < 0 || idx > 9)
        return -1;
    return publishPublished[idx];
}

void CloudFun_resetPublishComplete(size_t idx)
{
    if (idx >= 0 && idx <= 9)
    {
        completePublish[idx] = false;
        publishError[idx] = -1;
    }
}

void CloudFun_resetPublishSent(size_t idx)
{
    if (idx >= 0 && idx <= 9)
    {
        sentPublish[idx] = false;
        publishPublished[idx] = -1;
    }
}