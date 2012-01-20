LIBWEBM := ../../libwebm
OBJECTS := indent.o dash_model.o representation.o adaptation_set.o period.o
OBJECTS += webm_dash_manifest.o webm_file.o
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
	$(RM) -r $(OBJECTS) $(EXE) Makefile.bak

.PHONY: clean

