#!/usr/bin/env python3.10

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

import requests as req
import requests.auth as req_auth

import credentials as cred
import lorem_ipsum as lorem
import sseclient_nowait as scnw
import device
import trackle_enums

# Settings behaviours based on command line arguments
LOG_LEVEL = 100 # 100 means all logs disabled, otherwise, choose the level you desire
log.basicConfig(level=LOG_LEVEL, format="[%(levelname)s] %(processName)s : %(msg)s")

def wait_event_name(evt_queue: mp.Queue, event_name: str, test_class: ut.TestCase = None) -> dict:
    """
    Wait queue event with given name.
    Events with other names found before that are put back to the queue.
    """
    period_s = 0.1
    periods_elapsed = 0
    msg = evt_queue.get_nowait() if not evt_queue.empty() else None
    while not isinstance(msg, dict) or msg["name"] != event_name:
        evt_queue.put(msg)
        time.sleep(period_s)
        msg = evt_queue.get_nowait() if not evt_queue.empty() else None
        periods_elapsed += 1
        if periods_elapsed * period_s > 5:
            if test_class is None:
                raise TimeoutError("couldn't receive expected queue event within 5 seconds")
            else:
                test_class.fail("couldn't receive expected queue event within 5 seconds")
    return msg

def wait_sse_publish_event(sse_client: scnw.SSEClientNoWait, event_name: str | set,
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
    if test_class is None:
        raise TimeoutError(f"couldn't receive expected SSE event within {timeout_seconds} seconds")
    else:
        test_class.fail(f"couldn't receive expected SSE event within {timeout_seconds} seconds")

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
    def setUpClass(cls):

        cls.to_proxy = mp.Queue()
        cls.from_proxy = mp.Queue()

        cls.proxy_port = random.randint(49152, 65535)
        cls.proxy_proc = mp.Process(target=proxy_code,
                            args=(cls.to_proxy, cls.from_proxy, cls.proxy_port),
                            name="proxy")
        cls.proxy_proc.start()

        # spawned devices to 0
        cls.spawned_devices = 0
        # oauth authentication
        log.info("authenticating to Trackle through OAuth ...")
        oauth_url = "https://api.trackle.io/oauth/token"
        oauth_headers = {"Content-Type": "application/x-www-form-urlencoded;charset=utf-8"}
        oauth_data = {"grant_type": "client_credentials"}
        oauth_basic = req_auth.HTTPBasicAuth(cred.TRACKLE_CLIENT_ID, cred.TRACKLE_CLIENT_SECRET)
        resp = req.post(oauth_url, oauth_data, headers=oauth_headers, auth=oauth_basic, timeout=15)
        if resp.status_code != 200:
            raise req.HTTPError(f"auth return code {resp.status_code}")
        if "access_token" not in resp.json():
            raise req.HTTPError("No access token in response")
        temp_token = resp.json()["access_token"]
        log.info("oauth authentication done")
        # building metadata for every single request
        cls.headers = {"Authorization": f"Bearer {temp_token}"}
        # opening sse events stream
        sse_url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/events"
        cls.sse_client = scnw.SSEClientNoWait(sse_url, headers=cls.headers)

    @classmethod
    def tearDownClass(cls):
        cls.to_proxy.put({"name": "tests_completed"})
        cls.proxy_proc.join()

    def setUp(self):
        # Switching on proxy and reset
        self.to_proxy.put({"name" : "proxy_on"})
        self.to_proxy.put({"name": "reset_server_conn"})
        wait_event_name(self.from_proxy, "proxy_switched_on")
        wait_event_name(self.from_proxy, "server_conn_was_reset")
        # Wait a moment
        time.sleep(2)
        # Ignore events from previous test case
        self.sse_client.clear_pending_events()

    def tearDown(self):
        self.to_device.put({"name":"kill_device"})
        wait_event_name(self.from_device, "killing")

    def test_connect_1(self):
        """
        connessione con successo
        connect ritorna true, in cloud arriva l'online
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result", self)
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected", self)
        # Check for connection on cloud
        result = wait_sse_publish_event(self.sse_client, "trackle/status", 5, self)
        self.assertIn(result["data"], {"online", "ip-changed"}, "not online from cloud")

    def test_connect_2(self):
        """
        riconnessione dopo errore per rete assente, con proxy
        pausa 20s, connect ritorna true, in cloud arriva l'online dopo aver riattivato il proxy
        """
        # Switching off proxy
        self.to_proxy.put({"name" : "proxy_off"})
        wait_event_name(self.from_proxy, "proxy_switched_off")
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result", self)
        self.assertTrue(res["return"])
        # Wait
        for _ in range(4):
            with self.assertRaises(TimeoutError):
                wait_sse_publish_event(self.sse_client, "trackle/status", 5)
        # Switching on proxy
        self.to_proxy.put({"name" : "proxy_on"})
        wait_event_name(self.from_proxy, "proxy_switched_on")
        # Check for connection on cloud
        log.info("waiting online")
        result = wait_sse_publish_event(self.sse_client, "trackle/status", 15, self)
        self.assertIn(result["data"], {"online", "ip-changed"}, "not online from cloud")
        wait_event_name(self.from_device, "connected", self)

    def test_connect_3(self):
        """
        connessione con connettività alla rete, senza internet, con proxy, errore handshake
        connect ritorna true, i log stampano handshake error, in cloud non arriva l'online
        """
        # Switching off proxy
        self.to_proxy.put({"name" : "proxy_off"})
        wait_event_name(self.from_proxy, "proxy_switched_off")
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result", self)
        self.assertTrue(res["return"])
        # Wait
        with self.assertRaises(TimeoutError):
            wait_sse_publish_event(self.sse_client, "trackle/status", 15)

    def test_connect_4(self):
        """
        connessione con chiave privata errata, errore handshake
        connect ritorna true, i log stampano handshake error, in cloud non arriva l'online
        """
        # Changing private keys
        new_private_key = copy.deepcopy(cred.TRACKLE_PRIVATE_KEY_LIST)
        new_private_key[20] = new_private_key[20] + 1
        # Connection
        params = device.DeviceStartupParams(
            new_private_key,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result", self)
        self.assertTrue(res["return"])
        # Wait
        with self.assertRaises(TimeoutError):
            wait_sse_publish_event(self.sse_client, "trackle/status", 5)

    def test_get_1_1(self):
        """
        get di una variabile boolean
        il valore di ritorno della GET corrisponde al valore impostato alla variabile
        caso true
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send GET True
        method = "getEchoBool"
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/{method}"
        params = {"args" : "1"}
        resp = req.get(url, headers=self.headers, params=params, timeout=15)
        self.assertEqual(resp.json().get("id"), cred.TRACKLE_ID_STRING, "unexpected trackle id")
        self.assertEqual(resp.json().get("name"), "getEchoBool", "unexpected method name")
        self.assertIsInstance(resp.json().get("result"), bool)
        self.assertEqual(resp.json().get("result"), True, "unexpected result")

    def test_get_1_2(self):
        """
        get di una variabile boolean
        il valore di ritorno della GET corrisponde al valore impostato alla variabile
        caso false
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send GET False
        method = "getEchoBool"
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/{method}"
        params = {"args" : "0"}
        resp = req.get(url, headers=self.headers, params=params, timeout=15)
        self.assertEqual(resp.json().get("id"), cred.TRACKLE_ID_STRING, "unexpected trackle id")
        self.assertEqual(resp.json().get("name"), "getEchoBool", "unexpected method name")
        self.assertIsInstance(resp.json().get("result"), bool)
        self.assertEqual(resp.json().get("result"), False, "unexpected result")

    def test_get_2(self):
        """
        get di una variabile numerica (intero)
        il valore di ritorno della GET corrisponde al valore impostato alla variabile
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send GET
        method = "getEchoInt"
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/{method}"
        params = {"args" : "14"}
        resp = req.get(url, headers=self.headers, params=params, timeout=15)
        self.assertEqual(resp.json().get("id"), cred.TRACKLE_ID_STRING, "unexpected trackle id")
        self.assertEqual(resp.json().get("name"), "getEchoInt", "unexpected method name")
        self.assertIsInstance(resp.json().get("result"), numbers.Number)
        self.assertEqual(resp.json().get("result"), 14, "unexpected result")

    def test_get_3(self):
        """
        get di una variabile numerica (double)
        il valore di ritorno della GET corrisponde al valore impostato alla variabile
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send GET
        method = "getEchoDouble"
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/{method}"
        params = {"args" : "1.23456"}
        resp = req.get(url, headers=self.headers, params=params, timeout=15)
        self.assertEqual(resp.json().get("id"), cred.TRACKLE_ID_STRING, "unexpected trackle id")
        self.assertEqual(resp.json().get("name"), "getEchoDouble", "unexpected method name")
        self.assertIsInstance(resp.json().get("result"), numbers.Number)
        self.assertAlmostEqual(resp.json().get("result"), 1.23456, 5, "unexpected result")

    def test_get_4(self):
        """
        get di una variabile stringa
        il valore di ritorno della GET corrisponde al valore impostato alla variabile
        """
        quick_brown_fox = "the quick brown fox jumps over the lazy dog"
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send GET
        method = "getEchoString"
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/{method}"
        params = {"args" : quick_brown_fox}
        resp = req.get(url, headers=self.headers, params=params, timeout=15)
        self.assertEqual(resp.json().get("id"), cred.TRACKLE_ID_STRING, "unexpected trackle id")
        self.assertEqual(resp.json().get("name"), "getEchoString", "unexpected method name")
        self.assertIsInstance(resp.json().get("result"), str)
        self.assertEqual(resp.json().get("result"), quick_brown_fox, "unexpected result")

    def test_get_5(self):
        """
        get di una variabile json
        il valore di ritorno della GET corrisponde al valore impostato alla variabile
        """
        test_json_dict = {"hello":"world"}
        test_json = str(test_json_dict).replace("'","\"")
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send GET
        method = "getEchoJson"
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/{method}"
        params = {"args" : test_json}
        resp = req.get(url, headers=self.headers, params=params, timeout=15)
        self.assertEqual(resp.json().get("id"), cred.TRACKLE_ID_STRING, "unexpected trackle id")
        self.assertEqual(resp.json().get("name"), "getEchoJson", "unexpected method name")
        self.assertIsInstance(resp.json().get("result"), dict)
        self.assertDictEqual(resp.json().get("result"), test_json_dict, "unexpected res")

    def test_post_1(self):
        """
        post con ritorno positivo
        il valore di ritorno della POST corrisponde al valore ritornato dalla funzione nel firmware
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send POST
        method = "postSuccess"
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/{method}"
        json_body = {"args": ""}
        resp = req.post(url, headers=self.headers, data=json_body, timeout=15)
        self.assertEqual(resp.json().get("id"), cred.TRACKLE_ID_STRING, "unexpected trackle id")
        self.assertEqual(resp.json().get("name"), "postSuccess", "unexpected method name")
        self.assertEqual(resp.json().get("return_value"), 10, "unexpected return value")

    def test_post_2(self):
        """
        post con ritorno negativo
        il valore di ritorno della POST corrisponde al valore ritornato dalla funzione nel firmware
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send POST
        method = "postFailing"
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/{method}"
        json_body = {"args": ""}
        resp = req.post(url, headers=self.headers, data=json_body, timeout=15)
        self.assertEqual(resp.json().get("id"), cred.TRACKLE_ID_STRING, "unexpected trackle id")
        self.assertEqual(resp.json().get("name"), "postFailing", "unexpected method name")
        self.assertEqual(resp.json().get("return_value"), -10, "unexpected return value")

    def test_post_3(self):
        """
        post di una funzione privata da utente non customer, errore 403
        il codice di stato della richiesta HTTP è 403, nel firmware non viene chiamata la funzione
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send POST
        method = "postPrivate"
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/{method}"
        json_body = {"args": ""}
        resp = req.post(url, headers=self.headers, data=json_body, timeout=15)
        self.to_device.put({"name":"was_private_post_executed"})
        res = wait_event_name(self.from_device, "private_post_exec_status", self)
        self.assertFalse(res["executed"])
        self.assertEqual(resp.status_code, 403)

    def test_publish_1(self):
        """
        pubblicazione evento singolo with ack -
            1: return true,
            2: published true,
            3: error 0
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Publish event
        self.to_device.put({"name" : "publish",
                            "event" : "testing/test_publish_1",
                            "data" : lorem.LOREM_IPSUM[:500],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK,
                            "key" : 3})
        result = wait_event_name(self.from_device, "publish_result", self)
        self.assertTrue(result["return"], "unexpected function return value")
        result = wait_event_name(self.from_device, "publish_sent", self)
        self.assertEqual(result["published"], 1, "published result in sent callback differs from 1")
        self.assertEqual(result["idx"], 3, "msg key in sent callback differs from 3")
        result = wait_sse_publish_event(self.sse_client, "testing/test_publish_1", 5, self)
        self.assertEqual(result["data"], lorem.LOREM_IPSUM[:500], "cloud data doesn't match")
        result = wait_event_name(self.from_device, "publish_completed", self)
        self.assertEqual(result["error"], 0, "error code in completed callback differs from 0")
        self.assertEqual(result["idx"], 3, "msg key in completed callback differs from 3")

    def test_publish_2(self):
        """
        pubblicazione evento singolo without ack
            1: return true
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Publish event
        self.to_device.put({"name" : "publish",
                            "event" : "testing/test_publish_2",
                            "data" : lorem.LOREM_IPSUM[:500],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.NO_ACK,
                            "key" : 7})
        result = wait_event_name(self.from_device, "publish_result", self)
        self.assertTrue(result["return"], "unexpected function return value")
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_sent")
        result = wait_sse_publish_event(self.sse_client, "testing/test_publish_2", 5, self)
        self.assertEqual(result["data"], lorem.LOREM_IPSUM[:500], "cloud data doesn't match")
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_completed")

    def test_publish_3(self):
        """
        pubblicazione evento lungo, blockwise
            1: return true,
            2: published true,
            3: error 0
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Publish event
        self.to_device.put({"name" : "publish",
                            "event" : "testing/test_publish_3",
                            "data" : lorem.LOREM_IPSUM[:3800],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK,
                            "key" : 4})
        result = wait_event_name(self.from_device, "publish_result", self)
        self.assertTrue(result["return"], "unexpected function return value")
        result = wait_event_name(self.from_device, "publish_sent", self)
        self.assertEqual(result["published"], 1, "published result in sent callback differs from 1")
        self.assertEqual(result["idx"], 4, "msg key in sent callback differs from 4")
        result = wait_sse_publish_event(self.sse_client, "testing/test_publish_3", 15, self)
        self.assertEqual(result["data"], lorem.LOREM_IPSUM[:3800], "cloud data doesn't match")
        result = wait_event_name(self.from_device, "publish_completed", self)
        self.assertEqual(result["error"], 0, "error code in completed callback differs from 0")
        self.assertEqual(result["idx"], 4, "msg key in completed callback differs from 4")

    def test_publish_4(self):
        """
        pubblicazione evento lungo, blockwise without ack
            1: return true
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Publish event
        self.to_device.put({"name" : "publish",
                            "event" : "testing/test_publish_4",
                            "data" : lorem.LOREM_IPSUM[:3700],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.NO_ACK,
                            "key" : 6})
        result = wait_event_name(self.from_device, "publish_result", self)
        self.assertTrue(result["return"], "unexpected function return value")
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_sent")
        result = wait_sse_publish_event(self.sse_client, "testing/test_publish_4", 15, self)
        self.assertEqual(result["data"], lorem.LOREM_IPSUM[:3700], "cloud data doesn't match")
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_completed")

    def test_publish_5(self):
        """
        pubblicazione 5 eventi singoli, errore BANDWIDTH_EXCEDED  
            - per i primi 4 eventi1: return true, 2: published true: 3: error 0, 4
            - per il 5 evento return false e log BANDWIDTH_EXCEDED, 5
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Publish event
        self.to_device.put({"name" : "multipublish",
                            "event" : "testing/test_publish_5",
                            "data" : lorem.LOREM_IPSUM[:500],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "key" : 0,
                            "times" : 5})
        result = wait_event_name(self.from_device, "multipublish_result", self)
        self.assertListEqual(result["return"], [True, True, True, True, False], "unexpected result")
        for _ in range(4):
            result = wait_sse_publish_event(self.sse_client, "testing/test_publish_5", 5, self)
            self.assertEqual(result["data"], lorem.LOREM_IPSUM[:500], "cloud data doesn't match")
        with self.assertRaises(TimeoutError):
            wait_sse_publish_event(self.sse_client, "testing/test_publish_5", 5)

    def test_publish_6(self):
        """
        pubblicazione evento di sistema "trackle", errore no system event
            - 1 return false e log no system event
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Publish event
        self.to_device.put({"name" : "publish",
                            "event" : "trackle/test_publish_6",
                            "data" : lorem.LOREM_IPSUM[:500],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK,
                            "key" : 0})
        result = wait_event_name(self.from_device, "publish_result", self)
        self.assertFalse(result["return"], "unexpected function return value")
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_sent")
        with self.assertRaises(TimeoutError):
            wait_sse_publish_event(self.sse_client, "trackle/test_publish_6", 5)
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_completed")

    def test_publish_7(self):
        """
        pubblicazione evento di sistema "iotready", errore no system event
            - 1 return false e log no system event
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Publish event
        self.to_device.put({"name" : "publish",
                            "event" : "iotready/test_publish_7",
                            "data" : lorem.LOREM_IPSUM[:500],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK,
                            "key" : 0})
        result = wait_event_name(self.from_device, "publish_result", self)
        self.assertFalse(result["return"], "unexpected function return value")
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_sent")
        with self.assertRaises(TimeoutError):
            wait_sse_publish_event(self.sse_client, "trackle/test_publish_7", 5)
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_completed")

    def test_publish_8(self):
        """
        pubblicazione blockwise con ritrasmissione, con proxy, ok
            - si spegne il proxy,
            - 1: return true,
            - 2: published true: si riattiva il proxy,
            - 3: error 0
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Switch off proxy
        self.to_proxy.put({"name" : "proxy_off"})
        wait_event_name(self.from_proxy, "proxy_switched_off")
        # Publish event
        self.to_device.put({"name" : "publish",
                            "event" : "testing/test_publish_8",
                            "data" : lorem.LOREM_IPSUM[:3900],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK,
                            "key" : 2})
        # Wait with proxy off
        time.sleep(10)
        # Switch on proxy
        self.to_proxy.put({"name" : "proxy_on"})
        wait_event_name(self.from_proxy, "proxy_switched_on")
        # Check publish result
        result = wait_event_name(self.from_device, "publish_result", self)
        self.assertTrue(result["return"], "unexpected function return value")
        result = wait_event_name(self.from_device, "publish_sent", self)
        self.assertEqual(result["published"], 1, "published result in sent callback differs from 1")
        self.assertEqual(result["idx"], 2, "msg key in sent callback differs from 2")
        result = wait_sse_publish_event(self.sse_client, "testing/test_publish_8", 15, self)
        self.assertEqual(result["data"], lorem.LOREM_IPSUM[:3900], "cloud data doesn't match")
        result = wait_event_name(self.from_device, "publish_completed", self)
        self.assertEqual(result["error"], 0, "error code in completed callback differs from 0")
        self.assertEqual(result["idx"], 2, "msg key in completed callback differs from 2")

    def test_publish_9(self):
        """
        pubblicazione con ritrasmissione, con proxy, errore
            - si spegne il proxy,
            - 1: return true,
            - 2: published true
            - 3: error > 0
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Switch off proxy
        self.to_proxy.put({"name" : "proxy_off"})
        wait_event_name(self.from_proxy, "proxy_switched_off")
        # Publish event
        self.to_device.put({"name" : "publish",
                            "event" : "testing/test_publish_8",
                            "data" : lorem.LOREM_IPSUM[:3900],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK,
                            "key" : 0})
        # Check publish result
        result = wait_event_name(self.from_device, "publish_result", self)
        self.assertTrue(result["return"])
        result = wait_event_name(self.from_device, "publish_sent", self)
        self.assertEqual(result["published"], 1)
        for _ in range(7):
            try:
                result = wait_event_name(self.from_device, "publish_completed")
            except TimeoutError:
                pass
            else:
                break
        else:
            self.fail("timeout waiting for completion call")
        self.assertNotEqual(result["error"], 0)

    def test_publish_10(self):
        """
        pubblicazione 5 eventi blockwise, errore no free message block 
            - per i primi 4 eventi1: return true, 2: published true: 3: error 0, 4
            - per il 5 evento return false e log no free message block , 5
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Publish event
        self.to_device.put({"name" : "multipublish_long",
                            "event" : ["testing/test_publish_10_" + str(i) for i in range(1,6)],
                            "data" : lorem.LOREM_IPSUM[:2400],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK})
        result = wait_event_name(self.from_device, "multipublish_long_result", self)
        self.assertListEqual(result["return"], [True, True, True, True, False], "unexpected result")
        to_send_msg_keys = {1,2,3,4}
        for _ in range(4):
            result = wait_event_name(self.from_device, "publish_sent", self)
            self.assertEqual(result["published"], 1, "published result in sent callback is not 1")
            self.assertIn(result["idx"], to_send_msg_keys, "this msg key should have been sent")
            self.assertNotEqual(result["idx"], 5, "this shouldn't have been sent")
            to_send_msg_keys.remove(result["idx"])
        to_complete_msg_keys = {1,2,3,4}
        for _ in range(4):
            result = wait_event_name(self.from_device, "publish_completed", self)
            self.assertEqual(result["error"], 0, "error code in completed callback differs from 0")
            self.assertIn(result["idx"], to_complete_msg_keys, "msg shouldn't have been completed")
            self.assertNotEqual(result["idx"], 5, "this shouldn't have been completed")
            to_complete_msg_keys.remove(result["idx"])
        to_sse_events = set("testing/test_publish_10_"+str(s) for s in range(1,5))
        for _ in range(4):
            result = wait_sse_publish_event(self.sse_client, to_sse_events, 15, self)
        # Check that no other things are arriving on device or through SSE
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_sent")
        with self.assertRaises(TimeoutError):
            wait_event_name(self.from_device, "publish_completed")
        with self.assertRaises(TimeoutError):
            wait_sse_publish_event(self.sse_client, "testing/test_publish_10_5", 5)

    def test_publish_11(self):
        """
        pubblicazione singola con ritrasmissione, con proxy, ok
            - si spegne il proxy,
            - 1: return true,
            - 2: published true: si riattiva il proxy,
            - 3: error 0
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Switch off proxy
        self.to_proxy.put({"name" : "proxy_off"})
        wait_event_name(self.from_proxy, "proxy_switched_off")
        # Publish event
        self.to_device.put({"name" : "publish",
                            "event" : "testing/test_publish_11",
                            "data" : lorem.LOREM_IPSUM[:500],
                            "ttl" : 30,
                            "visibility" : trackle_enums.PublishVisibility.PUBLIC,
                            "ack" : trackle_enums.PublishType.WITH_ACK,
                            "key" : 2})
        # Wait with proxy off
        time.sleep(10)
        # Switch on proxy
        self.to_proxy.put({"name" : "proxy_on"})
        wait_event_name(self.from_proxy, "proxy_switched_on")
        # Check publish result
        result = wait_event_name(self.from_device, "publish_result", self)
        self.assertTrue(result["return"], "unexpected function return value")
        result = wait_event_name(self.from_device, "publish_sent", self)
        self.assertEqual(result["published"], 1, "published result in sent callback differs from 1")
        self.assertEqual(result["idx"], 2, "msg key in sent callback differs from 2")
        result = wait_sse_publish_event(self.sse_client, "testing/test_publish_11", 15, self)
        self.assertEqual(result["data"], lorem.LOREM_IPSUM[:500], "cloud data doesn't match")
        result = wait_event_name(self.from_device, "publish_completed", self)
        self.assertEqual(result["error"], 0, "error code in completed callback differs from 0")
        self.assertEqual(result["idx"], 2, "msg key in completed callback differs from 2")

    def test_signal_1(self):
        """
        signal - la chiamata alle api ritorna 200, viene chiamata la callback signal
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send PUT with signal
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}"
        json_body = {"signal": "1"}
        resp = req.put(url, headers=self.headers, data=json_body, timeout=15)
        self.assertEqual(resp.json().get("id"), cred.TRACKLE_ID_STRING, "unexpected trackle id")
        self.assertTrue(resp.json().get("ok"), "unexpected method name")
        self.assertTrue(resp.json().get("signaling"), "unexpected return value")
        wait_event_name(self.from_device, "signal_called", self)

    def test_ping_1(self):
        """
        ping - la chiamata alle api ritorna 200
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send PUT with ping
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}/ping"
        json_body = {}
        resp = req.put(url, headers=self.headers, data=json_body, timeout=15)
        self.assertTrue(resp.json().get("online"), "unexpected return value")

    def test_reboot_1(self):
        """
        reset - la chiamata alle api ritorna 200, viene chiamata la callback reset
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send PUT with reset
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}"
        json_body = {"reset": "reboot"}
        resp = req.put(url, headers=self.headers, data=json_body, timeout=15)
        self.assertTrue(resp.json().get("ok"), "unexpected method name")
        wait_event_name(self.from_device, "reboot_called", self)

    def test_claim_code_1(self):
        """
        reset - la chiamata alle api ritorna 200, viene chiamata la callback reset
        """
        # Connection
        claim_code = "test_claim_code"
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port,
            claim_code
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Check for claim code
        result = wait_sse_publish_event(self.sse_client, "trackle/device/claim/code", 5, self)
        self.assertEqual(result["data"], claim_code, "wrong claim code")

    def test_get_time_1(self):
        """
        claim code - si setta un claimcode prima della connect,
        si controlla dopo la connessione che arrivi il publish su sse e che sia quello settato
        """
        # Connection
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Call get time and wait
        self.to_device.put({"name" : "get_time"})
        wait_event_name(self.from_device, "get_time_called", self)

    def test_component_list_1(self):
        """
        components list - si imposta una stringa con trackle.setComponentsList prima della connect
        si controllo dopo la connessione avvenuta con una chiamata all'api device che l'attributo
        sia uguale alla stringa inviata
        """
        # Connection
        components_list = "component1, component2, component3"
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port,
            components_list=components_list
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send GET for device information
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}"
        resp = req.get(url, headers=self.headers, timeout=15)
        self.assertEqual(resp.json().get("firmware_components_list"), components_list, "wrong list")

    def test_imei_iccid_1(self):
        """
        imei and iccid list - si imposta l'imei con trackle.setImei e l'iccid con trackle.setIccid 
        prima della connect, si controllo dopo la connessione avvenuta con una chiamata all'api device 
        che gli attributi siano uguali alle stringhe inviate
        """
        # Connection
        imei = "123456789012345"
        iccid = "123456789012345678"
        params = device.DeviceStartupParams(
            cred.TRACKLE_PRIVATE_KEY_LIST,
            self.proxy_port,
            imei=imei,
            iccid=iccid
        )
        self.spawn_device(params)
        res = wait_event_name(self.from_device, "connect_result")
        self.assertTrue(res["return"])
        wait_event_name(self.from_device, "connected")
        # Send GET for device information
        url = f"https://api.trackle.io/v1/products/1000/devices/{cred.TRACKLE_ID_STRING}"
        resp = req.get(url, headers=self.headers, timeout=15)
        self.assertEqual(resp.json().get("imei"), imei, "wrong imei")
        self.assertEqual(resp.json().get("iccid"), iccid, "wrong iccid")

