PIO ?= pio

.PHONY: build upload monitor flash test clean

build:
	$(PIO) run -e xiao_esp32c6

upload:
	$(PIO) run -e xiao_esp32c6 -t upload

monitor:
	$(PIO) device monitor

flash: upload monitor

test:
	$(PIO) test -e native

clean:
	$(PIO) run -e xiao_esp32c6 -t clean
