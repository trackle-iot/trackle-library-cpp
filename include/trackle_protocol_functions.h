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

#include <stdint.h>
#include <time.h>
#include "protocol_defs.h"
#include "trackle_descriptor.h"
#include "events.h"
#include "file_transfer.h"
#include "protocol_selector.h"
#include "protocol_defs.h"
#include "completion_handler.h"
#include "hal_platform.h"
#include "defines.h"

#ifdef __cplusplus
extern "C"
{
#endif

	struct TrackleKeys
	{
		uint16_t size;
		unsigned char *core_private;
		unsigned char *server_public;
		unsigned char *core_public;
	};

	// TRACKLE_STATIC_ASSERT(TrackleKeys_size, sizeof(TrackleKeys)==16 || sizeof(void*)!=4);

	enum ProtocolFactory
	{
		PROTOCOL_NONE,
		PROTOCOL_LIGHTSSL,
		PROTOCOL_DTLS,
	};

	struct TrackleCallbacks
	{
		uint16_t size;
		/**
		 * The type of protocol to instantiate.
		 */
		uint8_t protocolFactory;

		uint8_t reserved;

		int (*send)(const unsigned char *buf, uint32_t buflen, void *handle);
		int (*receive)(unsigned char *buf, uint32_t buflen, void *handle);

		/**
		 * @param flags 1 dry run only.
		 * Return 0 on success.
		 */
		int (*prepare_for_firmware_update)(FileTransfer::Descriptor &data, uint32_t flags, void *);

		/**
		 *
		 * @return 0 on success
		 */
		int (*save_firmware_chunk)(FileTransfer::Descriptor &descriptor, const unsigned char *chunk, void *);

		/**
		 * Finalize the data storage.
		 * #param reset - if the device should be reset to apply the changes.
		 * #return 0 on success. Other values indicate an issue with the file.
		 */
		int (*finish_firmware_update)(FileTransfer::Descriptor &data, uint32_t flags, void *);

		uint32_t (*calculate_crc)(const unsigned char *buf, uint32_t buflen);

		void (*signal)(bool on, unsigned int param, void *reserved);
		system_tick_t (*millis)();

		/**
		 * Sets the time. Time is given in milliseconds since the epoch, UCT.
		 */
		void (*set_time)(time_t t, unsigned int param, void *reserved);

		// size == 40

		/**
		 * A pointer that is passed back to the send/receive functions.
		 */
		void *transport_context;

		// size == 44

		enum PersistType
		{
			PERSIST_SESSION = 0
		};
		int (*save)(const void *data, size_t length, uint8_t type, void *reserved);
		/**
		 * Restore to the given buffer. Returns the number of bytes restored.
		 */
		int (*restore)(void *data, size_t max_length, uint8_t type, void *reserved);

		// size == 52

		/**
		 * Notify the client that all messages sent to the server have been processed.
		 */
		void (*notify_client_messages_processed)(void *reserved);

		// size == 56
	};

	// TRACKLE_STATIC_ASSERT(TrackleCallbacks_size, sizeof(TrackleCallbacks)==(sizeof(void*)*14));

	/**
	 * Application-supplied callbacks. (Deliberately distinct from the system-supplied
	 * callbacks.)
	 */
	typedef struct CommunicationsHandlers
	{
		uint16_t size;

		/**
		 * Handle the cryptographically secure random seed from the cloud.
		 * @param seed  A random value. This is typically used to seed a pseudo-random generator.
		 */
		void (*random_seed_from_cloud)(unsigned int seed);

	} CommunicationsHandlers;

	// TRACKLE_STATIC_ASSERT(CommunicationHandlers_size, sizeof(CommunicationsHandlers)==8 || sizeof(void*)!=4);

	typedef struct
	{
		uint16_t size;
		product_id_t product_id;
		product_firmware_version_t product_version;
		uint16_t reserved; // make the padding explicit
	} product_details_t;

	// TRACKLE_STATIC_ASSERT(product_details_size, sizeof(product_details_t)==8);

	void trackle_protocol_communications_handlers(ProtocolFacade *protocol, CommunicationsHandlers *handlers);

	void trackle_protocol_init(ProtocolFacade *protocol, const char *id,
							   const TrackleKeys &keys,
							   const TrackleCallbacks &callbacks,
							   const TrackleDescriptor &descriptor,
							   const trackle::protocol::Connection_Properties_Type &conPropType,
							   void *reserved = NULL);
	int trackle_protocol_handshake(ProtocolFacade *protocol, void *reserved = NULL);
	bool trackle_protocol_event_loop(ProtocolFacade *protocol, void *reserved = NULL);
	bool trackle_protocol_is_initialized(ProtocolFacade *protocol);
	int trackle_protocol_presence_announcement(ProtocolFacade *protocol, unsigned char *buf, const unsigned char *id, void *reserved = NULL);

	// Additional parameters for trackle_protocol_send_event()
	typedef struct
	{
		size_t size;
		completion_callback handler_callback;
		void *handler_data;
		uint8_t handler_token;
	} completion_handler_data;

	typedef completion_handler_data trackle_protocol_send_event_data;

	bool trackle_protocol_send_event(ProtocolFacade *protocol, uint8_t token, const char *event_name,
									 const char *data, uint16_t length, int ttl, uint8_t block_id,
									 uint8_t block_num, uint32_t flags, void *reserved);
	bool trackle_protocol_send_subscription_device(ProtocolFacade *protocol, const char *event_name, const char *device_id, void *reserved = NULL);
	bool trackle_protocol_send_subscription_scope(ProtocolFacade *protocol, const char *event_name, SubscriptionScope::Enum scope, void *reserved = NULL);
	bool trackle_protocol_add_event_handler(ProtocolFacade *protocol, const char *event_name, EventHandler handler, SubscriptionScope::Enum scope, const char *id, void *handler_data = NULL);
	bool trackle_protocol_send_time_request(ProtocolFacade *protocol, void *reserved = NULL);
	void trackle_protocol_send_subscriptions(ProtocolFacade *protocol, void *reserved = NULL);
	void trackle_protocol_remove_event_handlers(ProtocolFacade *protocol, const char *event_name, void *reserved = NULL);
	void trackle_protocol_set_product_id(ProtocolFacade *protocol, product_id_t product_id, unsigned int param = 0, void *reserved = NULL);
	void trackle_protocol_set_product_firmware_version(ProtocolFacade *protocol, product_firmware_version_t product_firmware_version, unsigned int param = 0, void *reserved = NULL);
	void trackle_protocol_get_product_details(ProtocolFacade *protocol, product_details_t *product_details, void *reserved = NULL);

	// int trackle_protocol_set_connection_property(ProtocolFacade *protocol, unsigned property_id, unsigned data, trackle::protocol::connection_properties_t *conn_prop, void *reserved);
	bool trackle_protocol_time_request_pending(ProtocolFacade *protocol, void *reserved = NULL);
	system_tick_t trackle_protocol_time_last_synced(ProtocolFacade *protocol, time_t *tm, void *reserved = NULL);

	int trackle_protocol_to_system_error(int error);

	typedef struct
	{
		size_t size;	// size of this structure
		uint32_t flags; // for now 0, may be used to influence the details retrieved
		uint16_t current_size;
		uint16_t maximum_size;
	} trackle_protocol_describe_data;

	int trackle_protocol_get_describe_data(ProtocolFacade *protocol, trackle_protocol_describe_data *limits, void *reserved);

	/**
	 * @brief Publish vitals information
	 *
	 * Provides a mechanism to control the interval at which system
	 * diagnostic messages are sent to the cloud. Subsequently, this
	 * controls the granularity of detail on the fleet health metrics.
	 *
	 * @param[in] protocol The protocol used to send cloud messages
	 * @param desc_flags The information description flags (default value: \p DESCRIBE_METRICS)
	 * @arg \p DESCRIBE_APPLICATION
	 * @arg \p DESCRIBE_METRICS
	 * @arg \p DESCRIBE_SYSTEM
	 * @param[in,out] reserved Reserved for future use (default value: \p NULL).
	 *
	 * @returns \p ProtocolError result code
	 * @retval \p ProtocolError::NO_ERROR
	 * @retval \p ProtocolError::IO_ERROR_GENERIC_SEND
	 */
	int trackle_protocol_post_description(ProtocolFacade *protocol, int desc_flags = trackle::protocol::DESCRIBE_METRICS, void *reserved = NULL);

	namespace ProtocolCommands
	{
		enum Enum
		{
			SLEEP,
			WAKE,
			DISCONNECT,
			TERMINATE,
			FORCE_PING
		};
	};

	int trackle_protocol_command(ProtocolFacade *protocol, ProtocolCommands::Enum cmd, uint32_t data = 0, void *reserved = NULL);

	/**
	 * Protocol status flags.
	 *
	 * @see `protocol_status`
	 */
	typedef enum protocol_status_flag
	{
		/**
		 * This flag is set if there are client messages waiting for an acknowledgement.
		 *
		 * @see `TrackleCallbacks::notify_client_messages_processed`
		 */
		PROTOCOL_STATUS_HAS_PENDING_CLIENT_MESSAGES = 0x01
	} protocol_status_flag;

	/**
	 * Protocol status.
	 */
	typedef struct protocol_status
	{
		uint16_t size;	///< Size of this structure.
		uint32_t flags; ///< Status flags (see `protocol_status_flag`).
	} protocol_status;

	/**
	 * Get protocol status.
	 *
	 * @param protocol Protocol instance.
	 * @param status Status info.
	 * @param reserved This argument should be set to NULL.
	 * @param 0 on success.
	 */
	int trackle_protocol_get_status(ProtocolFacade *protocol, protocol_status *status, void *reserved);

	/**
	 * Decrypt a buffer using the given public key.
	 * @param ciphertext        The ciphertext to decrypt
	 * @param private_key       The private key (in DER format).
	 * @param plaintext         buffer to hold the resulting plaintext
	 * @param max_plaintext_len The size of the plaintext buffer
	 * @return The number of plaintext bytes in the plain text buffer, or <0 on error.
	 */
	extern int decrypt_rsa(const uint8_t *ciphertext, const uint8_t *private_key,
						   uint8_t *plaintext, int32_t max_plaintext_len);

	void extract_public_rsa_key(uint8_t *device_pubkey, const uint8_t *device_privkey);

	/**
	 * Retrieves a pointer to a statically allocated instance.
	 * @return A statically allocated instance of ProtocolFacade.
	 */
	extern ProtocolFacade *trackle_protocol_instance();

#ifdef __cplusplus
}
#endif
