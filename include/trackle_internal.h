/**
 ******************************************************************************
  Copyright (c) 2026 IOTREADY S.r.l.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */

#pragma once

#include "trackle.h"
#include "protocol_defs.h"
#include "dtls_protocol.h"
#include "events.h"
#include <string>
#include <vector>
#include <stdint.h>

using namespace std;

#define PUB_KEY_OFFSET 26
#define PUB_KEY_MARKER 0x04
#define PUB_KEY_XY_SIZE 64

#define DEFAULT_CONNECTION_TIMEOUT 5000
#define RECONNECTION_TIMEOUT 3750
#define MAX_RECONNECTION_RETRY_INCREMENT 4

#define MAX_COUNTER 9999999
#define MAX_PING_INTERVAL 1000

const int CLAIM_CODE_SIZE = 63;
const int COMPONENTS_LIST_SIZE = trackle::protocol::MAX_COMPONENTS_LIST_LENGTH + 20;
const int DESCRIBE_ATTR_SIZE = 35;

const uint32_t PUBLISH_EVENT_FLAG_PUBLIC = 0x0;
const uint32_t PUBLISH_EVENT_FLAG_PRIVATE = 0x1;

struct _ota_data
{
    bool running;
    bool has_signature;
    bool has_firmware_key;
    char ota_job_id[64];
    uint8_t firmware_signature[64];
    uint8_t firmware_signature_key[PUB_KEY_XY_SIZE];
};

inline uint32_t convert_publish_flags(uint32_t flags)
{
    bool priv = flags & PUBLISH_EVENT_FLAG_PRIVATE;
    flags &= ~PUBLISH_EVENT_FLAG_PRIVATE;
    flags |= !priv ? EventType::PUBLIC : EventType::PRIVATE;
    return flags;
}

// Protocol / keys / descriptor
extern trackle::protocol::DTLSProtocol protocol_instance;
extern ProtocolFacade *protocol;
extern TrackleKeys keys;
extern TrackleCallbacks callbacks;
extern TrackleDescriptor descriptor;

// User callbacks
extern connectCallback *connectCb;
extern disconnectCallback *disconnectCb;
extern receiveCallback *receiveCb;
extern sendCallback *sendCb;
extern publishCompletionCallback *completedPublishCb;
extern publishSendCallback *sendPublishCb;
extern prepareFirmwareUpdateCallback *prepareFirmwareCb;
extern firmwareChunkCallback *firmwareChunkCb;
extern finishFirmwareUpdateCallback *finishUpdateCb;
extern randomNumberCallback *getRandomCb;
extern rebootCallback *systemRebootCb;
extern otaUpdateCallback *otaUpdateCb;
extern deviceClaimedCallback *deviceClaimedCb;
extern pincodeCallback *pincodeCb;
extern connectionStatusCallback *connectionStatusCb;
extern updateStateCallback *updateStateCb;

// Connection state
extern Connection_Status_Type connectionStatus;
extern int cloudStatus;
extern uint16_t connection_retry;
extern uint32_t connection_timeout;
extern uint32_t pingInterval;
extern uint8_t coapPingRatio;
extern Connection_Type connectionType;
extern trackle::protocol::Connection_Properties_Type connectionPropType;
extern trackle::protocol::Connection_Properties_Type connectionPropTypeList[5];
extern bool cloudEnabled;
extern bool connectToCloud;
extern system_tick_t millis_last_disconnection;
extern system_tick_t millis_started_at;
extern system_tick_t millis_last_sent_received_time;
extern system_tick_t millis_last_sent_health_check;
extern system_tick_t health_check_interval;

// OTA / describe
extern const char *OTA_EVENT_NAME;
extern struct _ota_data ota_data;
extern Ota_Method otaMethod;
extern bool updates_pending;
extern bool updates_enabled;
extern bool updates_forced;
extern string string_device_id;
extern char t_device_id[DEVICE_ID_LENGTH];
extern unsigned char server_public_key[PUBLIC_KEY_LENGTH];
extern unsigned char client_private_key[PRIVATE_KEY_LENGTH];
extern char claim_code[CLAIM_CODE_SIZE + 1];
extern char components_list[COMPONENTS_LIST_SIZE + 1];
extern char describe_imei[DESCRIBE_ATTR_SIZE + 1];
extern char describe_iccid[DESCRIBE_ATTR_SIZE + 1];

// Cloud owners list (shared by function auth and system event handler)
extern std::vector<string> owners;

// Helpers shared across translation units
uint8_t getNextToken();
std::string hexStr(char *data, int len);
void setConnectionStatus(Connection_Status_Type newStatus);
void connectionError(int error_type, bool force = false, int protocol_error_code = 0);
void increase_connection_timeout();
void reset_connection_timeout();

void subscribe_trackle_handler(void *handler, const char *event_name, const char *data);

// Descriptor callbacks (implemented in trackle_cloud_api.cpp)
bool was_ota_upgrade_successful(void);
void HAL_OTA_Flashed_ResetStatus(void);
TrackleReturnType::Enum wrapVarTypeInEnum(const char *varKey);
int num_functions(void);
const char *getUserFunctionKey(int function_index);
void event_handler_trackle(const char *event_name, const char *data);
int update_state(const char *function_key, const char *arg, const char *user_caller_id,
                 TrackleDescriptor::FunctionResultCallback callback, void *reserved);
int call_function(const char *function_key, const char *arg, const char *user_caller_id,
                  TrackleDescriptor::FunctionResultCallback callback, void *reserved);
int numUserVariables(void);
const char *getUserVariableKey(int variable_index);
const void *getUserVar(const char *varKey);
bool appendSystemInfo(appender_fn appender, void *append, void *reserved);
uint32_t calculateCrc(const unsigned char *data, uint32_t len);

int wrapSend(const unsigned char *buf, uint32_t buflen, void *tmp);
int wrapReceive(unsigned char *buf, uint32_t buflen, void *tmp);
int default_restore_session(void *buffer, size_t length, uint8_t type, void *reserved);
int default_save_session(const void *buffer, size_t length, uint8_t type, void *reserved);
uint32_t HAL_RNG_GetRandomNumber(void);

void TrackleLib_tinydtls_millis_wrapper(uint32_t *t);
void TrackleLib_set_latest_millis_callback_for_tinydtls(uint32_t (*new_latest_millis_callback)());
