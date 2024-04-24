#include <verilated.h>
#include "VLucid64.h"
#include <verilated_vcd_c.h>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdexcept> 


std::string hexString(uint64_t num, int width);
void writeBytes(uint64_t* dest, uint8_t strobe, uint64_t writeValue);
std::vector<uint64_t> readHexFile(const std::string& filename);


int main(int argc, char** argv) 
{
    Verilated::commandArgs(argc, argv);
    VLucid64* cpu = new VLucid64;
    std::ofstream signatureFile("DUT-Lucid64.signature");
    VerilatedVcdC* vcd = new VerilatedVcdC;

    try 
    {
        // Initialize VCD tracing
        Verilated::traceEverOn(true);
        cpu->trace(vcd, 99);
        vcd->open("output.vcd");

        std::vector<uint64_t> memory     = readHexFile("test.hex");
        std::vector<uint64_t> bootloader = readHexFile("bootloader.hex");

        // Define a dynamic memory map for writes outside of defined memory
        std::unordered_map<uint64_t, uint64_t> dynamic_memory;
        uint64_t sig_file_data = 0;

        // Constants
        const uint64_t   text_offset    = 0x80000000;
        const uint32_t   mem_size       = memory.size();
        const uint64_t   mem_max        = text_offset + memory.size()*8;
        const uint64_t   sig_addr       = 0x00000000FFFFFFF8;
        const uint64_t   end_addr       = 0x00000000F0F0F0F0;
        const uint32_t   bl_size        = bootloader.size();
        const vluint64_t timeout_value  = 2000000;

        // Persistent data buffers for timing
        uint32_t queued_instr      = 0;
        uint32_t latched_instr     = 0;
        uint64_t queued_data_read  = 0;
        uint64_t latched_data_read = 0;

        std::cout << "\n\nMemory Size: " + hexString((uint64_t)memory.size(), 8) + "\n";

        vluint64_t main_time = 0; // Add a simulation time variable
        cpu->clk_i = 0;
        cpu->rst_ni = 0;

        cpu->imem_gnt_i    = 1;
        cpu->imem_rvalid_i = 1;

        cpu->dmem_gnt_i    = 1;
        cpu->dmem_rvalid_i = 1;

        while (!Verilated::gotFinish()) 
        {
            // Timing Control
            main_time++;
            if (main_time > 10)
                cpu->rst_ni = 1;
            if (main_time % 2)
                cpu->clk_i = !cpu->clk_i;
            if (main_time > timeout_value)
                throw std::out_of_range("Timed out.");

            // Logging only for alert signal, arch tests do not quit on unaligned access
            if (cpu->alert_o && cpu->clk_i)
                std::cout << "CPU alert signal received";

            // Non-persistent dynamic variables
            bool     i_req          =     (bool)cpu->imem_req_o;
            uint64_t i_addr_whole   = (uint64_t)cpu->imem_addr_o;
            uint64_t i_bl_idx       = (i_addr_whole) >> 3;
            uint64_t i_mem_idx      = (i_addr_whole - text_offset) >> 3;
            bool     i_offset_word  = (((i_addr_whole >> 2) % 2) != 0);

            bool     d_req          =     (bool)cpu->dmem_req_o;
            bool     d_we           =     (bool)cpu->dmem_we_o;
            uint8_t  d_be           =  (uint8_t)cpu->dmem_be_o;
            uint32_t d_addr_whole   = (uint32_t)cpu->dmem_addr_o;
            uint32_t d_bl_idx       = (d_addr_whole) >> 3;
            uint32_t d_mem_idx      = (d_addr_whole - text_offset) >> 3;

            uint64_t write_data     = (uint64_t)cpu->dmem_wdata_o;

            /////////////////////////
            // MEMORY READ CONTROL //
            /////////////////////////

            // Delay data reads on sim cycle (1/4 clock period) or hold times violated, timing will be wrong
            cpu->imem_rdata_i = latched_instr;
            cpu->dmem_rdata_i = latched_data_read;
            if (cpu->clk_i)
            {
                latched_instr     = queued_instr;
                latched_data_read = queued_data_read;
            }

            // Instruction Reads
            if (i_req && ~cpu->clk_i)
            {
                // Main memory
                if (i_addr_whole < mem_max && i_addr_whole >= text_offset)
                    queued_instr = (i_offset_word) ? 
                                    (uint32_t) (memory[i_mem_idx] >> 32) : 
                                    (uint32_t) (memory[i_mem_idx]);
                // Bootloader
                else if (i_bl_idx < bl_size) 
                    queued_instr = (i_offset_word) ? 
                                    (uint32_t) (bootloader[i_bl_idx] >> 32) : 
                                    (uint32_t) (bootloader[i_bl_idx]);
                // Signature
                else if (i_addr_whole == sig_addr)
                    queued_instr = (i_offset_word) ? 
                                    (uint32_t) (sig_file_data >> 32) : 
                                    (uint32_t) (sig_file_data);
                // Reading Previously Written Values Anywhere Else
                else if (dynamic_memory.count(i_addr_whole))
                    queued_instr = (i_offset_word) ? 
                                    (uint32_t) (dynamic_memory[d_addr_whole] >> 32) : 
                                    (uint32_t) (dynamic_memory[d_addr_whole]);
                // Misprediction buffer
                else if ((i_mem_idx >= mem_size && i_mem_idx < mem_size + 5) 
                    || (i_bl_idx >= bl_size && i_bl_idx < bl_size + 5))
                    queued_instr = 0x00000013;
                // Invalid access
                else 
                    throw std::out_of_range("INSTRUCTION_ADDR out of range: " + hexString((uint64_t)i_addr_whole, 8));
            }


            // Data Reads
            if (d_req && !d_we && ~cpu->clk_i)
            {
                // Main memory
                if (d_addr_whole < mem_max && d_addr_whole >= text_offset)
                    queued_data_read = memory[d_mem_idx];
                // Bootloader
                else if (d_bl_idx < bl_size-1)
                    queued_data_read = bootloader[d_bl_idx];
                // Signature
                else if (d_addr_whole == sig_addr)
                    queued_data_read = sig_file_data;
                // Reading Previously Written Values Anywhere Else
                else if (dynamic_memory.count(d_addr_whole))
                    queued_data_read = dynamic_memory[d_addr_whole];
                // Invalid access
                else
                {
                    queued_data_read = 0x0;
                    throw std::out_of_range("Reading uninitialized memory: " + hexString((uint64_t)d_addr_whole, 8));
                }
                if (main_time % 2)
                    std::cout << "READ ADDR: "  + hexString((uint64_t)d_addr_whole, 16) + 
                                " DATA: "       + hexString((uint64_t)queued_data_read, 16);
            }

            //////////////////////////
            // MEMORY WRITE CONTROL //
            //////////////////////////
            if (d_req && d_we && cpu->clk_i && (main_time % 2)) 
            {

                std::cout << "WRITE ADDR: " + hexString((uint64_t)d_addr_whole, 16) + 
                            " DATA: "       + hexString((uint64_t)write_data, 16)   +
                            " STROBE: "     + hexString((uint64_t)d_be, 2) << std::endl;
                // Signature
                if (d_addr_whole == sig_addr) 
                {
                    writeBytes(&sig_file_data, d_be, write_data);
                    signatureFile << std::setfill('0') << std::setw(8) << std::hex << (sig_file_data) << std::endl;
                }
                // Main memory
                else if (d_addr_whole < mem_max && d_addr_whole >= text_offset)
                    writeBytes(&(memory[d_mem_idx]), d_be, write_data);
                // End Simulation
                else if (d_addr_whole == end_addr)
                    break;
                // Writing outside defined regions (store so it can be read back)
                else
                    dynamic_memory[d_addr_whole] = write_data;
            }

            //////////////////////////
            // Post Cycle Analytics //
            //////////////////////////
            // Logging (once per clock cycle)
            if (cpu->clk_i && main_time % 2) 
            {
                std::cout << " INSTRUCTION_ADDR: " + hexString((uint64_t)i_addr_whole, 8)
                           + " INST: " + hexString((uint64_t)latched_instr, 8)
                           + "\n";
            }
            
            // Dump waveforms to VCD
            vcd->dump(main_time);
            cpu->eval();
        }
    } 
    catch (const std::out_of_range& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
    }
    catch (...) 
    {
        std::cerr << "Unknown Exception occurred." << std::endl;
    }

    
    vcd->close();
    signatureFile.close();
    Verilated::threadContextp()->coveragep()->write("coverage.dat");
    delete cpu;
    delete vcd;

    return 0;
}



