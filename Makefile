SERVER = CC_server

############# Main application #################
all:	$(SERVER)
.PHONY: all

# source files
# DEBUG_INFO = YES
SERVER_SOURCES = CurrentCost_main.c CurrentCost_network.c CurrentCost_xml.c
SERVER_OBJECTS = $(SERVER_SOURCES:.c=.o)

######## compiler- and linker settings #########
CXXFLAGS = -I/usr/include -I/usr/local/include -I/usr/include/libxml2 -W -Wall -Werror -pipe
LIBSFLAGS = -L/usr/lib -L/usr/local/lib -lxml2
ifdef DEBUG_INFO
 CXXFLAGS += -g
 LIBSFLAGS += -lctbd-0.16
else
 CXXFLAGS += -O3
 LIBSFLAGS += -lctb-0.16
endif

%.o: %.c
	g++ $(CXXFLAGS) -o $@ -c $<

############# Main application #################
$(SERVER):	$(SERVER_OBJECTS)
	g++ -o $@ $(SERVER_OBJECTS) $(LIBSFLAGS)

################### Clean ######################
clean:
	find . -name '*~' -delete
	rm -f $(SERVER) $(SERVER_OBJECTS)

install:
	strip -s $(SERVER) && cp $(SERVER) /usr/local/bin/ && \
	cp ./CurrentCost /etc/init.d/ && \
	chmod 755 /etc/init.d/CurrentCost && \
	cp -i ./CC_munin_watts /usr/share/munin/plugins/ && \
	cp -i ./CC_munin_tmpr /usr/share/munin/plugins/ && \
	ln -sf /usr/share/munin/plugins/CC_munin_watts /etc/munin/plugins/CC_munin_watts && \
	ln -sf /usr/share/munin/plugins/CC_munin_tmpr /etc/munin/plugins/CC_munin_tmpr && \
	munin-node-configure && \
	/etc/init.d/munin-node restart
