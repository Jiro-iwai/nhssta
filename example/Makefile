SHELL = /bin/sh

all: 
	@cd ../src && $(MAKE) DEBUG=0
	sh ./nhssta_test

clean:
	rm -f *_ result*_ test
	rm -rf __pycache__ 2>/dev/null || true


