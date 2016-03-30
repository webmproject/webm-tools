LIBWEBM = ../../libwebm
OBJECTS = webm_crypt.o
EXE = webm_crypt
INCLUDES = -I$(LIBWEBM) -I../shared
ALL_CXXFLAGS = $(INCLUDES) -W -Wall -g -std=c++11 $(CXXFLAGS)

$(EXE): $(OBJECTS)
	$(CXX) $(OBJECTS) -L$(LIBWEBM) \
		-lwebm -lcrypto -ldl -o $@

%.o: %.cc
	$(CXX) -c $(ALL_CXXFLAGS) $< -o $@

clean:
	$(RM) -r $(OBJECTS) $(EXE) Makefile.bak

.PHONY: clean
