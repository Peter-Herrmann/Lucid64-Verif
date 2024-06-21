`include "Lucid64.vh"

module M_alu_tb (
    input              clk_i,
    input              rst_ni,

    input  [63:0]      a_i,
    input  [63:0]      b_i,

    input  [5:0]       alu_operation_i,

    output wire [63:0] res_dut, res_ref,
    output wire        stall_dut, stall_ref
    );

    M_alu_ref reference 
    (
        .clk_i           (clk_i),
        .rst_ni          (rst_ni),

        .a_i             (a_i),
        .b_i             (b_i),
        .alu_operation_i (alu_operation_i),

        .alu_result_oa   (res_ref),
        .stall_o         (stall_ref)
    );

    M_alu dut 
    (
        .clk_i           (clk_i),
        .rst_ni          (rst_ni),

        .a_i             (a_i),
        .b_i             (b_i),
        .alu_operation_i (alu_operation_i),

        .alu_result_oa   (res_dut),
        .stall_o         (stall_dut)
    );

    always @(posedge clk_i) begin

        if (alu_operation_i == `ALU_OP_MUL) 
            assert (res_dut == res_ref);
        else   
            $display("Multiply operation failed formal verification");

        if (alu_operation_i == `ALU_OP_MULH) 
            assert (res_dut == res_ref);
        else
            $display("Multiply Signed (high byte) operation failed formal verification");

        if (alu_operation_i == `ALU_OP_MULHSU) 
            assert (res_dut == res_ref);
        else
            $display("Multiply Signed-Unsigned (high byte) operation failed formal verification");

        if (alu_operation_i == `ALU_OP_MULHU) 
            assert (res_dut == res_ref);
        else
            $display("Multiply Unsigned (high byte) operation failed formal verification");

        if (alu_operation_i == `ALU_OP_MULW) 
            assert (res_dut == res_ref);
        else
            $display("Multiply Word (32 bit) operation failed formal verification");

        if (alu_operation_i == `ALU_OP_DIV) 
            assert (res_dut == res_ref);
        else
            $display("Division Signed operation failed formal verification");

        if (alu_operation_i == `ALU_OP_DIVU) 
            assert (res_dut == res_ref);
        else
            $display("Division Unsigned operation failed formal verification");

        if (alu_operation_i == `ALU_OP_REM) 
            assert (res_dut == res_ref);
        else
            $display("Remainder Signed operation failed formal verification");

        if (alu_operation_i == `ALU_OP_REMU) 
            assert (res_dut == res_ref);
        else   $display("Remainder Unsigned operation failed formal verification");

        if (alu_operation_i == `ALU_OP_DIVW) 
            assert (res_dut == res_ref);
        else
            $display("Division Signed Word operation failed formal verification");

        if (alu_operation_i == `ALU_OP_DIVUW) 
            assert (res_dut == res_ref);
        else
            $display("Division Unsigned Word operation failed formal verification");

        if (alu_operation_i == `ALU_OP_REMW) 
            assert (res_dut == res_ref);
        else
            $display("Remainder Signed Word operation failed formal verification");

        if (alu_operation_i == `ALU_OP_REMUW) 
            assert (res_dut == res_ref);
        else
            $display("Remainder Unsigned Word operation failed formal verification");

    end


endmodule
