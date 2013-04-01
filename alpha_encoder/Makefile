LIBWEBM := ../../libwebm
OBJECTS := alpha_encoder.o
EXE := alpha_encoder
INCLUDES = -I$(LIBWEBM)
DEBUG := -g
CXXFLAGS = -W -Wall -O3 $(DEBUG)

$(EXE): $(OBJECTS)
	$(CXX) $(OBJECTS) -L$(LIBWEBM) -lwebm -o $(EXE)

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

all: $(EXE)

clean:
	$(RM) $(OBJECTS) $(EXE) Makefile.bak

.PHONY: all clean
