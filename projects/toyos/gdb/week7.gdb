set pagination off
set confirm off
set architecture i386:x86-64
file build/week7/toyos.elf
target remote :1234
break kernel_main
break memory_init
break context_switch
continue
