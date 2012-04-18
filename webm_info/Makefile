LIBWEBM := ../../libwebm
OBJECTS := ../shared/indent.o ../shared/webm_endian.o webm_info.o
EXE := webm_info
INCLUDES = -I$(LIBWEBM) -I../shared
DEBUG := -g
CXXFLAGS = -W -Wall -O2 $(DEBUG)

$(EXE): $(OBJECTS)
	$(CXX) $(OBJECTS) -L$(LIBWEBM) -lwebm -o $(EXE)

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

all: $(EXE)

clean:
	$(RM) $(OBJECTS) $(EXE) Makefile.bak

.PHONY: all clean
