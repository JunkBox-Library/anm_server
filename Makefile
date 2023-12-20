#

#
#
SUBJ = anm_server
OBJ  = anm_log_server.o anm_server.o anm_main.o anm_help.o anm_config.o
#
all: $(SUBJ)

#
CC   = gcc
AR   = ar
TC	 = touch

XLIBD = /usr/X11R6/lib
XINCD = /usr/X11R6/include

SSLINC = /usr/local/ssl/include 

LIB_DIR = ../JunkBox_Lib
LIB_BSC_DIR = $(LIB_DIR)/Lib
LIB_GRA_DIR = $(LIB_DIR)/gLib
LIB_EXT_DIR = $(LIB_DIR)/xLib

LIB_BSC = $(LIB_BSC_DIR)/libbasic.a
LIB_GRA = $(LIB_GRA_DIR)/libgraph.a
LIB_EXT = $(LIB_EXT_DIR)/libextend.a


CFLAGS  = -DHAVE_CONFIG_H -I$(LIB_DIR) -I$(LIB_BSC_DIR) -I$(LIB_GRA_DIR) -I$(LIB_EXT_DIR) -I$(XINCD) -I$(SSLINC)

XLIB = -L$(XLIBD) -lX11 
SLIB = -L$(LIB_BSC_DIR) -lbasic -lm
GLIB = -L$(LIB_GRA_DIR) -lgraph $(XLIB)
ELIB = -L$(LIB_EXT_DIR) -lextend
#
#
#

#.h.c:
#	$(TC) $@

.c.o:
	$(CC) $< $(CFLAGS) -c -O2 


clean:
	rm -f *.o *~ $(SUBJ)


#
#
#
#

anm_main.c: anm_server.h anm_data.h
	$(TC) $@

anm_log_server.c: anm_log_server.h anm_server.h anm_data.h
	$(TC) $@

anm_server.c: anm_log_server.h anm_server.h anm_data.h
	$(TC) $@

anm_config.c: anm_server.h
	$(TC) $@

anm_help.c: anm_help.h
	$(TC) $@


anm_server: $(OBJ) $(LIB_BSC) $(LIB_EXT)
	$(CC) $(OBJ) $(ELIB) $(SLIB) -O2 -o $@ -lcrypt





$(LIB_BSC):
	(cd $(LIB_BSC_DIR) && make)


$(LIB_GRA):
	(cd $(LIB_GRA_DIR) && make)

	
$(LIB_EXT):
	(cd $(LIB_EXT_DIR) && make)




install: anm_server
	mkdir -p /var/anm_server
	chown nobody /var/anm_server
	chmod 0750 /var/anm_server
	mkdir -p /usr/local/etc/anm_server
	chmod 0750 /usr/local/etc/anm_server
	[ -f /usr/local/etc/anm_server/anm_server.conf ] || install -m 0644 conf/anm_server.conf.sample /usr/local/etc/anm_server/anm_server.conf
	chown -R nobody /usr/local/etc/anm_server
	install -m 0755 anm_server /usr/local/bin
	[ -f /etc/init.d/anm_server ] || install -m 0755 conf/anm_server.init /etc/init.d/anm_server
	./set_config anm_server


