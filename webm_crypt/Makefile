LIBWEBM = /media/hkuang/drive1/libwebm
#LIBWEBM = ../../libwebm
OBJECTS = webm_crypt.o
EXE = webm_crypt
INCLUDES = -I$(LIBWEBM) -I../shared
CXXFLAGS = -W -Wall -g -std=c++11

$(EXE): $(OBJECTS)
	$(CXX) $(OBJECTS) -L$(LIBWEBM) \
		-lwebm -lcrypto -lssl \
		-ldl -lrt -lglib-2.0 \
		-lpthread -o $@

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) -r $(OBJECTS) $(EXE) Makefile.bak

.PHONY: clean
