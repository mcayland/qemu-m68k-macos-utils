## decl-comp.py: MacOS declaration ROM compiler

When I was first attempting to boot MacOS under `qemu-system-m68k` I thought that the reason the in-built framebuffer wasn't initialising was because the `macfb` device needed a Nubus declaration ROM.

It eventually transpired that the display initialisation problem was a completely different bug, but I'd already written `decl-comp.py` to generate a binary declaration ROM from an input YAML file.

Perhaps one day someone may use this to help create new Nubus cards or virtual Nubus cards for running under `qemu-system-m68k`.

### Usage
    decl-comp.py <input-file.yml>

This parses the input YAML file and generates a corresponding binary `decl.rom` file in the current directory.

### Example
    decl-comp.py macfb-declrom.yml

This parses the supplied `macfb-declrom.yml` file and generates an output binary `decl.rom` file which may or may not be suitable for use in a Nubus virtual framebuffer device in QEMU.

### Testing
The script is only lightly tested, but to be best of my knowledge it does produce a valid binary declaration ROM based upon my reading of  "Designing Cards and Drivers for the Macintosh Family".

### TODO
- Add support for non-framebuffer resources
- Add option to output assembler source for embedding in a 68k assembler file

