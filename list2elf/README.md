## list2elf: generate stub 68k ELF from MPW ROM Map

QEMU allows guest debugging by implementing the `gdb` protocol which allows an external 68k gdb client to disassemble, view memory, single step etc.  Whilst attempting to boot MacOS under `qemu-system-m68k` I realised I could make things much easier if I could use the ROM Map files that were distributed as part of MPW to generate a stub 68k ELF file with ROM symbols.

list2elf is a small C program I wrote which parses the `Quadra800ROM.lst` from MPW to generate an ELF file containing the ROM symbols which can be used to help debug the MacOS toolbox ROM in QEMU's q800 machine.

The repository contains the generated stub 68k ELF for reference: even though Apple eventually released MPW for free, I'm not sure that I can distribute the ROM maps themselves for copyright reasons.

### Build
Ensure that you have a C compiler installed and then you should be able to build the list2elf binary with simply:

    make

### Usage
    list2elf Quadra800ROM.lst Quadra800ROM.elf

This parses the input `Quadra800ROM.lst` ROM map and generates the stub 68k ELF binary `Quadra800ROM.elf` for use with `qemu-system-m68k`'s gdbstub. Alternatively you can just use the pre-built `Quadra800ROM.elf` file included in the repository.

### How to use Quadra800ROM.elf with QEMU
When list2elf was written, there was a bug in gdbstub which meant newer gdb versions would incorrectly use the Coldfire registers instead of the 68k registers. This has now been fixed so a recent QEMU and gdb combination should work fine - otherwise it is possible to build a 68k chroot using QEMU's linux-user emulation based upon Debian etch:


```
sudo scripts/qemu-binfmt-conf.sh --credential yes --systemd m68k \
     --qemu-path _/usr/bin/_ --qemu-suffix -static  --persistent yes
sudo systemctl restart systemd-binfmt.service
sudo debootstrap --arch=m68k --variant=minbase --no-check-gpg \
     etch-m68k chroot-m68k [http://archive.debian.org/debian](http://archive.debian.org/debian)

sudo chroot chroot-m68k/
cat > /etc/apt/sources.list <<EOF
deb [http://archive.debian.org/debian](http://archive.debian.org/debian) etch-m68k  main
EOF
apt-get install gdb
```

Once you've got a working `qemu-system-m68k` binary with a MacOS toolbox ROM and 68k-capable gdbstub, you can simply attach to the guest as a remote target:

#### QEMU
    qemu-system-m68k -M q800 -bios Quadra800.ROM -s -S

#### gdb
    kentang:/# gdb /tmp/Quadra800ROM.elf
    GNU gdb 6.4.90-debian
    Copyright (C) 2006 Free Software Foundation, Inc.
    GDB is free software, covered by the GNU General Public License, and you are
    welcome to change it and/or distribute copies of it under certain conditions.
    Type "show copying" to see the conditions.
    There is absolutely no warranty for GDB.  Type "show warranty" for details.
    This GDB was configured as "m68k-linux-gnu"...(no debugging symbols found)
    Using host libthread_db library "/lib/libthread_db.so.1".
    
    (gdb) target remote :1234
    Remote debugging using :1234
    [New thread 1]
    0x4080002a in CRITICAL ()
    warning: shared library handler failed to enable breakpoint
    (gdb) b DRAWBEEPSCREEN
    Breakpoint 1 at 0x40800530
    (gdb) c
    Continuing.
    
    Breakpoint 1, 0x40800530 in DRAWBEEPSCREEN ()
    (gdb) disas $pc,$pc+0x10
    Dump of assembler code for function DRAWBEEPSCREEN:
    0x40800530 <DRAWBEEPSCREEN+0>:  pea %a5@(-4)
    0x40800534 <DRAWBEEPSCREEN+4>:  macl %fp,%d4,%fp@(-512)&,%a4,%acc1
    0x4080053a <DRAWBEEPSCREEN+10>: 0125000
    0x4080053c <DRAWBEEPSCREEN+12>: moveal %a5@,%a2
    0x4080053e <DRAWBEEPSCREEN+14>: pea %a2@(-108)
    0x40800542 <DRAWBEEPSCREEN+18>: macw %a7u,%d6u,%a1@&,%a4,%acc3
    0x40800546 <DRAWBEEPSCREEN+22>: orib #102,%d0
    End of assembler dump.
    (gdb)

### TODO
- Consider adding in symbols for MacOS low memory variables based upon Basilisk II/SheepShaver's `mon_lowmem.cpp`
- Add support for more machines/map files

