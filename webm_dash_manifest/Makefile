LIBWEBM := ../../libwebm
OBJECTS := ../shared/indent.o dash_model.o representation.o adaptation_set.o
OBJECTS += period.o webm_dash_manifest.o ../shared/webm_file.o
OBJECTS += ../shared/webm_incremental_reader.o
EXE := webm_dash_manifest
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

