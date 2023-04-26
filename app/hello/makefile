# EPOS Application Makefile

include ../../makedefs

all: install

$(APPLICATION):	$(APPLICATION).o $(LIB)/*
		$(ALD) $(ALDFLAGS) -g -O0 -o $@ $(APPLICATION).o

$(APPLICATION).o: $(APPLICATION).cc $(SRC)
		$(ACC) $(ACCFLAGS) -g -O0 -o $@ $<

install: $(APPLICATION)
		$(INSTALL) $(APPLICATION) $(IMG)

clean:
		$(CLEAN) *.o $(APPLICATION)
