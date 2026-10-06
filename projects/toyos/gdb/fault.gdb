# Before use, replace the file path with the expected-fault profile being run.
set pagination off
set architecture i386:x86-64
file build/week6-page/toyos.elf
target remote :1234
break interrupt_handler
continue
# At the breakpoint: p/x *frame, p/x $cr2, x/10i frame->rip
