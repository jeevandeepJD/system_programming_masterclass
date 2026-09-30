/*
 * Day 7 synthesis observation.
 *
 * Predict which source constructs imply a mux, an adder, and clocked state.
 * This is intentionally small: the goal is to inspect transformations, not
 * to estimate a real chip from generic Yosys cells.
 */
`timescale 1ns/1ps

module synthesis_demo (
    input  logic       clk,
    input  logic       reset,
    input  logic       enable,
    input  logic       choose_b,
    input  logic [7:0] a,
    input  logic [7:0] b,
    output logic [7:0] result
);
    logic [7:0] selected;
    logic [7:0] next_result;

    always_comb begin
        selected = choose_b ? b : a;
        next_result = selected + 8'd3;
    end

    always_ff @(posedge clk) begin
        if (reset)
            result <= 8'd0;
        else if (enable)
            result <= next_result;
    end
endmodule
