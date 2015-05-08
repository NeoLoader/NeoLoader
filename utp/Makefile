SRCS = utp.cpp utp_utils.cpp
OBJS = utp.o utp_utils.o
CXXFLAGS = -fno-exceptions -fno-rtti -Wall -g -fpic -std=c++0x

all: libutp.so

libutp.so: $(OBJS)
	-rm -f libutp.so
#	ar q libutp.so $(OBJS)
#	ranlib libutp.so
	g++ -shared -o libutp.so $(OBJS)

.cpp.o:
	g++ -c -DPOSIX -I . -I utp_config_lib $(CXXFLAGS) $<

.PHONY: clean

clean:
	-rm -f $(OBJS) libutp.so
