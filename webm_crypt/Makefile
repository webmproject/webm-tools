LIBWEBM = ../../libwebm/
CHROMIUMINC = /home/zoltan.kuscsik/chrome/src/
CHROMIUMOBJ = $(CHROMIUMINC)/out/Debug/lib
OBJECTS = ../shared/webm_endian.o webm_crypt.o
EXE = webm_crypt
INCLUDES = -I$(LIBWEBM) -I../shared -I$(CHROMIUMINC) -I/usr/include/nss \
           -I/usr/include/nspr
CXXFLAGS = -W -Wall -g -DUSE_NSS -std=c++11

$(EXE): $(OBJECTS)
	$(CXX) $(OBJECTS) -L$(LIBWEBM) -L$(CHROMIUMOBJ) -Wl,-rpath \
	$(CHROMIUMOBJ) -lwebm -lcrcrypto -lbase -ldl -lrt -lglib-2.0 -lnss3 \
	-lnssutil3 -lnspr4 -lpthread -o $@

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) -r $(OBJECTS) $(EXE) Makefile.bak

.PHONY: clean
