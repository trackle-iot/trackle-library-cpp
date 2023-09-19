CC = gcc
CCX = g++
TRACKLE_LIB = ../..

# OS detection for DLL extension
ifeq ($(shell uname -s),Darwin)
    DLL_EXTENSION = dylib
else
    ifeq ($(shell uname -s),Linux)
        DLL_EXTENSION = so
    else
        $(error Unsupported OS)
    endif
endif


# Trackle library and its components compilation variables
TRACKLE_LIB_SRCS = $(TRACKLE_LIB)/src/*.cpp
TRACKLE_LIB_INCLUDES = -I"$(TRACKLE_LIB)/include"

UECC_SRCS = $(TRACKLE_LIB)/lib/micro-ecc/uECC.c
UECC_INCLUDES = -I"$(TRACKLE_LIB)/lib/micro-ecc"

TINY_SRCS = $(TRACKLE_LIB)/lib/tinydtls/ccm.c \
			$(TRACKLE_LIB)/lib/tinydtls/crypto.c \
			$(TRACKLE_LIB)/lib/tinydtls/dtls.c \
			$(TRACKLE_LIB)/lib/tinydtls/dtls_debug.c \
			$(TRACKLE_LIB)/lib/tinydtls/dtls_time.c \
			$(TRACKLE_LIB)/lib/tinydtls/dtls_prng.c \
			$(TRACKLE_LIB)/lib/tinydtls/hmac.c \
			$(TRACKLE_LIB)/lib/tinydtls/netq.c \
			$(TRACKLE_LIB)/lib/tinydtls/peer.c \
			$(TRACKLE_LIB)/lib/tinydtls/session.c \
			$(TRACKLE_LIB)/lib/tinydtls/aes/*.c \
			$(TRACKLE_LIB)/lib/tinydtls/sha2/sha2.c
TINY_INCLUDES = -I"$(TRACKLE_LIB)/lib/tinydtls" \
			    -I"$(TRACKLE_LIB)/lib/tinydtls/aes" \
			    -I"$(TRACKLE_LIB)/lib/tinydtls/platform-specific" \
			    -I"$(TRACKLE_LIB)/lib/tinydtls/sha2"


# Include directories used by all examples
SHARED_INCLUDES = -I"$(TRACKLE_LIB)/example/posix/include"

# Compilation units of calbacks
CALLBACKS_SRCS = $(TRACKLE_LIB)/example/posix/src/callbacks.c

# Compilation units of cloud functions for tests
CLOUD_FUNCTIONS_SRCS = cloud_functions.c

# All object files in base directory
OBJS = *.o

# Flags to be passed to compiler before compiling DLL pieces
DLL_FLAGS = -DDISABLE_EXAMPLES_LOGGING

default: dll_trackle dll_callbacks dll_cloudfunctions

trackle_library_fpic:
	$(CCX) -w -std=c++11 -fpermissive -fms-extensions -c -fPIC $(TRACKLE_LIB_SRCS) $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES) $(DLL_FLAGS)

uecc_fpic:
	$(CC) -w -c -fPIC $(UECC_SRCS) $(UECC_INCLUDES) $(DLL_FLAGS)

tinydtls_fpic:
	$(CC) -w -c -fPIC $(TINY_SRCS) $(TINY_INCLUDES) $(UECC_INCLUDES) $(DLL_FLAGS) -DWITH_SHA256

callbacks_fpic:
	$(CC) -w -c -fPIC $(CALLBACKS_SRCS) $(SHARED_INCLUDES) $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES) $(DLL_FLAGS)

cloud_functions_fpic:
	$(CC) -w -c -fPIC $(CLOUD_FUNCTIONS_SRCS) $(SHARED_INCLUDES) $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES) $(DLL_FLAGS)

dll_trackle: trackle_library_fpic uecc_fpic tinydtls_fpic
	mkdir -p lib
	$(CC) -w -shared $(OBJS) -o lib/trackle_library.$(DLL_EXTENSION) -lstdc++ -lm
	rm -f *.o

dll_callbacks: callbacks_fpic
	mkdir -p lib
	$(CC) -w -shared $(OBJS) -o lib/callbacks.$(DLL_EXTENSION) -lstdc++ -lm
	rm -f *.o

dll_cloudfunctions: cloud_functions_fpic
	mkdir -p lib
	$(CC) -w -shared $(OBJS) -o lib/cloud_functions.$(DLL_EXTENSION) -lstdc++ -lm
	rm -f *.o

clean:
	rm -f *.o
