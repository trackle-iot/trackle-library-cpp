/**
 ******************************************************************************
  Copyright (c) 2022 IOTREADY S.r.l.
  Copyright (c) 2015 Particle Industries, Inc.

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

#include "appender.h"
#include "events.h"
#include <functional>

// Deferring to ASN.1 type codes
namespace TrackleReturnType
{
  enum Enum
  {
    BOOLEAN = 1,
    INT = 2,
    STRING = 4,
    LONG = 6,
    JSON = 7,
    DOUBLE = 9
  };
}

/**
 * The application state is divided into distinct types.
 */
namespace TrackleAppStateSelector
{
  enum Enum
  {
    DESCRIBE_APP,
    DESCRIBE_SYSTEM,
    SUBSCRIPTIONS,
  };
}

namespace TrackleAppStateUpdate
{
  enum Enum
  {
    COMPUTE = 1,
    PERSIST = 2,
    COMPUTE_AND_PERSIST = 3
  };
}

struct TrackleDescriptor
{
  typedef std::function<bool(const void *, TrackleReturnType::Enum)> FunctionResultCallback;

  size_t size;
  int (*num_functions)(void);
  const char *(*get_function_key)(int function_index);
  int (*call_function)(const char *function_key, const char *arg, const char *owner_id, FunctionResultCallback callback, void *reserved);
  int (*update_state)(const char *function_key, const char *arg, const char *owner_id, FunctionResultCallback callback, void *reserved);

  int (*num_variables)(void);
  const char *(*get_variable_key)(int variable_index);
  TrackleReturnType::Enum (*variable_type)(const char *variable_key);
  const void *(*get_variable)(const char *variable_key);

  bool (*was_ota_upgrade_successful)(void);
  void (*ota_upgrade_status_sent)(void);

  bool (*append_system_info)(appender_fn appender, void *append, void *reserved);

  void (*call_event_handler)(uint16_t size, FilteringEventHandler *handler, const char *event, const char *data, void *reserved);

  /**
   * Optional callback - may be null.
   * @param selector	The app state information to retrieve or update
   * @param operation	COMPUTE to retrieve, the value. PESIST to set the persistent storage to the given value, COMPUTE_AND_PERSIST to compute and persist a given value. funcs/vars crc can be retrieved,
   * 	subscriptions crc can be set.
   * 	The descriptor state (DESCRIBE_APP/DESCRIBE_SYSTEM) can be computed by the callback and can be used with COMPUTE and COMPUTE_AND_PERSIST operations.
   * 	The subscription state (SUBSCRIPTIONS) is computed by the caller and passed to the callback (secifying PERSIST as the operation.)
   * @param data		when operation==1 this is the value ot set. otherwise unused.
   * @return when operation==COMPUTE, the crc of the application state is retrieved when operation is COMPUTE. Otherwise the return value is 0.
   */
  uint32_t (*app_state_selector_info)(TrackleAppStateSelector::Enum selector, TrackleAppStateUpdate::Enum operation, uint32_t data, void *reserved);

  /**
   * Append metrics to the given appender.
   * @param appender	The appender function to call with the "append" data and the string to append
   * @param append		Opaque data to be passed to appender
   * @param flags		0x01 - append as binary daata, otherwise append as json
   * @param page		A key to select which metrics data to output. Presently unused and should be 0, which means the default metrics.
   * @param reserved	For future expansion.
   * @return
   */
  bool (*append_metrics)(appender_fn appender, void *append, uint32_t flags, uint32_t page, void *reserved);

  void *reserved[1]; // add a few additional pointers
};
