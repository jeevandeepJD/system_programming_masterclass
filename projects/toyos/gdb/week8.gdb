set pagination off
set confirm off
set architecture i386:x86-64
file build/week8/toyos.elf
target remote :1234
break kernel_main
break process_init
break syscall_dispatch
continue
