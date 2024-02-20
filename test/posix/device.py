
""" Virtual device used for running tests """

import multiprocessing as mp
import logging as log
from dataclasses import dataclass
import ctypes
import time
import importlib
import warnings

import credentials
import messages as msgs

@dataclass
class DeviceStartupParams:
    """Startup parameters for a virtual device implemented by device_code"""
    private_key: list
    proxy_port: int
    claim_code: str = ""
    components_list: str = ""
    imei: str = ""
    iccid: str = ""
    fw_version: int = 1

class ConnectionStatus:

    """Update connection status based on trackleConnected function"""

    def __init__(self, trackle_instance: ctypes.c_void_p, to_tester: mp.Queue, trackle):
        self.__trackle_instance = trackle_instance
        self.__to_tester = to_tester
        self.__connected = False
        self.__trackle = trackle

    def update(self) -> None:
        """Update connection status and notify test runner of its changes"""
        if not self.__connected and self.__trackle.connected(self.__trackle_instance):
            self.__connected = True
            log.info("connection status changed: connected")
            self.__to_tester.put({"msg": msgs.CONNECTED})
        elif self.__connected and not self.__trackle.connected(self.__trackle_instance):
            self.__connected = False
            log.info("connection status changed: disconnected")
            self.__to_tester.put({"msg": "disconnected"})

