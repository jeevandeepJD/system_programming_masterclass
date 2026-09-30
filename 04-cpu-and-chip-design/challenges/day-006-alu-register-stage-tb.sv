`timescale 1ns/1ps
`default_nettype none

module day_006_alu_register_stage_tb;
    logic       clk;
    logic       rst;
    logic       valid_in;
    logic [7:0] a_in;
    logic [7:0] b_in;
    logic [1:0] op_in;
    logic       valid_out;
    logic [7:0] result_out;

    int unsigned checks;

    alu_register_stage dut (
        .clk        (clk),
        .rst        (rst),
        .valid_in   (valid_in),
        .a_in       (a_in),
        .b_in       (b_in),
        .op_in      (op_in),
        .valid_out  (valid_out),
        .result_out (result_out)
    );

    initial begin
        clk = 1'b0;
        forever #5 clk = ~clk;
    end

    task automatic drive_and_check(
        input logic [7:0] a,
        input logic [7:0] b,
        input logic [1:0] op,
        input logic [7:0] expected
    );
        @(negedge clk);
        valid_in = 1'b1;
        a_in = a;
        b_in = b;
        op_in = op;

        @(posedge clk);
        #1;
        assert (valid_out === 1'b1)
            else $fatal(1, "valid_out was not asserted");
        assert (result_out === expected)
            else $fatal(1,
                        "op=%0b a=%0h b=%0h expected=%0h observed=%0h",
                        op, a, b, expected, result_out);
        checks++;
    endtask

    initial begin
        $dumpfile("day-006-alu-register-stage.vcd");
        $dumpvars(0, day_006_alu_register_stage_tb);

        rst = 1'b1;
        valid_in = 1'b0;
        a_in = 8'd0;
        b_in = 8'd0;
        op_in = 2'b00;
        checks = 0;

        repeat (2) @(posedge clk);
        #1;
        assert (valid_out === 1'b0 && result_out === 8'd0)
            else $fatal(1, "reset state is incorrect");
        rst = 1'b0;

        drive_and_check(8'd3,   8'd5,   2'b00, 8'd8);
        drive_and_check(8'hf0,  8'h3c,  2'b01, 8'h30);
        drive_and_check(8'haa,  8'h0f,  2'b10, 8'ha5);
        drive_and_check(8'd7,   8'd9,   2'b11, 8'd1);
        drive_and_check(8'hff,  8'd1,   2'b00, 8'd0);

        @(negedge clk);
        valid_in = 1'b0;
        a_in = 8'h55;
        b_in = 8'haa;
        op_in = 2'b10;
        @(posedge clk);
        #1;
        assert (valid_out === 1'b0)
            else $fatal(1, "valid_out did not follow an invalid input");
        checks++;

        $display("PASS: %0d checks; VCD written", checks);
        $finish;
    end
endmodule

`default_nettype wire
