""" Messages used in the test suite """

from trackle_enums import OtaError

class QueueMessage:

    """ Message to be sent down a queue used by the test suite """

    def __init__(self, name, not_recvd_error_msg="<no error for this message>"):
        self.__name = name
        self.__not_recvd_error_msg = not_recvd_error_msg

    def __eq__(self, __value: object) -> bool:
        if isinstance(__value, (str, QueueMessage)):
            return self.__name == str(__value)
        raise ValueError("QueueMessage can only be compared with strings or other QueueMessages")

    def __str__(self) -> str:
        return self.__name

    def __repr__(self) -> str:
        return self.__name

    def not_recvd_err_msg(self, timeout):
        """ Return the error message to be shown if something goes wrong with this QueueMessage. """
        return self.__not_recvd_error_msg % (timeout,)

# Messages sent through queue between tester and other processes with their error messages.

PROXY_SWITCHED_ON = QueueMessage("proxy_switched_on", "Proxy didn't switch on within %d seconds.")
SERVER_CONN_WAS_RESET = QueueMessage("server_conn_was_reset", "Couldn't reset proxy connection within %d seconds.")
KILLING = QueueMessage("killing", "Couldn't kill device simulator process within %d seconds.")
CONNECT_RESULT = QueueMessage("connect_result", "Couldn't receive trackleConnect result from device simulator process within %d seconds.")
CONNECTED = QueueMessage("connected", "Device simulation process didn't connect to cloud within %d seconds.") #
PROXY_SWITCHED_OFF = QueueMessage("proxy_switched_off", "Proxy didn't switch off within %d seconds.")
PRIVATE_POST_EXEC_STATUS = QueueMessage("private_post_exec_status", "Didn't receive private POST execution status within %d seconds.")
PUBLISH_RESULT = QueueMessage("publish_result", "Couldn't receive tracklePublish return value from device simulation process within %d seconds.")
PUBLISH_SENT = QueueMessage("publish_sent", "Couldn't receive tracklePublish sent callback confirmation from device simulation process within %d seconds.")
PUBLISH_COMPLETED = QueueMessage("publish_completed", "Couldn't receive tracklePublish completed callback confirmation from device simulation process within %d seconds.")
MULTIPUBLISH_RESULT = QueueMessage("multipublish_result", "Couldn't receive tracklePublish return value from device simulation process within %d seconds.")
MULTIPUBLISH_LONG_RESULT = QueueMessage("multipublish_long_result", "Couldn't receive tracklePublish return value from device simulation process within %d seconds.")
SIGNAL_CALLED = QueueMessage("signal_called", "Couldn't receive signal callback confirmation from device simulation process within %d seconds.")
REBOOT_CALLED = QueueMessage("reboot_called", "Couldn't receive reboot callback confirmation from device simulation process within %d seconds.")
GET_TIME_CALLED = QueueMessage("get_time_called", "Couldn't receive get_time call confirmation from device simulation process within %d seconds.")
OTA_URL_RECEIVED = QueueMessage("ota_url_received", "Couldn't receive confirmation that device simulation process received OTA URL from cloud within %d seconds.")
CRC32_NOT_CHECKED = QueueMessage("crc32_not_checked", "Couldn't receive confirmation that device simulation process finished OTA process without checking CRC32 within %d seconds.")
CRC32_CORRECT = QueueMessage("crc32_correct", "Couldn't receive confirmation that device simulation process finished OTA process with correct CRC32 within %d seconds.")
CRC32_MISMATCH = QueueMessage("crc32_mismatch", "Couldn't receive confirmation that device simulation process finished OTA process with CRC32 mismatch within %d seconds.")
DOWNLOAD_INTERRUPTED = QueueMessage("download_interrupted", "Couldn't receive confirmation that device simulation process finished OTA process with a download interrupted error within %d seconds.")

TESTS_COMPLETED = QueueMessage("tests_completed")
PROXY_ON = QueueMessage("proxy_on")
PROXY_OFF = QueueMessage("proxy_off")
RESET_SERVER_CONN = QueueMessage("reset_server_conn")
KILL_DEVICE = QueueMessage("kill_device")
WAS_PRIVATE_POST_EXECUTED = QueueMessage("was_private_post_executed")
PUBLISH = QueueMessage("publish")
MULTIPUBLISH = QueueMessage("multipublish")
MULTIPUBLISH_LONG = QueueMessage("multipublish_long")
GET_TIME = QueueMessage("get_time")

OTA_ERR_OK = QueueMessage(str(OtaError.OTA_ERR_OK))
OTA_ERR_ALREADY_RUNNING = QueueMessage(str(OtaError.OTA_ERR_ALREADY_RUNNING))
OTA_ERR_PARTITION = QueueMessage(str(OtaError.OTA_ERR_PARTITION))
OTA_ERR_MEMORY = QueueMessage(str(OtaError.OTA_ERR_MEMORY))
OTA_ERR_VALIDATE_FAILED = QueueMessage(str(OtaError.OTA_ERR_VALIDATE_FAILED))
OTA_ERR_INCOMPLETE = QueueMessage(str(OtaError.OTA_ERR_INCOMPLETE))
OTA_ERR_COMPLETING = QueueMessage(str(OtaError.OTA_ERR_COMPLETING))
OTA_ERR_GENERIC = QueueMessage(str(OtaError.OTA_ERR_GENERIC))
