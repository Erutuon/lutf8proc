SO ?= so
CFLAGS = -shared -fPIC -s -O2
LUA_DIR = /usr/local
LUA_LIBDIR = $(LUA_DIR)/lib
LUA_INCDIR = $(LUA_DIR)/include

# The utf8proc shared library (utf8proc.dll or utf8proc.so)
# must be in this directory.
UTF8PROC_DIR = $(LUA_LIBDIR)

# For instance, to use bitflag options rather than string options
# in map and map_custom:
# make shared MYCFLAGS=-DUSE_BIT_OPTIONS
MYCFLAGS ?=

UTF8PROC_LIB = lutf8proc.$(SO)

$(UTF8PROC_LIB): lutf8proc.c utf8proc.h
	gcc $(CFLAGS) $(MYCFLAGS) -o $(UTF8PROC_LIB) lutf8proc.c -I$(LUA_INCDIR) \
		-lutf8proc -L$(UTF8PROC_DIR) \
		-llua5.3 -L$(LUA_LIBDIR)
