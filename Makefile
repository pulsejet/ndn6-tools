CXX = g++
CXXFLAGS = -std=c++11 -Wall -Werror `pkg-config --cflags libndn-cxx` -DBOOST_LOG_DYN_LINK
LIBS = `pkg-config --libs libndn-cxx`
DESTDIR ?= /usr/local

PROGRAMS = facemon prefix-allocate prefix-request tap-tunnel remote-register-prefix register-prefix-cmd

all: $(PROGRAMS)

%: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

tap-tunnel: tap-tunnel*.cpp
	$(CXX) $(CXXFLAGS) -o $@ tap-tunnel*.cpp $(LIBS) -lcrypto

clean:
	rm -f $(PROGRAMS)

install: all
	cp $(PROGRAMS) $(DESTDIR)/bin/

uninstall:
	cd $(DESTDIR)/bin && rm -f $(PROGRAMS)