def device_code(from_tester : mp.Queue, to_tester : mp.Queue, startup_params : DeviceStartupParams):

    """Code for process that simulates device connected to Trackle"""

    trackle = importlib.import_module("trackle", "")
    callbacks = importlib.import_module("callbacks", "")
    callbacks.trackle_module = trackle
    callbacks.to_tester_queue = to_tester
    
    cloud_functions = importlib.import_module("cloud_functions", "")

    log_cb = trackle.LOG_CB(callbacks.log)
    millis_cb = trackle.MILLIS_CB(callbacks.get_millis)
    send_udp_cb = trackle.SEND_UDP_CB(callbacks.send_udp)
    recv_udp_cb = trackle.RECV_UDP_CB(callbacks.receive_udp)
    connect_udp_cb = trackle.CONNECT_UDP_CB(callbacks.connect_udp)
    disconnect_udp_cb = trackle.DISCONNECT_UDP_CB(callbacks.disconnect_udp)
    system_time_cb = trackle.SYSTEM_TIME_CB(cloud_functions.set_time_callback)
    send_publish_cb = trackle.SEND_PUBLISH_CB(cloud_functions.send_publish)
    completed_publish_cb = trackle.COMPLETED_PUBLISH_CB(cloud_functions.complete_publish)
    reboot_cb = trackle.REBOOT_CB(cloud_functions.reboot)
    signal_cb = trackle.SIGNAL_CB(cloud_functions.signal_callback)
    post_success_cb = trackle.POST_CB(cloud_functions.post_success)
    post_failing_cb = trackle.POST_CB(cloud_functions.post_failing)
    post_private_cb = trackle.POST_CB(cloud_functions.post_private)
    get_echo_bool_cb = trackle.GET_BOOL_CB(cloud_functions.get_echo_bool)
    get_echo_int_cb = trackle.GET_INT32_CB(cloud_functions.get_echo_int)
    get_echo_double_cb = trackle.GET_DOUBLE_CB(cloud_functions.get_echo_double)
    get_echo_string_cb = trackle.GET_STRING_CB(cloud_functions.get_echo_string)
    get_echo_json_cb = trackle.GET_JSON_CB(cloud_functions.get_echo_json)

    trackle_s = trackle.new()
    callbacks.trackle_instance = trackle_s

    trackle.init(trackle_s)

    trackle.setDeviceId(trackle_s, credentials.TRACKLE_ID)

    trackle.setLogCallback(trackle_s, log_cb)
    trackle.setLogLevel(trackle_s, trackle.LogLevel.WARN)

    trackle.setEnabled(trackle_s, True)

    trackle.setKeys(trackle_s, credentials.list_to_private_key(startup_params.private_key))
    trackle.setFirmwareVersion(trackle_s, startup_params.fw_version)
    trackle.setOtaMethod(trackle_s, trackle.OTAMethod.SEND_URL)
    trackle.setOtaUpdateCallback(trackle_s, callbacks.ota_callback)
    trackle.setConnectionType(trackle_s, trackle.ConnectionType.UNDEFINED)
    if startup_params.claim_code:
        trackle.setClaimCode(trackle_s, startup_params.claim_code.encode("utf-8"))
    if startup_params.components_list:
        trackle.setComponentsList(trackle_s, startup_params.components_list.encode("utf-8"))
    if startup_params.imei:
        trackle.setImei(trackle_s, startup_params.imei.encode("utf-8"))
    if startup_params.iccid:
        trackle.setIccid(trackle_s, startup_params.iccid.encode("utf-8"))

    trackle.setMillis(trackle_s, millis_cb)
    trackle.setSendCallback(trackle_s, send_udp_cb)
    trackle.setReceiveCallback(trackle_s, recv_udp_cb)
    trackle.setConnectCallback(trackle_s, connect_udp_cb)
    trackle.setDisconnectCallback(trackle_s, disconnect_udp_cb)
    trackle.setSystemTimeCallback(trackle_s, system_time_cb)
    trackle.setSystemRebootCallback(trackle_s, reboot_cb)
    trackle.setPublishHealthCheckInterval(trackle_s, 60 * 60 * 1000)
    trackle.setSendPublishCallback(trackle_s, send_publish_cb)
    trackle.setCompletedPublishCallback(trackle_s, completed_publish_cb)
    trackle.trackleSetSignalCallback(trackle_s, signal_cb)

    trackle.post(trackle_s, b"postSuccess", post_success_cb, trackle.PermissionDef.ALL_USERS)
    trackle.post(trackle_s, b"postFailing", post_failing_cb, trackle.PermissionDef.ALL_USERS)
    trackle.post(trackle_s, b"postPrivate", post_private_cb, trackle.PermissionDef.OWNER_ONLY)

    trackle.register_get_bool(trackle_s, b"getEchoBool", get_echo_bool_cb)
    trackle.register_get_int32(trackle_s, b"getEchoInt", get_echo_int_cb)
    trackle.register_get_double(trackle_s, b"getEchoDouble", get_echo_double_cb)
    trackle.register_get_string(trackle_s, b"getEchoString", get_echo_string_cb)
    trackle.register_get_json(trackle_s, b"getEchoJson", get_echo_json_cb)

    callbacks.set_connection_override(True, b"127.0.0.1", startup_params.proxy_port)

    conn_status = ConnectionStatus(trackle_s, to_tester, trackle)

    res = trackle.connect(trackle_s)
    to_tester.put({"msg" : msgs.CONNECT_RESULT, "return":res})

    log.info("setup completed")

    while True:

        conn_status.update()

        time.sleep(0.02)
        
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            trackle.loop(trackle_s)

        # --------- Test runner commands interpretation ---------

        in_msg = from_tester.get_nowait() if not from_tester.empty() else None
        if isinstance(in_msg, dict):
            match in_msg.get("msg"):
                case msgs.KILL_DEVICE:
                    log.info("msgs.KILLING device")
                    to_tester.put({"msg" : msgs.KILLING})
                    break
                case msgs.MULTIPUBLISH:
                    if {"event", "data", "ttl", "visibility", "key", "times"}.issubset(set(in_msg)):
                        result = []
                        for _ in range(in_msg["times"]):
                            with warnings.catch_warnings():
                                warnings.simplefilter("ignore")
                                res = trackle.publish(trackle_s,
                                                    in_msg["event"].encode("utf-8"),
                                                    in_msg["data"].encode("utf-8"),
                                                    in_msg["ttl"],
                                                    in_msg["visibility"],
                                                    trackle.PublishType.NO_ACK,
                                                    in_msg["key"])
                            result.append(res)
                        to_tester.put({"msg" : msgs.MULTIPUBLISH_RESULT, "return" : result})
                        log.info("multipublish received")
                    else:
                        log.error("invalid multipublish")
                case msgs.MULTIPUBLISH_LONG:
                    if {"event", "data", "ttl", "visibility"}.issubset(set(in_msg)):
                        result = []
                        for msg_key, event_name in enumerate(in_msg["event"]):
                            with warnings.catch_warnings():
                                warnings.simplefilter("ignore")
                                res = trackle.publish(trackle_s,
                                                    event_name.encode("utf-8"),
                                                    in_msg["data"].encode("utf-8"),
                                                    in_msg["ttl"],
                                                    in_msg["visibility"],
                                                    trackle.PublishType.WITH_ACK,
                                                    msg_key+1)
                            result.append(res)
                        to_tester.put({"msg" : msgs.MULTIPUBLISH_LONG_RESULT, "return" : result})
                        log.info("multipublish-long received")
                    else:
                        log.error("invalid multipublish-long")
                case msgs.PUBLISH:
                    if {"event", "data", "ttl", "visibility", "ack", "key"}.issubset(set(in_msg)):
                        with warnings.catch_warnings():
                            warnings.simplefilter("ignore")
                            res = trackle.publish(trackle_s,
                                                in_msg["event"].encode("utf-8"),
                                                in_msg["data"].encode("utf-8"),
                                                in_msg["ttl"],
                                                in_msg["visibility"],
                                                in_msg["ack"],
                                                in_msg["key"])
                        to_tester.put({"msg" : msgs.PUBLISH_RESULT, "return" : res})
                        log.info("publish received")
                    else:
                        log.error("invalid publish")
                case msgs.WAS_PRIVATE_POST_EXECUTED:
                    executed = cloud_functions.was_private_post_executed()
                    to_tester.put({"msg": msgs.PRIVATE_POST_EXEC_STATUS, "executed": executed})
                    log.info("was private post executed received")
                case msgs.GET_TIME:
                    with warnings.catch_warnings():
                        warnings.simplefilter("ignore")
                        trackle.get_time(trackle_s)
                    log.info("get time received")

        # ----- Asynchronous events signaling towards test runner --------

        # Check if available new completed publish
        for i in range(10):
            if cloud_functions.get_publish_completed(i):
                publish_error = cloud_functions.get_publish_error(i)
                log.info("completed msg publish %d with error %d", i, publish_error)
                to_tester.put({"msg": msgs.PUBLISH_COMPLETED, "error": publish_error, "idx": i})
                cloud_functions.reset_publish_completed(i)

        # Check if available new sent publish
        for i in range(10):
            if cloud_functions.get_publish_send(i):
                publish_published = cloud_functions.get_publish_published(i)
                log.info("sent msg publish %d that succeeded? %d", i, publish_published)
                to_tester.put({"msg": msgs.PUBLISH_SENT, "published": publish_published, "idx": i})
                cloud_functions.reset_publish_sent(i)

        # Check if signal called
        if cloud_functions.was_signal_called():
            to_tester.put({"msg": msgs.SIGNAL_CALLED})
            cloud_functions.reset_signal_called()

        # Check if reboot called
        if cloud_functions.was_reboot_called():
            to_tester.put({"msg": msgs.REBOOT_CALLED})
            cloud_functions.reset_reboot_called()

        # Check if get time called
        if cloud_functions.was_get_time_called():
            to_tester.put({"msg": msgs.GET_TIME_CALLED})
            cloud_functions.reset_get_time_called()
