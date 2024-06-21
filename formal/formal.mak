SHELL := /bin/bash

CONFIG=config.sby

.PHONY: all formal

formal:
	sby -f config.sby
