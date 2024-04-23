SRC_DIR=../src
RTL_TOP=$(SRC_DIR)/Lucid64.v
INC_DIRS=$(SRC_DIR)

TEST_CPP=testbench.cpp
TEST_EXE=test_exe
SIGNATURE=DUT-Lucid64.signature
MEM_HEX=test.hex
MEM_ELF=my.elf

$(MEM_HEX): 
	riscv64-unknown-elf-objcopy -O binary $(MEM_ELF) tmp.bin
	python3 -c "import sys; \
				data = open('tmp.bin', 'rb').read(); \
				lines = [data[i:i+8] for i in range(0, len(data), 8)]; \
				reversed_lines = [line[::-1] for line in lines]; \
				open('tmp_reversed.bin', 'wb').write(b''.join(reversed_lines))"
	xxd -p -c 8 tmp_reversed.bin $@


$(TEST_EXE):
	verilator -Wall -O3 --trace --cc $(RTL_TOP) --exe $(TEST_CPP) -I$(INC_DIRS) -o $@ --build

$(SIGNATURE): $(TEST_EXE) $(MEM_HEX)
	./obj_dir/$(TEST_EXE) >> log.txt
	printf "\n\nTest complete, Signature File:\n\n" >> log.txt
	cat $(SIGNATURE) >> log.txt
	printf "\n\nEnd Signature:\n\n" >> log.txt
	rm $(MEM_HEX)

.PHONY: compile clean run

compile: $(TEST_EXE)

clean:
	rm -f *.o *.out *.hex *.signature *.bin *.txt $(TEST_EXE)
	rm -rf obj_dir

run: $(SIGNATURE)