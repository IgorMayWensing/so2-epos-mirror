# EPOS Source Makefile

include	../makedefs

SUBDIRS := utility architecture machine transducer network api setup boot system init

all:		$(SUBDIRS)

$(SUBDIRS):	FORCE
		(cd $@ && $(MAKE))

clean:
		make MAKE:="$(MAKECLEAN)" $(SUBDIRS)

FORCE:
