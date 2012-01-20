LIBWEBM = ../../libwebm
OBJECTS = indent.o dash_model.o representation.o adaptation_set.o period.o webm_dash_manifest.o webm_file.o
EXE = webm_dash_manifest
INCLUDES = -I$(LIBWEBM)
CXXFLAGS = -W -Wall -g

$(EXE): $(OBJECTS)
	$(CXX) $(OBJECTS) -L$(LIBWEBM) -lwebm -o $(EXE)

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) -r $(OBJECTS) $(EXE) Makefile.bak

.PHONY: clean

