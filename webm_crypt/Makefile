LIBWEBM = ../../libwebm
CHROMIUMINC = /usr/local/google/Chromium/src
CHROMIUMOBJ = $(CHROMIUMINC)/out/Debug/obj.target
BASEDIR = $(CHROMIUMOBJ)/base
DYNANNDIR = $(BASEDIR)/third_party/dynamic_annotations
CRYPTODIR = $(CHROMIUMOBJ)/crypto
OBJECTS = ../shared/webm_endian.o webm_crypt.o
EXE = webm_crypt
INCLUDES = -I$(LIBWEBM) -I../shared -I$(CHROMIUMINC) -I/usr/include/nss -I/usr/include/nspr
CXXFLAGS = -W -Wall -g -DUSE_NSS

$(EXE): $(OBJECTS)
	$(CXX) $(OBJECTS) -L$(LIBWEBM) -L$(BASEDIR) -L$(DYNANNDIR) -L$(CRYPTODIR) \
	-lwebm -lcrcrypto -ldynamic_annotations -lbase -lsymbolize \
	-ldl -lrt -lglib-2.0 -lnss3 -lnssutil3 -lnspr4 -lpthread -o $@

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) -r $(OBJECTS) $(EXE) Makefile.bak

.PHONY: clean
