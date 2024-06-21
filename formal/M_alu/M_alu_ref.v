///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Module Name: M_alu_ref                                                                        //
// Description: Arithmetic and Logic Unit (ALU) for the M extension (integer multiplication and  //
//              division) of a RV64I processor.                                                  //
// Author     : Peter Herrmann                                                                   //
//                                                                                               //
// SPDX-License-Identifier: CC-BY-NC-ND-4.0                                                      //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////
`include "Lucid64.vh"


module M_alu_ref (
    input               clk_i,
    input               rst_ni,

    input  [`XLEN-1:0]  a_i,
    input  [`XLEN-1:0]  b_i,

    input  [5:0]        alu_operation_i,

    output [`XLEN-1:0]  alu_result_oa,
    output reg          stall_o
);


    ///////////////////////////////////////////////////////////////////////////////////////////////
    //                                       Timing Control                                      //
    ///////////////////////////////////////////////////////////////////////////////////////////////
    
    reg [`XLEN-1:0] a_r, b_r;
    reg op_mul_r;

    // TODO - This is total bogus to test the M extension - need to pipeline this
    // Also, need to understand the carry_su thing
    always @(*) begin
        a_r = a_i;
        b_r = b_i;
        op_mul_r = (alu_operation_i == `ALU_OP_MUL   || 
                    alu_operation_i == `ALU_OP_MULH  || 
                    alu_operation_i == `ALU_OP_MULHSU|| 
                    alu_operation_i == `ALU_OP_MULHU || 
                    alu_operation_i == `ALU_OP_MULW  );
        stall_o = 'b0;
    end

    wire signed [`XLEN-1:0] a_sr  = $signed(a_r);
    wire signed [`XLEN-1:0] b_sr  = $signed(b_r);
    wire        [31:0]      a_32  = a_r[31:0];
    wire        [31:0]      b_32  = b_r[31:0];
    wire signed [31:0]      a_s32 = a_sr[31:0];
    wire signed [31:0]      b_s32 = b_sr[31:0];


    ///////////////////////////////////////////////////////////////////////////////////////////////
    //                                       Multiplication                                      //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    reg    [((2*`XLEN)-1):0]      product_u, product_s, product_su_u;
    reg signed [`XLEN-1:0]  product_su, product_hsu, abs_a, carry_su;
    reg        [`XLEN-1:0]  mul_res_a, product_wu;

    always @(*) begin
        product_s  = a_sr * b_sr;
        product_u  = a_r  * b_r;
        product_wu = a_32 * b_32;

        // MULHSU calculation: if a is negative, negate and multiply unsigned, then negate result
        //                     if a is positive or 0, result is same as unsigned MULHU
        abs_a        = a_sr[`XLEN-1] ? -a_sr : a_sr;
        product_su_u = abs_a * b_r;
        product_su   = $signed(product_su_u[((2*`XLEN)-1):`XLEN]);
        carry_su     = (product_su_u[`XLEN-1:0] == 'b0) ? 'b1 : 'b0;
        product_hsu  = a_sr[`XLEN-1] ? ~product_su + carry_su : product_su;

        case (alu_operation_i)
            `ALU_OP_MUL     : mul_res_a = product_u[`XLEN-1:0];
            `ALU_OP_MULH    : mul_res_a = product_s[((2*`XLEN)-1):`XLEN];
            `ALU_OP_MULHSU  : mul_res_a = product_hsu;
            `ALU_OP_MULHU   : mul_res_a = product_u[((2*`XLEN)-1):`XLEN];
            `ALU_OP_MULW    : mul_res_a = { {32{product_wu[31]}}, product_wu[31:0] };
            default         : mul_res_a = 'b0;
        endcase
    end

    // Resolve this when pipelining!
    wire _unused = &{clk_i, rst_ni, product_s[`XLEN-1:0], product_su_u[`XLEN-1:0], product_wu[`XLEN-1:32]};


    ///////////////////////////////////////////////////////////////////////////////////////////////
    //                                          Division                                         //
    ///////////////////////////////////////////////////////////////////////////////////////////////

    reg        [`XLEN-1:0] quotient_u,  rem_u;
    reg signed [`XLEN-1:0] quotient_s,  rem_s;
    reg        [31:0] quotient_wu, rem_wu;
    reg signed [31:0] quotient_ws, rem_ws;
    reg        [`XLEN-1:0] div_res_a;

    always @(*) begin
        quotient_s  = (b_sr == 0) ? $signed(`NEGATIVE_1)   : a_sr  / b_sr;
        quotient_u  = (b_r  == 0) ? `NEGATIVE_1            : a_r   / b_r;
        quotient_ws = (b_32 == 0) ? $signed(`NEGATIVE_1_W) : a_s32 / b_s32;
        quotient_wu = (b_32 == 0) ? `NEGATIVE_1_W          : a_32  / b_32;
        rem_s       = (b_sr == 0) ? a_sr                   : a_sr  % b_sr;
        rem_u       = (b_r  == 0) ? a_r                    : a_r   % b_r;
        rem_ws      = (b_32 == 0) ? a_s32                  : a_s32 % b_s32;
        rem_wu      = (b_32 == 0) ? a_32                   : a_32  % b_32;

        // Check for signed division overflow (64-bit)
        if ( (a_sr == `DIV_MOST_NEG_INT) && (b_sr == `NEGATIVE_1) ) begin
            quotient_s  = $signed(`DIV_MOST_NEG_INT);
            rem_s       = 'b0;
        end

        // Check for signed division overflow (32-bit)
        if ( (a_s32 == `DIV_MOST_NEG_INT_W) && (b_s32 == `NEGATIVE_1_W) ) begin
            quotient_ws = `DIV_MOST_NEG_INT_W;
            rem_ws      = 'b0;
        end

        case (alu_operation_i)
            `ALU_OP_DIV     : div_res_a = quotient_s;
            `ALU_OP_DIVU    : div_res_a = quotient_u;
            `ALU_OP_REM     : div_res_a = rem_s;
            `ALU_OP_REMU    : div_res_a = rem_u;
            `ALU_OP_DIVW    : div_res_a = { {32{quotient_ws[31]}}, quotient_ws };
            `ALU_OP_DIVUW   : div_res_a = { {32{quotient_wu[31]}}, quotient_wu };
            `ALU_OP_REMW    : div_res_a = { {32{rem_ws[31]}}, rem_ws };
            `ALU_OP_REMUW   : div_res_a = { {32{rem_wu[31]}}, rem_wu };
            default         : div_res_a = 'b0;
        endcase
    end 

    assign alu_result_oa = op_mul_r ? mul_res_a : div_res_a;

endmodule


///////////////////////////////////////////////////////////////////////////////////////////////////
////   Copyright 2024 Peter Herrmann                                                           ////
////                                                                                           ////
////   Licensed under the Creative Commons Attribution-NonCommercial-NoDerivatives 4.0         ////
////   International License (the "License"); you may not use this file except in compliance   ////
////   with the License. You may obtain a copy of the License at                               ////
////                                                                                           ////
////       https://creativecommons.org/licenses/by-nc-nd/4.0/                                  ////
////                                                                                           ////
////   Unless required by applicable law or agreed to in writing, software                     ////
////   distributed under the License is distributed on an "AS IS" BASIS,                       ////
////   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.                ////
////   See the License for the specific language governing permissions and                     ////
////   limitations under the License.                                                          ////
///////////////////////////////////////////////////////////////////////////////////////////////////
