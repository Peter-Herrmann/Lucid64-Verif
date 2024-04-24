SHELL := /bin/bash

VERILATOR = verilator
VERILATOR_FLAGS = --language 1364-2005 --lint-only -Wall -Isrc/

YOSYS = yosys
YOSYS_FLAGS = -s synth/synth_generic.ys

SRC_DIR = src
VERILOG_SOURCES = $(shell find $(SRC_DIR) -name '*.v')
VERILOG_TOP=$(SRC_DIR)/Lucid64.v

# Define colors
RED = \033[1;31m
GREEN = \033[1;32m
BLUE = \033[1;34m
NC = \033[0m # No Color

.PHONY: all lint synth always 

all: lint synth finish

lint: 
	@printf "\n${BLUE}%79s${NC}\n\n" "======================== Linting With Verilator ========================"
	@$(MAKE) -s $(VERILOG_SOURCES:%=%.lint)

synth: 
	@printf "\n${BLUE}%79s${NC}\n\n" "======================= Synthesizing with Yosys ========================"
	@$(MAKE) -s $(VERILOG_TOP:%=results/synth/%.synth.log)

%.lint: %
	@printf "Linting %-47s" "$<"
	@{ $(VERILATOR) $(VERILATOR_FLAGS) $< > /dev/null 2>&1 && printf "${GREEN}%10s${NC}\n" "PASSED"; } \
	|| { printf "${RED}%10s${NC}\n" "FAILED" && $(VERILATOR) $(VERILATOR_FLAGS) $<; }

results/synth/%.synth.log: % always
	@mkdir -p $(dir $@)
	@printf "Synthesizing %-41s" "$<"
	@{ $(YOSYS) $(YOSYS_FLAGS) $< > $@ 2>&1 && ! grep -i -e warning -e error $@ > /dev/null && printf "${GREEN}%10s${NC}\n" "PASSED"; } \
	|| { printf "${RED}%10s${NC}\n" "FAILED" && grep -B 1 -i -e warning -e error $@; }

always: ;

finish: always
	@echo ""