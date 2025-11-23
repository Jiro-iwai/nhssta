SHELL = /bin/sh
DIR = src 

CXX      = g++
#CXXFLAGS = -g -Wall -mno-cygwin
CXXFLAGS = -O -std=c++17
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

# Integration tests (legacy check target)
integration-test:
	cd example; make

# Development setup check
dev-setup:
	@./scripts/dev-setup.sh

# Run all development checks (build, test, lint, coverage)
check:
	@./scripts/run-all-checks.sh

.PHONY: test tidy coverage coverage-clean dev-setup check integration-test

test:
	@for i in $(DIR) ; \
	     do (test -d $$i && cd $$i && $(MAKE) test) || exit 1; done

tidy:
	@for i in $(DIR) ; \
	     do (test -d $$i && cd $$i && $(MAKE) tidy) || exit 1; done

coverage:
	@for i in $(DIR) ; \
	     do (test -d $$i && cd $$i && $(MAKE) coverage) || exit 1; done

coverage-clean:
	@for i in $(DIR) ; \
	     do (test -d $$i && cd $$i && $(MAKE) coverage-clean) || exit 1; done

