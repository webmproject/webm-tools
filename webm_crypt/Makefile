LIBWEBM = ../../libwebm
CHROMIUMINC = /usr/local/google/Chromium/src
CHROMIUMOBJ = $(CHROMIUMINC)/out/Debug/obj.target
BASEDIR = $(CHROMIUMOBJ)/base
DYNANNDIR = $(BASEDIR)/third_party/dynamic_annotations
CRYPTODIR = $(CHROMIUMOBJ)/crypto
NETDIR = $(CHROMIUMOBJ)/net
SSLDIR = $(CHROMIUMOBJ)/net/third_party/nss
OBJECTS = webm_crypt.o
EXE = webm_crypt
INCLUDES = -I$(LIBWEBM) -I$(CHROMIUMINC)
CXXFLAGS = -W -Wall -g

$(EXE): $(OBJECTS)
	$(CXX) $(OBJECTS) -L$(LIBWEBM) -L$(BASEDIR) -L$(DYNANNDIR) -L$(CRYPTODIR) \
	-L$(NETDIR) -L$(SSLDIR) \
	-lwebm -lcrcrypto -ldynamic_annotations -lbase -lsymbolize \
	-ldl -lrt -lglib-2.0 -lnss3 -lnssutil3 -lnspr4 -lpthread -o $@

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) -r $(OBJECTS) $(EXE) Makefile.bak

.PHONY: clean
