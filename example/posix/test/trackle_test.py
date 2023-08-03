#!/usr/bin/env python3.10

import time

import trackle
import callbacks
import credentials

log_cb = trackle.LOG_CB(callbacks.log)
millis_cb = trackle.MILLIS_CB(callbacks.get_millis)
send_udp_cb = trackle.SEND_UDP_CB(callbacks.send_udp)
recv_udp_cb = trackle.RECV_UDP_CB(callbacks.receive_udp)
connect_udp_cb = trackle.CONNECT_UDP_CB(callbacks.connect_udp)
disconnect_udp_cb = trackle.DISCONNECT_UDP_CB(callbacks.disconnect_udp)
system_time_cb = trackle.SYSTEM_TIME_CB(callbacks.set_time)
completed_publish_cb = trackle.COMPLETED_PUBLISH_CB(callbacks.completed_publish)
reboot_cb = trackle.REBOOT_CB(callbacks.reboot)
sleep_cb = trackle.SLEEP_CB(callbacks.sleep_ms)

SOFTWARE_VERSION = 1

print("initializing...", flush=True)

trackle_s = trackle.new()
trackle.init(trackle_s)

trackle.setDeviceId(trackle_s, credentials.TRACKLE_ID)

trackle.setLogCallback(trackle_s, log_cb)
trackle.setLogLevel(trackle_s, trackle.LogLevel.WARN)

trackle.setEnabled(trackle_s, True)

trackle.setKeys(trackle_s, credentials.TRACKLE_PRIVATE_KEY)
trackle.setFirmwareVersion(trackle_s, SOFTWARE_VERSION)
trackle.setOtaMethod(trackle_s, trackle.OTAMethod.NO_OTA)
trackle.setConnectionType(trackle_s, trackle.ConnectionType.UNDEFINED)

trackle.setMillis(trackle_s, millis_cb)
trackle.setSendCallback(trackle_s, send_udp_cb)
trackle.setReceiveCallback(trackle_s, recv_udp_cb)
trackle.setConnectCallback(trackle_s, connect_udp_cb)
trackle.setDisconnectCallback(trackle_s, disconnect_udp_cb)
trackle.setSystemTimeCallback(trackle_s, system_time_cb)
# trackle.setSleepCallback(trackle_s, sleep_cb)
trackle.setSystemRebootCallback(trackle_s, reboot_cb)
trackle.setPublishHealthCheckInterval(trackle_s, 60 * 60 * 1000)
trackle.setCompletedPublishCallback(trackle_s, completed_publish_cb)

def postFun(args):
    print("POST FUN SUCCESS")
    return 1

def getFun(args):
    print("GET FUN SUCCESS")
    # return "hi".encode("utf-8") # TODO Fix returned value, it crashes

post_cb = trackle.POST_CB(postFun)
get_cb = trackle.GET_CB(getFun)

trackle.post(trackle_s, "postFun".encode("utf-8"), post_cb, 1)
trackle.get(trackle_s, "getFun".encode("utf-8"), get_cb, 4)

trackle.connect(trackle_s)

print("done", flush=True)

while True:
    time.sleep(0.05)
    trackle.loop(trackle_s)
