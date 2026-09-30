`timescale 1ns/1ps
`default_nettype none

module alu_register_stage (
    input  logic       clk,
    input  logic       rst,
    input  logic       valid_in,
    input  logic [7:0] a_in,
    input  logic [7:0] b_in,
    input  logic [1:0] op_in,
    output logic       valid_out,
    output logic [7:0] result_out
);
    logic [7:0] alu_result;

    always_comb begin
        case (op_in)
            2'b00:   alu_result = a_in + b_in;
            2'b01:   alu_result = a_in & b_in;
            2'b10:   alu_result = a_in ^ b_in;
            default: alu_result = (a_in < b_in) ? 8'd1 : 8'd0;
        endcase
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            valid_out  <= 1'b0;
            result_out <= 8'd0;
        end else begin
            valid_out  <= valid_in;
            result_out <= alu_result;
        end
    end
endmodule

`default_nettype wire
