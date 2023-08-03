#!/usr/bin/env python3

from collections import namedtuple
from contextlib import contextmanager
from dateutil import parser as du_parser
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Tuple, Union
import cec as libcec
import ctypes as ct
import datetime
import glob
import inspect
import json
import logging
import math
import os
import pathlib
import queue
import re
import RPi.GPIO as GPIO
import shlex
import socket
import subprocess
import sys
import tempfile
import textwrap
import threading
import time
import traceback

FROM_SSH = ("SSH_CONNECTION" in os.environ)

ENABLE_TTS = False # TODO

ENABLE_IOT_MANUAL_PING = True
IOT_KEEPALIVE_S = 30

class Sicurphone_logger:
    class Formatter(logging.Formatter):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.start_time = time.time()
        # absolute time
        def formatTime(self, record, datefmt=None) -> str:
            ct = time.gmtime(record.created)
            t = time.strftime("%Y-%m-%dT%H:%M:%S", ct)
            s = "%s.%03dZ" % (t, record.msecs)
            return s
        # relative time
        #def formatTime(self, record, datefmt=None) -> str:
        #    elapsed_seconds = record.created - self.start_time
        #    td = datetime.timedelta(seconds=elapsed_seconds)
        #    return str(td)

    def __init__(self):

        self.CRITICAL = logging.CRITICAL # 50
        self.ERROR    = logging.ERROR    # 40
        self.WARNING  = logging.WARNING  # 30
        self.INFO     = logging.INFO     # 20
        self.VERBOSE  = 15
        self.DEBUG    = logging.DEBUG    # 10
        self.DVERBOSE = 5

        self.ENABLE_LOG_COLORS = False

        logging.addLevelName(self.VERBOSE, "VERBOSE")
        logging.addLevelName(self.DVERBOSE, "DVERBOSE")
        self.logger = logging.getLogger()
        handler = logging.StreamHandler(sys.stdout)
        formatter = Sicurphone_logger.Formatter('[%(asctime)s %(levelname)-8s %(threadName)s] %(message)s')
        handler.setFormatter(formatter)
        self.logger.addHandler(handler)
        self.logger.setLevel(999)
        self.logger.disabled = True

    def set_log_level(self, level : Optional[int]):
        if level is None:
            self.logger.setLevel(999)
            self.logger.disabled = True
        else:
            self.logger.setLevel(level)
            self.logger.disabled = False

    def red(self, msg) -> str:
        return self.colored([255, 64, 64], msg)

    def yellow(self, msg) -> str:
        return self.colored([255, 255, 64], msg)

    def blue(self, msg) -> str:
        return self.colored([64, 255, 255], msg)

    def green(self, msg) -> str:
        return self.colored([64, 255, 64], msg)

    def purple(self, msg) -> str:
        return self.colored([255, 64, 255], msg)

    def colored(self, color, msg) -> str:
        if not self.ENABLE_LOG_COLORS:
            return msg
        # 'color' must be an array of RGB components in [0,255]
        # See ANSI color escape sequences.
        # An escape sequence is: \033[<command>m
        # Command '0' is a reset. Command '38;2' sets the foreground color as a 24-bit value
        return "\033[38;2;{};{};{}m{}\033[0m".format(color[0], color[1], color[2], msg)

    def log(self, lvl, msg, *args, **kwargs):
        self.logger.log(lvl, msg, *args, **kwargs)

    def dv(self, msg, *args, **kwargs):
        self.logger.log(self.DVERBOSE, msg, *args, **kwargs)

    def v(self, msg, *args, **kwargs):
        self.logger.log(self.VERBOSE, msg, *args, **kwargs)

    def d(self, msg, *args, **kwargs):
        self.logger.debug(msg, *args, **kwargs)

    def i(self, msg, *args, **kwargs):
        self.logger.info(msg, *args, **kwargs)

    def w(self, msg, *args, **kwargs):
        self.logger.warning(msg, *args, **kwargs)

    def e(self, msg, *args, **kwargs):
        self.logger.error(msg, *args, **kwargs)

    def c(self, msg, *args, **kwargs):
        self.logger.critical(msg, *args, **kwargs)

    def x(self, msg=None, *args, **kwargs):
        if msg is None or msg == "":
            msg = "Exception"
        self.logger.exception(msg, *args, **kwargs)

    def xx(self, exc, msg=None, *args, **kwargs):
        if msg is None or msg == "":
            msg = "Exception"
        self.logger.exception(msg, *args, **kwargs)
        s = msg + "\n"
        if exc is None:
            s += str(exc)
        else:
            tb_list = traceback.format_exception(etype=type(exc), value=exc, tb=exc.__traceback__)
            s += str(exc) + "\n" + "".join(tb_list)
        self.e(s, *args, **kwargs)

lg = Sicurphone_logger()
SICURPHONE_LOG_LEVEL = lg.DEBUG
lg.set_log_level(SICURPHONE_LOG_LEVEL)

os.environ['KIVY_NO_ARGS'] = '1'
os.environ['KIVY_NO_CONSOLELOG'] = '1'
import kivy
kivy.require('1.11.1')
from kivy.config import Config
# log level: either of 'trace', 'debug', 'info', 'warning', 'error', 'critical'
Config.set('kivy', 'log_enable', 1)
Config.set('kivy', 'log_level', 'info')

if FROM_SSH:
    Config.set('graphics', 'width', '1280')
    Config.set('graphics', 'height', '800')
else:
    Config.set('graphics', 'borderless', '1')
    Config.set('graphics', 'fullscreen', 'auto')
Config.set("graphics", "show_cursor", 0)

from kivy.app import App
from kivy.clock import Clock
from kivy.core.window import Keyboard, Window
from kivy.effects.scroll import ScrollEffect
from kivy.graphics import Color, Rectangle
from kivy.metrics import sp
from kivy.properties import StringProperty
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.button import Button
from kivy.uix.floatlayout import FloatLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.image import Image
from kivy.uix.label import Label
from kivy.uix.popup import Popup
from kivy.uix.screenmanager import ScreenManager, Screen, NoTransition
from kivy.uix.scrollview import ScrollView
from kivy.uix.textinput import TextInput
from kivy.uix.widget import Widget

NAME_FOR_CEC = "SICURPHONE"
PROG_NAME = "Sicurphone"
PROG_VERSION = "0.1"
PROG_VERSION_FOR_IOT = 0x0001  # the library requires a uint32/int32, but the low-level protocol only handles uint16 (it gets truncated)
PRODUCT_ID_FOR_IOT = 1

TTS_FOLDER_PATH = "tts"
TTS_FILE_EXT = "wav"

# either "." or ","
DECIMAL_POINT_CHAR = "."

# These are OR-able bitfield values
# libcec.CEC_LOG_ERROR
# libcec.CEC_LOG_WARNING
# libcec.CEC_LOG_NOTICE
# libcec.CEC_LOG_TRAFFIC
# libcec.CEC_LOG_DEBUG
# 0 for none
#CEC_LOG_LEVEL = libcec.CEC_LOG_WARNING | libcec.CEC_LOG_ERROR
CEC_LOG_LEVEL = 0

IOT_LOG_LEVEL = 0  # see Iot.Log_Level; 0 should be an alias for no logging

GUI_TEXT_COLOR_DEFAULT = (1.0, 1.0, 1.0, 1.0)
#GUI_TEXT_COLOR_ERROR = (1.0, 0.4, 0.4, 1.0)
GUI_TEXT_COLOR_ERROR = (1.0, 0.5, 0.1, 1.0)

KEY_0 = libcec.CEC_USER_CONTROL_CODE_NUMBER0
KEY_1 = libcec.CEC_USER_CONTROL_CODE_NUMBER1
KEY_2 = libcec.CEC_USER_CONTROL_CODE_NUMBER2
KEY_3 = libcec.CEC_USER_CONTROL_CODE_NUMBER3
KEY_4 = libcec.CEC_USER_CONTROL_CODE_NUMBER4
KEY_5 = libcec.CEC_USER_CONTROL_CODE_NUMBER5
KEY_6 = libcec.CEC_USER_CONTROL_CODE_NUMBER6
KEY_7 = libcec.CEC_USER_CONTROL_CODE_NUMBER7
KEY_8 = libcec.CEC_USER_CONTROL_CODE_NUMBER8
KEY_9 = libcec.CEC_USER_CONTROL_CODE_NUMBER9
KEY_DOT = libcec.CEC_USER_CONTROL_CODE_DOT
KEY_UP = libcec.CEC_USER_CONTROL_CODE_UP
KEY_DOWN = libcec.CEC_USER_CONTROL_CODE_DOWN
KEY_LEFT = libcec.CEC_USER_CONTROL_CODE_LEFT
KEY_RIGHT = libcec.CEC_USER_CONTROL_CODE_RIGHT
KEY_OK = libcec.CEC_USER_CONTROL_CODE_SELECT
KEY_ENTER = libcec.CEC_USER_CONTROL_CODE_ENTER
KEY_EXIT = libcec.CEC_USER_CONTROL_CODE_EXIT
KEY_CLEAR = libcec.CEC_USER_CONTROL_CODE_CLEAR

#debug
def print_dir(o):
    keys = sorted(dir(o))
    N = max([len(k) for k in keys])
    N = min(32, N)
    N = max(1, N)
    fmt = "{:" + str(N) + "s} = {}"
    for k in keys:
        print(fmt.format(k, getattr(o, k)))

#debug
def print_stack_info(frame = None, args_info : bool = False):
    if frame is None:
        f = inspect.currentframe()
    else:
        f = frame
    # skip our own frame
    if f is not None:
        f = f.f_back
    out : List[str] = []
    N = 0
    while f is not None:
        fi = inspect.getframeinfo(f)
        av = inspect.getargvalues(f)

        if fi.function == "<module>":
            #break
            pass

        N += 1
        ff = "{}() @ {}:{}".format(fi.function, fi.filename, fi.lineno)
        if args_info:
            ffa = []
            for a in av.args:
                ffa.append(" {}={}".format(a, repr(av.locals.get(a, None))))
            if av.varargs is not None:
                va = av.locals.get(av.varargs)
                ffa.append(" *{}={}".format(va, "()" if va is None else repr(va)))
            if av.keywords is not None:
                kw = av.locals.get(av.keywords, None)
                v = {key: av.locals.get(key, None) for key in kw} if kw is not None else dict()
                ffa.append(" **{}={}".format(kw, repr(v)))
            ffl = repr(av.locals)
            ff = ff + " | " + " ".join(ffa) + " | " + ffl
        out.append(ff)
        f = f.f_back

    thr = threading.current_thread()
    outs = "{}-deep stack (thread {}):\n\t".format(N, repr(thr.name)) + "\n\t".join(out)
    print(outs)

def clip_lower(x, x0):
    if x < x0:
        return x0
    else:
        return x

def clip_upper(x, x1):
    if x > x1:
        return x1
    else:
        return x

def clip_both(x, x0, x1):
    if x < x0:
        return x0
    elif x > x1:
        return x1
    else:
        return x

def index_or(lst, elem, if_not_found=None):
    if elem in lst:
        return lst.index(elem)
    else:
        return if_not_found

# monotonic timestamp in [s] (float)
def get_tick() -> float:
    return time.monotonic()

def get_datetime() -> datetime.datetime:
    return datetime.datetime.now().astimezone()

def get_datetime_str_for_publish(dt : Optional[datetime.datetime] = None):
    if dt is None:
        dt = get_datetime()
    return dt.isoformat()

def ct_is_null(p) -> bool:
    return p is None or ct.cast(p, ct.c_void_p).value is None

def ct_cstring_to_string(v) -> Optional[str]:
    if ct_is_null(v):
        return None
    v = ct.cast(v, ct.c_char_p).value
    if v is None:
        return None
    return v.decode('utf-8', errors='replace')

def time_is_in_interval(t: datetime.time, t0: datetime.time, t1: datetime.time) -> bool:
    if t1 >= t0:
        # interval: [t0, t1)
        return t >= t0 and t < t1
    else:
        # around midnight
        return t >= t0 or t < t1

def get_tick_ms() -> int:
    return int(round(time.monotonic() * 1000))

def is_main_thread(thread=None):
    if thread is None:
        thread = threading.current_thread()
    return thread is threading.main_thread()

def make_object(name=''):
    """ Creates an object to which new attributes can be added and accessed with
    the dot notation, jsut like in a class """
    return type(name, (), {})

def json_dump_compact(o) -> str:
    return json.dumps(o, ensure_ascii=True, indent=None, separators=(',', ':'))

def delete_by_indices(lst : List[Any], indices : List[int]):
    N = len(lst)
    indices = [(idx if idx >= 0 else N + idx) for idx in indices]
    indices = sorted(indices, reverse=True)
    for idx in indices:
        del lst[idx]

@contextmanager
def lock_acquire(lock, timeout : Optional[float] = None):
    if timeout is None:
        timeout = -1
    result = lock.acquire(timeout=timeout)
    yield result
    if result:
        lock.release()

def asciify(b : Optional[Union[bytearray, bytes]]) -> Optional[str]:
    if b is None:
        return None
    out = bytearray(len(b))
    for (i, bb) in enumerate(b):
        if bb >= 32 and bb < 127:
            out[i] = bb
        else:
            out[i] = 126 # '~'
    return out.decode('ascii', errors='replace')

################################

class Queue(queue.Queue):
    '''
    A custom queue subclass that provides a :meth:`clear` method.
    '''

    def clear(self):
        '''
        Clears all items from the queue.
        '''

        with self.mutex:
            unfinished = self.unfinished_tasks - len(self.queue)
            if unfinished <= 0:
                if unfinished < 0:
                    raise ValueError('task_done() called too many times')
                self.all_tasks_done.notify_all()
            self.unfinished_tasks = unfinished
            self.queue.clear()
            self.not_full.notify_all()

qitem_t = namedtuple("qitem_t", ["tick", "datetime", "data"])
def q_put_data(q: Queue, data) -> bool:
    qi = qitem_t(tick=get_tick(), datetime=get_datetime(), data=data)
    return q_put(q, qi)

def q_put(q: Queue, data) -> bool:
    try:
        q.put(data)
        return True
    except queue.Full:
        return False

def q_get(q: Queue, timeout: float = 0):
    if q.empty():
        return None
    try:
        if timeout == 0:
            return q.get(False)
        else:
            return q.get(True, timeout)
    except queue.Empty:
        return None

################################

