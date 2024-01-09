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

#include "protocol_selector.h"

#include <string.h>
#include "protocol_defs.h"
#include "message_channel.h"
#include "messages.h"
#include "trackle_descriptor.h"
#include "protocol.h"
#include "dtls_message_channel.h"
#include "coap_channel.h"
#include <limits>
#include "logging.h"

namespace trackle
{
	namespace protocol
	{

		class DTLSProtocol : public Protocol
		{
			CoAPChannel<CoAPReliableChannel<DTLSMessageChannel, decltype(TrackleCallbacks::millis)>> channel;

			static void handle_seed(const uint8_t *data, size_t len)
			{
			}

			uint8_t device_id[12];

		public:
			// todo - this a duplicate of LightSSLProtocol - factor out

			DTLSProtocol() : Protocol(channel) {}

			void init(const char *id,
					  const TrackleKeys &keys,
					  const TrackleCallbacks &callbacks,
					  const TrackleDescriptor &descriptor,
					  const trackle::protocol::Connection_Properties_Type &conPropType) override;

			size_t build_hello(Message &message, uint8_t flags) override
			{
				product_details_t deets;
				deets.size = sizeof(deets);
				get_product_details(deets);
				size_t len = Messages::hello(message.buf(), 0,
											 flags, PLATFORM_ID, deets.product_id,
											 deets.product_version, true,
											 device_id, sizeof(device_id));
				return len;
			}

			virtual int command(ProtocolCommands::Enum command, uint32_t data) override
			{
				int result = UNKNOWN;
				switch (command)
				{
				case ProtocolCommands::SLEEP:
					result = wait_confirmable();
					break;
				case ProtocolCommands::DISCONNECT:
					result = wait_confirmable();
					ack_handlers.clear();
					break;
				case ProtocolCommands::WAKE:
					wake();
					result = NO_ERROR;
					break;
				case ProtocolCommands::TERMINATE:
					ack_handlers.clear();
					result = NO_ERROR;
					break;
				case ProtocolCommands::FORCE_PING:
				{
					LOG(INFO, "Forcing a cloud ping");
					pinger.process(UINT32_MAX, [this]
								   { return ping(true); });
					break;
				}
				}
				return result;
			}

			int get_status(protocol_status *status) const override
			{
				status->flags = 0;
				if (channel.has_unacknowledged_client_requests())
				{
					status->flags |= PROTOCOL_STATUS_HAS_PENDING_CLIENT_MESSAGES;
				}
				return NO_ERROR;
			}

			/**
			 * Ensures that all outstanding sent coap messages have been acknowledged.
			 */
			int wait_confirmable(uint32_t timeout = 60000);

			void wake()
			{
				ping();
			}
		};

	}
}
