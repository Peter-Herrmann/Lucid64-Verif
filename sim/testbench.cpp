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
#include <random>
#include <chrono>


std::string hexString(uint64_t num, int width);
void writeBytes(uint64_t* dest, uint8_t strobe, uint64_t writeValue);
std::vector<uint64_t> readHexFile(const std::string& filename);
uint64_t readMemory(uint64_t addr, const std::vector<uint64_t>& memory, const std::vector<uint64_t>& bootloader,
                    std::unordered_map<uint64_t, uint64_t>& dynamic_memory,
                    uint64_t mem_max, uint64_t text_offset, uint64_t bl_size, uint64_t sig_addr, uint64_t sig_file_data);
bool setGnt(int *gnt_delay, bool *read_outstanding, bool req, bool we, bool rvalid);
bool setRvalid(int *rvalid_delay, bool *read_outstanding);
int genGntDelay();
int genRvalidDelay();


unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
std::default_random_engine generator(seed);
std::uniform_int_distribution<int> delay_time_dist(0, 5);
std::bernoulli_distribution no_delay_dist(0.875);


///////////////////////////////////////////////////////////////////////////////////////////////////
//                                              Main                                             //
///////////////////////////////////////////////////////////////////////////////////////////////////


int main(int argc, char** argv) 
{
    Verilated::commandArgs(argc, argv);
    VLucid64* cpu = new VLucid64;
    std::ofstream signatureFile("DUT-Lucid64.signature");
    VerilatedVcdC* vcd = new VerilatedVcdC;

    try 
    {
        std::cout << "Test beginning with random seed: " << seed << std::endl;

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
        const vluint64_t timeout_value  = 1000000;

        // Persistent data buffers for timing
        uint32_t queued_instr      = 0;
        uint32_t latched_instr     = 0;
        bool     latched_i_rvalid  = 0;
        bool     queued_i_rvalid   = 0;

        uint64_t queued_data_read  = 0;
        uint64_t latched_data_read = 0;
        bool     queued_d_rvalid   = 0;
        bool     latched_d_rvalid  = 0;

        // Variable Memory Latency Timing State
        int  i_gnt_delay        = genGntDelay();
        int  i_rvalid_delay     = genRvalidDelay();
        bool i_read_outstanding = false;

        int  d_gnt_delay        = genGntDelay();
        int  d_rvalid_delay     = genRvalidDelay();
        bool d_read_outstanding = false;


        std::cout << "\n\n" <<
                     "Memory Size:     " << hexString((uint64_t)mem_size, 8) << "\n" <<
                     "Bootloader Size: " << hexString((uint64_t)bl_size,  8) << "\n";

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

            bool posedge        = (main_time % 4) == 1;
            bool negedge        = (main_time % 4) == 3;
            bool mem_resp_cycle = (main_time % 4) == 2;
            bool mem_gnt_cycle  = (main_time % 4) == 0;

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

            // Delay data reads one sim cycle (1/4 clock period) or hold times violated, timing will be wrong
            if (mem_resp_cycle)
            {
                cpu->imem_rdata_i  = queued_instr;
                cpu->imem_rvalid_i = queued_i_rvalid;
                cpu->dmem_rdata_i  = queued_data_read;
                cpu->dmem_rvalid_i = queued_d_rvalid;
            }

            if (negedge)
            {
                cpu->imem_gnt_i = setGnt(&i_gnt_delay, &i_read_outstanding, i_req, false, cpu->imem_rvalid_i);
                cpu->dmem_gnt_i = setGnt(&d_gnt_delay, &d_read_outstanding, d_req, d_we,  cpu->dmem_rvalid_i);
            }

            if (mem_gnt_cycle)
            {
                queued_i_rvalid = setRvalid(&i_rvalid_delay, &i_read_outstanding);
                queued_d_rvalid = setRvalid(&d_rvalid_delay, &d_read_outstanding);
            }

            // Instruction Reads
            if (queued_i_rvalid && posedge) 
            {
                uint64_t instruction_double = readMemory(i_addr_whole, memory, bootloader, 
                                                         dynamic_memory, mem_max, text_offset, 
                                                         bl_size, sig_addr, sig_file_data);

                queued_instr = (i_offset_word) ? (uint32_t)(instruction_double >> 32) :
                                                 (uint32_t)(instruction_double);
            }


            // Data Reads
            if (queued_d_rvalid && posedge) 
            {
                queued_data_read = readMemory(d_addr_whole, memory, bootloader, dynamic_memory, 
                                              mem_max, text_offset, bl_size, sig_addr, sig_file_data);

                std::cout << "READ ADDR: " + hexString((uint64_t)d_addr_whole, 16) +
                             " DATA: " + hexString((uint64_t)queued_data_read, 16);
            }

            //////////////////////////
            // MEMORY WRITE CONTROL //
            //////////////////////////
            if (cpu->dmem_gnt_i && d_we && posedge) 
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
            if (posedge) 
            {
                std::cout << " INSTRUCTION_ADDR: " + hexString((uint64_t)i_addr_whole,  8)
                           + " INST: "             + hexString((uint64_t)queued_instr, 8)
                           + "\n";
            }
            
            // Dump waveforms to VCD
            vcd->dump(main_time);
            cpu->eval();
        }
    } 
    catch (const std::out_of_range& e)
    {
        std::cout << "Error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) 
    {
        std::cout << "Standard Exception: " << e.what() << std::endl;
    }
    catch (...) 
    {
        std::cout << "Unknown Exception occurred." << std::endl;
    }

    
    vcd->close();
    signatureFile.close();
    Verilated::threadContextp()->coveragep()->write("coverage.dat");
    delete cpu;
    delete vcd;

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                       Utility Functions                                       //
///////////////////////////////////////////////////////////////////////////////////////////////////


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
    std::ifstream hexFile(filename, std::ios::binary);
    if (!hexFile.is_open()) {
        std::cout << "Failed to open file: " << filename << std::endl;
        return {};
    }

    std::vector<uint64_t> memory;
    std::string line;
    uint64_t value;

    while (std::getline(hexFile, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // std::cout << "Line read: " << line << std::endl; // Debug output

        line.erase(std::remove_if(line.begin(), line.end(), [](char c) { return !std::isxdigit(c); }), line.end());

        if (!line.empty()) {
            std::istringstream(line) >> std::hex >> value;
            memory.push_back(value);
        }

        // std::cout << "Current vector size: " << memory.size() << std::endl; // Debug output
    }
    hexFile.close();

    return memory;
}


uint64_t readMemory(uint64_t addr, const std::vector<uint64_t>& memory, const std::vector<uint64_t>& bootloader,
                    std::unordered_map<uint64_t, uint64_t>& dynamic_memory,
                    uint64_t mem_max, uint64_t text_offset, uint64_t bl_size, uint64_t sig_addr, uint64_t sig_file_data) 
{
    uint64_t mem_idx = (addr - text_offset) >> 3;
    uint64_t bl_idx = addr >> 3;
    uint64_t bl_max = bl_size * 8;

    if (addr < mem_max && addr >= text_offset)
        return memory[mem_idx];
    else if (bl_idx < bl_size)
        return bootloader[bl_idx];
    else if (addr == sig_addr)
        return sig_file_data;
    else if (dynamic_memory.count(addr))
        return dynamic_memory[addr];
    else if ((addr >= mem_max && addr < mem_max + 0x10) 
          || (addr >= bl_max  && addr < bl_max  + 0x10))
        return 0x0000001300000013; 
    else
        throw std::out_of_range("Memory address out of range: " + hexString(addr, 8));
}


bool setGnt(int *gnt_delay, bool *read_outstanding, bool req, bool we, bool rvalid)
{
    if (!req || (*read_outstanding && !rvalid))
    {
        *gnt_delay = genGntDelay();
    }
    else if ( ( (*gnt_delay)-- ) <= 0 )
    {
        *gnt_delay = genGntDelay();
        *read_outstanding = (!we);
        return true;
    }
    return false;
}


bool setRvalid(int *rvalid_delay, bool *read_outstanding)
{
    if (!(*read_outstanding))
    {
        *rvalid_delay = genRvalidDelay();
    }
    else if ( ( (*rvalid_delay)-- ) <= 0 )
    {
        *rvalid_delay = genRvalidDelay();
        *read_outstanding = false;
        return true;
    }
    return false;
}

int genGntDelay()    { return no_delay_dist(generator) ? 0 : delay_time_dist(generator); }  
int genRvalidDelay() { return no_delay_dist(generator) ? 0 : delay_time_dist(generator); }
