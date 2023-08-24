import ctypes
import os

def int_list_from_cpp_hex_array(string):
    " return integer list from string containing a hex array "
    return [int(s.strip(), 16) for s in string.split(",")]

# Library credentials

TRACKLE_ID = (ctypes.c_uint8*12)(
    *int_list_from_cpp_hex_array(os.environ["TRACKLE_ID_LIB_TEST"])
)

TRACKLE_ID_STRING = "".join([hex(i)[2:].rjust(2, "0") for i in TRACKLE_ID])

TRACKLE_PRIVATE_KEY = (ctypes.c_uint8*121)(
    *int_list_from_cpp_hex_array(os.environ["TRACKLE_PRIVATE_KEY_LIB_TEST"])
)

#  Test suite credentials

TRACKLE_CLIENT_ID = os.environ["TRACKLE_CLIENT_ID_LIB_TEST"]
TRACKLE_CLIENT_SECRET = os.environ["TRACKLE_CLIENT_SECRET_LIB_TEST"]
