.PHONY: build upload monitor flash test clean

build upload monitor flash test clean:
	$(MAKE) -C firmware $@
