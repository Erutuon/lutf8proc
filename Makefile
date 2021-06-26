# To use bitflag options rather than string options
# in map and map_custom:
# make shared MYCFLAGS=-DUSE_BIT_OPTIONS

PICFLAG = -fPIC
DEFINES = -DUTF8PROC_STATIC
LDFLAG_SHARED = -shared
CFLAGS = -O2 $(MYCFLAGS) $(PICFLAG) $(DEFINES)

LUA_DIR = /usr/local
LUA_LIBDIR = $(LUA_DIR)/lib
LUA_INCDIR = $(LUA_DIR)/include

LUA_VERSION ?= 5.3
LUA_LIB = lua$(LUA_VERSION)

UTF8PROC_DIR = utf8proc
UTF8PROC_SRCDIR = $(UTF8PROC_DIR)
UTF8PROC_INCDIR = $(UTF8PROC_DIR)

SO = so
UTF8PROC_LIB = lib/lutf8proc.$(SO)
UTF8PROC_OBJ = obj/utf8proc.o
LUTF8PROC_OBJ = obj/lutf8proc.o

.PHONY: clean lib

$(UTF8PROC_LIB): $(LUTF8PROC_OBJ) $(UTF8PROC_OBJ)
	mkdir -p lib
	gcc $(LDFLAGS) $(LDFLAG_SHARED) -o $@ $^ -l $(LUA_LIB) -L $(LUA_LIBDIR)

$(LUTF8PROC_OBJ): lutf8proc.c $(UTF8PROC_INCDIR)/utf8proc.h
	mkdir -p obj
	gcc -c $(CFLAGS) -o $@ lutf8proc.c -I $(UTF8PROC_INCDIR) -I $(LUA_INCDIR)

$(UTF8PROC_OBJ): $(UTF8PROC_INCDIR)/utf8proc.h $(UTF8PROC_SRCDIR)/utf8proc.c $(UTF8PROC_SRCDIR)/utf8proc_data.c
	mkdir -p obj
	gcc -c $(CFLAGS) -o $@ $(UTF8PROC_SRCDIR)/utf8proc.c

lib: $(UTF8PROC_LIB)

clean:
	rm -f $(UTF8PROC_LIB) $(UTF8PROC_OBJ) $(LUTF8PROC_OBJ)
