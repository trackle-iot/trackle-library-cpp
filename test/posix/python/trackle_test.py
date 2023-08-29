#!/usr/bin/env python3.10

import logging as log
import time
import multiprocessing
import unittest

import sseclient as sse
import requests as req
import requests.auth as req_auth

import trackle
import callbacks
import credentials

log.basicConfig(level=log.INFO, format="[%(levelname)s] %(processName)s : %(msg)s")

SOFTWARE_VERSION = 1

def postEcho(args):
    return int(args)

def getFun(args):
    pass
    # return "hi".encode("utf-8") # TODO Fix returned value, it crashes


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
post_cb = trackle.POST_CB(postEcho)
get_cb = trackle.GET_CB(getFun)


class TrackleLibraryTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        # oauth authentication
        log.info("authenticating to Trackle through OAuth ...")
        oauth_url = "https://api.trackle.io/oauth/token"
        oauth_headers = {"Content-Type": "application/x-www-form-urlencoded;charset=utf-8"}
        oauth_params = {"grant_type": "client_credentials"}
        oauth_basic = req_auth.HTTPBasicAuth(credentials.TRACKLE_CLIENT_ID, credentials.TRACKLE_CLIENT_SECRET)
        resp = req.post(url=oauth_url, headers=oauth_headers, data=oauth_params, auth=oauth_basic, timeout=15, )
        if resp.status_code != 200:
            raise Exception(f"auth return code {resp.status_code}")
        if "access_token" not in resp.json():
            raise Exception("No access token in response")
        temp_token = resp.json()["access_token"]
        log.info("authentication done")

        # building metadata for every single request
        cls.headers = {"Authorization": f"Bearer {temp_token}"}

    def test_post_1(self):
        url = f"https://api.trackle.io/v1/devices/{credentials.TRACKLE_ID_STRING}/postEcho"
        json_body = {"args": str(1)}
        resp = req.post(url, headers=self.headers, data=json_body, timeout=15)
        self.assertEqual(resp.json().get("id"), credentials.TRACKLE_ID_STRING)
        self.assertEqual(resp.json().get("name"), "postEcho")
        self.assertEqual(resp.json().get("return_value"), 1)

    def test_post_2(self):
        url = f"https://api.trackle.io/v1/devices/{credentials.TRACKLE_ID_STRING}/postEcho"
        json_body = {"args": str(-10)}
        resp = req.post(url, headers=self.headers, data=json_body, timeout=15)
        self.assertEqual(resp.json().get("id"), credentials.TRACKLE_ID_STRING)
        self.assertEqual(resp.json().get("name"), "postEcho")
        self.assertEqual(resp.json().get("return_value"), -10)

    def test_post_3(self):
        url = f"https://api.trackle.io/v1/devices/{credentials.TRACKLE_ID_STRING}/postEcho"
        json_body = {"args": str(0)}
        resp = req.post(url, headers=self.headers, data=json_body, timeout=15)
        self.assertEqual(resp.json().get("id"), credentials.TRACKLE_ID_STRING)
        self.assertEqual(resp.json().get("name"), "postEcho")
        self.assertEqual(resp.json().get("return_value"), 0)

    def test_post_4(self):
        url = f"https://api.trackle.io/v1/devices/{credentials.TRACKLE_ID_STRING}/postEcho"
        json_body = {"args": str(10)}
        resp = req.post(url, headers=self.headers, data=json_body, timeout=15)
        self.assertEqual(resp.json().get("id"), credentials.TRACKLE_ID_STRING)
        self.assertEqual(resp.json().get("name"), "postEcho")
        self.assertEqual(resp.json().get("return_value"), 10)


def tester_code(in_queue : multiprocessing.Queue, out_queue : multiprocessing.Queue):
    try:
        log.info("started")

        # wait for a starting message
        msg = in_queue.get()
        if not msg or msg != "connected":
            raise Exception("invalid message, aborting tests")
        log.info("device ready")
        
        # run tests
        unittest.main()

    finally:
        out_queue.put("tests_completed")


def device_code(in_queue : multiprocessing.Queue, out_queue : multiprocessing.Queue):
    log.info("started")

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

    trackle.post(trackle_s, "postEcho".encode("utf-8"), post_cb, 1)
    trackle.get(trackle_s, "getFun".encode("utf-8"), get_cb, 4)

    trackle.connect(trackle_s)

    log.info("setup completed")

    tester_started = False
    while True:
    
        time.sleep(0.05)
        trackle.loop(trackle_s)

        # if connected to cloud, tell test process to start
        if not tester_started and trackle.connected(trackle_s):
            log.info("library connected")
            out_queue.put("connected")
            tester_started = True
        
        # if test process terminated, terminate main process too
        if not in_queue.empty() and in_queue.get(block=False) == "tests_completed":
            log.info("tester terminated, quitting")
            break


if __name__  == "__main__":

    log.info("started. Hi!")

    tester_to_device_queue = multiprocessing.Queue()
    device_to_tester_queue = multiprocessing.Queue()

    tester_proc = multiprocessing.Process(target=tester_code,
                                           args=(device_to_tester_queue, tester_to_device_queue),
                                           name="tester")
    device_proc = multiprocessing.Process(target=device_code,
                                           args=(tester_to_device_queue, device_to_tester_queue),
                                           name="device")
    
    tester_proc.start()
    device_proc.start()

    tester_proc.join()
    device_proc.join()

    log.info("joined with tester and device. Bye!")
