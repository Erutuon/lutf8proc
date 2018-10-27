SO ?= dll
CFLAGS = -shared -s -O2
LUADIR = ../lua-5.3.4
LUA_LIBDIR = $(LUADIR)/bin
LUA_INCDIR = $(LUADIR)/src

# Compile utf8proc as a shared library (utf8proc.dll or utf8proc.so)
# and put it here.
UTF8PROC_DIR ?= $(LUA_LIBDIR)

shared: lutf8proc.c utf8proc.h
	gcc $(CFLAGS) -o $(LUA_LIBDIR)/lutf8proc.${SO} lutf8proc.c -I$(LUA_INCDIR) \
	-lutf8proc -L$(UTF8PROC_DIR) \
	-llua53 -L$(LUA_LIBDIR)
