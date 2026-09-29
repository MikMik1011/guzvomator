PIO ?= pio

.PHONY: build upload monitor flash clean

build:
	$(PIO) run

upload:
	$(PIO) run -t upload

monitor:
	$(PIO) device monitor

flash: upload monitor

clean:
	$(PIO) run -t clean
