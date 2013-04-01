LIBVPX := ../../libvpx
GCC := gcc
OBJECTS := alpha_encoder.c.o
SRC := alpha_encoder.c
EXTERNAL_OBJECTS = ../../libvpx/libmkv/EbmlWriter.c.o
EXE := alpha_encoder
INCLUDES = -I$(LIBVPX)
DEBUG := -g
GCCFLAGS = -O3 $(DEBUG) -c

$(EXE): $(OBJECTS)
	$(GCC) $(OBJECTS) $(EXTERNAL_OBJECTS) -L$(LIBVPX) -lvpx -lm -lpthread -o $(EXE)

$(OBJECTS):$(SRC)
	$(GCC) $(GCCFLAGS) $(INCLUDES) -o $(OBJECTS) $(SRC)

all: $(EXE)

clean:
	$(RM) $(OBJECTS) $(EXE) Makefile.bak

.PHONY: all clean