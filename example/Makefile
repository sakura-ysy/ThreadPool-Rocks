# step 1:
# prepare the compiler
# prepare targe file name
# prepare dependecy libarary
CC = g++
CFLAGS  := -Wall -O2 -fPIC -std=c++11
LDFLAGS = -lpthread

# ifneq ($(USE_RTTI), 1)
# 	CFLAGS += -fno-rtti
# endif

# header file's path
INCLUDE_PATH = -I ../include
LIBPATH = ../libthreadpool.so
STATIC_LIBPATH = ../libthreadpool.a

all : test_env

test_env: test_env.cc
	$(CC) $(CFLAGS) $@.cc $(INCLUDE_PATH) $(STATIC_LIBPATH) -o $@  $(LDFLAGS)


# 4. clean the files except source file
clean:
	rm -f ./test_env