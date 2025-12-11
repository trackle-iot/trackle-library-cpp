#!/usr/bin/env python3.10

# to run single test use command ./test.py TrackleLibraryTest.xxx_test_name_xxx
# 


""" Test cases and test runner """

import logging as log
import time
import multiprocessing as mp
import unittest as ut
import json
import datetime
import random
import socket
import queue
import copy
import numbers
import contextlib
import sys
import argparse
import threading

import requests as req
import requests.auth as req_auth

import credentials as cred
import lorem_ipsum as lorem
import sseclient_nowait as scnw
import device
import trackle_enums
import messages as msgs

# Settings behaviours based on command line arguments
LOG_LEVEL = 100 # 100 means all logs disabled, otherwise, choose the level you desire

global API_URL
global SERVER_ADDRESS
global SERVER_PORT

API_URL = "https://api.trackle.io"
SERVER_ADDRESS = f"{cred.TRACKLE_ID_STRING}.udp.device.trackle.io"
SERVER_PORT = 5684

log.basicConfig(level=LOG_LEVEL, format="[%(levelname)s] %(processName)s : %(msg)s")

def print_http_response(resp, method="HTTP", url=""):
    """Stampa le informazioni della risposta HTTP per il debugging"""
    print(f"\n{'='*80}")
    print(f"HTTP Response - {method} {url}")
    print(f"{'='*80}")
    print(f"Status Code: {resp.status_code}")
    print(f"Status Reason: {resp.reason}")
    print(f"\nHeaders:")
    for key, value in resp.headers.items():
        print(f"  {key}: {value}")
    print(f"\nBody:")
    try:
        if resp.headers.get('content-type', '').startswith('application/json'):
            print(json.dumps(resp.json(), indent=2))
        else:
            print(resp.text[:1000])  # Limita a 1000 caratteri per evitare output troppo lungo
            if len(resp.text) > 1000:
                print(f"... (troncato, lunghezza totale: {len(resp.text)} caratteri)")
    except Exception as e:
        print(f"Errore nel parsing della risposta: {e}")
        print(f"Raw content: {resp.content[:1000]}")
    print(f"{'='*80}\n")

def wait_queue_message(evt_queue: mp.Queue, expect_msg: msgs.QueueMessage,
                       test_class: ut.TestCase = None, timeout: int = 10) -> dict:
    """
    Wait queue event with given name.
    Events with other names found before that are put back to the queue.
    """
    period_s = 0.1
    periods_elapsed = 0
    got_msg_dict = evt_queue.get_nowait() if not evt_queue.empty() else None
    while not isinstance(got_msg_dict, dict) or got_msg_dict["msg"] != expect_msg:
        evt_queue.put(got_msg_dict)
        time.sleep(period_s)
        got_msg_dict = evt_queue.get_nowait() if not evt_queue.empty() else None
        periods_elapsed += 1
        if periods_elapsed * period_s > timeout:
            if test_class is None:
                raise TimeoutError(expect_msg.not_recvd_err_msg(timeout))
            else:
                test_class.fail(expect_msg.not_recvd_err_msg(timeout))
    return got_msg_dict

def wait_sse_event(sse_client: scnw.SSEClientNoWait, event_name: str | set,
                           timeout_seconds: int, test_class: ut.TestCase = None) -> dict:
    """
    Wait SSE event with given name.
    Events with other names found before that are discarded.
    """
    start = datetime.datetime.now()
    while datetime.datetime.now() - start < datetime.timedelta(seconds=timeout_seconds):
        try:
            event = sse_client.pop_nowait()
            if (isinstance(event_name, str) and event.event == event_name) or \
               (isinstance(event_name, set) and event.event in event_name):
                return json.loads(event.data)
        except queue.Empty:
            pass
        time.sleep(0.05)
    # Make error string
    if isinstance(event_name, str):
        error_str = f"Couldn't receive SSE event \"{event_name}\" within {timeout_seconds} seconds."
    elif isinstance(event_name, set):
        error_str = f"Couldn't receive one of the following SSE events within {timeout_seconds} seconds: "
        error_str += ", ".join(event_name)
    else:
        raise RuntimeError("Unexpected type for event_name: " + type(event_name))
    # Fail or raise error ("is this a failure of the test case or an error while preparing for the test case?")
    if test_class is None:
        raise TimeoutError(error_str)
    else:
        test_class.fail(error_str)

