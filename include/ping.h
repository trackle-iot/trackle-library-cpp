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

#include "protocol_defs.h"

namespace trackle
{
	namespace protocol
	{

		class Pinger
		{
			system_tick_t ping_interval;
			system_tick_t ping_timeout;
			keepalive_source_t keepalive_source;

		public:
			Pinger() : ping_interval(0), ping_timeout(10000), keepalive_source(KeepAliveSource::SYSTEM) {}

			/**
			 * Sets the ping interval that the client will send pings to the server, and the expected maximum response time.
			 */
			void init(system_tick_t interval, system_tick_t timeout)
			{
				this->ping_interval = interval;
				this->ping_timeout = timeout;
				this->keepalive_source = KeepAliveSource::SYSTEM;
			}

			void set_interval(system_tick_t interval, keepalive_source_t source)
			{
				/**
				 * LAST  CURRENT  UPDATE?
				 * ======================
				 * SYS   SYS      YES
				 * SYS   USER     YES
				 * USER  SYS      NO
				 * USER  USER     YES
				 */
				if (!(this->keepalive_source == KeepAliveSource::USER && source == KeepAliveSource::SYSTEM))
				{
					this->ping_interval = interval;
					this->keepalive_source = source;
				}
			}

			void reset()
			{
			}

			/**
			 * Handle ping messages. If a message is not received
			 * within the timeout, the connection is considered unreliable.
			 * @param millis_since_last_message Elapsed number of milliseconds since the last message was received.
			 * @param callback a no-arg callable that is used to perform a ping to the cloud.
			 */
			template <typename Callback>
			ProtocolError process(system_tick_t millis_since_last_message, Callback ping)
			{

				// ping interval set, so check if we need to send a ping
				// The ping is sent based on the elapsed time since the last message
				if (ping_interval && ping_interval < millis_since_last_message)
				{
					return ping();
				}

				return NO_ERROR;
			}

			/**
			 * Notifies the Pinger that a message has been received
			 * and that there is presently no need to resend a ping
			 * until the ping interval has elapsed.
			 */
			void message_received() {}
		};

	}
}