std::string hexString(uint64_t num, int width) 
{
    std::stringstream ss;
    ss << "0x"
       << std::setfill('0') << std::setw(width)
       << std::hex << num;
    return ss.str();
}


void writeBytes(uint64_t* dest, uint8_t strobe, uint64_t writeValue) 
{
    if (!dest)
        throw std::out_of_range("Writing to null pointer");

    for (int i = 0; i < 8; ++i) 
    {
        uint8_t mask = 1 << i;  // Create a mask for each byte position
        if (strobe & mask) 
        {
            // Clear the byte we want to change, then OR it with the new byte from writeValue
            *dest = (*dest & ~(0xFFull << (i * 8))) | ((writeValue & (0xFFull << (i * 8))));
        }
    }
}


std::vector<uint64_t> readHexFile(const std::string& filename) {
    // Open the file to count lines
    std::ifstream hexFileCount(filename);
    if (!hexFileCount.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return {};
    }

    // Count the lines in the file
    std::string tempLine;
    size_t lineCount = std::count(std::istreambuf_iterator<char>(hexFileCount),
                                  std::istreambuf_iterator<char>(), '\n');
    hexFileCount.close();

    // Resize vector to hold the values from the file
    std::vector<uint64_t> memory(lineCount);

    // Open the file to read hex values
    std::ifstream hexFile(filename);
    if (!hexFile.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return {};
    }

    std::string line;
    size_t address = 0;
    uint64_t value;
    while (std::getline(hexFile, line)) {
        // Remove any non-hexadecimal characters
        line.erase(std::remove_if(line.begin(), line.end(), [](char c) { return !std::isxdigit(c); }), line.end());

        // Convert the cleaned line from hex to an integer
        std::istringstream(line) >> std::hex >> value;
        memory[address++] = value;
    }
    hexFile.close();

    return memory;
}