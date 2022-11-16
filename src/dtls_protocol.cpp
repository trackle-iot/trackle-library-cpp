#include "dtls_protocol.h"

namespace trackle
{
	namespace protocol
	{

		void DTLSProtocol::init(const char *id,
								const TrackleKeys &keys,
								const TrackleCallbacks &callbacks,
								const TrackleDescriptor &descriptor,
								const trackle::protocol::Connection_Properties_Type &conPropType)
		{
			set_protocol_flags(0);
			memcpy(device_id, id, sizeof(device_id));

			DTLSMessageChannel::Callbacks channelCallbacks = {};
			channelCallbacks.millis = callbacks.millis;
			channelCallbacks.handle_seed = handle_seed;
			channelCallbacks.receive = callbacks.receive;
			channelCallbacks.send = callbacks.send;
			channelCallbacks.sleep = callbacks.sleep;
			channelCallbacks.calculate_crc = callbacks.calculate_crc;
			if (callbacks.size >= 52)
			{ // todo - get rid of this magic number and define it by the size of some struct.
				channelCallbacks.save = callbacks.save;
				channelCallbacks.restore = callbacks.restore;
			}
			if (offsetof(TrackleCallbacks, notify_client_messages_processed) + sizeof(TrackleCallbacks::notify_client_messages_processed) <= callbacks.size)
			{
				channelCallbacks.notify_client_messages_processed = callbacks.notify_client_messages_processed;
			}

			channel.set_millis(callbacks.millis);

			channel.set_ack_timeout(conPropType.ack_timeout * 1000);
			channel.set_handshake_timeout(conPropType.handshake_timeout * 1000);
			initialize_ping(conPropType.ping_interval * 1000, 30000);

			ProtocolError error = channel.init(keys.core_private, 121,
											   keys.server_public, 91,
											   (const uint8_t *)device_id, channelCallbacks, &channel.next_id_ref());
			if (error)
			{
				WARN("error initializing DTLS channel: %d", error);
			}
			else
			{
				INFO("channel inited");
				Protocol::init(callbacks, descriptor);
			}
		}

		int DTLSProtocol::wait_confirmable(uint32_t timeout)
		{
			system_tick_t start = millis();
			LOG(INFO, "Waiting for Confirmed messages to be sent.");
			ProtocolError err = NO_ERROR;
			// FIXME: Additionally wait for 1 second before going into sleep to give
			// a chance for some requests to arrive (e.g. application describe request)
			while ((channel.has_unacknowledged_requests() && (millis() - start) < timeout) ||
				   (millis() - start) <= 1000)
			{
				CoAPMessageType::Enum message;
				err = event_loop(message);
				if (err)
				{
					LOG(WARN, "error receiving acknowledgements: %d", err);
					break;
				}
			}
			LOG(INFO, "All Confirmed messages sent: client(%s) server(%s)",
				channel.client_messages().has_messages() ? "no" : "yes",
				channel.server_messages().has_unacknowledged_requests() ? "no" : "yes");

			if (err == ProtocolError::NO_ERROR && channel.has_unacknowledged_requests())
			{
				err = ProtocolError::MESSAGE_TIMEOUT;
				LOG(WARN, "Timeout while waiting for confirmable messages to be processed");
			}

			return (int)err;
		}

	}
}
