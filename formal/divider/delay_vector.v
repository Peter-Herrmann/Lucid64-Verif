module delay_vector #(
    parameter WIDTH = 64,  // Bit width of the vector
    parameter DELAY = 10   // Number of cycles to delay
)(
    input clk,
    input rst_n,
    input [WIDTH-1:0] data_in,
    output [WIDTH-1:0] data_out
);

// Declare the shift register array
reg [WIDTH-1:0] shift_reg[DELAY-1:0];

integer i;

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        // Reset all elements of the shift register to 0
        for (i = 0; i < DELAY; i = i + 1) begin
            shift_reg[i] <= 0;
        end
    end else begin
        // Shift the data through the register every clock cycle
        shift_reg[0] <= data_in;
        for (i = 1; i < DELAY; i = i + 1) begin
            shift_reg[i] <= shift_reg[i-1];
        end
    end
end

// Output the last element of the shift register
assign data_out = shift_reg[DELAY-1];

endmodule
