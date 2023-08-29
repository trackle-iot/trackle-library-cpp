CC = gcc
CCX = g++
TRACKLE_LIB = ../..
DLL_EXTENSION = dylib

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
SHARED_INCLUDES = -I"include"

# Compilation units used by all examples
SHARED_SRCS = src/callbacks.c

# All object files in base directory
OBJS = *.o

# Flags to be passed to compiler before compiling DLL pieces
DLL_FLAGS = -DDISABLE_EXAMPLES_LOGGING

all: trackle_library uecc tinydtls

trackle_library:
	$(CCX) -w -std=c++11 -fpermissive -fms-extensions -c $(TRACKLE_LIB_SRCS) $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES)

uecc:
	$(CC) -w -c $(UECC_SRCS) $(UECC_INCLUDES)

tinydtls:
	$(CC) -w -c $(TINY_SRCS) $(TINY_INCLUDES) $(UECC_INCLUDES) -DWITH_SHA256

trackle_library_fpic:
	$(CCX) -w -std=c++11 -fpermissive -fms-extensions -c -fPIC $(TRACKLE_LIB_SRCS) $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES) $(DLL_FLAGS)

uecc_fpic:
	$(CC) -w -c -fPIC $(UECC_SRCS) $(UECC_INCLUDES) $(DLL_FLAGS)

tinydtls_fpic:
	$(CC) -w -c -fPIC $(TINY_SRCS) $(TINY_INCLUDES) $(UECC_INCLUDES) $(DLL_FLAGS) -DWITH_SHA256

callbacks_fpic:
	$(CC) -w -c -fPIC $(SHARED_SRCS) $(SHARED_INCLUDES) $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES) $(DLL_FLAGS)

dll: trackle_library_fpic uecc_fpic tinydtls_fpic callbacks_fpic
	mkdir -p lib
	$(CC) -w -shared $(OBJS) -o lib/trackle_library.$(DLL_EXTENSION) -lstdc++ -lm

clean:
	rm -f *.o
