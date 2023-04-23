# EPOS Application Makefile

include ../../makedefs

all: install

$(APPLICATION):	$(APPLICATION).o $(LIB)/*
		$(ALD) $(ALDFLAGS) -g -o $@ $(APPLICATION).o

$(APPLICATION).o: $(APPLICATION).cc $(SRC)
		$(ACC) $(ACCFLAGS) -g -o $@ $<

install: $(APPLICATION)
		$(INSTALL) $(APPLICATION) $(IMG)

clean:
		$(CLEAN) *.o $(APPLICATION)
