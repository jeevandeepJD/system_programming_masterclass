`timescale 1ns/1ps
`default_nettype none

module day_002_alu_register_file_tb;
    localparam logic [3:0] ALU_ADD  = 4'd0;
    localparam logic [3:0] ALU_SUB  = 4'd1;
    localparam logic [3:0] ALU_AND  = 4'd2;
    localparam logic [3:0] ALU_OR   = 4'd3;
    localparam logic [3:0] ALU_XOR  = 4'd4;
    localparam logic [3:0] ALU_SLT  = 4'd5;
    localparam logic [3:0] ALU_SLTU = 4'd6;

    logic        clk;
    logic [31:0] a;
    logic [31:0] b;
    logic [3:0]  op;
    logic [31:0] result;
    logic        eq;
    logic        lt_signed;
    logic        lt_unsigned;

    logic        write_enable;
    logic [4:0]  waddr;
    logic [31:0] wdata;
    logic [4:0]  raddr1;
    logic [4:0]  raddr2;
    logic [31:0] rdata1;
    logic [31:0] rdata2;

    rv32_alu dut_alu (
        .a           (a),
        .b           (b),
        .op          (op),
        .result      (result),
        .eq          (eq),
        .lt_signed   (lt_signed),
        .lt_unsigned (lt_unsigned)
    );

    rv32_regfile dut_regfile (
        .clk          (clk),
        .write_enable (write_enable),
        .waddr        (waddr),
        .wdata        (wdata),
        .raddr1       (raddr1),
        .raddr2       (raddr2),
        .rdata1       (rdata1),
        .rdata2       (rdata2)
    );

    initial begin
        clk = 1'b0;
        forever #5 clk = ~clk;
    end

    task automatic check_alu(
        input logic [31:0] test_a,
        input logic [31:0] test_b,
        input logic [3:0]  test_op,
        input logic [31:0] expected_result,
        input logic        expected_eq,
        input logic        expected_lt_signed,
        input logic        expected_lt_unsigned
    );
        a = test_a;
        b = test_b;
        op = test_op;
        #1;
        assert (result === expected_result)
            else $fatal(1, "ALU result: a=%h b=%h op=%0d expected=%h got=%h",
                        test_a, test_b, test_op, expected_result, result);
        assert (eq === expected_eq)
            else $fatal(1, "EQ mismatch: a=%h b=%h", test_a, test_b);
        assert (lt_signed === expected_lt_signed)
            else $fatal(1, "signed LT mismatch: a=%h b=%h", test_a, test_b);
        assert (lt_unsigned === expected_lt_unsigned)
            else $fatal(1, "unsigned LT mismatch: a=%h b=%h", test_a, test_b);
    endtask

    task automatic write_register(
        input logic [4:0]  address,
        input logic [31:0] value
    );
        @(negedge clk);
        write_enable = 1'b1;
        waddr = address;
        wdata = value;
        @(posedge clk);
        #1;
        write_enable = 1'b0;
    endtask

    initial begin
        a = 32'd0;
        b = 32'd0;
        op = ALU_ADD;
        write_enable = 1'b0;
        waddr = 5'd0;
        wdata = 32'd0;
        raddr1 = 5'd0;
        raddr2 = 5'd0;
        #1;

        check_alu(32'd3, 32'd5, ALU_ADD, 32'd8, 1'b0, 1'b1, 1'b1);
        check_alu(32'd3, 32'd5, ALU_SUB, 32'hffff_fffe, 1'b0, 1'b1, 1'b1);
        check_alu(32'hf0f0_aa55, 32'h0ff0_0f0f, ALU_AND,
                  32'h00f0_0a05, 1'b0, 1'b1, 1'b0);
        check_alu(32'hf0f0_aa55, 32'h0ff0_0f0f, ALU_OR,
                  32'hfff0_af5f, 1'b0, 1'b1, 1'b0);
        check_alu(32'hf0f0_aa55, 32'h0ff0_0f0f, ALU_XOR,
                  32'hff00_a55a, 1'b0, 1'b1, 1'b0);

        // The same bits compare differently under signed and unsigned rules.
        check_alu(32'hffff_ffff, 32'd1, ALU_SLT,
                  32'd1, 1'b0, 1'b1, 1'b0);
        check_alu(32'hffff_ffff, 32'd1, ALU_SLTU,
                  32'd0, 1'b0, 1'b1, 1'b0);
        check_alu(32'hffff_ffff, 32'd1, ALU_ADD,
                  32'd0, 1'b0, 1'b1, 1'b0);
        check_alu(32'h1234_5678, 32'h1234_5678, ALU_SUB,
                  32'd0, 1'b1, 1'b0, 1'b0);

        // A write becomes visible after the rising edge.
        write_register(5'd5, 32'd7);
        raddr1 = 5'd5;
        raddr2 = 5'd0;
        #1;
        assert (rdata1 === 32'd7 && rdata2 === 32'd0)
            else $fatal(1, "initial register read or x0 read failed");

        @(negedge clk);
        write_enable = 1'b1;
        waddr = 5'd5;
        wdata = 32'd9;
        #1;
        assert (rdata1 === 32'd7)
            else $fatal(1, "x5 changed before the active write edge");
        @(posedge clk);
        #1;
        assert (rdata1 === 32'd9)
            else $fatal(1, "x5 did not update after the active write edge");
        write_enable = 1'b0;

        // Both combinational read ports can select retained values.
        write_register(5'd6, 32'hcafe_babe);
        raddr1 = 5'd5;
        raddr2 = 5'd6;
        #1;
        assert (rdata1 === 32'd9 && rdata2 === 32'hcafe_babe)
            else $fatal(1, "dual combinational read failed");

        // Writes to x0 are discarded and every read of x0 remains zero.
        write_register(5'd0, 32'hdead_beef);
        raddr1 = 5'd0;
        raddr2 = 5'd0;
        #1;
        assert (rdata1 === 32'd0 && rdata2 === 32'd0)
            else $fatal(1, "x0 did not remain zero");

        $display("PASS: ALU and register-file checks completed");
        $finish;
    end
endmodule

`default_nettype wire
