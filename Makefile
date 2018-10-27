SO ?= dll
CFLAGS = -shared -s -O2
LUADIR = ../lua-5.3.4
LUA_LIBDIR = $(LUADIR)/bin
LUA_INCDIR = $(LUADIR)/src

# The utf8proc shared library (utf8proc.dll or utf8proc.so)
# must be in this directory.
UTF8PROC_DIR ?= $(LUA_LIBDIR)

# For instance, to use bitflag options rather than string options
# in map and map_custom:
# make shared MYCFLAGS=-DUSE_BIT_OPTIONS
MYCFLAGS ?=

shared: lutf8proc.c utf8proc.h
	gcc $(CFLAGS) $(MYCFLAGS) -o $(LUA_LIBDIR)/lutf8proc.${SO} lutf8proc.c -I$(LUA_INCDIR) \
		-lutf8proc -L$(UTF8PROC_DIR) \
		-llua53 -L$(LUA_LIBDIR)
