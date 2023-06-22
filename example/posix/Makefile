CC = gcc
CCX = g++
TRACKLE_LIB = ../..

# Trackle library and its components compilation variables
TRACKLE_LIB_SRCS = $(TRACKLE_LIB)/src/*.cpp
TRACKLE_LIB_INCLUDES = -I"$(TRACKLE_LIB)/include"

UECC_SRCS = $(TRACKLE_LIB)/lib/micro-ecc/uECC.c
UECC_INCLUDES = -I"$(TRACKLE_LIB)/lib/micro-ecc"

TINY_SRCS = $(TRACKLE_LIB)/lib/tinydtls/ccm.c \
			$(TRACKLE_LIB)/lib/tinydtls/crypto.c \
			$(TRACKLE_LIB)/lib/tinydtls/dtls.c \
			$(TRACKLE_LIB)/lib/tinydtls/dtls_config.h.cmake.in \
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

# Example in C
EXAMPLE_C_SRCS = src/main.c

# Example in C++
EXAMPLE_CPP_SRCS = src/main.cpp

# All object files in base directory
OBJS = *.o

all: trackle_library uecc tinydtls callbacks example_c example_cpp

trackle_library:
	$(CCX) -w -std=c++11 -fpermissive -fms-extensions -c $(TRACKLE_LIB_SRCS) $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES)

uecc:
	$(CC) -w -c $(UECC_SRCS) $(UECC_INCLUDES)

tinydtls:
	$(CC) -w -c $(TINY_SRCS) $(TINY_INCLUDES) $(UECC_INCLUDES) -DWITH_SHA256

callbacks:
	$(CC) -w -c $(SHARED_SRCS) $(SHARED_INCLUDES) $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES)

example_c: trackle_library uecc tinydtls callbacks
	mkdir -p bin
	$(CC) -w $(EXAMPLE_C_SRCS) $(OBJS) -o bin/example_c $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES) $(SHARED_INCLUDES) -lstdc++ -lm

example_cpp: trackle_library uecc tinydtls callbacks
	mkdir -p bin
	$(CCX) -w $(EXAMPLE_CPP_SRCS) $(OBJS) -o bin/example_cpp $(TRACKLE_LIB_INCLUDES) $(UECC_INCLUDES) $(TINY_INCLUDES) $(SHARED_INCLUDES) -lstdc++ -lm

clean:
	rm -f *.o
