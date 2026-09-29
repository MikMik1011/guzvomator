.PHONY: build upload monitor flash clean

build upload monitor flash clean:
	$(MAKE) -C firmware $@
