LIBWEBM = ../libwebm
OBJECTS = indent.o manifest_model.o media.o media_group.o adaptive_prototype_manifest.o
EXE = adaptive_prototype_manifest 
CFLAGS = -W -Wall -g -I$(LIBWEBM)

$(EXE): $(OBJECTS) 
	$(CXX) $(OBJECTS)  -L$(LIBWEBM) -lmkvparser -o $(EXE)

indent.o: indent.cc
	$(CXX) -c $(CFLAGS) indent.cc  -o indent.o

manifest_model.o: manifest_model.cc
	$(CXX) -c $(CFLAGS) manifest_model.cc  -o manifest_model.o

media.o: media.cc
	$(CXX) -c $(CFLAGS) media.cc  -o media.o

media_group.o: media_group.cc
	$(CXX) -c $(CFLAGS) media_group.cc  -o media_group.o

adaptive_prototype_manifest.o: adaptive_prototype_manifest.cc
	$(CXX) -c $(CFLAGS) adaptive_prototype_manifest.cc  -o adaptive_prototype_manifest.o

clean:
	rm -rf $(OBJECTS) $(EXE) Makefile.bak