class TrackleLibraryTest(ut.TestCase):

    """Trackle Library test suite"""

    @classmethod
    def spawn_device(cls, startup_params : device.DeviceStartupParams):
        """Spawn new process that simulates a Trackle device by connecting through the gateway"""
        cls.from_device = mp.Queue()
        cls.to_device = mp.Queue()
        cls.device_proc = mp.Process(target=device.device_code,
                                     args=(cls.to_device, cls.from_device, startup_params),
                                     name=f"device{cls.spawned_devices}")
        cls.spawned_devices += 1
        cls.device_proc.start()

    @classmethod
    def switch_development_mode(cls, mode: bool):
        """ Switch development mode ON or OFF on the cloud for test device """
        url = f"{API_URL}/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}"
        json_body = {"development": mode}
        resp = req.put(url, headers=cls.headers, json=json_body, timeout=15)
        # print_http_response(resp, "PUT", url)
        if resp.json().get("development") != mode:
            raise Exception("Failed putting in development mode. Can't continue test case.")
        
    @classmethod
    def force_release(cls, version: int | None, intelligent: bool):
        """ Force a particular release on device """
        url = f"{API_URL}/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}"
        json_body = {"desired_firmware_version": str(version) if version else None, "flash":intelligent}
        resp = req.put(url, headers=cls.headers, json=json_body, timeout=15)
        # print_http_response(resp, "PUT", url)
        if resp.status_code != 200:
            raise Exception(f"Failed publishing version: {resp.status_code} {resp.content}")

    @classmethod
    def setUpClass(cls):

        # These will be references to queues that communicate with device.
        # Created by spawn_device.
        cls.to_device = None
        cls.from_device = None

        mp.set_start_method('spawn')
        # spawned devices to 0
        cls.spawned_devices = 0
        # oauth authentication
        log.info("authenticating to Trackle through OAuth ...")
        oauth_url = f"{API_URL}/oauth/token"
        oauth_headers = {"Content-Type": "application/x-www-form-urlencoded;charset=utf-8"}
        oauth_data = {"grant_type": "client_credentials"}
        oauth_basic = req_auth.HTTPBasicAuth(cred.TRACKLE_CLIENT_ID, cred.TRACKLE_CLIENT_SECRET)
        resp = req.post(oauth_url, oauth_data, headers=oauth_headers, auth=oauth_basic, timeout=15)
        # print_http_response(resp, "POST", oauth_url)
        if resp.status_code != 200:
            raise req.HTTPError(f"auth return code {resp.status_code}")
        if "access_token" not in resp.json():
            raise req.HTTPError("No access token in response")
        temp_token = resp.json()["access_token"]
        log.info("oauth authentication done")
        # building metadata for every single request
        cls.headers = {"Authorization": f"Bearer {temp_token}"}
        # opening sse events stream
        sse_url = f"{API_URL}/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/events"
        cls.sse_client = scnw.SSEClientNoWait(sse_url, headers=cls.headers)

    @classmethod
    def tearDownClass(cls):
        cls.to_device.put({"msg": msgs.TESTS_COMPLETED})
        # cls.proxy_proc.join()

    def setUp(self):
        # Ignore events from previous test case
        self.sse_client.clear_pending_events()
        # switching ON development mode to prevent undesired OTA during test
        # and unlock version in release mode
        self.switch_development_mode(False)
        self.force_release(None, False)
        self.switch_development_mode(True)
        # Wait a moment
        time.sleep(1)
        

    def tearDown(self):
        if self.to_device is not None:
            self.to_device.put({"msg":msgs.KILL_DEVICE})
        if self.from_device is not None:
            wait_queue_message(self.from_device, msgs.KILLING)

    def test_16_publish_3(self):
        """
        pubblicazione evento lungo, blockwise
            1: return true,
            2: published true,
            3: error 0
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            SERVER_ADDRESS,
            SERVER_PORT,
            True
        )
        self.spawn_device(params)
        res = wait_queue_message(self.from_device, msgs.CONNECT_RESULT)
        self.assertTrue(res["return"])
        wait_queue_message(self.from_device, msgs.CONNECTED)
        time.sleep(1)
        # Publish event
        self.to_device.put({"msg" : msgs.PUBLISH,
                            "event" : "testing/test_publish_3",
                            "data" : lorem.LOREM_IPSUM[:25000],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK,
                            "key" : 4})
        result = wait_queue_message(self.from_device, msgs.PUBLISH_RESULT, self)
        self.assertTrue(result["return"], "unexpected function return value")
        result = wait_queue_message(self.from_device, msgs.PUBLISH_SENT, self)
        self.assertEqual(result["published"], 1, "published result in sent callback differs from 1")
        self.assertEqual(result["idx"], 4, "msg key in sent callback differs from 4")
        result = wait_sse_event(self.sse_client, "testing/test_publish_3", 15, self)
        self.assertEqual(result["data"], lorem.LOREM_IPSUM[:25000], "cloud data doesn't match")
        result = wait_queue_message(self.from_device, msgs.PUBLISH_COMPLETED, self)
        self.assertEqual(result["error"], 0, "error code in completed callback differs from 0")
        self.assertEqual(result["idx"], 4, "msg key in completed callback differs from 4")

    def test_17_publish_4(self):
        """
        pubblicazione evento lungo, blockwise without ack
            1: return true
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            SERVER_ADDRESS,
            SERVER_PORT,
            True
        )
        self.spawn_device(params)
        res = wait_queue_message(self.from_device, msgs.CONNECT_RESULT)
        self.assertTrue(res["return"])
        wait_queue_message(self.from_device, msgs.CONNECTED)
        time.sleep(1)
        # Publish event
        self.to_device.put({"msg" : msgs.PUBLISH,
                            "event" : "testing/test_publish_4",
                            "data" : lorem.LOREM_IPSUM[:25000],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.NO_ACK,
                            "key" : 6})
        result = wait_queue_message(self.from_device, msgs.PUBLISH_RESULT, self)
        self.assertTrue(result["return"], "unexpected function return value")
        with self.assertRaises(TimeoutError):
            wait_queue_message(self.from_device, msgs.PUBLISH_SENT)
        result = wait_sse_event(self.sse_client, "testing/test_publish_4", 15, self)
        self.assertEqual(result["data"], lorem.LOREM_IPSUM[:25000], "cloud data doesn't match")
        with self.assertRaises(TimeoutError):
            wait_queue_message(self.from_device, msgs.PUBLISH_COMPLETED)

    def test_23_publish_10(self):
        """
        pubblicazione 5 eventi blockwise, errore no free message block 
            - per i primi 4 eventi1: return true, 2: published true: 3: error 0, 4
            - per il 5 evento return false e log no free message block , 5
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            SERVER_ADDRESS,
            SERVER_PORT,
            True
        )
        self.spawn_device(params)
        res = wait_queue_message(self.from_device, msgs.CONNECT_RESULT)
        self.assertTrue(res["return"])
        wait_queue_message(self.from_device, msgs.CONNECTED)
        time.sleep(1)
        # Publish event
        self.to_device.put({"msg" : msgs.MULTIPUBLISH_LONG,
                            "event" : ["testing/test_publish_10_" + str(i) for i in range(1,6)],
                            "data" : lorem.LOREM_IPSUM[:2400],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK})
        result = wait_queue_message(self.from_device, msgs.MULTIPUBLISH_LONG_RESULT, self)
        self.assertListEqual(result["return"], [True, True, True, True, False], "unexpected result")
        to_send_msg_keys = {1,2,3,4}
        for _ in range(4):
            result = wait_queue_message(self.from_device, msgs.PUBLISH_SENT, self)
            self.assertEqual(result["published"], 1, "published result in sent callback is not 1")
            self.assertIn(result["idx"], to_send_msg_keys, "this msg key should have been sent")
            self.assertNotEqual(result["idx"], 5, "this shouldn't have been sent")
            to_send_msg_keys.remove(result["idx"])
        to_complete_msg_keys = {1,2,3,4}
        for _ in range(4):
            result = wait_queue_message(self.from_device, msgs.PUBLISH_COMPLETED, self)
            self.assertEqual(result["error"], 0, "error code in completed callback differs from 0")
            self.assertIn(result["idx"], to_complete_msg_keys, "msg shouldn't have been completed")
            self.assertNotEqual(result["idx"], 5, "this shouldn't have been completed")
            to_complete_msg_keys.remove(result["idx"])
        to_sse_events = set("testing/test_publish_10_"+str(s) for s in range(1,5))
        for _ in range(4):
            result = wait_sse_event(self.sse_client, to_sse_events, 15, self)
        # Check that no other things are arriving on device or through SSE
        with self.assertRaises(TimeoutError):
            wait_queue_message(self.from_device, msgs.PUBLISH_SENT)
        with self.assertRaises(TimeoutError):
            wait_queue_message(self.from_device, msgs.PUBLISH_COMPLETED)
        with self.assertRaises(TimeoutError):
            wait_sse_event(self.sse_client, "testing/test_publish_10_5", 5)

if __name__  == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument('-t','--test', nargs='+', help='Test list', required=False)
    parser.add_argument('-a','--address', help='Coap server address', required=False)
    parser.add_argument('-p','--port', help='Coap server port', required=False)
    parser.add_argument('-s','--apiserver', help='Api server url', required=False)

    # add test.py to test array
    args = parser.parse_args()
    test = "all"

    if not args.test:
        args.test = []
    else:
        test = ' '.join(args.test)

    # add TrackleLibraryTest. to test name
    for i in range(len(args.test)):
        args.test[i] = "TrackleLibraryTest." + args.test[i] 

    # add first parameter to test array
    args.test.insert(0, "./test.py")
    
    if args.address:
        SERVER_ADDRESS = args.address

    if args.port:
        SERVER_PORT = args.port

    if args.apiserver:
        API_URL = args.apiserver

    print("Api url:", API_URL)
    print("Coap server address:", SERVER_ADDRESS)
    print("Coap server port:", SERVER_PORT)
    print("Test:", test)

    ut.main(argv=args.test)