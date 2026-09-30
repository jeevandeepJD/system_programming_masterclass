`timescale 1ns/1ps
`default_nettype none

module rv32_alu (
    input  logic [31:0] a,
    input  logic [31:0] b,
    input  logic [3:0]  op,
    output logic [31:0] result,
    output logic        eq,
    output logic        lt_signed,
    output logic        lt_unsigned
);
    localparam logic [3:0] ALU_ADD  = 4'd0;
    localparam logic [3:0] ALU_SUB  = 4'd1;
    localparam logic [3:0] ALU_AND  = 4'd2;
    localparam logic [3:0] ALU_OR   = 4'd3;
    localparam logic [3:0] ALU_XOR  = 4'd4;
    localparam logic [3:0] ALU_SLT  = 4'd5;
    localparam logic [3:0] ALU_SLTU = 4'd6;

    always_comb begin
        eq          = (a == b);
        lt_signed   = ($signed(a) < $signed(b));
        lt_unsigned = (a < b);

        case (op)
            ALU_ADD:  result = a + b;
            ALU_SUB:  result = a - b;
            ALU_AND:  result = a & b;
            ALU_OR:   result = a | b;
            ALU_XOR:  result = a ^ b;
            ALU_SLT:  result = {31'd0, lt_signed};
            ALU_SLTU: result = {31'd0, lt_unsigned};
            default:  result = 32'd0;
        endcase
    end
endmodule

module rv32_regfile (
    input  logic        clk,
    input  logic        write_enable,
    input  logic [4:0]  waddr,
    input  logic [31:0] wdata,
    input  logic [4:0]  raddr1,
    input  logic [4:0]  raddr2,
    output logic [31:0] rdata1,
    output logic [31:0] rdata2
);
    // There is deliberately no physical array entry for architectural x0.
    // RV32I does not define reset values for x1..x31, so they are not reset.
    logic [31:0] registers [1:31];

    assign rdata1 = (raddr1 == 5'd0) ? 32'd0 : registers[raddr1];
    assign rdata2 = (raddr2 == 5'd0) ? 32'd0 : registers[raddr2];

    always_ff @(posedge clk) begin
        if (write_enable && (waddr != 5'd0))
            registers[waddr] <= wdata;
    end
endmodule

`default_nettype wire
