`timescale 1ns/1ps

module synthesis_demo_tb;
    logic       clk = 1'b0;
    logic       reset = 1'b1;
    logic       enable = 1'b0;
    logic       choose_b = 1'b0;
    logic [7:0] a = 8'd0;
    logic [7:0] b = 8'd0;
    logic [7:0] result;

    synthesis_demo dut (
        .clk,
        .reset,
        .enable,
        .choose_b,
        .a,
        .b,
        .result
    );

    always #5 clk = ~clk;

    task automatic check_result(input logic [7:0] wanted, input string label);
        if (result !== wanted) begin
            $error("%s: expected %0d, observed %0d", label, wanted, result);
            $fatal(1);
        end
    endtask

    initial begin
        @(posedge clk);
        #1 check_result(8'd0, "reset");

        reset = 1'b0;
        enable = 1'b1;
        a = 8'd10;
        b = 8'd40;
        choose_b = 1'b0;
        @(posedge clk);
        #1 check_result(8'd13, "choose a, add three");

        choose_b = 1'b1;
        @(posedge clk);
        #1 check_result(8'd43, "choose b, add three");

        enable = 1'b0;
        a = 8'd99;
        b = 8'd100;
        @(posedge clk);
        #1 check_result(8'd43, "disabled register holds state");

        $display("PASS: mux, addition, reset, enable, and registered state");
        $finish;
    end
endmodule