def proxy_code(from_tester : mp.Queue, to_tester : mp.Queue, local_port : int):
    """
    Code for process that implements UDP proxy.
    Proxy is enabled by default.
    """

    # sockets init
    server_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    device_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_sock.setblocking(False)
    device_sock.setblocking(False)
    device_sock.bind(("127.0.0.1", local_port))
    log.info("Proxy listening on port %d of 127.0.0.1", local_port)

    enabled = True
    device_addr = None
    while True:

        # device to server
        with contextlib.suppress(OSError):
            payload, device_addr = device_sock.recvfrom(2048)
            if enabled:
                try:
                    server_addr = (f"{cred.TRACKLE_ID_STRING}.udp.device.trackle.io", 5684)
                    server_sock.sendto(payload, server_addr)
                except OSError as exc:
                    log.error(exc)

        # server to device
        with contextlib.suppress(OSError):
            payload, _ = server_sock.recvfrom(2048)
            if enabled:
                try:
                    device_sock.sendto(payload, device_addr)
                except OSError as exc:
                    log.error(exc)

        # interpret commands
        in_msg = from_tester.get_nowait() if not from_tester.empty() else None
        if isinstance(in_msg, dict):
            match in_msg.get("name"):
                case "proxy_off":
                    enabled = False
                    log.info("proxy switched off")
                    to_tester.put({"name" : "proxy_switched_off"})
                case "proxy_on":
                    enabled = True
                    log.info("proxy switched on")
                    to_tester.put({"name" : "proxy_switched_on"})
                case "tests_completed":
                    log.info("tester terminated, quitting")
                    break
                case "reset_server_conn":
                    server_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    server_sock.setblocking(False)
                    log.info("server conn reset")
                    to_tester.put({"name" : "server_conn_was_reset"})

        time.sleep(0.05)

if __name__  == "__main__":
    ut.main(argv=sys.argv)
