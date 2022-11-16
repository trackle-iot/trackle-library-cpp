#pragma once

#include <string.h>
#include "protocol_defs.h"
#include "message_channel.h"
#include "messages.h"
#include "trackle_descriptor.h"

namespace trackle
{
	namespace protocol
	{

		template <size_t max, size_t prefix = 0, size_t suffix = 0>
		class BufferMessageChannel : public AbstractMessageChannel
		{
		protected:
			unsigned char queue[max];

		public:
			virtual ProtocolError create(Message &message, size_t minimum_size = 0) override
			{
				if (minimum_size > sizeof(queue) - prefix - suffix)
				{
					WARN("Insufficient storage for message size %d", minimum_size);
					return INSUFFICIENT_STORAGE;
				}
				message.clear();
				message.set_buffer(queue + prefix, sizeof(queue) - suffix - prefix);
				message.set_length(0);
				return NO_ERROR;
			}

			/**
			 * Fill out a message struct to contain storage for a response.
			 */
			ProtocolError response(Message &original, Message &response, size_t required) override
			{
				return original.splinter(response, required, prefix + suffix) ? NO_ERROR : INSUFFICIENT_STORAGE;
			}
		};

	}
}
