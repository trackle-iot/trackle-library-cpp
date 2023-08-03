#!/usr/bin/env python3.10

import time

import trackle
import callbacks
import credentials

SOFTWARE_VERSION = 1

print("initializing...", flush=True)

trackle_s = trackle.new()
trackle.init(trackle_s)

trackle.setDeviceId(trackle_s, credentials.TRACKLE_ID)

trackle.setLogCallback(trackle_s, trackle.LOG_CB(callbacks.log))
trackle.setLogLevel(trackle_s, trackle.LogLevel.ERROR)

trackle.setEnabled(trackle_s, True)

trackle.setKeys(trackle_s, credentials.TRACKLE_PRIVATE_KEY)
trackle.setFirmwareVersion(trackle_s, SOFTWARE_VERSION)
trackle.setOtaMethod(trackle_s, trackle.OTAMethod.NO_OTA)
trackle.setConnectionType(trackle_s, trackle.ConnectionType.WIFI)

trackle.setMillis(trackle_s, trackle.MILLIS_CB(callbacks.get_millis))
trackle.setSendCallback(trackle_s, trackle.SEND_UDP_CB(callbacks.send_udp))
trackle.setReceiveCallback(trackle_s, trackle.RECV_UDP_CB(callbacks.receive_udp))
trackle.setConnectCallback(trackle_s, trackle.CONNECT_UDP_CB(callbacks.connect_udp))
trackle.setDisconnectCallback(trackle_s, trackle.DISCONNECT_UDP_CB(callbacks.disconnect_udp))
trackle.setSystemTimeCallback(trackle_s, trackle.SYSTEM_TIME_CB(callbacks.set_time))
trackle.setSystemRebootCallback(trackle_s, trackle.REBOOT_CB(callbacks.reboot))
trackle.setPublishHealthCheckInterval(trackle_s, 60 * 60 * 1000)
trackle.setCompletedPublishCallback(trackle_s, trackle.COMPLETED_PUBLISH_CB(callbacks.completed_publish))

# trackle.setSleepCallback(trackle_s, trackle.SLEEP_CB(callbacks.sleep_ms))

def postFun(args):
    print("POST FUN SUCCESS")

def getFun(args):
    print("GET FUN SUCCESS")

trackle.post(trackle_s, "postFun".encode("utf-8"), trackle.POST_CB(postFun), 1)
trackle.get(trackle_s, "getFun".encode("utf-8"), trackle.GET_CB(getFun), 4)

trackle.connect(trackle_s)

print("done", flush=True)
"""
while True:
    time.sleep(0.2)
    trackle.loop(trackle_s)
"""