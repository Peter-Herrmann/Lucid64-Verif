module tb #(
    parameter MAX_XLEN = 7 
) (
    input clk,
    input [MAX_XLEN-1:0] A,
    input [MAX_XLEN-1:0] B
);

    integer cycle;

    initial begin
        cycle = 0;
    end

    always @(posedge clk) begin
        cycle++;
    end

    genvar xlen, depth, offset;
    generate
        for (xlen = 1; xlen <= MAX_XLEN; xlen++) begin: xlen_loop
            for (depth = 0; depth <= xlen; depth++) begin: depth_loop
                for (offset = 0; offset <= depth; offset++) begin: offset_loop
                    wire rst_ni = (cycle != 0);
                    wire vld = (cycle == 1);
                    wire ack;
                    wire [xlen-1:0] quotient, remainder;
                    reg [xlen-1:0] A_del, B_del;

                    // Instantiate the DividerPipeline with generated parameters
                    divider #(.XLEN(xlen), 
                              .STAGE_DEPTH(depth),
                              .STAGE_OFFSET(offset) ) dut (
                        .clk_i(clk),
                        .rst_ni(rst_ni),
                        .op_a_i(A[xlen-1:0]),
                        .op_b_i(B[xlen-1:0]),
                        .req_i(vld),
                        .quotient_o(quotient),
                        .remainder_o(remainder),
                        .done_o(ack)
                    );

                    always @(posedge clk) begin
                        if (~rst_ni) begin
                            A_del <= 0;
                            B_del <= 0;
                        end else if (vld) begin
                            A_del <= A[xlen-1:0];
                            B_del <= B[xlen-1:0];
                        end
                    end

                    wire [xlen-1:0] q_ref = A_del / B_del;
                    wire [xlen-1:0] r_ref = A_del % B_del;

                    `ifdef FORMAL
                        always @(posedge clk) begin
                            if (cycle > 0 && ack && B_del != 0) begin
                                assert(quotient  == q_ref && remainder == r_ref);
                                cover(quotient  == q_ref && remainder == r_ref);
                            end
                        end
                    `endif
                end
            end
        end
    endgenerate

endmodule
