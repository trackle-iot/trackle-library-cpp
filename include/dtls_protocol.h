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
											 deets.product_version, deets.product_build, true,
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
					ack_handlers.clear();
					// Drop local DTLS peer so a new UDP socket cannot reuse CONNECTED state.
					channel.command(MessageChannel::CLOSE);
					result = NO_ERROR;
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
					pinger.process(UINT32_MAX, [this](bool)
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
