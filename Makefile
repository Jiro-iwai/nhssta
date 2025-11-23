SHELL = /bin/sh
DIR = src 

CXX      = g++
#CXXFLAGS = -g -Wall -mno-cygwin
CXXFLAGS = -O
CXXLIBS  = 
AR       = ar
ARFLAGS  = cru
RANLIB   = ranlib
LDFLAGS  =

MKOPTS = CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS)" CXXLIBS="$(CXXLIBS)" \
         AR="$(AR)" ARFLAGS="$(ARFLAGS)" RANLIB="$(RANLIB)" \
         LDFLAGS="$(LDFLAGS)"
###

all: target

target:
	@for i in $(DIR) ; \
	     do (test -d $$i && cd $$i && $(MAKE) $(MKOPTS)) || exit 1; done

clean:
	@for i in $(DIR) ; \
	     do (test -d $$i && cd $$i && $(MAKE) $@); done
	cd example; make $@

check:
	cd example; make

test:
	@for i in $(DIR) ; \
	     do (test -d $$i && cd $$i && $(MAKE) test) || exit 1; done

