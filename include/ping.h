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

#include "protocol_defs.h"

namespace trackle
{
	namespace protocol
	{

		class Pinger
		{
			system_tick_t ping_interval;
			keepalive_source_t keepalive_source;
			uint8_t coap_ping_ratio; // send a CoAP ping every N dumb pings (0 = disabled)
			uint8_t dumb_ping_count; // counts dumb pings since last CoAP message

		public:
			Pinger() : ping_interval(0), keepalive_source(KeepAliveSource::SYSTEM),
					   coap_ping_ratio(0), dumb_ping_count(0) {}

			/**
			 * Sets the ping interval and CoAP ping ratio.
			 * @param interval Interval between keepalive pings (ms).
			 * @param ratio Send a CoAP CON ping every N dumb pings. 0 disables CoAP pings.
			 *
			 * Dumb pings are fire-and-forget UDP keepalives.
			 * CoAP pings are Confirmable empty messages; ACK timeout / retransmission
			 * is handled by the CoAP reliable channel, not by this class.
			 */
			void init(system_tick_t interval, uint8_t ratio = 0)
			{
				this->ping_interval = interval;
				this->keepalive_source = KeepAliveSource::SYSTEM;
				this->coap_ping_ratio = ratio;
				this->dumb_ping_count = 0;
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
				dumb_ping_count = 0;
			}

			/**
			 * Send a keepalive when idle longer than ping_interval.
			 * Every coap_ping_ratio-th ping is a CoAP CON ping; otherwise a dumb ping.
			 * @param millis_since_last_message Elapsed ms since the last message activity.
			 * @param callback callable(bool forceCoAP) that sends the ping.
			 */
			template <typename Callback>
			ProtocolError process(system_tick_t millis_since_last_message, Callback ping)
			{
				if (ping_interval && ping_interval < millis_since_last_message)
				{
					dumb_ping_count++;
					bool force_coap = (coap_ping_ratio > 0 && (dumb_ping_count % coap_ping_ratio == 0));
					return ping(force_coap);
				}

				return NO_ERROR;
			}

			/**
			 * Notifies the Pinger that a CoAP message has been received,
			 * resetting the dumb ping counter since the CoAP session is confirmed alive.
			 */
			void message_received()
			{
				dumb_ping_count = 0;
			}
		};

	}
}