class Stoppable_thread(threading.Thread):
    threads: List['Stoppable_thread'] = []

    def __init__(self, run, shutdown_on_exception=False, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._stop_event = threading.Event()
        self._run = run
        self._last_alive : Optional[float] = None
        self._last_alive_lock = threading.RLock()
        self._shutdown_on_exception : bool = shutdown_on_exception

    def stop(self):
        self._stop_event.set()

    def is_stopped(self) -> bool:
        return self._stop_event.is_set()

    def set_last_alive(self, tick : float):
        with self._last_alive_lock:
            self._last_alive = tick

    def get_last_alive(self) -> Optional[float]:
        with self._last_alive_lock:
            return self._last_alive

    def run(self, *args, **kwargs):
        try:
            Stoppable_thread.threads.append(self)
            self._run(*args, **kwargs)
        except SystemExit:
            if self._shutdown_on_exception:
                lg.x("SystemExit in Stoppable_thread")
                Gui_utils.close_gui()
            raise
        except:
            lg.x("Exception in Stoppable_thread")
            if self._shutdown_on_exception:
                Gui_utils.close_gui()
                exit(1)
            raise
        finally:
            if self in Stoppable_thread.threads:
                Stoppable_thread.threads.remove(self)

def thread_has_problems(thread : Optional[Stoppable_thread], now : float, start : float, timeout : float, timeout_first_time : float) -> bool:
    if start is not None and now - start < timeout_first_time:
        return False

    if thread is None:
        return True

    if not thread.is_alive():
        return True

    last_alive = thread.get_last_alive()
    if last_alive is None:
        return True
    if now - last_alive > timeout:
        return True

    return False

################################

class Cmd:
    @staticmethod
    def is_root():
        uid = os.getuid()
        return uid == 0

    @staticmethod
    def quote_cmd(cmd : Union[str, List[str]]) -> str:
        if not isinstance(cmd, list):
            cmd = [cmd]
        return " ".join(map(shlex.quote, cmd))

    @staticmethod
    def async_cmd(c : Union[str, List[str]]) -> Optional[subprocess.Popen]:
        c = Cmd.quote_cmd(c)
        lg.d("async cmd `{}`".format(c))
        p = subprocess.Popen(c, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, shell=True)
        return p

    @staticmethod
    def cmd(c : Union[str, List[str]], timeout : float) -> Tuple[Optional[int], Optional[subprocess.Popen], Optional[str], Optional[str]]:
        c = Cmd.quote_cmd(c)
        lg.d("cmd `{}`".format(c))
        p = subprocess.Popen(c, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, shell=True)

        out, err = p.communicate(timeout=timeout)
        if err is None:
            err = ''
        if out is None:
            out = ''
        rc = p.returncode
        return (rc, p, out ,err)

################################

class Gpio_button:
    def __init__(
            self,
            pin_number : int,
            pull_up : Optional[bool] = None,
            active_state : Optional[bool] = None,
            bounce_time : float = None,
            antispike_time : float = None,
            when_pressed : Callable[['Gpio_button'], None] = None,
            when_released : Callable[['Gpio_button'], None] = None
            ):

        if active_state is None:
            active_state = True if pull_up == False else False

        self.pin_number = pin_number
        self.pull_up = pull_up
        self.active_state = active_state
        self.bounce_time = bounce_time
        self.antispike_time = antispike_time

        self.when_pressed = when_pressed
        self.when_released = when_released
        self.pin_state = False
        self.is_active = not active_state

        self.last_pin_change_tick = None
        self.last_state_change_tick = None
        self.last_press_tick = None
        self.last_release_tick = None
        self.last_sample_tick = None

        pud = {None: GPIO.PUD_OFF, False: GPIO.PUD_DOWN, True: GPIO.PUD_UP}[pull_up]
        GPIO.setup(self.pin_number, GPIO.IN, pull_up_down=pud)

        self.thread = Stoppable_thread(self.run, name="gpio_{}".format(pin_number))
        self.thread.daemon = True
        self.thread.start()

    def __del__(self):
        self.thread.stop()

    def _read(self) -> bool:
        return bool(GPIO.input(self.pin_number))

    def run(self):
        thr = threading.current_thread()
        self.pin_state = self._read()
        self.last_sample_tick = get_tick()
        self.is_active = (self.pin_state == self.active_state)
        while True:
            time.sleep(5e-3)
            if thr.is_stopped():
                return
            thr.set_last_alive(get_tick())
            pin_state = self._read()
            now = get_tick()
            self.last_sample_tick = now
            if pin_state != self.pin_state:
                if (self.antispike_time is None
                        or self.last_pin_change_tick is None
                        or now - self.last_pin_change_tick >= self.antispike_time):
                    self.last_pin_change_tick = now
                    self.pin_state = pin_state
            is_active = (self.pin_state == self.active_state)
            if is_active != self.is_active:
                if (self.bounce_time is None
                        or self.last_state_change_tick is None
                        or now - self.last_state_change_tick >= self.bounce_time):
                    self.last_state_change_tick = now
                    self.is_active = is_active
                    if is_active:
                        self.last_press_tick = now
                        if self.when_pressed is not None:
                            self.when_pressed(self)
                    else:
                        self.last_release_tick = now
                        if self.when_released is not None:
                            self.when_released(self)

    def time_since_last_press(self) -> Optional[float]:
        if self.last_press_tick is None:
            return None
        return self.last_sample_tick - self.last_press_tick

    def time_since_last_release(self) -> Optional[float]:
        if self.last_release_tick is None:
            return None
        return self.last_sample_tick - self.last_release_tick

class Gpio:
    EMG_PIN = 23

    class Evt_type(Enum):
        EMG = 1
    evt_t = namedtuple("evt_t", ["type"])

    def __init__(self):
        self.evt_queue = None
        self.emg_button = None
        self.last_loop = None

    def __del__(self):
        pass

    def loop(self):
        now = get_tick()

        if self.last_loop is not None and now - self.last_loop < 10:
            return
        self.last_loop = now

        e = self.emg_button
        ok = False
        if e is not None:
            t = e.thread
            if t.is_alive() and now - t.get_last_alive() < 10:
                ok = True

        if not ok:
            lg.e("Emg gpio thread has problems, restart it")
            if e is not None:
                 e.thread.stop()
            self.emg_button = self.create_emg_btn()

    def create_emg_btn(self) -> Gpio_button:
        emg_button = Gpio_button(Gpio.EMG_PIN, pull_up=True, bounce_time=0.3, antispike_time=0.05) # pullup=True implies active_state=False
        emg_button.when_pressed = self.on_emg_press
        return emg_button

    def init(self, queue : Queue):
        self.emg_button = self.create_emg_btn()
        self.evt_queue = queue

    def on_emg_press(self, btn):
        if btn == self.emg_button:
            lg.i("EMG press detected")
            if self.evt_queue is not None:
                evt = Gpio.evt_t(type=Gpio.Evt_type.EMG)
                q_put_data(self.evt_queue, evt)
        else:
            lg.w("Spurious event: button press on {}".format(btn))

################################

class Item:
    class Type(Enum):
        MSG = 1
        REQ_DATA = 2

    class Outcome(Enum):
        PENDING = 0
        CANCELLED = 1
        DONE = 2
        MERGED = 3

    class Source(Enum):
        USER = 1
        SCHEDULE = 2
        SERVER = 3

    def __init__(self, source : Source, type : Type, req = None, deadline_tick : float = None, timestamp : Optional[datetime.datetime] = None):
        self.source = source
        self.type = type
        self.req = req
        self.deadline_tick = deadline_tick
        self.lock = threading.RLock()
        self.outcome = Item.Outcome.PENDING
        self.timed_out = False
        self.timestamp = timestamp if timestamp is not None else get_datetime()

    def __repr__(self) -> str:
        return "<Item src={}, type={}, req={}, dl={}, oc={}, tout?={}>".format(
            self.source,
            self.type,
            self.req,
            self.deadline_tick,
            self.outcome,
            self.timed_out
        )

################################

# *** IOTReady communication library docs ***
# The communication library is loaded from a DLL (into an object which will be called 'lib' from now on).
# The following functions must be called on the 'lib' wrapper object.
# The type of the functions is not checked; they must be called with proper python types.
# Callback types are not declared. Callbacks will be called with the proper python types.
#
# (int is 32-bit)
#
# Iotready* newIotready(void);
#   to create the library
#
# void deleteIotready(Iotready* v);
#
# void iotreadyInit(Iotready* v);
#
# void iotreadySetEnabled(Iotready* v, bool status);
#
# bool iotreadyIsEnabled(Iotready* v);
#
# bool iotreadyGet(Iotready* v, const char* varKey, void* var, Data_TypeDef type);
#   registers a "variable" to the library. The app can overwrite `var`'s memory (its address must not change, though);
#   the server will be able to request the variable, and the library will provide its current value (without intervention from the app)
#     varKey : the name of the variable
#     var : the address of the memory location where the variable can be accessed
#     type : the type of the variable
#
# bool iotreadyPost(Iotready* v, const char *funcKey, user_function_int_char_t* func, Function_PermissionDef permission);
#   registers a "function" to the library. The server can "call" the function by specifying its name and one single string argument.
#   The library will call the registered callback (one per each function name), passing the string argument
#     funcKey: the name of the function
#     func: the callback to be called by the library when it receives a function request from the server
#
# bool iotreadyPublish(Iotready* v, const char *eventName, const char *data, int ttl, Event_Type eventType, Event_Flags eventFlag);
#     ttl = time-to-live (seconds); now unused, might be used for data-related server-side logic
#     pubtype : see PUBTYPE_*
#     flags : see PUBFLAG_*
#     return : true if sent (no info about ack), false if not sent (immediate error)
#   called by the app to send data to the server. The library will immediately return false if the transmission has an error (e.g. not inited).
#   The library will return true if the data was sent (no info about the actual reception).
#   If ACK is used, the library will remember the request and retry it a few times. It will call the publishSendCallback whener a send occurs, and
#   publishCompletionCallback whenever the transmission is completed (no error) or it failed definitively (with an error code).
#   The app can use the associated ids to know which request a callback refers to.
#   If more retries are needed, the app should try a new (identical) transmission after the failure callback.
#   Both callbacks are only ever called if the ACK publish flag is used. Otherwise, a single attempt is made (and the result is the return value of the publish function)
#
# bool iotreadySubscribe(Iotready* v, const char *eventName, EventHandler handler, Subscription_Scope_Type scope, const char *deviceID);
#
# void iotreadyUnsubscribe(Iotready* v);
#
# typedef uint32_t system_tick_t;
# typedef system_tick_t(millisCallback)(void);
# void iotreadySetMillis(Iotready* v, millisCallback* millis);
#   register the function called by the library to get the current time
#
# void iotreadySetDeviceId(Iotready* v, char *deviceid);
#
# void iotreadySetKeys(Iotready* v, unsigned char *server, unsigned char *client);
#
# typedef int (sendCallback)(const unsigned char *buf, uint32_t buflen, void* tmp);
# void iotreadySetSendCallback(Iotready* v, sendCallback* send);
#   register the function called by the library to ask the app to request sending data over the connection
#
# typedef int (receiveCallback)(unsigned char *buf, uint32_t buflen, void* tmp);
# void iotreadySetReceiveCallback(Iotready* v, receiveCallback* receive);
#   register the function called by the library to ask the app to request receiving data over the connection
#
# typedef int (connectCallback)(const char* address, int port);
# void iotreadySetConnectCallback(Iotready* v, connectCallback *connect);
#   register the function called by the library to ask the app to open the connection
#
# typedef int (disconnectCallback)(void);
# void iotreadySetDisconnectCallback(Iotready* v, disconnectCallback *disconnect);
#   register the function called by the library to ask the app to close the connection
#
# typedef void (publishCompletionCallback)(int error, const void* data, void* callbackData, void* reserved);
# void iotreadySetCompletedPublishCallback(Iotready* v, publishCompletionCallback *publish);
#     error : error code (0 no error; see Iot.PUBERR_*)
#     data : to be ignored; sometimes it's a number (some internal id), if successful; on error it seems to be always NULL
#     callbackData : the 'key' identifier returned by the send callback (as a void* to be cast to uint32_t; this is not a string)
#     reserved : to be ignored; it seems to be always NULL
#
# typedef void (publishSendCallback)(const char *eventName, const char *data, const char *key, bool published);
# void iotreadySetSendPublishCallback(Iotready* v, publishSendCallback *publish);
#     eventName, data : as passed to the publish function
#     key : an identifier for the request (as a uint32_t converted to string)
#     published : true if sent (no info about actual reception), false if error
#
# typedef void (prepareFirmwareUpdateCallback)(struct Chunk data, uint32_t flags, void* reserved);
# void iotreadySetPrepareForFirmwareUpdateCallback(Iotready* v, prepareFirmwareUpdateCallback *prepare);
#
# typedef void (firmwareChunkCallback)(struct Chunk data, const unsigned char* chunk, void*);
# void iotreadySetSaveFirmwareChunkCallback(Iotready* v, firmwareChunkCallback *chunk);
#
# typedef void (finishFirmwareUpdateCallback)(char *data, uint32_t fileSize);
# void iotreadySetFinishFirmwareUpdateCallback(Iotready* v, finishFirmwareUpdateCallback *finish);
#
# void iotreadySetClaimCode(Iotready* v, const char *claimCode);
#
# typedef int (saveSessionCallback)(const void* buffer, size_t length, uint8_t type, void* reserved);
# void iotreadySetSaveSessionCallback(Iotready* v, saveSessionCallback *save);
#
# typedef int (restoreSessionCallback)(void* buffer, size_t length, uint8_t type, void* reserved);
# void iotreadySetRestoreSessionCallback(Iotready* v, restoreSessionCallback *restore);
#
# typedef void (signalCallback)(bool on, unsigned int param, void* reserved);
# void iotreadySetSignalCallback(Iotready* v, signalCallback *signal);
#
#typedef void (timeCallback)(time_t time, unsigned int param, void*);
# void iotreadySetSystemTimeCallback(Iotready* v, timeCallback *time);
#
# typedef uint32_t (randomNumberCallback)(void);
# void iotreadySetRandomCallback(Iotready* v, randomNumberCallback *random);
#
# typedef void (logCallback)(const char *msg, int level, const char *category, void* attribute, void *reserved);
# void iotreadySetLogCallback(Iotready* v, logCallback *log);
#   register the function called by the library to send log messages
#     category : contains additional info (e.g. file/line info, or context)
#
# void iotreadySetLogLevel(Iotready* v, Log_Level level);
#
# const char* iotreadyGetLogLevelName(Iotready* v, int level);
#
# void iotreadyDisableUpdates(Iotready* v);
#
# void iotreadyEnableUpdates(Iotready* v);
#
# bool iotreadyUpdatesEnabled(Iotready* v);
#
# bool iotreadyUpdatesPending(Iotready* v);
#
# bool iotreadyUpdatesForced(Iotready* v);
#
# int iotreadyConnect(Iotready* v);
#
# void iotreadyConnectionCompleted(Iotready* v);
#
# bool iotreadyConnected(Iotready* v);
#
# void iotreadyDisconnect(Iotready* v);
#
# void iotreadyLoop(Iotready* v);
#   to be called periodically by the app. No work is ever done asynchronously; all the work (including callbacks) is done in this loop.
#   This loop will never perform busy waits.
#
# void iotreadySetKeepalive(Iotready* v, uint32_t interval);
#
# void iotreadySetFirmwareVersion(Iotready* v, int firmwareversion);
#
# void iotreadySetProductId(Iotready* v, int productid);
#
# typedef int (usedMemoryCallback)(void);
# void iotreadySetUsedMemoryCallback(Iotready* v, usedMemoryCallback *usedmemory);
# void iotreadyDiagnosticPower(Iotready* v, Power key, double value);
# void iotreadyDiagnosticSystem(Iotready* v, System key, double value);
# void iotreadyDiagnosticNetwork(Iotready* v, Network key, double value);

class Iot:
    # Data_TypeDef
    VARTYPE_BOOL = 1
    VARTYPE_INT = 2
    VARTYPE_CPPSTRING = 4
    VARTYPE_CSTRING = 5
    VARTYPE_LONG = 6
    VARTYPE_DOUBLE = 9

    # Function_PermissionDef
    FUNCPERM_ALL_USERS = 1
    FUNCPERM_OWNER_ONLY = 2

    # Event_Type
    PUBTYPE_PUBLIC = ord('e')
    PUBTYPE_PRIVATE = ord('E')

    # Event_Flags
    PUBFLAG_NONE  = 0x00000000
    PUBFLAG_NOACK = 0x00000002
    PUBFLAG_ACK   = 0x00000008

    # Subscription_Scope_Type
    SUBSCOPE_MY_DEVICES = 0
    SUBSCOPE_ALL_DEVICES = 1

    # Publish errors
    PUBERR_NONE = 0
    PUBERR_UNKNOWN = -100
    PUBERR_BUSY = -110 # Resource busy
    PUBERR_NOT_SUPPORTED = -120
    PUBERR_NOT_ALLOWED = -130
    PUBERR_CANCELLED = -140 # Operation cancelled
    PUBERR_ABORTED = -150 # Operation aborted
    PUBERR_TIMEOUT = -160
    PUBERR_NOT_FOUND = -170
    PUBERR_ALREADY_EXISTS = -180
    PUBERR_TOO_LARGE = -190 # Data too large
    PUBERR_LIMIT_EXCEEDED = -200
    PUBERR_INVALID_STATE = -210
    PUBERR_IO = -220
    PUBERR_NETWORK = -230
    PUBERR_PROTOCOL = -240
    PUBERR_INTERNAL = -250
    PUBERR_NO_MEMORY = -260
    PUBERR_NOT_SENT = -999 # not from the library; we set it when the library didn't even try to send it (e.g. not inited, ...)

    # Log_Level
    LOGLVL_TRACE = 1
    LOGLVL_INFO = 30
    LOGLVL_WARN = 40
    LOGLVL_ERROR = 50
    LOGLVL_PANIC = 60
    LOGLVL_NO_LOG = 70

    DEFAULT_TTL = 60 # [s]

    MIN_DELAY_BETWEEN_MSGS = 0.25 # [s]

    DEVICE_ID_LEN = 12
    DEVICE_KEY_LEN = 121
    SERVER_KEY_LEN = 91

    fn_t = namedtuple("fn_t", ["name", "args"])

    class Out_msg:
        def __init__(self, name : str, args : str):
            self.name : str = name
            self.args : str = args
            self.start_tick : float = get_tick()
            self.last_attempt_tick : float = self.start_tick
            self.num_attempts : int = 1
            self.msg_key : Optional[int] = None
            self.outcome : Optional[int] = None # PUBERR_*

        def __repr__(self):
            return "<Out_msg \"{}\" #{}, outcome {} ({}), start {}, last {}, n {}>".format(
                self.name,
                self.msg_key,
                self.outcome,
                Iot.puberr_to_str(self.outcome),
                self.start_tick,
                self.last_attempt_tick,
                self.num_attempts
            )

    def __init__(self):
        #self.server_ip = "157.245.21.142"
        self.server_host: str = None
        self.server_port: int = None
        self.server_key = (ct.c_ubyte * Iot.SERVER_KEY_LEN)(0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x2B, 0x19, 0x9D, 0xC9, 0xF2, 0xB0, 0x2D, 0xD1, 0xF1, 0x7D, 0xF0, 0x2B, 0xD1, 0xEC, 0xD1, 0x57, 0xD6, 0x74, 0x51, 0xD7, 0x9C, 0x09, 0xE1, 0x70, 0x43, 0x4A, 0x5B, 0xC2, 0x40, 0xC0, 0x49, 0x67, 0x34, 0xC8, 0xA4, 0xF8, 0xB4, 0xF7, 0xFB, 0xB4, 0xD0, 0x3F, 0xCC, 0xAF, 0x1F, 0xAA, 0x2E, 0x1D, 0x76, 0x82, 0xCF, 0x3A, 0x1A, 0x0B, 0x42, 0x38, 0x14, 0x6D, 0x54, 0x42, 0x05, 0xDC, 0x4D, 0x27)
        self.device_key = None
        self.device_id = None
        self.socket = None
        self.lib = None
        self.c_obj = None
        self.is_exiting: bool = False
        self.exc_during_cb : List[BaseException] = []
        self.is_connected: bool = False
        self.fn_queue: Queue = None
        self.last_connection_attempt_tick: float = None
        self.last_msg_tick = None # to avoid triggering the rate-limiting threshold of 4 messages/second (and also to send manual pings)

        self.current_out_msg : Optional['Iot.Out_msg'] = None # active during the current publish callback
        self.out_msg_queue : List['Iot.Out_msg'] = []
        self.out_msg_queue_idx = 0

        # IOT examples
        #self.var_bool = ct.c_bool(False)
        #self.var_bool.value = True
        #self.var_int = ct.c_int(0)
        #self.var_int.value = 15
        #self.var_double = ct.c_double(0.0)
        #self.var_double.value = 10.4
        #self.var_str = ct.create_string_buffer(32)
        #self.var_str.value = b'Hello'

        # Time at which to automatically ask for covid data, daily
        # As "hh:mm:ss" string
        # Empty if not scheduled
        self.var_sched_covid = ct.create_string_buffer(64)
        self.var_sched_covid.value = b''

    def __del__(self):
        if self.socket is not None:
            try:
                self.socket.close()
            except socket.error:
                lg.x("Error closing socket")
            self.socket = None

    def patch_iot_library(self, lib):
        # sets typo info for various functions

        ct_enum = ct.c_int
        ct_Iotready_p = ct.c_void_p
        ct_Data_TypeDef = ct_enum
        ct_Function_PermissionDef = ct_enum
        ct_Event_Type = ct_enum
        ct_Event_Flags = ct_enum
        ct_Log_Level = ct_enum
        ct_Subscription_Scope_Type = ct_enum
        ct_system_tick_t = ct.c_uint32
        ct_Power = ct_enum
        ct_System = ct_enum
        ct_Network = ct_enum
        ct_time_t = ct.c_int32

        class ct_Chunk(ct.Structure):
            _fields_ = [
                ("chunk_count", ct.c_uint32),
                ("chunk_address", ct.c_uint32),
                ("chunk_size", ct.c_uint16),
                ("file_length", ct.c_uint32)
            ]

        ct_user_function_int_char_t = ct.CFUNCTYPE(ct.c_int, ct.c_char_p)
        ct_EventHandler = ct.CFUNCTYPE(None, ct.c_char_p, ct.c_char_p)
        ct_millisCallback = ct.CFUNCTYPE(ct_system_tick_t)
        ct_sendCallback = ct.CFUNCTYPE(ct.c_int, ct.c_char_p, ct.c_uint32, ct.c_void_p)
        ct_receiveCallback = ct.CFUNCTYPE(ct.c_int, ct.c_char_p, ct.c_uint32, ct.c_void_p)
        ct_connectCallback = ct.CFUNCTYPE(ct.c_int, ct.c_char_p, ct.c_int)
        ct_disconnectCallback = ct.CFUNCTYPE(ct.c_int)

        ct_publishCompletionCallback = (None, ct.c_int, ct.c_void_p, ct.c_void_p, ct.c_void_p)
        ct_publishSendCallback = (None, ct.c_char_p, ct.c_char_p, ct.c_char_p, ct.c_bool)
        ct_prepareFirmwareUpdateCallback = (None, ct_Chunk, ct.c_uint32, ct.c_void_p)
        ct_firmwareChunkCallback = (None, ct_Chunk, ct.POINTER(ct.c_ubyte), ct.c_void_p)
        ct_finishFirmwareUpdateCallback = (None, ct.c_char_p, ct.c_uint32)
        ct_usedMemoryCallback = (ct.c_int)
        ct_signalCallback = (None, ct.c_bool, ct.c_uint, ct.c_void_p);
        ct_timeCallback = (None, ct_time_t, ct.c_uint, ct.c_void_p)
        ct_logCallback = (None, ct.c_char_p, ct.c_int, ct.c_char_p, ct.c_void_p, ct.c_void_p)
        ct_restoreSessionCallback = (ct.c_int, ct.c_void_p, ct.c_size_t, ct.c_uint8, ct.c_void_p)
        ct_saveSessionCallback = (ct.c_int, ct.c_void_p, ct.c_size_t, ct.c_uint8, ct.c_void_p)
        ct_randomNumberCallback = (ct.c_uint32)


        lib.newIotready.restype = ct_Iotready_p
        lib.newIotready.argtypes = ()

        try:
            _ = lib.deleteIotready
        except AttributeError:
            lg.w("IOT no 'deleteIotready' function found in the library, will provide a stub")
            lib.deleteIotready = lambda c: None

        lib.deleteIotready.restype = None
        lib.deleteIotready.argstype = (ct_Iotready_p)

        lib.iotreadyInit.restype = None
        lib.iotreadyInit.argstype = (ct_Iotready_p)

        lib.iotreadySetEnabled.restype = None
        lib.iotreadySetEnabled.argstype = (ct_Iotready_p, ct.c_bool)

        lib.iotreadyIsEnabled.restype = ct.c_bool
        lib.iotreadyIsEnabled.argstype = (ct_Iotready_p)

        lib.iotreadyGet.restype = ct.c_bool
        lib.iotreadyGet.argstype = (ct_Iotready_p, ct.c_char_p, ct.c_void_p, ct_Data_TypeDef)

        lib.iotreadyPost.restype = ct.c_bool
        lib.iotreadyPost.argstype = (ct_Iotready_p, ct.c_char_p, ct_user_function_int_char_t, ct_Function_PermissionDef)

        lib.iotreadyPublish.restype = ct.c_bool
        lib.iotreadyPublish.argstype = (ct_Iotready_p, ct.c_char_p, ct.c_char_p, ct.c_int, ct_Event_Type, ct_Event_Flags)

        lib.iotreadySubscribe.restype = ct.c_bool
        lib.iotreadySubscribe.argstype = (ct_Iotready_p, ct.c_char_p, ct_EventHandler, ct_Subscription_Scope_Type, ct.c_char_p)

        lib.iotreadyUnsubscribe.restype = None
        lib.iotreadyUnsubscribe.argstype = (ct_Iotready_p)

        lib.iotreadySetMillis.restype = None
        lib.iotreadySetMillis.argstype = (ct_Iotready_p, ct_millisCallback)

        lib.iotreadySetDeviceId.restype = None
        lib.iotreadySetDeviceId.argstype = (ct_Iotready_p, ct.c_char_p)

        lib.iotreadySetKeys.restype = None
        lib.iotreadySetKeys.argstype = (ct_Iotready_p, ct.POINTER(ct.c_ubyte), ct.POINTER(ct.c_ubyte))

        lib.iotreadySetSendCallback.restype = None
        lib.iotreadySetSendCallback.argstype = (ct_Iotready_p, ct_sendCallback)

        lib.iotreadySetReceiveCallback.restype = None
        lib.iotreadySetReceiveCallback.argstype = (ct_Iotready_p, ct_receiveCallback)

        lib.iotreadySetConnectCallback.restype = None
        lib.iotreadySetConnectCallback.argstype = (ct_Iotready_p, ct_connectCallback)

        lib.iotreadySetDisconnectCallback.restype = None
        lib.iotreadySetDisconnectCallback.argstype = (ct_Iotready_p, ct_disconnectCallback)

        lib.iotreadySetCompletedPublishCallback.restype = None
        lib.iotreadySetCompletedPublishCallback.argstype = (ct_Iotready_p, ct_publishCompletionCallback)

        # This annotation causes the application to stop working (the data received by the callback is mangled).
        # Is the problem with the annotation, or with the casts inside the callback?
        #lib.iotreadySetSendPublishCallback.restype = None
        #lib.iotreadySetSendPublishCallback.argstype = (ct_Iotready_p, ct_publishSendCallback)

        lib.iotreadySetPrepareForFirmwareUpdateCallback.restype = None
        lib.iotreadySetPrepareForFirmwareUpdateCallback.argstype = (ct_Iotready_p, ct_prepareFirmwareUpdateCallback)

        lib.iotreadySetSaveFirmwareChunkCallback.restype = None
        lib.iotreadySetSaveFirmwareChunkCallback.argstype = (ct_Iotready_p, ct_firmwareChunkCallback)

        lib.iotreadySetFinishFirmwareUpdateCallback.restype = None
        lib.iotreadySetFinishFirmwareUpdateCallback.argstype = (ct_Iotready_p, ct_finishFirmwareUpdateCallback)

        lib.iotreadySetClaimCode.restype = None
        lib.iotreadySetClaimCode.argstype = (ct_Iotready_p, ct.c_char_p)

        lib.iotreadySetSaveSessionCallback.restype = None
        lib.iotreadySetSaveSessionCallback.argstype = (ct_Iotready_p, ct_saveSessionCallback)

        lib.iotreadySetRestoreSessionCallback.restype = None
        lib.iotreadySetRestoreSessionCallback.argstype = (ct_Iotready_p, ct_restoreSessionCallback)

        lib.iotreadySetSignalCallback.restype = None
        lib.iotreadySetSignalCallback.argstype = (ct_Iotready_p, ct_signalCallback)

        lib.iotreadySetSystemTimeCallback.restype = None
        lib.iotreadySetSystemTimeCallback.argstype = (ct_Iotready_p, ct_timeCallback)

        lib.iotreadySetRandomCallback.restype = None
        lib.iotreadySetRandomCallback.argstype = (ct_Iotready_p, ct_randomNumberCallback)

        lib.iotreadySetLogCallback.restype = None
        lib.iotreadySetLogCallback.argstype = (ct_Iotready_p, ct_logCallback)

        lib.iotreadySetLogLevel.restype = None
        lib.iotreadySetLogLevel.argstype = (ct_Iotready_p, ct_Log_Level)

        lib.iotreadyGetLogLevelName.restype = ct.c_char_p
        lib.iotreadyGetLogLevelName.argstype = (ct_Iotready_p, ct.c_int)

        lib.iotreadyDisableUpdates.restype = None
        lib.iotreadyDisableUpdates.argstype = (ct_Iotready_p)

        lib.iotreadyEnableUpdates.restype = None
        lib.iotreadyEnableUpdates.argstype = (ct_Iotready_p)

        lib.iotreadyUpdatesEnabled.restype = ct.c_bool
        lib.iotreadyUpdatesEnabled.argstype = (ct_Iotready_p)

        lib.iotreadyUpdatesPending.restype = ct.c_bool
        lib.iotreadyUpdatesPending.argstype = (ct_Iotready_p)

        lib.iotreadyUpdatesForced.restype = ct.c_bool
        lib.iotreadyUpdatesForced.argstype = (ct_Iotready_p)

        lib.iotreadyConnect.restype = int
        lib.iotreadyConnect.argstype = (ct_Iotready_p)

        lib.iotreadyConnectionCompleted.restype = None
        lib.iotreadyConnectionCompleted.argstype = (ct_Iotready_p)

        lib.iotreadyConnected.restype = ct.c_bool
        lib.iotreadyConnected.argstype = (ct_Iotready_p)

        lib.iotreadyDisconnect.restype = None
        lib.iotreadyDisconnect.argstype = (ct_Iotready_p)

        lib.iotreadyLoop.restype = None
        lib.iotreadyLoop.argstype = (ct_Iotready_p)

        lib.iotreadySetKeepalive.restype = None
        lib.iotreadySetKeepalive.argstype = (ct_Iotready_p, ct.c_uint32)

        lib.iotreadySetFirmwareVersion.restype = None
        lib.iotreadySetFirmwareVersion.argstype = (ct_Iotready_p, ct.c_int)

        lib.iotreadySetProductId.restype = None
        lib.iotreadySetProductId.argstype = (ct_Iotready_p, ct.c_int)

        lib.iotreadySetUsedMemoryCallback.restype = None
        lib.iotreadySetUsedMemoryCallback.argstype = (ct_Iotready_p, ct_usedMemoryCallback)

        lib.iotreadyDiagnosticPower.restype = None
        lib.iotreadyDiagnosticPower.argstype = (ct_Iotready_p, ct_Power, ct.c_double)

        lib.iotreadyDiagnosticSystem.restype = None
        lib.iotreadyDiagnosticSystem.argstype = (ct_Iotready_p, ct_System, ct.c_double)

        lib.iotreadyDiagnosticNetwork.restype = None
        lib.iotreadyDiagnosticNetwork.argstype = (ct_Iotready_p, ct_Network, ct.c_double)

    def init(self, fn_queue: Queue):
        try:
            if self.is_inited():
                return

            self.fn_queue = fn_queue

            [id, key, id_str] = self._get_device_id_and_key()
            if id is None:
                lg.e("Error with device ID/key")
                return False
            self.device_id = id
            self.device_key = key
            self.device_id_str = id_str

            lg.i("Device ID: {}".format(id_str))

            # Load the shared library into ctypes
            try:
                self.c_obj = None
                lib = ct.CDLL("./libCommunication.so")
                self.lib = lib
                lg.d("IOT library loaded")
            except OSError:
                lg.x("Error loading communication library")
                return False

            self.patch_iot_library(lib)

            if self.c_obj is None:
                c = lib.newIotready()
                if ct_is_null(c):
                    lg.e("IOT error instantiating the library")
                    return False
                self.c_obj = c
                lg.d("IOT library instantiated")
            else:
                c =  self.c_obj

            lib.iotreadySetLogCallback(c, iotcb_log)
            if IOT_LOG_LEVEL > 0:
                lib.iotreadySetLogLevel(c, 1)

            lib.iotreadySetFirmwareVersion(c, PROG_VERSION_FOR_IOT)
            lib.iotreadySetProductId(c, PRODUCT_ID_FOR_IOT)

            lib.iotreadyInit(c)
            lib.iotreadySetEnabled(c, True)
            lib.iotreadySetKeys(c, self.server_key, self.device_key)
            lib.iotreadySetDeviceId(c, self.device_id)

            lib.iotreadySetMillis(c, iotcb_get_millis)
            lib.iotreadySetSendCallback(c, iotcb_send_udp)
            lib.iotreadySetReceiveCallback(c, iotcb_receive_udp)
            lib.iotreadySetConnectCallback(c, iotcb_connect_udp)
            lib.iotreadySetDisconnectCallback(c, iotcb_disconnect)

            lib.iotreadySetSendPublishCallback(c, iotcb_send_publish_callback)
            lib.iotreadySetCompletedPublishCallback(c, iotcb_completed_publish_callback)

            # IOT examples
            #lib.iotreadyGet(c, b'my_bool', ct.pointer(self.var_bool), Iot.VARTYPE_BOOL)
            #lib.iotreadyGet(c, b'my_int', ct.pointer(self.var_int), Iot.VARTYPE_INT)
            #lib.iotreadyGet(c, b'my_double', ct.pointer(self.var_double), Iot.VARTYPE_DOUBLE)
            #lib.iotreadyGet(c, b'my_str', self.var_str, Iot.VARTYPE_CSTRING)
            lib.iotreadyGet(c, b'sched_covid', self.var_sched_covid, Iot.VARTYPE_CSTRING)

            lib.iotreadyPost(c, b'action', iotcb_fn_action, Iot.FUNCPERM_ALL_USERS)

            keepalive = math.ceil(IOT_KEEPALIVE_S * 1000)
            if ENABLE_IOT_MANUAL_PING:
                # a bit longer, so that normally we do not send automatic pings
                keepalive = min(math.ceil(keepalive * 1.1), keepalive + 1000)
            lib.iotreadySetKeepalive(c, keepalive) # in [ms]

            lg.d("IOT library configured")

            self.last_connection_attempt_tick = get_tick()
            res = lib.iotreadyConnect(c)
            if res != 0:
                lg.w("IOT error connecting")
            else:
                lg.i("IOT connected")
        except:
            self.deinit()
            raise

    def is_inited(self) -> bool:
        return (self.c_obj is not None
                and self.lib is not None)

    def deinit(self):
        if self.is_inited() and self.is_connected():
            self.lib.iotreadyDisconnect(self.c_obj)
        if self.c_obj is not None:
            self.lib.deleteIotready(self.c_obj)
            self.c_obj = None

    def loop(self):
        if not self.is_inited():
            self.is_connected = False
            return
        was_connected = self.is_connected
        self.lib.iotreadyLoop(self.c_obj)
        self.is_connected = self.get_is_connected()

        if self.is_connected and not was_connected:
            self.last_msg_tick = get_tick()

        if ENABLE_IOT_MANUAL_PING and self.is_connected:
            now = get_tick()
            if self.last_msg_tick is None or now - self.last_msg_tick >= IOT_KEEPALIVE_S:
                lg.d("IOT ping")
                self.oneshot_publish("iotready/ping", "")

        if not self.is_connected:
            last = self.last_connection_attempt_tick
            if last is None or get_tick() - last >= 2*60:
                lg.i("Try IOT connection")
                res = self.lib.iotreadyConnect(self.c_obj)
                if res == 0:
                    lg.i("IOT connected")
                    self.is_connected = self.get_is_connected()
                    self.last_msg_tick = get_tick()
                else:
                    lg.i("IOT error connecting")
                self.last_connection_attempt_tick = get_tick()

        self.handle_msg_queue()

    def handle_msg_queue(self) -> None:
        now = get_tick()

        N = len(self.out_msg_queue)
        if N == 0:
            self.out_msg_queue_idx = 0
            return
        idx = self.out_msg_queue_idx
        if idx >= N:
            idx = 0

        connected = self.is_connected

        i = 0
        while i < N:
            m : Iot.Out_msg = self.out_msg_queue[i]
            # remove completed messages
            if m.outcome == Iot.PUBERR_NONE:
                lg.d("Msg send complete: {}".format(m))
                del self.out_msg_queue[i]
                if idx > i:
                    idx -= 1
                N -= 1
                continue

            if m.msg_key is not None and m.last_attempt_tick - now >= 60:
                # no outcome: it must have timed out without notification
                m.outcome = Iot.PUBERR_TIMEOUT
                m.msg_key = None

            # remove definitely-failed messages
            if m.outcome is not None and ((connected and m.num_attempts >= 10) or now - m.start_tick >= 8*60*60):
                lg.d("Msg failed, drop: {}".format(m))
                del self.out_msg_queue[i]
                if idx > i:
                    idx -= 1
                N -= 1
                continue

            i += 1

        # now `idx` is that of the next message to retry (if any)
        self.out_msg_queue_idx = idx

        if N == 0 or not connected:
            return

        if now - self.last_msg_tick < Iot.MIN_DELAY_BETWEEN_MSGS:
            return

        start_idx = idx
        while True:
            m = self.out_msg_queue[idx]
            idx = (idx + 1) % N

            if now - m.last_attempt_tick >= 30:
                self.out_msg_queue_idx = idx

                self.current_out_msg = m
                m.last_attempt_tick = now
                m.num_attempts += 1
                m.msg_key = None
                lg.d("Retry send {}".format(m))
                ok = self.publish(m.name, m.args, flags=Iot.PUBFLAG_ACK)
                if not ok:
                    m.outcome = Iot.PUBERR_NOT_SENT
                return

            if idx == start_idx:
                break

    def get_is_connected(self) -> bool:
        if self.c_obj is None:
            return False
        return self.socket is not None and self.lib.iotreadyConnected(self.c_obj)

    def on_exc_during_cb(self, e: BaseException):
        self.is_exiting = True

        lg.xx(e, "Exception inside iot callback")

        self.exc_during_cb.append(e)
        if (
                (
                    (isinstance(e, SystemError)
                        or isinstance(e, SystemExit)
                        or isinstance(e, KeyboardInterrupt))
                    and len(self.exc_during_cb) > 2
                )
                or len(self.exc_during_cb) > 10
                ):
            lg.w("Too many exceptions to postpone")
            raise e
        lg.d("Postpone the exception")

    #@ct.CFUNCTYPE(None, ct.POINTER(ct.c_char), ct.c_int, ct.POINTER(ct.c_char), ct.c_void_p, ct.c_void_p)
    def cb_log(self, msg, level, category, attribute, reserved):
        try:
            cat = ct_cstring_to_string(category)
            if cat is None:
                cat = ""
            m = ct_cstring_to_string(msg)
            if m is None:
                m = ""

            if IOT_LOG_LEVEL > 0:
                lg.v("[IOT {} {}] {}".format(Iot.loglevel_to_str(level), cat, m))
        except BaseException as e:
            self.on_exc_during_cb(e)

    @staticmethod
    def loglevel_to_str(level : int) -> str:
        return {
            Iot.LOGLVL_TRACE: "TRACE",
            Iot.LOGLVL_INFO: "INFO",
            Iot.LOGLVL_WARN: "WARN",
            Iot.LOGLVL_ERROR: "ERROR",
            Iot.LOGLVL_PANIC: "PANIC"
        }.get(level, str(level))

    @staticmethod
    def puberr_to_str(err : int) -> str:
        return {
            Iot.PUBERR_NONE : "NONE",
            Iot.PUBERR_UNKNOWN : "UNKNOWN",
            Iot.PUBERR_BUSY : "BUSY",
            Iot.PUBERR_NOT_SUPPORTED : "NOT_SUPPORTED",
            Iot.PUBERR_NOT_ALLOWED : "NOT_ALLOWED",
            Iot.PUBERR_CANCELLED : "CANCELLED",
            Iot.PUBERR_ABORTED : "ABORTED",
            Iot.PUBERR_TIMEOUT : "TIMEOUT",
            Iot.PUBERR_NOT_FOUND : "NOT_FOUND",
            Iot.PUBERR_ALREADY_EXISTS : "ALREADY_EXISTS",
            Iot.PUBERR_TOO_LARGE : "TOO_LARGE",
            Iot.PUBERR_LIMIT_EXCEEDED : "LIMIT_EXCEEDED",
            Iot.PUBERR_INVALID_STATE : "INVALID_STATE",
            Iot.PUBERR_IO : "IO",
            Iot.PUBERR_NETWORK : "NETWORK",
            Iot.PUBERR_PROTOCOL : "PROTOCOL",
            Iot.PUBERR_INTERNAL : "INTERNAL",
            Iot.PUBERR_NO_MEMORY : "NO_MEMORY",
            Iot.PUBERR_NOT_SENT : "NOT_SENT",
        }.get(err, "<UNKNOWN>")

    #@ct.CFUNCTYPE(ct.c_uint)
    def cb_get_millis(self):
        return int(round(time.monotonic() * 1000))

    #@ct.CFUNCTYPE(ct.c_uint, ct.POINTER(ct.c_ubyte), ct.c_int, ct.c_void_p)
    def cb_send_udp(self, buf, buflen, tmp):
        try:
            if not self.is_inited():
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't send bytes: not inited")
                return 0
            if self.is_exiting:
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't send bytes: is exiting")
                return 0
            if ct_is_null(buf) or buflen == 0:
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't send bytes: invalid args")
                return 0
            if self.socket is None:
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't send bytes: no socket")
                return 0
            py_buf = bytearray(buflen)
            for i in range(buflen):
                py_buf[i] = buf[i]
            if IOT_LOG_LEVEL > 0:
                lg.d("[IOT] Send {} bytes: {} (\"{}\")".format(buflen, py_buf, asciify(py_buf)))
            try:
                sent = self.socket.send(py_buf)
                if IOT_LOG_LEVEL > 0:
                    if sent == buflen:
                        lg.d("[IOT] Sent {} bytes".format(sent))
                    else:
                        lg.d("[IOT] Sent {}/{} bytes".format(sent, buflen))
            except socket.error as e:
                lg.x("[IOT] Socket error in send")
                return 0
            if IOT_LOG_LEVEL > 0:
                lg.d("[IOT] Sent {} bytes: {}".format(sent, py_buf))
            return sent
        except BaseException as e:
            self.on_exc_during_cb(e)
            return 0

    #@ct.CFUNCTYPE(ct.c_uint, ct.POINTER(ct.c_ubyte), ct.c_int, ct.c_void_p)
    def cb_receive_udp(self, buf, buflen, tmp):
        try:
            if not self.is_inited():
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't receive bytes: not inited")
                return 0
            if self.is_exiting:
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't receive bytes: is exiting")
                return 0
            if ct_is_null(buf):
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't receive bytes: invalid args")
                return 0
            if self.socket is None:
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't receive bytes: no socket")
                return 0

            if buflen == 0:
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Recv 0 bytes")
                return 0

            self.socket.settimeout(0.001)
            try:
                (rec, server) = self.socket.recvfrom(buflen)
            except socket.timeout as e:
                return 0
            except socket.error as e:
                lg.x("[IOT] Socket error in recv")
                return 0
            N = len(rec)
            if IOT_LOG_LEVEL > 0:
                lg.d("[IOT] Recv {} bytes: {} (\"{}\")".format(N, rec, asciify(rec)))
            for i in range(N):
                buf[i] = rec[i]
            return N
        except BaseException as e:
            self.on_exc_during_cb(e)
            return 0

    #@ct.CFUNCTYPE(ct.c_uint, ct.POINTER(ct.c_char), ct.c_int)
    def cb_connect_udp(self, address, port):
        try:
            if not self.is_inited():
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't connect: not inited")
                return 0
            if self.is_exiting:
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't connect: is exiting")
                return 0
            a = ct_cstring_to_string(address)
            if a is None:
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Can't connect: invalid args")
                return 0
            self.server_host = a
            self.server_port = port
            lg.i("[IOT] Connect to {}:{}".format(a, port))
            if self.socket is not None:
                try:
                    self.socket.close()
                except socket.error as e:
                    lg.x("[IOT] Error closing socket")
                self.socket = None

            try:
                lg.d("[IOT] Connect to {}:{}...".format(self.server_host, self.server_port))
                self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self.socket.settimeout(10)
                self.socket.connect((self.server_host, self.server_port))
            except (socket.error, socket.timeout) as e:
                lg.x("[IOT] Error opening socket")
                return 0

            if IOT_LOG_LEVEL > 0:
                lg.d("[IOT] Connection ok")
            self.lib.iotreadyConnectionCompleted(self.c_obj)
            return 1
        except BaseException as e:
            self.on_exc_during_cb(e)
            return 0

    #@ct.CFUNCTYPE(None)
    def cb_disconnect(self):
        try:
            if IOT_LOG_LEVEL > 0:
                lg.d("[IOT] Disconnect")
            if self.socket is not None:
                try:
                    self.socket.close()
                except socket.error as e:
                    lg.x("[IOT] Error closing socket")
                self.socket = None
            return 1
        except BaseException as e:
            self.on_exc_during_cb(e)
            return 0

    #@ct.CFUNCTYPE(ct.c_uint, ct.POINTER(ct.c_char))
    # with additional function name paramter
    def cb_function(self, fn_name, args):
        try:
            if not self.is_inited():
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Invalid function cb: not inited")
                return -99
            if self.is_exiting:
                if IOT_LOG_LEVEL > 0:
                    lg.d("[IOT] Invalid function cb: is exiting")
                return -99
            if ct_is_null(args):
                args = None
            else:
                args = ct.cast(args, ct.c_char_p).value

            lg.v("Function '{}': {}".format(fn_name, args))

            if args is None:
                params = None
            else:
                try:
                    params = args.decode('utf-8')
                except UnicodeDecodeError as e:
                    lg.w("Invalid utf-8 string received by iot function: {}\n{}".format(repr(args), str(e)))
                    return -1

            if fn_name == 'action':
                if params == "":
                    params = "{}"
                try:
                    # just try parsing, but then enqueue it as string
                    jparams = json.loads(params)
                    err_msg = self.cursorily_check_fn_action(jparams)
                    if err_msg is not None:
                        lg.w("Invalid json received by iot function: {}\n{}".format(repr(params), err_msg))
                        return -2
                except json.JSONDecodeError as e:
                    lg.w("Invalid json received by iot function: {}\n{}".format(repr(params), str(e)))
                    return -2
                q_put_data(self.fn_queue, Iot.fn_t(name=fn_name, args=params))
                return 1
            else:
                lg.i("Invalid function name by iot: {}".format(repr(fn_name)))
                return -3
        except BaseException as e:
            self.on_exc_during_cb(e)
            return 0

    def cursorily_check_fn_action(self, params) -> Optional[str]:
        cmd = params.get('cmd', None)
        id = params.get('id', None)
        if id is not None:
            if not isinstance(id, (int, str)):
                return "Malformed 'id'={} (should be int or string)".format(repr(id))

        if cmd == "sched_covid":
            t = params.get('time', None)
            if t is None:
                return "Malformed 'time'={}".format(repr(t))
            try:
                dt = du_parser.parse(t)
            except du_parser.ParserError:
                return "Malformed 'time'={}".format(repr(t))
        elif cmd == "req_data":
            tout = params.get('tout', None)
            if (tout is not None) and (not isinstance(tout, (int, float))):
                return "Malformed 'tout'={} (should be int or float)".format(repr(tout))
            req_type = params.get('type', None)
            allowed_req_types = ["covid"]
            if (req_type is not None) and (req_type not in allowed_req_types):
                return "Malformed 'type'={} (should be missing, or one of [{}])".format(repr(req_type), ", ".join([repr(t) for t in allowed_req_types]))
            values = params.get('values', None)
            if values is not None:
                if not isinstance(values, list):
                    return "Malformed 'values'={} (should be missing, or a list)".format(repr(values))
                for (i, v) in enumerate(values):
                    if not isinstance(v, dict):
                        return "Malformed 'values[{}]'={} (should be an object)".format(i, repr(v))
                    name = v.get('name', None)
                    if name is None or name == "" or not isinstance(name, str):
                        return "Malformed 'values[{}].name'={} (should be a non-empty string)".format(i, repr(name))
                    var_type = v.get('type', None)
                    allowed_var_types = ["yesno", "int", "float"]
                    if var_type not in allowed_var_types:
                        return "Malformed 'values[{}].type'={} (should be one of [{}])".format(i, repr(var_type), ", ".join([repr(t) for t in allowed_var_types]))
                    dec_digits = v.get('dec_digits', None)
                    if dec_digits is not None:
                        if not isinstance(dec_digits, int) or dec_digits < 0:
                            return "Malformed 'values[{}].dec_digits'={} (should be a non-negative integer)".format(i, repr(dec_digits))
                    rng = v.get('range', None)
                    if range is not None:
                        if not isinstance(range, list) or len(range) != 2:
                            return "Malformed 'values[{}].range'={} (should be a list of two numbers)".format(i, repr(range))
                        for i in range(2):
                            if range[i] is not None and not isinstance(range[i], (int, float)):
                                return "Malformed 'values[{}].range'={} (should be a list of two numbers)".format(i, repr(range))
        elif cmd == "msg":
            msg = params.get('msg', None)
            if msg is None or msg == "" or not isinstance(msg, (str)):
                return "Malformed 'msg'={} (should be a non-empty string)".format(repr(msg))
            tout = params.get('tout', None)
            if tout is not None and not isinstance(tout, (int, float)):
                return "Malformed 'tout'={} (should be int or float)".format(repr(tout))
            msg_type = params.get('type', None)
            allowed_msg_types = ["", "ok"]
            if msg_type is not None and msg_type not in allowed_msg_types:
                return "Malformed 'type'={} (should be missing, or one of [{}])".format(repr(msg_type), ", ".join([repr(t) for t in allowed_msg_types]))
        else:
            return "Unknown command {}".format(repr(cmd))

        return None

    # @ct.CFUNCTYPE(None, ct.POINTER(ct.c_char), ct.POINTER(ct.c_char), ct.POINTER(ct.c_char), ct.c_bool)
    def cb_send_publish_callback(self, event_name, data, key, published):
        # key is a uint32 converted to string
        try:
            event_name = ct_cstring_to_string(event_name)
            data = ct_cstring_to_string(data)
            if not isinstance(key, int):
                key = ct_cstring_to_string(key)
                try:
                    key = int(key)
                except ValueError:
                    lg.e("Iot send callback for \"{}\", key {} (ERROR, not an integer), published {}; data was: {}".format(event_name, key, published, data))
                    return
            lg.d("Iot send callback for \"{}\", key {}, published {}; data was: {}".format(event_name, key, published, data))
            if self.current_out_msg is None:
                lg.e("Iot send callback received (key {}), but current out msg is None".format(key))
            else:
                self.current_out_msg.outcome = None if published else Iot.PUBERR_NOT_SENT
                # if keys are reused, remove the old instances (leave the messages without keys: they will be retried later)
                for it in self.out_msg_queue:
                    if it.msg_key == key:
                        lg.w("Key {} is being reused; remove it from msg {}".format(key, it))
                        it.msg_key = None
                self.current_out_msg.msg_key = key
                lg.w("Key assigned to msg {}".format(self.current_out_msg))
        except BaseException as e:
            self.on_exc_during_cb(e)

    #@ct.CFUNCTYPE(None, ct.c_uint, ct.c_void_p, ct.c_void_p, ct.c_void_p)
    def cb_completed_publish_callback(self, error, data, callback_data, reserved):
        # data is an internal msg id (if success) or a string (if error)
        # callback data is the key (as uint32)
        try:
            if isinstance(callback_data, int):
                key = callback_data
            else:
                key = ct_cstring_to_string(callback_data)
                try:
                    key = int(key)
                except ValueError:
                    lg.e("Iot completion callback, key {} (ERROR, not an integer), arg {}, error = {} ({})".format(key, data, error, Iot.puberr_to_str(error)))
                    return
            lg.d("Iot completion callback, key {}, arg {}, error = {} ({})".format(key, data, error, Iot.puberr_to_str(error)))
            for it in self.out_msg_queue:
                if it.msg_key == key:
                    lg.d("Assign outcome {} ({}) to msg {}".format(error, Iot.puberr_to_str(error), it))
                    it.outcome = error
        except BaseException as e:
            self.on_exc_during_cb(e)

    def acked_publish(self, name: str, str_data: Optional[str]) -> Tuple[bool, 'Iot.Out_msg']:
        if str_data is None:
            str_data = ""

        msg = Iot.Out_msg(name, str_data)
        self.current_out_msg = msg
        ok = self.publish(name, str_data, flags=Iot.PUBFLAG_ACK)
        self.current_out_msg = None
        if not ok:
            msg.outcome = Iot.PUBERR_NOT_SENT

        lg.d("Iot acked publish: enqueue {}".format(msg))
        self.out_msg_queue.append(msg)

        return (ok, msg)

    def oneshot_publish(self, name: str, str_data: Optional[str]) -> bool:
        return self.publish(name, str_data)

    # 60 = Iot.DEFAULT_TTL
    # 0 = Iot.PUBFLAG_NONE
    def publish(self, name : Optional[str], str_data : Optional[str], ttl : int = 60, public : bool = False, flags : int = 0) -> bool:
        lg.d("Iot publish \"{}\" ({}): {}".format(name, "ACK" if (flags & Iot.PUBFLAG_ACK) else "NO ACK", str_data))
        if not self.is_inited():
            lg.d("Can't publish, iot not inited")
            return False

        if name is None:
            b_name = b''
        else:
            b_name = name.encode('utf-8', errors='replace')
        if str_data is None:
            b_data = b''
        else:
            b_data = str_data.encode('utf-8', errors='replace')

        ttl = clip_both(ttl, 0, 0x7FFFFFFF)
        flags = flags & 0xFFFFFFFF

        # name, data are 'bytes'
        # ttl = time-to-live (seconds)
        # return: boolean
        ok = self.lib.iotreadyPublish(
            self.c_obj,
            b_name,
            b_data,
            ttl,
            Iot.PUBTYPE_PUBLIC if public else Iot.PUBTYPE_PRIVATE,
            flags
        )
        if ok:
            self.last_msg_tick = get_tick()
        lg.d("Iot publish outcome: {}".format(ok))
        return ok

    def _get_device_id_and_key(self):
        err_res = (None, None, None)
        res = err_res
        try:
            # reads the device id and key from file and returns it as (key, id)
            # returns (None, None) on error
            path = '.'
            for fname in os.listdir(path):
                fpath = os.path.join(path, fname)
                if not os.path.isfile(fpath):
                    continue
                m = re.match(r'([0-9a-fA-F]{{{}}}).der'.format(Iot.DEVICE_ID_LEN*2), fname)
                if m is None:
                    continue
                with open(fpath, 'rb') as f:
                    device_key = f.read()
                if len(device_key) > Iot.DEVICE_KEY_LEN:
                    continue
                if res is not err_res:
                    # error: multiple files match, can't choose
                    return err_res
                device_id_str = m.group(1)
                device_id = bytes.fromhex(device_id_str)
                res = (
                    (ct.c_ubyte * Iot.DEVICE_ID_LEN)(*device_id),
                    (ct.c_ubyte * Iot.DEVICE_KEY_LEN)(*device_key),
                    device_id_str
                )
        except OSError:
            lg.x("Error getting device ID and key")
            return err_res

        return res

@ct.CFUNCTYPE(None, ct.POINTER(ct.c_char), ct.c_int, ct.POINTER(ct.c_char), ct.c_void_p, ct.c_void_p)
def iotcb_log(msg, level, category, attribute, reserved):
    return iot.cb_log(msg, level, category, attribute, reserved)

@ct.CFUNCTYPE(ct.c_uint)
def iotcb_get_millis():
    return iot.cb_get_millis()

@ct.CFUNCTYPE(ct.c_uint, ct.POINTER(ct.c_ubyte), ct.c_int, ct.c_void_p)
def iotcb_send_udp(buf, buflen, tmp):
    return iot.cb_send_udp(buf, buflen, tmp)

@ct.CFUNCTYPE(ct.c_uint, ct.POINTER(ct.c_ubyte), ct.c_int, ct.c_void_p)
def iotcb_receive_udp(buf, buflen, tmp):
    return iot.cb_receive_udp(buf, buflen, tmp)

@ct.CFUNCTYPE(ct.c_uint, ct.POINTER(ct.c_char), ct.c_int)
def iotcb_connect_udp(address, port):
    return iot.cb_connect_udp(address, port)

@ct.CFUNCTYPE(None)
def iotcb_disconnect():
    return iot.cb_disconnect()

@ct.CFUNCTYPE(ct.c_uint, ct.POINTER(ct.c_char))
def iotcb_fn_action(args):
    return iot.cb_function("action", args)

@ct.CFUNCTYPE(None, ct.POINTER(ct.c_char), ct.POINTER(ct.c_char), ct.POINTER(ct.c_char), ct.c_bool)
def iotcb_send_publish_callback(event_name, data, key, published):
    return iot.cb_send_publish_callback(event_name, data, key, published)

@ct.CFUNCTYPE(None, ct.c_int, ct.c_void_p, ct.c_void_p, ct.c_void_p)
def iotcb_completed_publish_callback(error, data, callback_data, reserved):
    return iot.cb_completed_publish_callback(error, data, callback_data, reserved)

# must be a global object because we have plain functions as callbacks (and they need to call this object)
iot: Optional[Iot] = None

################################

# As reference, here are some common keycodes (CEC_USER_CONTROL_CODE_*, e.g. CEC_USER_CONTROL_CODE_SELECT):
# UP, DOWN, LEFT, RIGHT, NUMBER0, ... NUMBER9
# SELECT (usually "OK" or "ENTER")
# EXIT (usually "EXIT" or "RETURN" or "BACK")
# CLEAR (usually "EXIT")
def create_cec_map_keycode_to_name() -> Dict[int, str]:
    map = {}
    prefix = "CEC_USER_CONTROL_CODE_"
    len_prefix = len(prefix)
    for k in dir(libcec):
        if k.startswith(prefix):
            name = k[len_prefix:]
            map[getattr(libcec, k)] = name
    return map

def create_cec_map_powerstatus_to_name() -> Dict[int, str]:
    map = {}
    prefix = "CEC_POWER_STATUS_"
    len_prefix = len(prefix)
    for k in dir(libcec):
        if k.startswith(prefix):
            name = k[len_prefix:]
            map[getattr(libcec, k)] = name
    return map

class Cec:
    class Evt_type(Enum):
        KEYPRESS = 1
        ACTIVATION = 2
    evt_t = namedtuple("evt_t", ["type", "data"])
    keypress_t = namedtuple("keypress_t", ["key", "duration"])
    activation_t = namedtuple("activation_t", ["activated", "logical_address"])

    map_key_to_digit = {
        KEY_0: 0,
        KEY_1: 1,
        KEY_2: 2,
        KEY_3: 3,
        KEY_4: 4,
        KEY_5: 5,
        KEY_6: 6,
        KEY_7: 7,
        KEY_8: 8,
        KEY_9: 9,
    }

    @staticmethod
    def key_to_digit(k : int) -> Optional[int]:
        return Cec.map_key_to_digit.get(k,None)

    @staticmethod
    def key_is_numeric(k : int) -> bool:
        return k in Cec.map_key_to_digit

    map_loglevel_to_name = {
        libcec.CEC_LOG_ERROR: "E",
        libcec.CEC_LOG_WARNING: "W",
        libcec.CEC_LOG_NOTICE: "N",
        libcec.CEC_LOG_TRAFFIC: "T",
        libcec.CEC_LOG_DEBUG: "D",
    }

    map_loglevel_cec_to_app = {
        libcec.CEC_LOG_ERROR: lg.ERROR,
        libcec.CEC_LOG_WARNING: lg.WARNING,
        libcec.CEC_LOG_NOTICE: lg.INFO,
        libcec.CEC_LOG_TRAFFIC: lg.DEBUG,
        libcec.CEC_LOG_DEBUG: lg.DEBUG,
    }

    map_keycode_to_name = create_cec_map_keycode_to_name()

    map_powerstatus_to_name = create_cec_map_powerstatus_to_name()

    def __init__(self):
        self.inited: bool = False
        self.evt_queue: Queue = None
        self.cec_adapter = None
        self.cec_config = None
        self.tv_was_on: bool = False
        self.is_activated: bool = False

    def init(self, evt_queue: Queue):
        if self.inited:
            return

        self.evt_queue = evt_queue

        cfg = libcec.libcec_configuration()
        self.cec_config = cfg
        cfg.strDeviceName = NAME_FOR_CEC
        cfg.bActivateSource = 0
        cfg.deviceTypes.Add(libcec.CEC_DEVICE_TYPE_PLAYBACK_DEVICE)
        cfg.clientVersion = libcec.LIBCEC_VERSION_CURRENT
        #cfg.strDeviceLanguage = "ita"

        # setup callbacks
        cfg.SetAlertCallback(self.cb_alert)
        cfg.SetCommandCallback(self.cb_command)
        cfg.SetConfigurationChangedCallback(self.cb_configuration_changed)
        cfg.SetKeyPressCallback(self.cb_key_press)
        if CEC_LOG_LEVEL != 0:
            cfg.SetLogCallback(self.cb_log)
        cfg.SetMenuStateCallback(self.cb_menu_state)
        cfg.SetSourceActivatedCallback(self.cb_source_activated)

        # create CEC adapter
        adapter = libcec.ICECAdapter.Create(cfg)
        self.cec_adapter = adapter
        lg.v((
            "libCEC loaded.\n" +
            "Version: {}\n" +
            "Info: {}"
        ).format(
            adapter.VersionToString(cfg.serverVersion),
            adapter.GetLibInfo()
        ))

        # search for adapters
        adapter_name = self.detect_adapter_name(adapter)
        if adapter_name is None:
            lg.e("No CEC adapters found.")
            return

        if not self.cec_adapter.Open(adapter_name):
            lg.e("Failed to open CEC adapter: {}".format(adapter_name))
            self.cec_adapter = None
            return

        lg.i("Opened CEC adapter: {}".format(adapter_name))
        self.inited = True

        self.update_tv_power_status()

        # some debug info
        #self.print_controlled_devices()
        #self.scan_and_print_results()
        #dbg_print("TV language is '{}'".format(self.get_tv_menu_language()))

    def deinit(self):
        self.inited = False
        if self.cec_adapter:
            self.cec_adapter.Close()
        self.cec_config.ClearCallbacks()
        self.cec_config = None

    def vendor_id_to_string(self, id: int) -> str:
        prefix = "CEC_VENDOR_"
        N = len(prefix)
        for k in dir(libcec):
            if not k.startswith(prefix):
                continue
            if getattr(libcec, k) == id:
                return k[N:]
        return "<UNKNOWN>"

    def adapter_type_to_string(self, id: int) -> str:
        prefix = "ADAPTERTYPE_"
        N = len(prefix)
        for k in dir(libcec):
            if not k.startswith(prefix):
                continue
            if getattr(libcec, k) == id:
                return k[N:]
        return "<UNKNOWN>"

    def detect_adapter_name(self, adapter) -> Optional[str]:
        adapters = adapter.DetectAdapters()
        if len(adapters) == 0:
            return None
        else:
            if SICURPHONE_LOG_LEVEL <= lg.VERBOSE:
                str_out = "CEC adapters found:"
                i = 1
                for a in adapters:
                    str_out += (
                        "\n" +
                        "#{}\n" +
                        "  type:       {} ({})\n" +
                        "  comName:    {}\n" +
                        "  comPath:    {}\n" +
                        "  vendorId:   0x{:04X} ({})\n" +
                        "  productId:  0x{:04X}\n" +
                        "  phys addr:  0x{:04X}"
                    ).format(
                        i,
                        a.adapterType,
                        self.adapter_type_to_string(a.adapterType),
                        a.strComName,
                        a.strComPath,
                        a.iVendorId,
                        self.vendor_id_to_string(a.iVendorId),
                        a.iProductId,
                        a.iPhysicalAddress
                    )
                lg.v(str_out)

        return adapters[0].strComName

    def we_are_active_source(self) -> bool:
        active = self.cec_adapter.IsLibCECActiveSource()
        lg.d("We are the active source? {}".format(active))
        return active

    def set_as_active_source(self):
        lg.i("Setting ourselves as active source")
        self.cec_adapter.SetActiveSource()

    def set_as_inactive_source(self):
        lg.i("Setting ourselves as inactive source")
        self.cec_adapter.SetInactiveView()

    def standby_tv(self):
        lg.i("Putting TV into standby")
        self.cec_adapter.StandbyDevices(libcec.CECDEVICE_TV)

    def turn_on_tv(self):
        lg.i("Turning TV on")
        self.cec_adapter.PowerOnDevices(libcec.CECDEVICE_TV)

    def get_tv_power_status(self) -> int:
        power_status = self.cec_adapter.GetDevicePowerStatus(libcec.CECDEVICE_TV)

        if SICURPHONE_LOG_LEVEL <= lg.INFO:
            power_str = self.map_powerstatus_to_name.get(power_status, "<unknown>")
            lg.v("TV power status: {} (code {})".format(
                power_str,
                power_status
            ))

        return power_status

    def update_tv_power_status(self):
        tv_power_status = self.get_tv_power_status()

        self.tv_was_on = (
            tv_power_status == libcec.CEC_POWER_STATUS_ON
            or tv_power_status == libcec.CEC_POWER_STATUS_IN_TRANSITION_STANDBY_TO_ON
            or tv_power_status == libcec.CEC_POWER_STATUS_UNKNOWN
        )

        return tv_power_status

    def get_tv_menu_language(self) -> Optional[str]:
        """Returns the TV setting for the menu language, which is a
           3-char code ISO 639-2 standard)
        """
        lang = libcec.cec_menu_language()
        self.cec_adapter.GetDeviceMenuLanguage(libcec.CECDEVICE_TV, lang)
        return lang.language

    def show_osd_string(self, message: str):
        lg.i("CEC send OSD string: {}".format(repr(message)))
        # max 13-char messages
        self.cec_adapter.SetOSDString(
            libcec.CECDEVICE_TV,
            libcec.CEC_DISPLAY_CONTROL_DISPLAY_FOR_DEFAULT_TIME,
            message
        )

    def send_custom_command(self, data: str):
        cmd = self.cec_adapter.CommandFromString(data)
        lg.d("Send CEC command: {}".format(data))
        ok = self.cec_adapter.Transmit(cmd)
        if ok:
            lg.d("Command sent")
        else:
            lg.d("Command failed")

    def print_controlled_devices(self):
        """Prints the addresses controlled by libCEC"""
        addresses = self.cec_adapter.GetLogicalAddresses()
        str_out = "==== Addresses controlled by libCEC ====\n"
        for addr in range(15):
            if addresses.IsSet(addr):
                str_out += "{} (#{}){}\n".format(
                    self.cec_adapter.LogicalAddressToString(addr),
                    addr,
                    " (active)" if self.cec_adapter.IsActiveSource(addr) else ""
                )
        str_out += "========================================"
        lg.i(str_out)

    def scan_and_print_results(self):
        """Scan the bus and display devices that were found"""
        lg.i("Requesting CEC bus information ...")
        str_out = "==== CEC bus information ====\n"
        addresses = self.cec_adapter.GetActiveDevices()
        activeSource = self.cec_adapter.GetActiveSource()
        str_out += "Active source: {}\n".format(activeSource)
        for addr in range(15):
            if addresses.IsSet(addr):
                vendorId        = self.cec_adapter.GetDeviceVendorId(addr)
                physicalAddress = self.cec_adapter.GetDevicePhysicalAddress(addr)
                active          = self.cec_adapter.IsActiveSource(addr)
                cecVersion      = self.cec_adapter.GetDeviceCecVersion(addr)
                power           = self.cec_adapter.GetDevicePowerStatus(addr)
                osdName         = self.cec_adapter.GetDeviceOSDName(addr)
                str_out += "Device #{}: {}\n".format(addr, self.cec_adapter.LogicalAddressToString(addr))
                str_out += "  address:       {}\n".format(physicalAddress)
                str_out += "  active source: {}\n".format(active)
                str_out += "  vendor:        {}\n".format(self.cec_adapter.VendorIdToString(vendorId))
                str_out += "  CEC version:   {}\n".format(self.cec_adapter.CecVersionToString(cecVersion))
                str_out += "  OSD name:      {}\n".format(osdName)
                str_out += "  power status:  {}\n".format(self.cec_adapter.PowerStatusToString(power))
        str_out += "=============================\n"
        lg.i(str_out)

    # Used together with 'deactivate_screen' to establish a "session": turn off the TV when we're done,
    # if it was off before.
    # To just activate/deactivate the device (and/or power up/down the TV), just use the simple functions
    def activate_screen(self):
        lg.i("Activating screen")
        self.update_tv_power_status()

        if not self.tv_was_on:
            self.turn_on_tv()

        self.set_as_active_source()
        self.is_activated = True

    def deactivate_screen(self):
        lg.i("Deactivating screen")

        self.set_as_inactive_source()

        if self.is_activated and not self.tv_was_on:
            self.standby_tv()
        self.is_activated = False

    def cb_log(self, level: int, time, message):
        if level & CEC_LOG_LEVEL == 0:
            return
        level_str = Cec.map_loglevel_to_name.get(level, "?")
        msg = "[CEC {}] {}".format(level_str, message)
        lg_level = Cec.map_loglevel_cec_to_app.get(level, lg.WARNING)
        lg.log(lg_level, msg)

    def cb_source_activated(self, logical_address: int, activated: bool):
        if CEC_LOG_LEVEL & libcec.CEC_LOG_DEBUG:
            lg.d("CEC source {} (addr {})".format("activated" if activated else "deactivated", logical_address))
        ok = q_put_data(self.evt_queue, Cec.evt_t(type=Cec.Evt_type.ACTIVATION, data=Cec.activation_t(activated=activated, logical_address=logical_address)))

    def cb_key_press(self, key: int, duration: int):
        # duration == 0 for presses; != 0 for releases
        # multiple (duration == 0) for repeated presses
        if CEC_LOG_LEVEL & libcec.CEC_LOG_DEBUG:
            key_name = self.map_keycode_to_name.get(key, "<unknown>")
            lg.d("CEC key press: {} (code {}, duration {}ms)".format(
                key_name,
                key,
                duration
            ))
        ok = q_put_data(self.evt_queue, Cec.evt_t(type=Cec.Evt_type.KEYPRESS, data=Cec.keypress_t(key=key, duration=duration)))
        return 1 if ok else 0

    def cb_alert(self, type, param):
        if CEC_LOG_LEVEL & libcec.CEC_LOG_DEBUG:
            lg.d("CEC alert: {}, {}".format(type, param))

    def cb_command(self, command):
        if CEC_LOG_LEVEL & libcec.CEC_LOG_DEBUG:
            lg.d("CEC command: {}".format(command))

    def cb_configuration_changed(self, config):
        if CEC_LOG_LEVEL & libcec.CEC_LOG_DEBUG:
            lg.d("CEC configuration changed: {}".format(config))

    def cb_menu_state(self, menu_state):
        if CEC_LOG_LEVEL & libcec.CEC_LOG_DEBUG:
            lg.d("CEC menu state: {}".format(menu_state))

################################

class Tts:
    class Priority(Enum):
        NAVIGATIONAL = 1
        INFORMATIVE = 2
        EMERGENCY = 3

    class Msg:
        def __init__(self, text : str, priority : 'Tts.Priority', target_start_tick : float = None):
            self.text : str = text
            self.priority : 'Tts.Priority' = priority
            self.target_start_tick : float = target_start_tick if target_start_tick is not None else get_tick()
            self.start_tick : Optional[float] = None
            self.stop_tick : Optional[float] = None
            self.proc : Optional[subprocess.Popen] = None
            self.file_path : Optional[str] = None

        def __repr__(self):
            return "<Tts.Msg prio={} t={}/{}/{} text={} file={}>".format(
                self.priority,
                self.target_start_tick,
                self.start_tick,
                self.stop_tick,
                repr(self.text),
                repr(self.file_path)
            )

    def __init__(self, incoming_msg_queue : Queue):
        self.incoming = incoming_msg_queue
        self.thread = self.make_thread()
        self._last_loop = None
        self.msg_queue : List['Tts.Msg'] = []
        self.clean_tts_folder()

        self.thread_start_tick = get_tick()
        self.thread.start()

    def clean_tts_folder(self):
        try:
            path = os.path.abspath(TTS_FOLDER_PATH)
            if not os.path.isdir(path):
                lg.d("TTS folder doesn't exist, create it: \"{}\"".format(path))
                os.makedirs(path)
            else:
                lg.d("TTS folder exists (\"{}\"), clean it".format(path))
                pattern = os.path.join(path, "*.{}".format(TTS_FILE_EXT))
                files = glob.glob(pattern)
                for f in files:
                    try:
                        lg.d("remove TTS file {}".format(f))
                        os.remove(f)
                    except OSError:
                        lg.x("Error cleaning the TTS folder (file {})".format(f))
        except OSError:
            lg.x("Error cleaning the TTS folder")
            return

    def __del__(self):
        self.thread.stop()

    @staticmethod
    def can_preempt(p_new : 'Tts.Priority', p_old : 'Tts.Priority', old_is_playing : bool) -> bool:
        # determines whether p_new can preempt p_old (p_old will be truncated and discarded)
        if p_old == Tts.Priority.NAVIGATIONAL:
            return True
        if p_old == Tts.Priority.INFORMATIVE:
            if p_new.value < p_old.value:
                return False
            if p_new == p_old:
                return not old_is_playing
        if p_old == Tts.Priority.EMERGENCY:
            return False
        return False

    def make_thread(self):
        thread = Stoppable_thread(self.run, name="tts")
        thread.daemon = True
        return thread

    def loop(self):
        now = get_tick()
        if self._last_loop is not None and now - self._last_loop < 10:
            return

        self._last_loop = now

        if not thread_has_problems(
                self.thread,
                now=now,
                start=self.thread_start_tick,
                timeout=20,
                timeout_first_time=40
                ):
            return

        lg.e("TTS thread has problems, restart it")
        self.thread.stop()
        self.thread = self.make_thread()
        self.thread_start_tick = get_tick()
        self.thread.start()

    def remove_tts_file(self, msg : 'Tts.Msg'):
        if msg.file_path is None:
            return
        try:
            if os.path.isfile(msg.file_path):
                os.remove(msg.file_path)
            msg.file_path = None
        except OSError:
            lg.x("Error removing tts file {}".format(repr(msg.file_path)))

    def stop_msg(self, msg : 'Tts.Msg', now : float):
        lg.d("TTS stop {} @ {}".format(msg, now))
        if msg.stop_tick is None:
            msg.stop_tick = now
        self.remove_tts_file(msg)
        if msg.proc is None:
            return
        try:
            msg.proc.terminate()
            msg.proc.wait(timeout=0.5)
            msg.proc = None
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            lg.x("TTS error stopping msg {}".format(msg))

        if msg.proc is None:
            return
        try:
            msg.proc.kill()
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            lg.x("TTS error killing msg {}".format(msg))

        msg.proc = None

    def start_msg(self, msg : 'Tts.Msg', now : float):
        lg.d("TTS start {} @ {}".format(msg, now))
        msg.start_tick = now

        file_path = self.create_tts_file(msg.text)
        if file_path is None:
            msg.stop_tick = now
            return
        msg.file_path = file_path

        try:
            p = Cmd.async_cmd(["play", file_path, "speed", "0.95"])
            if p is None:
                lg.e("TTS error playing file {}".format(file_path))
                msg.stop_tick = now
                self.remove_tts_file(msg)
            else:
                msg.proc = p
        except subprocess.CalledProcessError:
            lg.x("TTS error playing file {}".format(file_path))
            msg.stop_tick = now
            self.remove_tts_file(msg)
            return

    def create_tts_file(self, msg : str) -> Optional[str]:
        msg = textwrap.shorten(msg, width=2000, placeholder=" . il messaggio è troppo lungo ed è stato tagliato . .")
        t_str = get_datetime().strftime("%Y%m%d_%H%M%S%f")
        file_name = "tts_{}.{}".format(t_str, TTS_FILE_EXT)
        file_path = os.path.join(TTS_FOLDER_PATH, file_name)
        lg.d("TTS create file {}:\n{}".format(file_path, msg))

        # existing languages:
        #   'en-US'
        #   'it-IT'
        #   'fr-FR'
        #   'es-ES'
        #   'de-DE'
        lang = 'it-IT'
        (rc, p, out, err) = Cmd.cmd(["pico2wave", "-l", lang, "-w", file_path, msg], timeout=10)
        if rc == 0:
            return file_path
        else:
            lg.e("Error creating TTS file (code {}):\n{}\n{}".format(rc, out, err))
            return None

    def get_process_out_err(self, p : subprocess.Popen) -> Tuple[Optional[str], Optional[str]]:
        out = None
        err = None
        try:
            if p is not None and p.stdout is not None:
                out = p.stdout.read()
        except OSError:
            lg.x()
        try:
            if p is not None and p.stderr is not None:
                err = p.stderr.read()
        except OSError:
            lg.x()

        return (out, err)

    def run(self):
        thr = threading.current_thread()
        while True:
            time.sleep(5e-3)
            if thr.is_stopped():
                return
            thr.set_last_alive(get_tick())

            if not ENABLE_TTS:
                self.incoming.clear()
                continue

            while True:
                it = q_get(self.incoming)
                if it is None:
                    break
                it.start_tick = None
                self.msg_queue.append(it)
                lg.d("TTS enqueued {}".format(it))

            now = get_tick()

            # check currently-playing messages
            N = len(self.msg_queue)
            i = 0
            while i < N:
                m = self.msg_queue[i]
                p = m.proc
                if p is None:
                    if m.stop_tick is not None:
                        lg.d("TTS msg completed: {}".format(m))
                        N -= 1
                        del self.msg_queue[i]
                        self.remove_tts_file(m)
                        continue
                    i += 1
                else:
                    try:
                        rc = p.poll()
                        if rc is not None:
                            (out, err) = self.get_process_out_err(p)
                            if rc == 0:
                                lg.d("TTS msg ended: {} @{}\{}\n{}".format(m, now, out, err))
                            else:
                                lg.e("TTS msg ended with error {}: {} @{}\n{}\n{}".format(rc, m, now, out, err))
                            m.proc = None
                            m.stop_tick = now
                            N -= 1
                            del self.msg_queue[i]
                            self.remove_tts_file(m)
                            continue
                        else:
                            i += 1
                    except subprocess.CalledProcessError:
                        lg.e("TTS error polling for completion {}; stop now".format(m))
                        stop_msg(m, now)
                        i += 1

            # preempt old messages
            N = len(self.msg_queue)
            i = 0
            while i+1 < N:
                m_old = self.msg_queue[i]
                m_new = self.msg_queue[i+1]

                old_playing = m_old.start_tick is not None and m_old.stop_tick is None
                new_could_start = (m_new.target_start_tick <= now) or (m_new.target_start_tick <= m_old.target_start_tick + 0.1)
                if new_could_start and Tts.can_preempt(p_new=m_new.priority, p_old=m_old.priority, old_is_playing=old_playing):
                    lg.d("TTS message preemption:\n\told {}\n\tnew {}".format(m_old, m_new))
                    if old_playing:
                        self.stop_msg(m_old, now)
                    del self.msg_queue[i]
                    N -= 1
                    if i > 0:
                        i -= 1
                    continue

                i += 1

            # stop long-running messages
            if len(self.msg_queue) > 0:
                m = self.msg_queue[0]
                if m.start_tick is not None and now - m.start_tick >= 60:
                    lg.d("TTS stop long-running msg: {}".format(m))
                    del self.msg_queue[0]
                    self.stop_msg(m, now)

            # discard old messages
            N = len(self.msg_queue)
            i = 0
            while i < N:
                m = self.msg_queue[i]
                if m.start_tick is None and now > m.target_start_tick + 120:
                    lg.d("TTS drop old message (never played): {}".format(m))
                    N -= 1
                    del self.msg_queue[i]
                else:
                    i += 1

            # play next message
            if len(self.msg_queue) != 0:
                m = self.msg_queue[0]
                if m.start_tick is None and now >= m.target_start_tick:
                    self.start_msg(m, now)

################################

# for textbox input (not related to CEC)
class Keycode(Enum):
    CLEAR = 1
    BACKSPACE = 2

class SelectableTextInput(Button):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.selected = None
        self.background_normal = ""
        #self.background_down = ""
        self.select(False)
        self.clbk_on_input : Callable[[SelectableTextInput, Union[Keycode, str], Optional[int]], None] = self.default_on_input

    def select(self, select : bool = True):
        if self.selected == select:
            return

        self.selected = select
        if select:
            self.background_color = (1.0, 0.8, 0.2, 1.0)
            self.color = (0, 0, 0, 1)
        else:
            self.background_color = (0.9, 0.9, 0.9, 1.0)
            self.color = (0, 0, 0, 1)

    def deselect(self):
        self.select(False)

    @staticmethod
    def default_change(text : str, key : Union[Keycode, str], position : Optional[int]) -> str:
        if position is None:
            position = len(text)
        if key == Keycode.CLEAR:
            return ""
        elif key == Keycode.BACKSPACE:
            if position == 0:
                return text
            return text[:position-1] + text[position:]
        elif isinstance(key, str):
            return text[:position] + key + text[position:]
        else:
            return text


    def default_on_input(self, key : Union[Keycode, str], position : Optional[int]):
        self.text = SelectableTextInput.default_change(
            self.text, # type: ignore
            key,
            position)

class SelectableButton(Button):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.selected = None
        self.background_normal = ""
        self.select(False)

    def select(self, select : bool = True):
        if self.selected == select:
            return

        self.selected = select
        if select:
            self.background_color = (1.0, 0.5, 0.1, 1.0)
            self.color = (0, 0, 0, 1)
        else:
            self.background_color = (0.3, 0.3, 0.3, 1.0)
            self.color = (1, 1, 1, 1)

    def deselect(self):
        self.select(False)

class MyLabel(Label):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    # TODBG this is for debug
    #def on_size(self, *args):
    #    r = Gui_utils.align_in(self.size, Rectangle(pos=self.pos, size=self.size), halign=self.halign, valign=self.valign)
    #    r_inner_sz = Gui_utils.shrink_rect_size(r.size, [-2, -2])
    #    r_inner = Gui_utils.align_in(r_inner_sz, r)
    #    print("rect", r, self)
    #    self.canvas.before.clear()
    #    with self.canvas.before:
    #        Color(0.8, 0, 0, 1)
    #        Rectangle(pos=r.pos, size=r.size)
    #    with self.canvas.before:
    #        Color(0, 0.3, 0, 1)
    #        Rectangle(pos=r_inner.pos, size=r_inner.size)

class Spacer(Widget):
    pass

def rect_to_str(rect: Rectangle):
    return "Rectangle({}x{} @{},{})".format(rect.size[0], rect.size[1], rect.pos[0], rect.pos[1])

class Gui_utils:
    SPACING_LARGE = sp(25)
    SPACING_MEDIUM = sp(17)
    SPACING_SMALL = sp(10)
    FONT_SIZE_HUGE = 100
    FONT_SIZE_LARGE = 60
    FONT_SIZE_MEDIUM = 45
    FONT_SIZE_SMALL = 35

    @staticmethod
    def align_in(size_or_r, rect : Rectangle, halign='center', valign='middle'):
        if isinstance(size_or_r, Rectangle):
            w = size_or_r.size[0]
            h = size_or_r.size[1]
        else:
            h = size_or_r[0]
            w = size_or_r[1]
        if w is None:
            w = 0
        if h is None:
            h = 0
        x = rect.pos[0]
        y = rect.pos[1]
        ww = rect.size[0]
        hh = rect.size[1]
        if halign == 'center':
            dw = (ww-w)/2
        elif halign == 'right':
            dw = (ww-w)
        else:
            dw = 0
        if valign == 'middle':
            dh = (hh-h)/2
        elif valign == 'bottom':
            dh = (hh-h)
        else:
            dh = 0
        r = Rectangle(pos=(x+dw, y+dh), size=(w, h))
        return r

    @staticmethod
    def shrink_rect_size(sz, delta):
        return (max(0, sz[0] + delta[0]), max(0, sz[1] + delta[1]))

    @staticmethod
    def clamp(n, minn, maxn):
        return max(min(maxn, n), minn)

    @staticmethod
    def remap_clamp(x, in0, in1, out0, out1):
        return Gui_utils.remap(Gui_utils.clamp(x, in0, in1), in0, in1, out0, out1)

    @staticmethod
    def remap(x, in0, in1, out0, out1):
        return (x - in0) / (in1 - in0) * (out1 - out0) + out0

    @staticmethod
    def close_gui():
        app = App.get_running_app()
        if app is None:
            lg.w("No gui is running, can't close it")
        else:
            app.stop()

    @staticmethod
    def run_gui_code(fn):
        if is_main_thread():
            fn()
        else:
            Clock.schedule_once(lambda *args: fn(), -1)

    @staticmethod
    def schedule_once(fn, delay : float):
        Clock.schedule_once(lambda *args: fn(), delay)

    # text that usually spans multiple lines and wraps
    @staticmethod
    def adapt_text_multiline(widget, font_size, halign='center', valign='middle', debug=False):
        #TODO:
        Gui_utils.adapt_text(widget, font_size, halign=halign, valign=valign)
        return

        base_size = font_size

        if halign is not None:
            widget.halign = halign
        if valign is not None:
            widget.valign = valign

        def adj_size(*args, **kwargs):
            wsz = Window.size
            wsz = min(wsz[0]*1.25, wsz[1])
            scale_max = Gui_utils.remap_clamp(wsz, 300, 2500, 0.333, 3)
            scale_min = scale_max * 0.1

            scale = scale_max
            sz = widget.size
            sz = (math.floor(sz[0]), math.floor(sz[1]))
            zero_area = (sz[0] == 0 or sz[1] == 0)

            if debug:
                print("RES", sz, widget) # TODO

            for i in range(20):
                widget.text_size = [sz[0], 2*sz[1]]
                font_size = sp(scale * base_size)
                widget.font_size = font_size
                font_size_max = sp(scale_max * base_size)
                font_size_min = sp(scale_min * base_size)
                widget.texture_update()
                txsz = widget.texture_size
                if debug:
                    print("RES#{}".format(i), "s={:.4f}/{:.4f}/{:.4f}".format(scale_min, scale, scale_max),
                        "f={:.2f}/{:.2f}/{:.2f}".format(font_size_min, font_size, font_size_max),
                        "txsz=", txsz)
                if (txsz[0] <= sz[0] and txsz[1] < sz[1]) or zero_area:
                    scale_min = scale
                else:
                    scale_max = scale

                scale = (scale_max + scale_min) / 2

                if font_size_max - font_size_min < 0.1:
                    if debug: # TODO
                        print("converged")
                    break

            widget.font_size = sp(scale_min * base_size)
            widget.text_size = widget.size
            if debug:
                print("FINAL", "s={:.4f}/{:.4f}/{:.4f}".format(scale_min, scale, scale_max),
                    "f={:.2f}/{:.2f}/{:.2f}".format(font_size_min, font_size, font_size_max),
                    "txsz=", txsz)

        widget.bind(size=adj_size)

    # text that usually spans one line, and only occasionally can wrap
    @staticmethod
    def adapt_text(widget, font_size, halign='center', valign='middle'):
        #TODO
        Gui_utils.adapt_text_singleline(widget, font_size, halign=halign, valign=valign)

    # text that usually spans multiple lines, and can overflow vertically
    @staticmethod
    def adapt_text_scrollable(widget, font_size, halign='center', valign='middle'):
        #TODO
        Gui_utils.adapt_text_singleline(widget, font_size, halign=halign, valign=valign)

    # text that must be on a single line (e.g. single-word buttons)
    @staticmethod
    def adapt_text_singleline(widget, font_size, halign='center', valign='middle'):
        base_size = font_size

        if halign is not None:
            widget.halign = halign
        if valign is not None:
            widget.valign = valign

        def adj_size(*args, **kwargs):
            wsz = Window.size
            wsz = min(wsz[0]*1.25, wsz[1])
            scale_max = Gui_utils.remap_clamp(wsz, 300, 2500, 0.333, 3)
            scale_min = scale_max * 0.1

            scale = scale_max
            sz = widget.size
            sz = (math.floor(sz[0]), math.floor(sz[1]))
            zero_area = (sz[0] == 0 or sz[1] == 0)

            for i in range(20):
                widget.text_size = [None, None]
                font_size = sp(scale * base_size)
                widget.font_size = font_size
                font_size_max = sp(scale_max * base_size)
                font_size_min = sp(scale_min * base_size)
                if font_size_max - font_size_min < 0.1:
                    break

                widget.texture_update()
                txsz = widget.texture_size
                if (txsz[0] <= sz[0] and txsz[1] <= sz[1]) or zero_area:
                    scale_min = scale
                else:
                    scale_max = scale

                scale = (scale_max + scale_min) / 2

            widget.font_size = sp(scale_min * base_size)
            widget.text_size = widget.size

        widget.bind(size=adj_size)

    @staticmethod
    def adapt_label(widget, halign='center', valign='middle'):
        if halign is not None:
            widget.halign = halign
        if valign is not None:
            widget.valign = valign
        widget.bind(size=widget.setter('text_size'))

    @staticmethod
    def adapt_font_size(widget, base_size):
        widget.font_size = sp(base_size)

        def on_window_size(*args):
            sz = Window.size
            sz = min(sz[0]*1.25, sz[1])
            scale = Gui_utils.remap_clamp(sz, 300, 2500, 0.333, 3)
            widget.font_size = sp(scale * base_size)

        Window.bind(size=on_window_size)

class Gui(App):
    DEAD_TIME_AFTER_SCREEN_CHANGE = 0.5

    SHUTDOWN_DEAD_TIME = 30

    class Conn_status(Enum):
        NONE = 0
        OK = 1

    class Evt_type(Enum):
        SEND_DATA_CANCEL = 1
        SEND_DATA_OK = 2
        MSG_CANCEL = 3
        MSG_OK = 4
        SCREEN_ACTIVATE = 5
        SCREEN_DEACTIVATE = 6
        SHUTDOWN = 7

    evt_t = namedtuple("evt_t", ["type", "data"])

    class MyScreen:
        def __init__(self, gui : 'Gui'):
            self.gui = gui
            self.selected_widget = None
            self.gui_root = None

        def select_widget(self, w : Optional[Widget]):
            if self.selected_widget != w:
                if self.selected_widget is not None:
                    self.selected_widget.select(False)
                if w is not None:
                    w.select(True)
                self.selected_widget = w

    class Screen_main(MyScreen):
        def __init__(self, gui : 'Gui'):
            super().__init__(gui)

            ly_page = BoxLayout(orientation='vertical', spacing=Gui_utils.SPACING_LARGE, padding=Gui_utils.SPACING_LARGE)

            img = Image(source='logo_w600.png')
            ly_page.add_widget(img)

            lbl_info = MyLabel(text='', size_hint=(1, 0.7))
            Gui_utils.adapt_text(lbl_info, Gui_utils.FONT_SIZE_MEDIUM)
            ly_page.add_widget(lbl_info)

            btn_send_covid_data = SelectableButton(text='Invia dati COVID', size_hint=(1, 0.8))
            Gui_utils.adapt_text(btn_send_covid_data, Gui_utils.FONT_SIZE_LARGE)
            btn_send_covid_data.bind(on_press=self.on_btn_send)
            ly_page.add_widget(btn_send_covid_data)

            lbl_last_send = MyLabel(text='', size_hint=(1, 0.8))
            Gui_utils.adapt_text(lbl_last_send, Gui_utils.FONT_SIZE_SMALL, 'center', 'top')
            ly_page.add_widget(lbl_last_send)

            ly_footer = BoxLayout(orientation='horizontal', spacing=Gui_utils.SPACING_MEDIUM, padding=0, size_hint=(1, 0.8))

            lbl_status = MyLabel(text='', size_hint=(5, 1))
            Gui_utils.adapt_text(lbl_status, Gui_utils.FONT_SIZE_SMALL, 'left', 'bottom')
            ly_footer.add_widget(lbl_status)

            btn_shutdown = SelectableButton(text='Spegni', size_hint=(1, 1))
            Gui_utils.adapt_text(btn_shutdown, Gui_utils.FONT_SIZE_MEDIUM)
            btn_shutdown.bind(on_press=self.on_btn_shutdown)
            ly_footer.add_widget(btn_shutdown)

            ly_page.add_widget(ly_footer)

            scr = Screen(name='main')
            scr.add_widget(ly_page)

            self.btn_send_covid_data = btn_send_covid_data
            self.lbl_info = lbl_info
            self.lbl_last_send = lbl_last_send
            self.lbl_status = lbl_status
            self.btn_shutdown = btn_shutdown
            self.gui_root = scr

        def select_default_widget(self):
            self.select_widget(self.btn_send_covid_data)

        def on_btn_shutdown(self, *args):
            def cancel_shutdown():
                gui = self.gui
                gui.is_shutting_down = False
                gui.pop_screen(gui.scr_shutdown)

            def on_confirm():
                lg.d("Shutdown")
                gui = self.gui
                gui.last_shutdown_attempt_tick = get_tick()
                gui.is_shutting_down = True
                evt = Gui.evt_t(type=Gui.Evt_type.SHUTDOWN, data=None)
                q_put_data(self.gui.evt_queue, evt)
                gui.close_modals()
                gui.push_screen(gui.scr_shutdown)
                Gui_utils.schedule_once(cancel_shutdown, Gui.SHUTDOWN_DEAD_TIME)

            self.gui.show_screen_confirm(
                "Sei sicuro di voler spegnere il sistema?"
                    "\nQuando il sistema è spento, non puoi ricevere messaggi né inviare dati."
                    "\nSe spegni il sistema, puoi togliere alimentazione quando la spia rimane accesa fissa in rosso.",
                on_confirm=on_confirm
            )

        def on_btn_send(self, *args):
            item = Item(source=Item.Source.USER, type=Item.Type.REQ_DATA)
            self.gui.enqueue_item(item)

        def handle_keypress(self, key):
            s = self.selected_widget
            if s is None:
                if key in [KEY_OK, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT]:
                    self.select_widget(self.btn_send_covid_data)
                return
            if key in [KEY_OK, KEY_ENTER]:
                s.trigger_action()
            elif key == KEY_UP:
                self.select_widget(self.btn_send_covid_data)
            elif key == KEY_DOWN:
                self.select_widget(self.btn_shutdown)

    class Screen_emg(MyScreen):
        def __init__(self, gui : 'Gui'):
            super().__init__(gui)

            ly_page = BoxLayout(orientation='vertical', spacing=Gui_utils.SPACING_LARGE, padding=Gui_utils.SPACING_LARGE)

            img = Image(source='logo_w600.png')
            ly_page.add_widget(img)

            lbl_msg = MyLabel(text='', size_hint=(1, 5))
            Gui_utils.adapt_text_multiline(lbl_msg, Gui_utils.FONT_SIZE_MEDIUM)
            ly_page.add_widget(lbl_msg)

            btn_ok = SelectableButton(text='OK')
            Gui_utils.adapt_text(btn_ok, Gui_utils.FONT_SIZE_LARGE)
            btn_ok.bind(on_press=self.on_btn_ok)
            ly_page.add_widget(btn_ok)

            scr = Screen(name='emg')
            scr.add_widget(ly_page)

            self.select_widget(btn_ok)

            self.lbl_msg = lbl_msg
            self.btn_ok = btn_ok
            self.gui_root = scr

        def on_btn_ok(self, *args):
            gui = self.gui
            gui.pop_screen(self)

        def handle_keypress(self, key):
            s = self.selected_widget
            if s is None:
                if key in [KEY_OK, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT]:
                    self.select_widget(self.btn_ok)
                    return
            if key in [KEY_CLEAR, KEY_EXIT]:
                self.btn_ok.trigger_action()
                return
            if key in [KEY_OK, KEY_ENTER]:
                if s == self.btn_ok:
                    self.btn_ok.trigger_action()
                    return

    class Screen_msg(MyScreen):
        def __init__(self, gui : 'Gui'):
            super().__init__(gui)

            ly_page = BoxLayout(orientation='vertical', spacing=Gui_utils.SPACING_LARGE, padding=Gui_utils.SPACING_LARGE)

            img = Image(source='logo_w600.png')
            ly_page.add_widget(img)

            lbl_header = MyLabel(text='Hai ricevuto un messaggio:', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl_header, Gui_utils.FONT_SIZE_MEDIUM)
            ly_page.add_widget(lbl_header)

            lbl_msg = MyLabel(text='', size_hint=(1,5))
            Gui_utils.adapt_text_multiline(lbl_msg, font_size=Gui_utils.FONT_SIZE_MEDIUM, debug=True)
            ly_page.add_widget(lbl_msg)

            btn_ok = SelectableButton(text='OK')
            Gui_utils.adapt_text(btn_ok, Gui_utils.FONT_SIZE_LARGE)
            btn_ok.bind(on_press=self.on_btn_ok)
            ly_page.add_widget(btn_ok)

            scr = Screen(name='msg')
            scr.add_widget(ly_page)

            self.select_widget(btn_ok)

            self.lbl_msg = lbl_msg
            self.btn_ok = btn_ok
            self.gui_root = scr
            self.item : Optional[Item] = None

        def on_btn_ok(self, *args):
            gui = self.gui
            def on_confirm():
                lg.d("Gui: msg acked, notify main thread")
                gui.dequeue_item(self.item)
                gui.pop_screen(self)
                evt = Gui.evt_t(type=Gui.Evt_type.MSG_OK, data={'item': self.item})
                q_put_data(gui.evt_queue, evt)

            gui.show_screen_confirm("Confermi di aver letto il messaggio?", on_confirm)

        def on_btn_cancel(self, *args):
            gui = self.gui
            def on_confirm():
                lg.d("Gui: msg nacked, notify main thread")
                gui.dequeue_item(self.item)
                gui.pop_screen(self)
                evt = Gui.evt_t(type=Gui.Evt_type.MSG_CANCEL, data={'item': self.item})
                q_put_data(gui.evt_queue, evt)

            gui.show_screen_confirm("Sei sicuro di voler chiudere il messaggio?", on_confirm)

        def handle_keypress(self, key):
            s = self.selected_widget
            if s is None:
                if key in [KEY_OK, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT]:
                    self.select_widget(self.btn_ok)
                    return
            if key in [KEY_CLEAR, KEY_EXIT]:
                self.on_btn_cancel()
                return
            if key in [KEY_OK, KEY_ENTER]:
                if s == self.btn_ok:
                    self.btn_ok.trigger_action()
                    return

    class Screen_send_outcome(MyScreen):
        def __init__(self, gui : 'Gui'):
            super().__init__(gui)

            ly_page = BoxLayout(orientation='vertical', spacing=Gui_utils.SPACING_LARGE, padding=Gui_utils.SPACING_LARGE)

            lbl_msg = MyLabel(text='', size_hint=(1, 5))
            Gui_utils.adapt_text_multiline(lbl_msg, Gui_utils.FONT_SIZE_MEDIUM)
            ly_page.add_widget(lbl_msg)

            btn_ok = SelectableButton(text='OK')
            Gui_utils.adapt_text(btn_ok, Gui_utils.FONT_SIZE_LARGE)
            btn_ok.bind(on_press=self.on_btn_ok)
            ly_page.add_widget(btn_ok)

            scr = Screen(name='send_outcome')
            scr.add_widget(ly_page)

            self.select_widget(btn_ok)

            self.lbl_msg = lbl_msg
            self.btn_ok = btn_ok
            self.gui_root = scr

        def on_btn_ok(self, *args):
            gui = self.gui
            gui.pop_screen(self)

        def handle_keypress(self, key):
            gui = self.gui
            s = self.selected_widget
            if s is None:
                if key in [KEY_OK, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT]:
                    self.select_widget(self.btn_ok)
                    return
            if key in [KEY_CLEAR, KEY_EXIT]:
                gui.pop_screen(self)
                return
            if key in [KEY_OK, KEY_ENTER]:
                if s == self.btn_ok:
                    gui.pop_screen(self)
                    return

    class Screen_req_data(MyScreen):
        def __init__(self, gui : 'Gui'):
            super().__init__(gui)

            ly_page = BoxLayout(orientation='vertical', spacing=Gui_utils.SPACING_SMALL, padding=Gui_utils.SPACING_LARGE)

            img = Image(source='logo_w600.png')
            ly_page.add_widget(img)

            lbl_header = MyLabel(text='Invia dati COVID', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl_header, Gui_utils.FONT_SIZE_MEDIUM)
            ly_page.add_widget(lbl_header)

            lbl_hint = MyLabel(text='', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl_hint, Gui_utils.FONT_SIZE_MEDIUM)
            ly_page.add_widget(lbl_hint)

            ly_inputs = GridLayout(
                size_hint=(1,5),
                spacing=Gui_utils.SPACING_SMALL,
                padding=Gui_utils.SPACING_SMALL
            )
            ly_inputs.cols = 2

            #[^C]
            lbl = MyLabel(text='Temperatura (°C)', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl, Gui_utils.FONT_SIZE_MEDIUM)
            ly_inputs.add_widget(lbl)
            txt_temperature = SelectableTextInput(text='')
            Gui_utils.adapt_text(txt_temperature, Gui_utils.FONT_SIZE_LARGE)
            txt_temperature.bind(on_press=self.on_txt_click)
            ly_inputs.add_widget(txt_temperature)

            # [mmHg]
            lbl = MyLabel(text='Pressione min', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl, Gui_utils.FONT_SIZE_MEDIUM)
            ly_inputs.add_widget(lbl)
            txt_pressure_min = SelectableTextInput(text='')
            Gui_utils.adapt_text(txt_pressure_min, Gui_utils.FONT_SIZE_LARGE)
            txt_pressure_min.bind(on_press=self.on_txt_click)
            ly_inputs.add_widget(txt_pressure_min)

            # [mmHg]
            lbl = MyLabel(text='Pressione max', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl, Gui_utils.FONT_SIZE_MEDIUM)
            ly_inputs.add_widget(lbl)
            txt_pressure_max = SelectableTextInput(text='')
            Gui_utils.adapt_text(txt_pressure_max, Gui_utils.FONT_SIZE_LARGE)
            txt_pressure_max.bind(on_press=self.on_txt_click)
            ly_inputs.add_widget(txt_pressure_max)

            # [bpm]
            lbl = MyLabel(text='Pulsazioni', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl, Gui_utils.FONT_SIZE_MEDIUM)
            ly_inputs.add_widget(lbl)
            txt_pulses = SelectableTextInput(text='')
            Gui_utils.adapt_text(txt_pulses, Gui_utils.FONT_SIZE_LARGE)
            txt_pulses.bind(on_press=self.on_txt_click)
            ly_inputs.add_widget(txt_pulses)

            # [%]
            lbl = MyLabel(text='Ossigeno (%)', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl, Gui_utils.FONT_SIZE_MEDIUM)
            ly_inputs.add_widget(lbl)
            txt_oxygen = SelectableTextInput(text='')
            Gui_utils.adapt_text(txt_oxygen, Gui_utils.FONT_SIZE_LARGE)
            txt_oxygen.bind(on_press=self.on_txt_click)
            ly_inputs.add_widget(txt_oxygen)

            ly_page.add_widget(ly_inputs)

            ly_btns = BoxLayout(orientation='horizontal', spacing=Gui_utils.SPACING_MEDIUM, padding=0)

            btn_exit = SelectableButton(text='ESCI')
            Gui_utils.adapt_text(btn_exit, Gui_utils.FONT_SIZE_LARGE)
            btn_exit.bind(on_press=self.on_btn_exit)
            ly_btns.add_widget(btn_exit)

            btn_send = SelectableButton(text='INVIA')
            Gui_utils.adapt_text(btn_send, Gui_utils.FONT_SIZE_LARGE)
            btn_send.bind(on_press=self.on_btn_send)
            ly_btns.add_widget(btn_send)

            ly_page.add_widget(ly_btns)

            scr = Screen(name='req_data')
            scr.add_widget(ly_page)

            self.select_widget(txt_temperature)

            self.lbl_hint = lbl_hint
            self.txt_temperature = txt_temperature
            self.txt_pressure_min = txt_pressure_min
            self.txt_pressure_max = txt_pressure_max
            self.txt_pulses = txt_pulses
            self.txt_oxygen = txt_oxygen
            self.btn_send = btn_send
            self.btn_exit = btn_exit

            self.w_inputs = [txt_temperature, txt_pressure_min, txt_pressure_max, txt_pulses, txt_oxygen]

            self.gui_root = scr

            self.item : Optional[Item] = None
            self.values : Dict[str, Any] = {}
            self.assign_item(None)

        def get_tts_hint_for_source(self, source : Optional[Item.Source]) -> str:
            if source == Item.Source.USER:
                return ""
            elif source == Item.Source.SCHEDULE:
                return "devi inviare i dati COVID giornalieri."
            elif source == Item.Source.SERVER:
                return "il medico ti chiede di inviare i dati COVID attuali."
            return ""

        def get_hint_for_source(self, source : Optional[Item.Source], dt : Optional[datetime.datetime] = None) -> str:
            t_str = "({})".format(self.gui.datetime_to_str(dt)) if dt is not None else ""
            if source == Item.Source.USER:
                return ""
            elif source == Item.Source.SCHEDULE:
                return "Devi inviare i dati COVID giornalieri{}".format(t_str)
            elif source == Item.Source.SERVER:
                return "Il medico ti chiede di inviare i dati COVID attuali{}".format(t_str)
            return ""

        def assign_item(self, item : Optional[Item]):
            lg.d("assign item:\nold={}\n->{}\nvalues={}".format(self.item, item, self.values))
            self.item = item
            self.values = {'temperatura': None, 'pressione min': None, 'pressione max': None, 'pulsazioni': None, 'ossigeno': None}
            self.txt_temperature.text = ""
            self.txt_pressure_min.text = ""
            self.txt_pressure_max.text = ""
            self.txt_pulses.text = ""
            self.txt_oxygen.text = ""
            if item is None:
                self.lbl_hint.text = self.get_hint_for_source(None)
            else:
                self.lbl_hint.text = self.get_hint_for_source(item.source, dt=item.timestamp)
            self.select_widget(self.txt_temperature)

        def reassign_item(self, item : Item):
            lg.d("reassign item:\n{}\n->{}\nvalues={}".format(self.item, item, self.values))
            self.item = item
            if item is None:
                self.lbl_hint.text = self.get_hint_for_source(None)
            else:
                self.lbl_hint.text = self.get_hint_for_source(item.source, dt=item.timestamp)

        def check_values(self, item : Item, values) -> Optional[str]:
            def empty(v):
                return v is None or v == ""

            to_check = ['temperatura', 'pressione min', 'pressione max', 'pulsazioni', 'ossigeno']
            missing = []
            for k in to_check:
                v = values.get(k, None)
                if empty(v):
                    missing.append(k)
            if len(missing) == 0:
                return None

            if len(missing) == 1:
                return "Non hai inserito il valore di {}.".format(missing[0])
            return "Non hai inserito tutti i valori."

        def on_btn_send(self, *args):
            gui = self.gui
            def on_confirm():
                lg.d("Send data ok")
                gui.dequeue_item(self.item)
                gui.pop_screen(self)
                evt = Gui.evt_t(type=Gui.Evt_type.SEND_DATA_OK, data={'item': self.item, 'values': self.values})
                q_put_data(gui.evt_queue, evt)

            err = self.check_values(self.item, self.values)
            if err is None:
                gui.show_screen_confirm("Sei sicuro di voler inviare i dati?", on_confirm)
            else:
                gui.show_screen_confirm("{}\nVuoi inviare comunque i dati?".format(err), on_confirm)

        def on_btn_exit(self, *args):
            gui = self.gui
            def on_confirm():
                lg.i("Send data cancelled")
                gui.dequeue_item(self.item)
                gui.pop_screen(self)
                evt = Gui.evt_t(type=Gui.Evt_type.SEND_DATA_CANCEL, data={'item': self.item})
                q_put_data(gui.evt_queue, evt)

            gui.show_screen_confirm("Sei sicuro di voler uscire senza inviare i dati?", on_confirm)

        def on_txt_click(self, widget, *args):
            lbl_error = self.gui.scr_keypad.lbl_error
            def str_to_float(s : str) -> float:
                if s == "" or s is None:
                    return float('nan')
                s = s.replace(DECIMAL_POINT_CHAR, '.')
                return float(s)
            def format_float_str(s : str, max_dec_digits : Optional[int] = None, max_int_digits : Optional[int] = None) -> str:
                if s == "" or s is None:
                    return ""
                vv = s.split(DECIMAL_POINT_CHAR)
                vv = vv[0:2]
                if max_dec_digits is not None:
                    if max_dec_digits == 0:
                        vv = vv[0:1]
                    else:
                        if len(vv) >= 2:
                            vv[1] = vv[1][0:max_dec_digits]
                if max_int_digits is not None:
                    if max_int_digits == 0:
                        vv[0] = ""
                    else:
                        vv[0] = vv[0][0:max_int_digits]
                if vv[0] == "":
                    vv[0] = "0"
                else:
                    vv[0] = str(int(vv[0]))
                s = DECIMAL_POINT_CHAR.join(vv[0:2])
                return s

            def check_temperature(s : str, accept_empty : bool = True) -> (Optional[str], str): # return (error, cleaned_string)
                if s == "":
                    if accept_empty:
                        return (None, "")
                    else:
                        return ("Devi inserire un valore", "")
                s = format_float_str(s, max_dec_digits=1, max_int_digits=2)
                f = str_to_float(s)
                if f < 0 or f >= 100:
                    return ("Valore non valido", s)
                return (None, s)
            def on_chg_temperature(w : SelectableTextInput, key : Union[Keycode, str], position : Optional[int]):
                old_text = w.text
                text = SelectableTextInput.default_change(old_text, key, position)
                if len(text) > 10:
                    err = "Valore non valido"
                else:
                    [err, text] = check_temperature(text)
                    w.text = text
                if err is None:
                    lbl_error.text = ""
                else:
                    lbl_error.text = err
            def on_conf_temperature(text : str) -> bool:
                [err, text] = check_temperature(text)
                if err is None:
                    f = str_to_float(text)
                    self.values['temperatura'] = f
                    self.txt_temperature.text = text
                    return True
                else:
                    lbl_error.text = err
                    return False

            def check_pressure_min(s : str, accept_empty : bool = False) -> (Optional[str], str): # return (error, cleaned_string)
                if s == "":
                    if accept_empty:
                        return (None, "")
                    else:
                        return ("Devi inserire un valore", "")
                s = format_float_str(s, max_dec_digits=0, max_int_digits=3)
                f = str_to_float(s)
                if f < 0 or f >= 1000:
                    return ("Valore non valido", s)
                return (None, s)
            def on_chg_pressure_min(w : SelectableTextInput, key : Union[Keycode, str], position : Optional[int]):
                old_text = w.text
                text = SelectableTextInput.default_change(old_text, key, position)
                if len(text) > 10:
                    err = "Valore non valido"
                else:
                    [err, text] = check_pressure_min(text)
                    w.text = text
                if err is None:
                    lbl_error.text = ""
                else:
                    lbl_error.text = err
            def on_conf_pressure_min(text : str) -> bool:
                [err, text] = check_pressure_min(text)
                if err is None:
                    f = str_to_float(text)
                    self.values['pressione min'] = f
                    self.txt_pressure_min.text = text
                    return True
                else:
                    lbl_error.text = err
                    return False

            def check_pressure_max(s : str, accept_empty : bool = False) -> (Optional[str], str): # return (error, cleaned_string)
                if s == "":
                    if accept_empty:
                        return (None, "")
                    else:
                        return ("Devi inserire un valore", "")
                s = format_float_str(s, max_dec_digits=0, max_int_digits=3)
                f = str_to_float(s)
                if f < 0 or f >= 1000:
                    return ("Valore non valido", s)
                return (None, s)
            def on_chg_pressure_max(w : SelectableTextInput, key : Union[Keycode, str], position : Optional[int]):
                old_text = w.text
                text = SelectableTextInput.default_change(old_text, key, position)
                if len(text) > 10:
                    err = "Valore non valido"
                else:
                    [err, text] = check_pressure_max(text)
                    w.text = text
                if err is None:
                    lbl_error.text = ""
                else:
                    lbl_error.text = err
            def on_conf_pressure_max(text : str) -> bool:
                [err, text] = check_pressure_max(text)
                if err is None:
                    f = str_to_float(text)
                    self.values['pressione max'] = f
                    self.txt_pressure_max.text = text
                    return True
                else:
                    lbl_error.text = err
                    return False

            def check_pulses(s : str, accept_empty : bool = False) -> (Optional[str], str): # return (error, cleaned_string)
                if s == "":
                    if accept_empty:
                        return (None, "")
                    else:
                        return ("Devi inserire un valore", "")
                s = format_float_str(s, max_dec_digits=0, max_int_digits=3)
                f = str_to_float(s)
                if f < 0 or f >= 1000:
                    return ("Valore non valido", s)
                return (None, s)
            def on_chg_pulses(w : SelectableTextInput, key : Union[Keycode, str], position : Optional[int]):
                old_text = w.text
                text = SelectableTextInput.default_change(old_text, key, position)
                if len(text) > 10:
                    err = "Valore non valido"
                else:
                    [err, text] = check_pulses(text)
                    w.text = text
                if err is None:
                    lbl_error.text = ""
                else:
                    lbl_error.text = err
            def on_conf_pulses(text : str) -> bool:
                [err, text] = check_pulses(text)
                if err is None:
                    f = str_to_float(text)
                    self.values['pulsazioni'] = f
                    self.txt_pulses.text = text
                    return True
                else:
                    lbl_error.text = err
                    return False

            def check_oxygen(s : str, accept_empty : bool = False) -> (Optional[str], str): # return (error, cleaned_string)
                if s == "":
                    if accept_empty:
                        return (None, "")
                    else:
                        return ("Devi inserire un valore", "")
                s = format_float_str(s, max_dec_digits=1, max_int_digits=3)
                f = str_to_float(s)
                if f < 0 or f > 100:
                    return ("Valore non valido", s)
                return (None, s)
            def on_chg_oxygen(w : SelectableTextInput, key : Union[Keycode, str], position : Optional[int]):
                old_text = w.text
                text = SelectableTextInput.default_change(old_text, key, position)
                if len(text) > 10:
                    err = "Valore non valido"
                else:
                    [err, text] = check_oxygen(text)
                    w.text = text
                if err is None:
                    lbl_error.text = ""
                else:
                    lbl_error.text = err
            def on_conf_oxygen(text : str) -> bool:
                [err, text] = check_oxygen(text)
                if err is None:
                    f = str_to_float(text)
                    self.values['ossigeno'] = f
                    self.txt_oxygen.text = text
                    return True
                else:
                    lbl_error.text = err
                    return False

            if widget == self.txt_temperature:
                hint = 'Inserisci il valore di temperatura (°C)'
                chg = on_chg_temperature
                conf = on_conf_temperature
            elif widget == self.txt_pressure_min:
                hint = 'Inserisci il valore di pressione min'
                chg = on_chg_pressure_min
                conf = on_conf_pressure_min
            elif widget == self.txt_pressure_max:
                hint = 'Inserisci il valore di pressione max'
                chg = on_chg_pressure_max
                conf = on_conf_pressure_max
            elif widget == self.txt_pulses:
                hint = 'Inserisci il valore di pulsazioni'
                chg = on_chg_pulses
                conf = on_conf_pulses
            elif widget == self.txt_oxygen:
                hint = 'Inserisci il valore di ossigeno (%)'
                chg = on_chg_oxygen
                conf = on_conf_oxygen
            else:
                return
            self.gui.show_screen_keypad(hint=hint, initial_text="", on_input=chg, on_confirm=conf)

        def handle_keypress(self, key):
            s = self.selected_widget
            if key in [KEY_CLEAR, KEY_EXIT]:
                self.select_widget(self.btn_exit)
                self.btn_exit.trigger_action()
            elif s is None:
                if key in [KEY_OK, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT]:
                    self.select_widget(self.txt_temperature)
            elif key in [KEY_OK, KEY_ENTER]:
                s.trigger_action()
            elif key == KEY_UP:
                Ni = len(self.w_inputs)
                if Ni != 0:
                    if s in [self.btn_exit, self.btn_send]:
                        self.select_widget(self.w_inputs[-1])
                    else:
                        idx = index_or(self.w_inputs, s, 0)
                        if idx == 0:
                            self.select_widget(self.btn_send)
                        else:
                            idx -= 1
                            self.select_widget(self.w_inputs[idx])
            elif key == KEY_DOWN:
                if s in [self.btn_exit, self.btn_send]:
                    self.select_widget(self.w_inputs[0])
                    return
                Ni = len(self.w_inputs)
                if Ni != 0:
                    idx = index_or(self.w_inputs, s, -1)
                    idx += 1
                    if idx >= Ni:
                        self.select_widget(self.btn_send)
                    else:
                        self.select_widget(self.w_inputs[idx])
            elif key == KEY_LEFT:
                if s == self.btn_send:
                    self.select_widget(self.btn_exit)
            elif key == KEY_RIGHT:
                if s == self.btn_exit:
                    self.select_widget(self.btn_send)

    class Screen_confirm_dialog(MyScreen):
        def __init__(self, gui : 'Gui'):
            super().__init__(gui)

            ly_page = BoxLayout(orientation='vertical', spacing=Gui_utils.SPACING_SMALL, padding=sp(150))

            lbl_msg = MyLabel(text='', size_hint=(1, 5))
            Gui_utils.adapt_text_multiline(lbl_msg, Gui_utils.FONT_SIZE_MEDIUM)
            ly_page.add_widget(lbl_msg)

            ly_btns = BoxLayout(orientation='horizontal', spacing=Gui_utils.SPACING_MEDIUM, padding=0)

            btn_yes = SelectableButton(text='SÌ')
            Gui_utils.adapt_text(btn_yes, Gui_utils.FONT_SIZE_LARGE)
            btn_yes.bind(on_press=self.on_btn_yes)
            ly_btns.add_widget(btn_yes)

            btn_no = SelectableButton(text='NO')
            Gui_utils.adapt_text(btn_no, Gui_utils.FONT_SIZE_LARGE)
            btn_no.bind(on_press=self.on_btn_no)
            ly_btns.add_widget(btn_no)

            ly_page.add_widget(ly_btns)

            scr = Screen(name='confirm_dialog')
            scr.add_widget(ly_page)

            self.select_widget(btn_no)

            self.clbk_on_yes : Optional[Callable[[],None]] = None
            self.clbk_on_no : Optional[Callable[[],None]] = None
            self.type : Optional[str] = None # to know what data is being asked, and how to validate the input
            self.lbl_msg = lbl_msg
            self.btn_yes = btn_yes
            self.btn_no = btn_no
            self.gui_root = scr

        def on_btn_yes(self, *args):
            gui = self.gui
            gui.pop_screen(self)
            clbk = self.clbk_on_yes
            if clbk is not None:
                clbk()

        def on_btn_no(self, *args):
            gui = self.gui
            gui.pop_screen(self)
            clbk = self.clbk_on_no
            if clbk is not None:
                clbk()

        def handle_keypress(self, key):
            s = self.selected_widget
            if key in [KEY_CLEAR, KEY_EXIT]:
                self.select_widget(self.btn_no)
                self.btn_no.trigger_action()
            elif s is None:
                if key in [KEY_OK, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT]:
                    self.select_widget(self.btn_no)
            elif key in [KEY_OK, KEY_ENTER]:
                s.trigger_action()
            elif key == KEY_LEFT:
                if s == self.btn_no:
                    self.select_widget(self.btn_yes)
            elif key == KEY_RIGHT:
                if s == self.btn_yes:
                    self.select_widget(self.btn_no)

    class Screen_keypad(MyScreen):
        def __init__(self, gui : 'Gui'):
            super().__init__(gui)

            ly_page = BoxLayout(orientation='vertical', spacing=Gui_utils.SPACING_SMALL, padding=sp(80))

            lbl_header = MyLabel(text='', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl_header, Gui_utils.FONT_SIZE_MEDIUM)
            ly_page.add_widget(lbl_header)

            txt_input = SelectableTextInput(text='', size_hint=(1, 1.2))
            Gui_utils.adapt_text(txt_input, Gui_utils.FONT_SIZE_HUGE)
            ly_page.add_widget(txt_input)

            def make_btn(text : str, on_click):
                btn = SelectableButton(text=text)
                Gui_utils.adapt_text(btn, Gui_utils.FONT_SIZE_LARGE)
                btn.bind(on_press=on_click)
                return btn

            btn_nums = [make_btn(str(i), self.on_btn_keypad) for i in range(10)]
            btn_dot = make_btn(DECIMAL_POINT_CHAR, self.on_btn_keypad)
            btn_clear = make_btn('C', self.on_btn_keypad)
            btn_ok = make_btn('OK', self.on_btn_ok)
            btn_exit = make_btn('ESCI', self.on_btn_exit)

            ly_pad = GridLayout(
                size_hint=(1,5),
                spacing=Gui_utils.SPACING_SMALL,
                padding=Gui_utils.SPACING_SMALL
            )
            ly_pad.cols = 6

            ly_pad.add_widget(Spacer())
            ly_pad.add_widget(btn_nums[1])
            ly_pad.add_widget(btn_nums[2])
            ly_pad.add_widget(btn_nums[3])
            ly_pad.add_widget(Spacer())
            ly_pad.add_widget(btn_ok)

            ly_pad.add_widget(Spacer())
            ly_pad.add_widget(btn_nums[4])
            ly_pad.add_widget(btn_nums[5])
            ly_pad.add_widget(btn_nums[6])
            ly_pad.add_widget(Spacer())
            ly_pad.add_widget(Spacer())

            ly_pad.add_widget(Spacer())
            ly_pad.add_widget(btn_nums[7])
            ly_pad.add_widget(btn_nums[8])
            ly_pad.add_widget(btn_nums[9])
            ly_pad.add_widget(Spacer())
            ly_pad.add_widget(Spacer())

            ly_pad.add_widget(Spacer())
            ly_pad.add_widget(btn_dot)
            ly_pad.add_widget(btn_nums[0])
            ly_pad.add_widget(btn_clear)
            ly_pad.add_widget(Spacer())
            ly_pad.add_widget(btn_exit)

            ly_page.add_widget(ly_pad)

            lbl_error = MyLabel(text='', size_hint=(1, 1))
            Gui_utils.adapt_text(lbl_error, Gui_utils.FONT_SIZE_MEDIUM)
            lbl_error.color = GUI_TEXT_COLOR_ERROR
            ly_page.add_widget(lbl_error)

            scr = Screen(name='keypad')
            scr.add_widget(ly_page)

            self.default_button = btn_ok
            self.select_widget(self.default_button)

            self.lbl_header = lbl_header
            self.txt_input = txt_input
            self.btn_nums = btn_nums
            self.btn_dot = btn_dot
            self.btn_clear = btn_clear
            self.btn_ok = btn_ok
            self.btn_exit = btn_exit
            self.lbl_error = lbl_error
            self.gui_root = scr

            self.clbk_on_confirm : Optional[Callable[[str], bool]] = None

        def on_btn_keypad(self, btn, *args):
            if btn == self.btn_clear:
                self.txt_input.clbk_on_input(self.txt_input, Keycode.BACKSPACE, None)
            else:
                self.txt_input.clbk_on_input(self.txt_input, btn.text, None)

        def on_btn_ok(self, *args):
            gui = self.gui
            if self.clbk_on_confirm:
                can_close = self.clbk_on_confirm(self.txt_input.text)
            else:
                can_close = True
            if can_close:
                gui.pop_screen(self)
            else:
                self.on_btn_exit

        def on_btn_exit(self, *args):
            gui = self.gui
            gui.pop_screen(self)

        def handle_keypress(self, key):
            s = self.selected_widget
            if key in [KEY_CLEAR, KEY_EXIT]:
                self.select_widget(self.btn_exit)
                self.btn_exit.trigger_action()
            elif s is None:
                if key in [KEY_OK, KEY_ENTER, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT]:
                    self.select_widget(self.default_button)
            elif key in [KEY_OK, KEY_ENTER]:
                s.trigger_action()
            elif key == KEY_LEFT:
                if s == self.btn_ok:
                    self.select_widget(self.btn_nums[3])
                elif s == self.btn_nums[3]:
                    self.select_widget(self.btn_nums[2])
                elif s == self.btn_nums[2]:
                    self.select_widget(self.btn_nums[1])
                elif s == self.btn_nums[6]:
                    self.select_widget(self.btn_nums[5])
                elif s == self.btn_nums[5]:
                    self.select_widget(self.btn_nums[4])
                elif s == self.btn_nums[9]:
                    self.select_widget(self.btn_nums[8])
                elif s == self.btn_nums[8]:
                    self.select_widget(self.btn_nums[7])
                elif s == self.btn_exit:
                    self.select_widget(self.btn_clear)
                elif s == self.btn_clear:
                    self.select_widget(self.btn_nums[0])
                elif s == self.btn_nums[0]:
                    self.select_widget(self.btn_dot)
            elif key == KEY_RIGHT:
                if s == self.btn_nums[1]:
                    self.select_widget(self.btn_nums[2])
                elif s == self.btn_nums[2]:
                    self.select_widget(self.btn_nums[3])
                elif s == self.btn_nums[3]:
                    self.select_widget(self.btn_ok)
                elif s == self.btn_nums[4]:
                    self.select_widget(self.btn_nums[5])
                elif s == self.btn_nums[5]:
                    self.select_widget(self.btn_nums[6])
                elif s == self.btn_nums[6]:
                    self.select_widget(self.btn_ok)
                elif s == self.btn_nums[7]:
                    self.select_widget(self.btn_nums[8])
                elif s == self.btn_nums[8]:
                    self.select_widget(self.btn_nums[9])
                elif s == self.btn_nums[9]:
                    self.select_widget(self.btn_exit)
                elif s == self.btn_dot:
                    self.select_widget(self.btn_nums[0])
                elif s == self.btn_nums[0]:
                    self.select_widget(self.btn_clear)
                elif s == self.btn_clear:
                    self.select_widget(self.btn_exit)
            elif key == KEY_UP:
                if s == self.btn_dot:
                    self.select_widget(self.btn_nums[7])
                elif s == self.btn_nums[7]:
                    self.select_widget(self.btn_nums[4])
                elif s == self.btn_nums[4]:
                    self.select_widget(self.btn_nums[1])
                elif s == self.btn_nums[0]:
                    self.select_widget(self.btn_nums[8])
                elif s == self.btn_nums[8]:
                    self.select_widget(self.btn_nums[5])
                elif s == self.btn_nums[5]:
                    self.select_widget(self.btn_nums[2])
                elif s == self.btn_clear:
                    self.select_widget(self.btn_nums[9])
                elif s == self.btn_nums[9]:
                    self.select_widget(self.btn_nums[6])
                elif s == self.btn_nums[6]:
                    self.select_widget(self.btn_nums[3])
                elif s == self.btn_exit:
                    self.select_widget(self.btn_ok)
            elif key == KEY_DOWN:
                if s == self.btn_nums[1]:
                    self.select_widget(self.btn_nums[4])
                elif s == self.btn_nums[4]:
                    self.select_widget(self.btn_nums[7])
                elif s == self.btn_nums[7]:
                    self.select_widget(self.btn_dot)
                elif s == self.btn_nums[2]:
                    self.select_widget(self.btn_nums[5])
                elif s == self.btn_nums[5]:
                    self.select_widget(self.btn_nums[8])
                elif s == self.btn_nums[8]:
                    self.select_widget(self.btn_nums[0])
                elif s == self.btn_nums[3]:
                    self.select_widget(self.btn_nums[6])
                elif s == self.btn_nums[6]:
                    self.select_widget(self.btn_nums[9])
                elif s == self.btn_nums[9]:
                    self.select_widget(self.btn_clear)
                elif s == self.btn_ok:
                    self.select_widget(self.btn_exit)
            elif key == KEY_DOT:
                self.btn_dot.trigger_action()
            elif Cec.key_is_numeric(key):
                self.btn_nums[Cec.key_to_digit(key)].trigger_action()

    class Screen_shutdown(MyScreen):
        def __init__(self, gui : 'Gui'):
            super().__init__(gui)

            ly_page = BoxLayout(orientation='vertical', spacing=Gui_utils.SPACING_SMALL, padding=sp(150))

            lbl_msg = MyLabel(text='Spegnimento...', size_hint=(1, 5))
            Gui_utils.adapt_text_multiline(lbl_msg, Gui_utils.FONT_SIZE_MEDIUM)
            ly_page.add_widget(lbl_msg)

            scr = Screen(name='shutdown')
            scr.add_widget(ly_page)

            self.lbl_msg = lbl_msg
            self.gui_root = scr

        def handle_keypress(self, key):
            pass

    # end of GUI screens

    def __init__(self, sicurphone):
        super().__init__()
        self._sicurphone = sicurphone # for DBG only

        self.lock = threading.RLock()

        self.evt_queue : Optional[Queue] = None
        self.tts_queue : Optional[Queue] = None
        self.conn_status : Gui.Conn_status = Gui.Conn_status.NONE
        self.last_covid_send : Optional[datetime.datetime] = None
        self.sched_covid : Optional[datetime.datetime] = None
        self.num_pending_out_msg : int = 0

        self.screen_is_active : bool = False
        self.last_screen_activation_tick : Optional[float] = None

        self.scr_main : Gui.Screen_main = None
        self.scr_emg : Gui.Screen_emg = None
        self.scr_msg : Gui.Screen_msg = None
        self.scr_send_outcome : Gui.Screen_send_outcome = None
        self.scr_req_data : Gui.Screen_req_data = None
        self.scr_confirm_dialog : Gui.Screen_confirm_dialog = None
        self.scr_keypad : Gui.Screen_keypad = None
        self.scr_shutdown : Gui.Screen_shutdown = None
        self.screens_stack : List[MyScreen] = []

        self.screen_manager : ScreenManager = None
        self.gui_root = None

        now = get_tick()
        self.last_screen_change_tick = now
        self.last_user_interaction_tick = now

        self.item_queue : List[Item] = []
        self.lock = threading.RLock()
        self.is_showing_msg : bool = False
        self.is_showing_req_data : bool = False
        self.is_showing_emg : bool = False
        self.last_shutdown_attempt_tick : Optional[float] = None
        self.is_shutting_down : bool = False

    def init(self, evt_queue : Queue, tts_queue : Queue):
        self.evt_queue = evt_queue
        self.tts_queue = tts_queue
        threading.current_thread().name = "gui"
        self.run()

    def build(self):
        self.title = '{} v{}'.format(PROG_NAME, PROG_VERSION)
        self.icon = 'icon.png'

        #Window.maximize()
        Window.bind(on_request_close=self.on_request_close)

        class Last_key:
            modifiers = None
            text = None
            t = None
        def on_key_down(obj, keyboard, keycode, text, modifiers):
            # modifiers is an array of (zero or more) strings among ['alt', 'ctrl', 'shift', 'capslock']
            # 'text' is the presed key
            # note that an event is fired on pressing control keys alone, too
            # the test is NOT influenced by shift/capslock (it's always the lowercase character)
            # keycodes are available in kivy.core.window.Keyboard.keycodes (it's a Dict[str, int])
            # however, codes are trickier because of different keyboard layouts; using text is easier

            # Kivy bug: on some platforms / graphics providers, key presses are triggered twice (with different codes, and codes that are no longer unique)
            # just work around it by rejecting the second event
            now = get_tick()
            if Last_key.t is not None and text is not None:
                if (Last_key.text == text
                        and Last_key.modifiers == modifiers
                        and now - Last_key.t < 0.05):
                    lg.d("DBG: reject duplicate key down: {} (code {}) {}".format(repr(text), keycode, modifiers))
                    return True
            Last_key.text = text
            Last_key.modifiers = modifiers
            Last_key.t = now

            lg.d("key down: {} (code {}), {}".format(repr(text), keycode, modifiers))
            if 'alt' not in modifiers and 'ctrl' not in modifiers:
                if text == 'e':
                    lg.i("DBG: simulate EMG press")
                    if self.evt_queue is not None:
                        evt = Gpio.evt_t(type=Gpio.Evt_type.EMG)
                        q_put_data(self.evt_queue, evt)
                    return True
                else:
                    kmap = {
                        '0': KEY_0,
                        '1': KEY_1,
                        '2': KEY_2,
                        '3': KEY_3,
                        '4': KEY_4,
                        '5': KEY_5,
                        '6': KEY_6,
                        '7': KEY_7,
                        '8': KEY_8,
                        '9': KEY_9,
                        '.': KEY_DOT,
                        ' ': KEY_OK,
                        'c': KEY_CLEAR,
                        'a': KEY_LEFT,
                        'd': KEY_RIGHT,
                        'w': KEY_UP,
                        's': KEY_DOWN,
                    }
                    ceckey = kmap.get(text, None)
                    if ceckey is not None:
                        if self.evt_queue is not None:
                            lg.i("DBG: simulate key press {}->{}".format(text, ceckey))
                            evt = Cec.evt_t(type=Cec.Evt_type.KEYPRESS, data=Cec.keypress_t(key=ceckey, duration=0))
                            q_put_data(self.evt_queue, evt)
                        return True
                return False
        # TODO: dbg
        Window.bind(on_key_down=on_key_down)

        self.scr_main = Gui.Screen_main(self)
        self.scr_emg = Gui.Screen_emg(self)
        self.scr_msg = Gui.Screen_msg(self)
        self.scr_send_outcome = Gui.Screen_send_outcome(self)
        self.scr_req_data = Gui.Screen_req_data(self)
        self.scr_confirm_dialog = Gui.Screen_confirm_dialog(self)
        self.scr_keypad = Gui.Screen_keypad(self)
        self.scr_shutdown = Gui.Screen_shutdown(self)

        sm = ScreenManager(transition=NoTransition())
        sm.add_widget(self.scr_main.gui_root)
        sm.add_widget(self.scr_emg.gui_root)
        sm.add_widget(self.scr_msg.gui_root)
        sm.add_widget(self.scr_send_outcome.gui_root)
        sm.add_widget(self.scr_req_data.gui_root)
        sm.add_widget(self.scr_confirm_dialog.gui_root)
        sm.add_widget(self.scr_keypad.gui_root)

        self.screens_stack = [self.scr_main]

        self.screen_manager = sm
        self.gui_root = sm

        return self.gui_root

    def get_safe_time_for_tts(self):
        # TTS should be played immediately, except when the TV is just switching source (in which case we should introduce a delay)
        with lock_acquire(self.lock, timeout=5) as acquired:
            if acquired:
                is_active = self.screen_is_active
                last_activation_tick = self.last_screen_activation_tick
            else:
                is_active = True
                last_activation_tick = 0

        now = get_tick()
        if not is_active or last_activation_tick is None:
            last_activation_tick = get_tick()

        t = max(now, last_activation_tick) + 1.5
        lg.d("Safe time for TTS: {} / {} (act={}, {})".format(t, now, is_active, last_activation_tick))
        return t

    def launch_tts(self, txt : str, priority : Tts.Priority):
        if self.tts_queue is None:
            return
        lg.d("Launch TTS ({}): {}".format(priority, txt))
        txt += " . " # extra pause
        t = self.get_safe_time_for_tts()
        msg = Tts.Msg(txt, priority, t)
        q_put(self.tts_queue, msg)

    def get_current_screen_name(self) -> str:
        n = self.screens_stack[-1].gui_root.name
        assert(n == self.screen_manager.current)
        return n

    def show_top_screen(self):
        stk = self.screens_stack
        scr = stk[-1]
        self.screen_manager.switch_to(scr.gui_root)
        self.last_screen_change_tick = get_tick()

        if len(stk) <= 1:
            self.deactivate_screen()

    def push_screen(self, scr : MyScreen):
        lg.d("push screen {}".format(scr))
        if scr is None:
            return
        stk = self.screens_stack
        N = len(stk)
        i = 0
        while i < N:
            if stk[i] != scr:
                i += 1
                continue
            if i == N - 1:
                return
            else:
                stk.pop(i)
                N -= 1
        stk.append(scr)
        self.show_top_screen()

    def pop_screen(self, scr : Optional[MyScreen] = None):
        lg.d("pop screen {}".format(scr))
        stk = self.screens_stack
        N = len(stk)
        if scr is None and N > 0:
            scr = stk[-1]
        i = 0
        while i < N:
            if stk[i] != scr:
                i += 1
                continue
            stk.pop(i)
            if scr == self.scr_emg:
                self.is_showing_emg = False
            elif scr == self.scr_msg:
                self.is_showing_msg = False
            elif scr == self.scr_req_data:
                self.is_showing_req_data = False
            N -= 1
            if i == N:
                self.show_top_screen_or_enqueued()

    def show_top_screen_or_enqueued(self):
        any_new = self.show_next_item_if_idle()
        if not any_new:
            self.show_top_screen()

    def show_next_item_if_idle(self) -> bool:
        with lock_acquire(self.lock) as acquired:
            for item in self.item_queue:
                if item.type == Item.Type.REQ_DATA:
                    if not self.is_showing_msg:
                        old_item = self.scr_req_data.item
                        if not self.is_showing_req_data or old_item is not item:
                            lg.d("show next: req data: {}".format(item))
                            if old_item is not None and old_item in self.item_queue:
                                self.item_queue.remove(old_item)
                            self.show_screen_req_data(item)
                            return True
                elif item.type == Item.Type.MSG:
                    if not self.is_showing_msg:
                        lg.d("show next: msg: {}".format(item))
                        self.show_screen_msg(item)
                        return True
            return False

    def close_modals(self, show_new_top=True):
        stk = self.screens_stack
        N = len(stk)
        popped = False
        i = N-1
        while i > 0:
            s = stk[i]
            if s == self.scr_confirm_dialog or s == self.scr_send_outcome:
                lg.d("close modals: {}".format(s))
                stk.pop(i)
                popped = True
            i -= 1
        if popped and show_new_top:
            self.show_top_screen()

    def activate_screen(self):
        lg.d("Activate screen")
        evt = Gui.evt_t(type=Gui.Evt_type.SCREEN_ACTIVATE, data=None)
        q_put_data(self.evt_queue, evt)

    def deactivate_screen(self):
        lg.d("Deactivate screen")
        evt = Gui.evt_t(type=Gui.Evt_type.SCREEN_DEACTIVATE, data=None)
        q_put_data(self.evt_queue, evt)

    def datetime_to_str(self, dt : Optional[datetime.datetime], if_none : str = "") -> str:
        if dt is None:
            return if_none
        else:
            wdays = ["", "lun", "mar", "mer", "gio", "ven", "sab", "dom"]
            return dt.strftime("%H:%M:%S") + " " + wdays[dt.isoweekday()] + " " + dt.strftime("%d/%m/%Y")

    def show_screen_send_outcome(self, msg : str):
        def fn_show_screen_send_outcome():
            scr = self.scr_send_outcome
            scr.lbl_msg.text = msg
            self.close_modals(show_new_top=False)
            self.push_screen(scr)
        Gui_utils.run_gui_code(fn_show_screen_send_outcome)

    def show_screen_confirm(self, msg : str, on_confirm : Optional[Callable[[],None]]):
        def fn_show_screen_confirm():
            scr = self.scr_confirm_dialog
            scr.clbk_on_no = None
            scr.clbk_on_yes = on_confirm
            scr.lbl_msg.text = msg
            scr.select_widget(scr.btn_no)
            self.close_modals(show_new_top=False)
            self.push_screen(scr)
        Gui_utils.run_gui_code(fn_show_screen_confirm)

    def show_screen_emg(self, ok : bool, dt : Optional[datetime.datetime]):
        def fn_show_screen_emg():
            scr = self.scr_emg
            lbl = scr.lbl_msg
            t_str = ""
            if dt is not None:
                t_str = "\n({})".format(self.datetime_to_str(dt))
            if ok:
                lbl.text = 'La tua richiesta d’aiuto è stata inviata.{}'.format(t_str)
                lbl.color = GUI_TEXT_COLOR_DEFAULT
                self.launch_tts('la tua richiesta d’aiuto è stata inviata.', Tts.Priority.EMERGENCY)
            else:
                lbl.text = 'Errore.\nLa richiesta d’aiuto non è stata inviata.{}'.format(t_str)
                lbl.color = GUI_TEXT_COLOR_ERROR
                self.launch_tts('errore. la tua richiesta d’aiuto non è stata inviata.', Tts.Priority.EMERGENCY)
            self.close_modals(show_new_top=False)
            self.push_screen(scr)
            self.activate_screen()
        Gui_utils.run_gui_code(fn_show_screen_emg)

    def req_data_assign_and_activate(self, item : Item, was_showing : bool):
        if was_showing:
            self.scr_req_data.reassign_item(item)
        else:
            self.scr_req_data.assign_item(item)
        self.activate_screen()

        tts_hint = self.scr_req_data.get_tts_hint_for_source(item.source)
        if tts_hint is not None and tts_hint != "":
            self.launch_tts(tts_hint, Tts.Priority.INFORMATIVE)

    def show_screen_req_data(self, item : Item):
        was_showing = self.is_showing_req_data
        self.is_showing_req_data = True
        def fn_show_req_data_screen():
            self.close_modals(show_new_top=False)
            self.push_screen(self.scr_req_data)
            self.req_data_assign_and_activate(item, was_showing)
        Gui_utils.run_gui_code(fn_show_req_data_screen)

    def show_screen_msg(self, item : Item):
        self.is_showing_msg = True
        def fn_show_screen_msg():
            txt = item.req.get("msg", "<messaggio vuoto>")
            self.close_modals(show_new_top=False)
            self.scr_msg.lbl_msg.text = txt
            self.scr_msg.item = item
            self.push_screen(self.scr_msg)
            self.activate_screen()
            self.launch_tts("messaggio. . {}".format(txt), Tts.Priority.INFORMATIVE)
        Gui_utils.run_gui_code(fn_show_screen_msg)

    def show_screen_keypad(
            self,
            hint : str,
            initial_text : str,
            on_input : Callable[[SelectableTextInput, Union[Keycode, str], Optional[int]], None],
            on_confirm : Callable[[str], bool]
            ):
        scr = self.scr_keypad
        scr.clbk_on_confirm = on_confirm
        scr.txt_input.clbk_on_input = on_input
        scr.txt_input.text = initial_text if initial_text is not None else ""
        scr.lbl_header.text = hint
        scr.lbl_error.text = ""
        scr.select_widget(scr.default_button)
        self.push_screen(scr)

    def dequeue_item(self, item : Item):
        lg.d("gui dequeue item {}".format(item))
        with lock_acquire(self.lock, timeout=0.5) as acquired:
            i = 0
            while i < len(self.item_queue):
                if item is self.item_queue[i]:
                    del self.item_queue[i]
                else:
                    i += 1
            lg.d("new gui item queue: {}".format(self.item_queue))
        self.show_next_item_if_idle()

    def req_types_are_compatible(self, item1 : Item, item2 : Item) -> bool:
        def get_type(it : Item) -> str:
            if it.req is None:
                return ""
            r = it.req
            if r is None:
                return ""
            t = r.get("type", None)
            if t is None or t == "":
                return ""
            return t

        t1 = get_type(item1)
        t2 = get_type(item2)

        return t1 == t2 and not t1 == ""

    def merge_item(self, item : Item, items : List[Item]) -> bool:
        assert(item.outcome == Item.Outcome.PENDING)
        lg.d("Merge item {} with {}".format(item, items))
        if item.type != Item.Type.REQ_DATA:
            lg.d("Non-mergeable type")
            self.item_queue.append(item)
            return False
        s_new = item.source
        for [i, it] in enumerate(items):
            if it != Item.Type.REQ_DATA:
                continue
            if not (it.outcome == Item.Outcome.PENDING):
                continue
            if not self.req_types_are_compatible(item, it):
                continue

            s_old = it.source
            if ((s_old == Item.Source.USER)
                    or (s_old == Item.Source.SCHEDULE and s_new in [Item.Source.SCHEDULE, Item.Source.SERVER])
                    or (s_old == Item.Source.SERVER and s_new == Item.Source.SERVER)
                    ):
                lg.d("merge {} and *{}".format(it, item))
                it.outcome = Item.Outcome.MERGED
                if item.deadline_tick is None and not it.timed_out:
                    item.deadline_tick = it.deadline_tick
                    item.timed_out = it.timed_out
                self.item_queue[i] = item
                self.req_data_assign_and_activate(item, was_showing=True)
                return True
            else:
                lg.d("merge *{} and {}".format(it, item))
                item.outcome = Item.Outcome.MERGED
                if it.deadline_tick is None:
                    it.deadline_tick = item.deadline_tick
                    it.timed_out = item.timed_out
                self.req_data_assign_and_activate(item, was_showing=True)
                return True

        lg.d("Nothing to merge, just add the new item")
        self.item_queue.append(item)
        return False

    def enqueue_item(self, item : Item):
        # TODO: handle types different from "type"=="covid"; for now, everything is covid (then, remove this block)
        if item.type == Item.Type.REQ_DATA:
            if item.req is None:
                item.req = {}
            t = item.req.get("type", None)
            if t != "covid":
                item.req['type'] = "covid"

        lg.d("gui enqueue item {}".format(item))
        locked = False
        with lock_acquire(self.lock, timeout=0.5) as acquired:
            self.merge_item(item, self.item_queue)
            lg.d("new gui item queue: {}".format(self.item_queue))
        shown = self.show_next_item_if_idle()
        if not shown:
            #activate anyway, even if the new item is now showing right now (so that we can get the user's attention if they changed video source)
            self.activate_screen()

    def on_request_close(self, *args, **kwargs):
        lg.w("Requested app close by GUI.")
        Gui_utils.close_gui()

    def handle_activation(self, activation : Cec.activation_t, evt_tick: float):
        self.change_screen_active_status(is_active=activation.activated, tick=evt_tick)

    def handle_keypress(self, keypress: Cec.keypress_t, evt_tick: float):
        self.last_user_interaction_tick = get_tick()
        # ignore key releases
        if keypress.duration != 0:
            return None
        if evt_tick - self.last_screen_change_tick < Gui.DEAD_TIME_AFTER_SCREEN_CHANGE:
            lg.i("Drop old keypress: {}".format(keypress.key))
            return None

        if self.is_shutting_down:
            lg.i("We're shutting down, drop keypress: {}".format(keypress.key))
            return None

        k = keypress.key

        def fn_handle_keypress():
            self.screens_stack[-1].handle_keypress(k)

            return
            #TODO dbg, remove
            if self.screens_stack[-1] != self.scr_main:
                return
            if keypress.key == KEY_0:
                lg.w("********DBG info********")
                lg.w("item queue: {}".format(self._sicurphone.item_queue))
                lg.w("gui item queue: {}".format(self.item_queue))
                lg.w("tts queue: {}".format(self._sicurphone.tts.msg_queue))
                lg.w("iot queue: {}".format(self._sicurphone.iot.out_msg_queue))
                lg.w("iot queue idx: {}".format(self._sicurphone.iot.out_msg_queue_idx))
                lg.w("********END DBG info********")
            if keypress.key == KEY_1:
                lg.w("DBG: go to root")
                self.screen_manager.switch_to(self.scr_main.gui_root)
            elif keypress.key == KEY_2:
                lg.w("DBG: emg error")
                self.show_screen_emg(False, datetime.datetime.now())
            elif keypress.key == KEY_3:
                lg.w("DBG: long msg")
                it = Item(source=Item.Source.SERVER, type=Item.Type.MSG, req={
                    "cmd": "msg",
                    "id": 1,
                    "msg": "Messaggio lungo\nsu più righe\n12345 67890\nabcdefghijklmno pqrstuvwxyz ABCDEFGHIJKLMNO PQRSTUVWXYZ abcdefghijklmno pqrstuvwxyz ABCDEFGHIJKLMNO PQRSTUVWXYZ.\n Altre righe ancora...\nFine."
                })
                self.enqueue_item(it)
            elif keypress.key == KEY_4:
                lg.w("DBG: server req data")
                it = Item(source=Item.Source.SERVER, type=Item.Type.REQ_DATA, req={
                    "cmd": "req_data",
                    "id": 2,
                    "type": "covid",
                    "tout": 10, # [s]
                })
                self.enqueue_item(it)
            elif keypress.key == KEY_5:
                lg.w("DBG: multiple msgs and reqs")
                self.enqueue_item(Item(source=Item.Source.SERVER, type=Item.Type.REQ_DATA, req={
                    "cmd": "req_data",
                    "id": 11,
                    "type": "covid",
                    "tout": 10, # [s]
                }))
                self.enqueue_item(Item(source=Item.Source.SERVER, type=Item.Type.MSG, req={
                    "cmd": "msg",
                    "id": 12,
                    "msg": "Messaggio 12."
                }))
                self.enqueue_item(Item(source=Item.Source.SERVER, type=Item.Type.MSG, req={
                    "cmd": "msg",
                    "id": 12,
                    "msg": "Messaggio 13."
                }))
                self.enqueue_item(Item(source=Item.Source.SERVER, type=Item.Type.MSG, req={
                    "cmd": "msg",
                    "id": 12,
                    "msg": "Messaggio 14."
                }))
                self.enqueue_item(Item(source=Item.Source.USER, type=Item.Type.REQ_DATA, req=None))
                self.enqueue_item(Item(source=Item.Source.SERVER, type=Item.Type.REQ_DATA, req={
                    "cmd": "req_data",
                    "id": 5678,
                    "type": "covid",
                    "tout": 10, # [s]
                    "msg": "Messaggio lungo\nsu più righe\n12345 67890\nabcdefghijklmno pqrstuvwxyz ABCDEFGHIJKLMNO PQRSTUVWXYZ abcdefghijklmno pqrstuvwxyz ABCDEFGHIJKLMNO PQRSTUVWXYZ.\n Altre righe ancora...\nFine."
                }))
                self.enqueue_item(Item(source=Item.Source.SERVER, type=Item.Type.MSG, req={
                    "cmd": "msg",
                    "id": 12,
                    "msg": "Messaggio 15."
                }))
                self.enqueue_item(Item(source=Item.Source.SERVER, type=Item.Type.MSG, req={
                    "cmd": "msg",
                    "id": 12,
                    "msg": "Messaggio 16."
                }))
                self.enqueue_item(Item(source=Item.Source.SCHEDULE, type=Item.Type.REQ_DATA, req=None))
            elif keypress.key == KEY_6:
                t_str = (datetime.datetime.now() + datetime.timedelta(seconds=5)).strftime("%H:%M:%S")
                lg.w("DBG: schedule: will trigger in a few seconds ({})".format(t_str))
                self._sicurphone.handle_iot_sched_covid({"cmd": "sched_covid", "time": t_str})
            elif keypress.key == KEY_7:
                lg.w("DBG: fill in covid data automatically")
                it = Item(source=Item.Source.USER, type=Item.Type.REQ_DATA)
                self.show_screen_req_data(it)
                scr = self.scr_req_data
                scr.values = {
                    'temperatura': 1,
                    'pressione min': 2,
                    'pressione max': 3,
                    'pulsazioni': 4,
                    'ossigeno': 5
                }
                scr.txt_temperature.text = str(scr.values['temperatura'])
                scr.txt_pressure_min.text = str(scr.values['pressione min'])
                scr.txt_pressure_max.text = str(scr.values['pressione max'])
                scr.txt_pulses.text = str(scr.values['pulsazioni'])
                scr.txt_oxygen.text = str(scr.values['ossigeno'])
                scr.select_widget(scr.btn_send)
            elif keypress.key == KEY_8:
                lg.i("DBG: simulate EMG press")
                if self.evt_queue is not None:
                    evt = Gpio.evt_t(type=Gpio.Evt_type.EMG)
                    q_put_data(self.evt_queue, evt)
            elif keypress.key == KEY_9:
                lg.w("DBG: cycle screens (will disrupt the logic; only for graphical checks)")
                ss = [
                    self.scr_main,
                    self.scr_emg,
                    self.scr_msg,
                    self.scr_send_outcome,
                    self.scr_req_data,
                    self.scr_confirm_dialog,
                    self.scr_keypad,
                ]
                idx = index_or(ss, self.screens_stack[-1], None)
                if idx is None:
                    idx = 0
                else:
                    idx += 1
                s = ss[idx]
                lg.w("DBG: cycle to {}: {}".format(idx, s))
                self.screen_manager.switch_to(s)
        Gui_utils.run_gui_code(fn_handle_keypress)

    def startup_done(self):
        def fn_startup_done():
            self.scr_main.lbl_info.text = 'Non hai alcun messaggio'
            self.scr_main.select_default_widget()
        Gui_utils.run_gui_code(fn_startup_done)

    def change_connection_status(self, status : 'Gui.Conn_status'):
        with self.lock:
            if self.conn_status == status:
                return
            self.conn_status = status
            lg.d("GUI: change conn status: {}".format(status))

            Gui_utils.run_gui_code(self.update_main_scr_debug_info)

    def change_last_covid_send(self, dt : Optional[datetime.datetime]):
        with self.lock:
            if self.last_covid_send == dt:
                return
            self.last_covid_send = dt
            lg.d("GUI: change last covid send: {}".format(dt))

            Gui_utils.run_gui_code(self.update_main_scr_send_info)

    def change_sched_covid(self, dt : Optional[datetime.datetime]):
        with self.lock:
            if self.sched_covid == dt:
                return
            self.sched_covid = dt
            lg.d("GUI: change sched covid: {}".format(dt))

            Gui_utils.run_gui_code(self.update_main_scr_send_info)

    def change_num_pending_out_msg(self, num):
        with self.lock:
            if self.num_pending_out_msg == num:
                return
            self.num_pending_out_msg = num
            lg.d("GUI: change num pending out msg: {}".format(num))

            Gui_utils.run_gui_code(self.update_main_scr_debug_info)

    def update_main_scr_send_info(self):
        dt_send_str = self.datetime_to_str(self.last_covid_send, if_none="mai")
        txt = "Ultimo invio: {}".format(dt_send_str)
        if self.sched_covid is not None:
            dt_sched_local = self.sched_covid.astimezone()
            if dt_sched_local.second == 0:
                dt_sched_str = dt_sched_local.strftime("%H:%M")
            else:
                dt_sched_str = dt_sched_local.strftime("%H:%M:%S")
            txt += "\nInvio giornaliero: {}".format(dt_sched_str)
        self.scr_main.lbl_last_send.text = txt

    def change_screen_active_status(self, is_active : bool, tick : float):
        lg.d("GUI: change screen active status: {} @{}".format(is_active, tick))
        with self.lock:
            self.screen_is_active = is_active
            if is_active:
                self.last_screen_activation_tick = tick

    def update_main_scr_debug_info(self):
        pending_msg = ""
        n = self.num_pending_out_msg
        if n == 1:
            pending_msg = " (1 notif. in coda)"
        elif n > 1:
            pending_msg = " ({} notif. in coda)".format(n)
        self.scr_main.lbl_status.text = "{}{}\n{} v{}".format(
            self.conn_status_to_string(self.conn_status),
            pending_msg,
            PROG_NAME,
            PROG_VERSION
        )

    def conn_status_to_string(self, conn_status : 'Gui.Conn_status') -> str:
        return {
            Gui.Conn_status.NONE: "NON connesso",
            Gui.Conn_status.OK: "Connesso"
        }.get(conn_status, "")


################################

def exit_if_exc_during_cb() -> None:
    global iot
    if iot is None:
        return

    if len(iot.exc_during_cb) == 0:
        return

    lg.e("Exceptions inside callback:")
    for e in iot.exc_during_cb:
        lg.xx(e)

    lg.e("Exit after delayed exceptions")
    exit(-99)

class Sicurphone:
    SETTINGS_FILENAME = 'sicurphone_settings.json'

    def __init__(self):
        GPIO.setwarnings(False)
        GPIO.setmode(GPIO.BCM)

    def __del__(self):
        GPIO.cleanup()

    def run(self):
        global iot

        if not Cmd.is_root():
            lg.e("The application needs sudo privileges. Some things won't work")

        self.prev_datetime = get_datetime()
        self.last_covid_data_sent_dt: Optional[datetime.datetime] = None
        self.last_covid_data_request_dt: Optional[datetime.datetime] = None # when we asked the user for covid data (by remote request or schedule)
        self.sched_covid : Optional[datetime.datetime] = None
        self.last_emg_event_tick = None

        self.tts_queue = Queue()

        iot = Iot()
        self.iot = iot
        cec = Cec()
        self.cec = cec
        gpio = Gpio()
        self.gpio = gpio
        gui = Gui(self)
        self.gui = gui
        tts = Tts(self.tts_queue)
        self.tts = tts

        self.item_queue : List[Item] = []
        self.shared_queue = Queue()
        self.last_shutdown_attempt_tick = None

        self.settings = self.load_settings()

        self.work_thread = Stoppable_thread(self.work_run, shutdown_on_exception=True, name="work")
        self.work_thread.daemon = True

        self.wdg_thread = Stoppable_thread(self.wdg_run, shutdown_on_exception=True, name="wdg")
        self.wdg_thread.daemon = True

        now = get_tick()
        self.work_thread_start_tick = now
        self.wdg_thread_start_tick = now
        self.work_thread.start()
        self.wdg_thread.start()

        gui.init(self.shared_queue, self.tts_queue)

    @staticmethod
    def setting_encoder(obj):
        if isinstance(obj, datetime.datetime):
            return obj.isoformat()
        raise TypeError("Can't encode object of type '{}'".format(type(obj)))

    def load_settings(self):
        sets = {}
        try:
            if os.path.isfile(Sicurphone.SETTINGS_FILENAME):
                with open(Sicurphone.SETTINGS_FILENAME, 'r', encoding='utf-8') as f:
                    sets = json.load(f)
                last = sets.get('last_covid_send', None)
                sets['last_covid_send'] = None
                if last is not None:
                    try:
                        sets['last_covid_send'] = du_parser.parse(last)
                    except du_parser.ParserError:
                        lg.x("Malformed 'last_covid_send' in settings: {}".format(repr(last)))
        except (OSError, json.JSONDecodeError):
            lg.x("Error loading settings file")
        lg.i("Loaded settings: {}".format(json.dumps(sets, default=Sicurphone.setting_encoder)))
        return sets

    def store_settings(self, settings):
        tmp_name = None
        try:
            lg.d("Store settings: {}".format(settings))
            f = tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', delete=False)
            tmp_name = f.name
            json.dump(settings, f, default=Sicurphone.setting_encoder)
            f.close()
            os.replace(tmp_name, Sicurphone.SETTINGS_FILENAME)
            return True
        except (OSError):
            lg.x("Error storing settings file")
            if tmp_name is not None:
                try:
                    if os.path.isfile(tmp_name):
                        os.remove(tmp_name)
                except OSError:
                    lg.x("Error cleaning up temporary file")
            return False

    def handle_connection_status(self):
        if self.iot.is_connected:
            self.gui.change_connection_status(Gui.Conn_status.OK)
        else:
            self.gui.change_connection_status(Gui.Conn_status.NONE)

    def work_run(self):
        thr = threading.current_thread()
        q = self.shared_queue

        self.iot.init(q)
        self.cec.init(q)
        self.gpio.init(q)

        if self.cec.we_are_active_source():
            self.gui.change_screen_active_status(True, get_tick())

        self.change_sched_covid(self.settings.get('sched_covid', None))

        self.gui.startup_done()

        self.gui.change_connection_status(Gui.Conn_status.NONE)
        self.gui.change_last_covid_send(self.settings.get('last_covid_send', None))
        self.gui.change_sched_covid(self.sched_covid)

        def activate_when_ready():
            lg.i("Ready, activate screen")
            self.cec.turn_on_tv()
            self.cec.set_as_active_source()
        Gui_utils.run_gui_code(activate_when_ready)

        lg.d("Work loop started.")
        while True:
            thr.set_last_alive(get_tick())
            time.sleep(5e-3)

            now_dt = get_datetime()

            self.gpio.loop()
            self.tts.loop()

            self.iot.loop()
            exit_if_exc_during_cb()

            self.gui.change_num_pending_out_msg(len(self.iot.out_msg_queue))

            self.handle_connection_status()

            self.handle_schedules(now_dt, self.prev_datetime)
            self.prev_datetime = now_dt

            for _ in range(10):
                item = q_get(q)
                if item is None:
                    break

                if self.is_shutting_down():
                    lg.i("We're shutting down, drop item: {}".format(item))
                    continue

                if isinstance(item.data, Iot.fn_t):
                    lg.d("Recv IOT fn: {}".format(item))
                    self.handle_iot_fn(item.data)
                elif isinstance(item.data, Cec.evt_t):
                    #lg.d("Recv CEC evt: {}".format(item))
                    self.handle_cec_evt(item.data, item.tick)
                elif isinstance(item.data, Gpio.evt_t):
                    lg.d("Recv GPIO evt: {}".format(item))
                    self.handle_gpio_evt(item.data, item.tick)
                elif isinstance(item.data, Gui.evt_t):
                    lg.d("Recv GUI evt: {}".format(item))
                    self.handle_gui_evt(item.data, item.tick)
                else:
                    lg.e("Recv wrong msg: {}".format(item))

            self.handle_queued_items()

            self.check_wdg_thread()

    def wdg_run(self):
        try:
            thr = threading.current_thread()
            while True:
                time.sleep(0.05)
                now = get_tick()
                thr.set_last_alive(now)
                if thread_has_problems(
                        self.work_thread,
                        now=now,
                        start=self.work_thread_start_tick,
                        timeout=15,
                        timeout_first_time=30
                        ):
                    lg.e("Work thread is down, exit app")
                    Gui_utils.close_gui()
                    exit(1)
        except SystemExit:
            raise
        except BaseException:
            lg.x("Exception in watchdog thread, exit app")
            Gui_utils.close_gui()
            exit(1)

    def check_wdg_thread(self):
        now = get_tick()
        if thread_has_problems(
                self.wdg_thread,
                now=now,
                start=self.wdg_thread_start_tick,
                timeout=10,
                timeout_first_time=20
                ):
            lg.e("Wdg thread is down, exit app")
            Gui_utils.close_gui()
            exit(1)

    def is_shutting_down(self) -> bool:
        last_sd = self.last_shutdown_attempt_tick
        return last_sd is not None and get_tick() < last_sd + 30

    def handle_queued_items(self):
        if self.is_shutting_down():
            return

        q = self.item_queue

        something_changed = False

        # handle timeouts
        for it in q:
            if it.timed_out or it.deadline_tick is None:
                continue

            oc = it.outcome
            if oc == Item.Outcome.DONE or oc == Item.Outcome.CANCELLED:
                continue

            now = get_tick()
            if now < it.deadline_tick:
                continue
            lg.i("Item timed out: {}".format(it))
            it.timed_out = True
            self.send_timeout_event_for(it.type, it.req)
            something_changed = True

        # remove completed items
        for i in reversed(range(len(q))):
            it = q[i]
            oc = it.outcome
            if oc != Item.Outcome.PENDING:
                lg.d("drop completed item: {}".format(q))
                something_changed = True
                del q[i]

        if something_changed:
            lg.d("new item queue: {}".format(self.item_queue))

        self.gui.show_next_item_if_idle()

    def send_timeout_event_for(self, type: Item.Type, req):
        if req is None:
            return
        id = req.get('id', None)
        if id is None:
            return

        if type == Item.Type.MSG:
            name = "msg"
            params = {"id": id, "res": "timeout"}
        elif type == Item.Type.REQ_DATA:
            name = "data"
            params = {"id": id, "res": "timeout"}
        else:
            return

        self.iot.acked_publish(name, json_dump_compact(params))

    def covid_data_requested_recently(self, now_dt):
        if self.last_covid_data_sent_dt is not None:
            next_allowed = self.last_covid_data_sent_dt + datetime.timedelta(hours=2)
            if now_dt <= next_allowed:
                return True

        if self.last_covid_data_request_dt is not None:
            next_allowed = self.last_covid_data_request_dt + datetime.timedelta(hours=2)
            if now_dt <= next_allowed:
                return True

        return False

    def handle_schedules(self, now_dt: datetime.datetime, prev_dt: datetime.datetime):
        s_covid = self.sched_covid
        if s_covid is None:
            return

        if not time_is_in_interval(
                s_covid.astimezone().time(),
                prev_dt.astimezone().time(),
                now_dt.astimezone().time()
                ):
            return

        if self.covid_data_requested_recently(now_dt):
            lg.i("Covid data requested recently (req {}, sent {}), skip schedule".format(
                self.last_covid_data_request_dt,
                self.last_covid_data_sent_dt
            ))
            return

        lg.d("Covid schedule: {} is in {} - {}: trigger".format(s_covid, prev_dt, now_dt))
        lg.i("Trigger scheduled covid request")
        if self.is_shutting_down():
            lg.i("We're shutting down, ignore covid schedule")
            return

        for it in self.item_queue:
            if it.type != Item.Type.REQ_DATA:
                continue
            req = it.req
            if req is None:
                continue
            t = req.get('type', None)
            if t is None or t == "" or t == "covid":
                lg.d("There's already a covid request, do not schedule another one: {}")
                return

        timeout = 10*60
        deadline_tick = get_tick()+timeout
        item = Item(source=Item.Source.SCHEDULE, type=Item.Type.REQ_DATA, req={"cmd": "req_data", "type": "covid", "tout": timeout}, deadline_tick=deadline_tick)
        lg.d("Trigger covid request {}".format(item))
        self.item_queue.append(item)
        lg.d("new item queue {}".format(self.item_queue))
        self.gui.enqueue_item(item)

    def handle_iot_fn(self, fn: Iot.fn_t):
        assert(fn.name == 'action')
        try:
            args = json.loads(fn.args)
        except json.JSONDecodeError as e:
            lg.xx(e, "Unexpected json parsing error (should have already been checked by now)")
            return

        cmd = args.get('cmd', None)
        if cmd == "sched_covid":
            self.handle_iot_sched_covid(args)
        elif cmd == "req_data":
            self.handle_iot_req_data(args)
        elif cmd == "msg":
            self.handle_iot_msg(args)
        else:
            lg.w("Unsupported IOT command: {}".format(args))

    def handle_iot_sched_covid(self, args):
        t = args.get('time', None)
        if t is None:
            lg.w("Malformed time in 'sched_covid' packet: {}".format(t))
            return

        self.change_sched_covid(t)
        self.store_settings(self.settings)

    def change_sched_covid(self, t_str: str):
        if t_str == "" or t_str is None:
            self.sched_covid = None
            self.iot.var_sched_covid.value = b""
            self.settings['sched_covid'] = None
        else:
            try:
                dt = du_parser.parse(t_str)
            except du_parser.ParserError:
                lg.x("Malformed 'sched_covid' setting: {}".format(t_str))
                return

            self.sched_covid = dt
            t_str = dt.astimezone().strftime("%H:%M:%S")
            self.iot.var_sched_covid.value = t_str.encode('utf-8')
            self.settings['sched_covid'] = t_str
        lg.i("Covid schedule set to {}".format(self.sched_covid))
        self.gui.change_sched_covid(self.sched_covid)

    def handle_iot_req_data(self, args):
        tout = args.get("tout", None)
        if tout:
            tout += get_tick()
        item = Item(source=Item.Source.SERVER, type=Item.Type.REQ_DATA, req=args, deadline_tick=tout)
        lg.d("handle_iot_req_data {}".format(item))
        self.item_queue.append(item)
        lg.d("new item queue {}".format(self.item_queue))
        self.gui.enqueue_item(item)

    def handle_iot_msg(self, args):
        tout = args.get("tout", None)
        if tout:
            tout += get_tick()
        item = Item(source=Item.Source.SERVER, type=Item.Type.MSG, req=args, deadline_tick=tout)
        lg.d("handle_iot_msg {}".format(item))
        self.item_queue.append(item)
        lg.d("new item queue {}".format(self.item_queue))
        self.gui.enqueue_item(item)

    def handle_cec_evt(self, evt: Cec.evt_t, evt_tick: float):
        if evt.type == Cec.Evt_type.KEYPRESS:
            self.gui.handle_keypress(evt.data, evt_tick)
        elif evt.type == Cec.Evt_type.ACTIVATION:
            self.gui.handle_activation(evt.data, evt_tick)
        else:
            lg.w("Unsupported CEC evt {}".format(evt))

    def handle_gpio_evt(self, evt: Gpio.evt_t, evt_tick: float):
        if evt.type == Gpio.Evt_type.EMG:
            now = get_tick()
            last = self.last_emg_event_tick
            if last is not None and now - last < 5:
                lg.w("Ignore EMG event, too close to the previous one")
                return
            self.last_emg_event_tick = now
            params = {"t": get_datetime_str_for_publish()}
            ok = self.publish("emg", json_dump_compact(params))
            self.gui.show_screen_emg(ok=ok, dt=datetime.datetime.now())
        else:
            lg.w("Unsupported GPIO evt {}".format(evt))

    def handle_gui_evt(self, evt: Gui.evt_t, evt_tick: float):
        if evt is None:
            return
        if isinstance(evt, list):
            for e in evt:
                self.handle_gui_evt(e)
            return

        lg.d("handle_gui_evt: {} @{}".format(evt, evt_tick))

        assert(isinstance(evt, Gui.evt_t))

        data = evt.data
        item : Optional[Item] = None if data is None else data['item']
        assert(item is None or isinstance(item, Item))
        source : Optional[Item.Source] = None if item is None else item.source
        req = None if item is None else item.req
        typ = None if req is None else req.get('type', None)
        id = None if req is None else req.get('id')

        if evt.type == Gui.Evt_type.SEND_DATA_CANCEL:
            lg.d("gui evt SEND_DATA_CANCEL")
            assert(item is not None)
            item.outcome = Item.Outcome.CANCELLED
            if source is not None and source != Item.Source.USER:
                params = {"t": get_datetime_str_for_publish(), "id": id, "type": typ, "res": "cancel"}
                self.publish("data", json_dump_compact(params))
        elif evt.type == Gui.Evt_type.SEND_DATA_OK:
            lg.d("gui evt SEND_DATA_OK")
            assert(item is not None)
            item.outcome = Item.Outcome.DONE
            params = {"t": get_datetime_str_for_publish(), "id": id, "type": typ, "res": "ok", "values": json_dump_compact(data['values'])}
            ok = self.publish("data", json_dump_compact(params))
            if ok:
                self.gui.show_screen_send_outcome("I dati sono stati inviati.")
                last_send = get_datetime()
                self.settings['last_covid_send'] = last_send
                self.store_settings(self.settings)
                self.gui.change_last_covid_send(last_send)
            else:
                self.gui.show_screen_send_outcome("Errore nella trasmissione.\nI dati non sono stati inviati.")
        elif evt.type == Gui.Evt_type.MSG_CANCEL:
            lg.d("gui evt MSG_CANCEL")
            assert(item is not None)
            item.outcome = Item.Outcome.CANCELLED
            assert(req is not None)
            if req.get('type', None) == "ok":
                params = {"t": get_datetime_str_for_publish(), "id": id, "res": "cancel"}
                self.publish("msg", json_dump_compact(params))
            else:
                lg.d("msg CANCEL, no-notification type: {}".format(item))
        elif evt.type == Gui.Evt_type.MSG_OK:
            lg.d("gui evt MSG_OK")
            assert(item is not None)
            item.outcome = Item.Outcome.DONE
            assert(req is not None)
            if req.get('type', None) == "ok":
                params = {"t": get_datetime_str_for_publish(), "id": id, "res": "ok"}
                ok = self.publish("msg", json_dump_compact(params))
                if not ok:
                    self.gui.show_screen_send_outcome("Errore nella trasmissione.\nNon è stato possibile notificare che il messaggio è stato letto.")
            else:
                lg.d("msg OK, no-notification type: {}".format(item))
        elif evt.type == Gui.Evt_type.SCREEN_ACTIVATE:
            lg.d("gui evt SCREEN_ACTIVATE")
            self.cec.activate_screen()
        elif evt.type == Gui.Evt_type.SCREEN_DEACTIVATE:
            lg.d("gui evt SCREEN_DEACTIVATE")
            self.cec.deactivate_screen()
        elif evt.type == Gui.Evt_type.SHUTDOWN:
            lg.d("gui evt SHUTDOWN")
            self.trigger_shutdown()
        else:
            lg.w("unknown evt type {}".format(evt.type))
            pass

    def publish(self, name: Optional[str], str_data: Optional[str]) -> bool:
        [ok, _] = self.iot.acked_publish(name, str_data)
        self.gui.change_num_pending_out_msg(len(self.iot.out_msg_queue))
        return ok

    def publish_log(self, msg : str):
        self.iot.acked_publish("log", json_dump_compact({"t": get_datetime_str_for_publish(), "msg" : msg}))

    def publish_err(self, msg : str):
        self.iot.acked_publish("err", json_dump_compact({"t": get_datetime_str_for_publish(), "msg" : msg}))

    def trigger_shutdown(self):
        lg.d("Perform shutdown")

        self.publish_log("shutdown")
        t0 = get_tick()
        while True:
            time.sleep(5e-3)
            self.iot.loop()
            if get_tick() - t0 >= 500 or len(self.iot.out_msg_queue) == 0:
                break

        self.last_shutdown_attempt_tick = get_tick()

        try:
            (rc, p, out, err) = Cmd.cmd(['shutdown', 'now'], timeout=15)
        except subprocess.CalledProcessError:
            lg.x("Error while triggering shutdown")
            return

        if rc != 0:
            lg.w("Error while triggering shutdown (return code {}):\n{}\n{}".format(rc, out, err))
            return

        time.sleep(3)

def test_cec():
    q = Queue()
    cec = Cec()
    cec.init(q)

    lg.i("CEC test")
    cec.print_controlled_devices()
    cec.scan_and_print_results()
    cec.activate_screen()
    cec.print_controlled_devices()
    cec.scan_and_print_results()

    lg.i("CEC test loop")
    while True:
        time.sleep(10e-3)

        for _ in range(10):
            item = q_get(q)
            if item is None:
                break

            if isinstance(item.data, Cec.evt_t):
                lg.i("Recv CEC evt: {}".format(item))
                if isinstance(item.data.data, Cec.keypress_t):
                    d = item.data.data
                    if d.key == KEY_EXIT:
                        lg.i("EXIT pressed: deactivate screen")
                        cec.deactivate_screen()
                    if d.key == KEY_0:
                        lg.i("0 pressed: show OSD string screen")
                        cec.show_osd_string("ABC")
            else:
                lg.e("Recv wrong item: {}".format(item))

def test_iot_send_receive():
    global iot

    q = Queue()

    iot = Iot()
    iot.init(q)

    t0 = None
    td = None
    was_connected = False
    while True:
        time.sleep(50e-3)

        connected = iot.is_connected
        if connected != was_connected:
            lg.i('Connected' if connected else "Disconnected")
            was_connected = connected

        iot.loop()
        exit_if_exc_during_cb()

        for _ in range(10):
            item = q_get(q)
            if item is None:
                break

            if isinstance(item.data, Iot.fn_t):
                lg.i("Recv IOT fn: {}".format(item))
            else:
                lg.e("Recv wrong item: {}".format(item))

        if connected:
            if True:
                t = get_tick_ms()
                if td is None or t - td >= 1000:
                    td = t
                    #iot.var_bool.value = (t % 2000) < 1000
                    #iot.var_int.value = t % 10000
                    #iot.var_double.value = t / 1e6
                    #iot.var_str.value = b'str_' + str(t).encode('utf-8')
                    iot.var_sched_covid.value = b'test_' + str(t).encode('utf-8')
            if True:
                t = get_tick_ms()
                if t0 is None or t - t0 >= 10000:
                    t0 = t

                    if 1:
                        lg.d('Publish event no-ack')
                        res = iot.oneshot_publish(
                            "log",
                            json_dump_compact({"t": get_datetime_str_for_publish(),
                            "test" : "test_value"}))
                        print("publish event no-ack res {}".format(res))

                        lg.d('Publish event ack')
                        res = iot.acked_publish(
                            "log_ack",
                            json_dump_compact({"t": get_datetime_str_for_publish(),
                            "test" : "test_value"}))
                        print("publish event ack res {}".format(res))
                    else:
                        lg.d('Publish event no-ack')
                        ttl = 30 & 0x7FFFFFFF
                        flags = 0 & 0xFFFFFFFF
                        res = iot.publish(
                            'testevt',
                            'evt_dat_' + str(t),
                            ttl=ttl,
                            flags=flags
                        )
                        print("publish event no-ack res {}".format(res))

                        lg.d('Publish event ack')
                        res = iot.publish(
                            'testevtack',
                            'evtack_dat_' + str(t),
                            ttl=ttl,
                            flags=flags|Iot.PUBFLAG_ACK
                        )
                        print("publish event ack res {}".format(res))

if __name__ == "__main__":
    lg.i("Started")
    try:
        threading.current_thread().name = "init"
        #test_iot_send_receive()
        #test_cec()
        Sicurphone().run()
    except KeyboardInterrupt:
        lg.d("Keyboard interrupt")
    except SystemExit as e:
        if e.code != 0:
            lg.x()
    lg.i("Exited")