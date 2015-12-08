LIBWEBM = ../../libwebm
CHROMIUMINC = /usr/local/google/Chromium/src
CHROMIUMOBJ = $(CHROMIUMINC)/out/Debug/obj
OBJECTS = ../shared/webm_endian.o webm_crypt.o
EXE = webm_crypt
INCLUDES = -I$(LIBWEBM) -I../shared -I$(CHROMIUMINC) -I/usr/include/nss \
           -I/usr/include/nspr
CXXFLAGS = -W -Wall -g -DUSE_NSS -std=c++11

$(EXE): $(OBJECTS)
	$(CXX) $(OBJECTS) -L$(LIBWEBM) \
		-L$(CHROMIUMOBJ)/base -L$(CHROMIUMOBJ)/crypto \
		-L$(CHROMIUMOBJ)/base/third_party/dynamic_annotations \
		-L$(CHROMIUMOBJ)/third_party/boringssl \
		-Wl,-rpath,$(CHROMIUMOBJ)/base,-rpath,$(CHROMIUMOBJ)/crypto \
		-Wl,-rpath,$(CHROMIUMOBJ)/base/third_party/dynamic_annotations \
		-Wl,-rpath,$(CHROMIUMOBJ)/third_party/boringssl \
		-lwebm -lcrcrypto \
		-lbase -lbase_static -lsymbolize -ldynamic_annotations -lboringssl \
		-ldl -lrt -lglib-2.0 -lnss3 \
		-lnssutil3 -lnspr4 -lpthread -o $@

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) -r $(OBJECTS) $(EXE) Makefile.bak

.PHONY: clean
