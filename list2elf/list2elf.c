/*
 * list2elf.c
 * 
 *  - Generate ELF symbol files from MacOS 68K ROMs
 *    for QEMU's gdbstub
 *
 * (C) Copyright 2019 Mark Cave-Ayland <mark.cave-ayland@ilande.co.uk>
 * 
 * Usage: list2elf <file.list>
 * 
 */

/*
 * FIXME: see other FIXMEs for bits hardcoded for the Quadra800 ROM
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>


/* Linked list of symbols */
typedef struct sym_list {
    struct sym_list *next;    
    char *symname;
    unsigned int symaddr;
} SymList;


void write_be16(char *p, int x) {
    *p++ = (x & 0x0000ff00) >> 8;
    *p++ = (x & 0x000000ff);
}

void write_be32(char *p, int x) {
    *p++ = (x & 0xff000000) >> 24;
    *p++ = (x & 0x00ff0000) >> 16;
    *p++ = (x & 0x0000ff00) >> 8;
    *p++ = (x & 0x000000ff);
}

void generate_elf_file(FILE *g, SymList *sym_list)
{
    /* Generate an ELF header */
    SymList *cur;
    unsigned int sym_offset, sym_size, str_offset, str_size;
    int i, symcount, symnamelen, symnameoffset;
    
    /* Symtab entry */
    char ste[] = {
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
    };

    /* Global header (0x34 bytes) */
    char gh[] = {
        '\x7f', 'E', 'L', 'F', 0x1 /* 32-bit */, 0x2 /* big endian */, 0x1, 0x0,
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0, 0x0, 0x0, 0x4, /* 68K */ 0x0, 0x0, 0x0, 0x1,
        0x0, 0x0, 0x0, 0x0, /* e_entry */ 0x0, 0x0, 0x0, 0x0, /* e_phoff */
        0x0, 0x0, 0x0, 0x34, /* e_shoff */ 0x0, 0x0, 0x0, 0x0, /* e_flags */
        0x0, 0x34, /* e_ehsize */ 0x0, 0x20, /* e_phentsize */
          0x0, 0x0, /* e_phnum */ 0x0, 0x28, /* e_shentsize */
        0x0, 0x5, /* e_shnum */ 0x0, 0x2, /* e_shstrndx */
    };

    /* Null section header (0x28 bytes) */
    char sh0[] = {
        0x0, 0x0, 0x0, 0x0, /* sh_name */
        0x0, 0x0, 0x0, 0x0, /* sh_type */
        0x0, 0x0, 0x0, 0x0, /* sh_flags */
        0x0, 0x0, 0x0, 0x0, /* sh_addr */
        0x0, 0x0, 0x0, 0x0, /* sh_offset */
        0x0, 0x0, 0x0, 0x0, /* sh_size */
        0x0, 0x0, 0x0, 0x0, /* sh_link */
        0x0, 0x0, 0x0, 0x0, /* sh_info */
        0x0, 0x0, 0x0, 0x0, /* sh_addralign */
        0x0, 0x0, 0x0, 0x0, /* sh_entsize */
    };
    
    /* .text section header (0x28 bytes) */
    char sh1[] = {
        0x0, 0x0, 0x0, 0x1b, /* sh_name */
        0x0, 0x0, 0x0, 0x1, /* sh_type */
        0x0, 0x0, 0x0, 0x6, /* sh_flags */
        0x0, 0x0, 0x0, 0x0, /* sh_addr (fixed up below) */
        0x0, 0x0, 0x0, 0x0, /* sh_offset */
        0x0, 0x0, 0x0, 0x0, /* sh_size (fixed up below) */
        0x0, 0x0, 0x0, 0x3, /* sh_link */
        0x0, 0x0, 0x0, 0x0, /* sh_info */
        0x0, 0x0, 0x0, 0x4, /* sh_addralign */
        0x0, 0x0, 0x0, 0x0, /* sh_entsize */
    };

    /* shstrtab section header (0x28 bytes) */
    char sh2[] = {
        0x0, 0x0, 0x0, 0x1, /* sh_name */
        0x0, 0x0, 0x0, 0x3, /* sh_type */
        0x0, 0x0, 0x0, 0x0, /* sh_flags */
        0x0, 0x0, 0x0, 0x0, /* sh_addr */
        0x0, 0x0, 0x0, 0x34 + (5 * 0x28), /* sh_offset */
        0x0, 0x0, 0x0, 0x2c, /* sh_size */
        0x0, 0x0, 0x0, 0x0, /* sh_link */
        0x0, 0x0, 0x0, 0x0, /* sh_info */
        0x0, 0x0, 0x0, 0x1, /* sh_addralign */
        0x0, 0x0, 0x0, 0x0, /* sh_entsize */
    };

    /* symtab section header (0x28 bytes) */
    char sh3[] = {
        0x0, 0x0, 0x0, 0xb, /* sh_name */
        0x0, 0x0, 0x0, 0x2, /* sh_type */
        0x0, 0x0, 0x0, 0x0, /* sh_flags */
        0x0, 0x0, 0x0, 0x0, /* sh_addr */
        0x0, 0x0, 0x0, 0x0,  /* sh_offset (to be calculated) */
        0x0, 0x0, 0x0, 0x0, /* sh_size (to be calculated) */
        0x0, 0x0, 0x0, 0x4, /* sh_link */
        0x0, 0x0, 0x0, 0x0, /* sh_info */
        0x0, 0x0, 0x0, 0x1, /* sh_addralign */
        0x0, 0x0, 0x0, 0x10, /* sh_entsize */
    };

    /* string table for symbols (0x28 bytes) */
    char sh4[] = {
        0x0, 0x0, 0x0, 0x13, /* sh_name */
        0x0, 0x0, 0x0, 0x3, /* sh_type */
        0x0, 0x0, 0x0, 0x0, /* sh_flags */
        0x0, 0x0, 0x0, 0x0, /* sh_addr */
        0x0, 0x0, 0x0, 0x0, /* sh_offset (to be calculated) */
        0x0, 0x0, 0x0, 0x13, /* sh_size */
        0x0, 0x0, 0x0, 0x0, /* sh_link */
        0x0, 0x0, 0x0, 0x0, /* sh_info */
        0x0, 0x0, 0x0, 0x1, /* sh_addralign */
        0x0, 0x0, 0x0, 0x0, /* sh_entsize */
    };
    
    /* .shstrtab (0x1b bytes rounded to 0x2c) */
    char sh5[] = {
        0x0, /* NULL (len: 0x1) */
        0x2e, 0x73, 0x68, 0x73, 0x74, 0x72, 0x74, 0x61, 0x62, 0x0, /* ".shstrtab" (len: 0xa) */
        0x2e, 0x73, 0x79, 0x6d, 0x74, 0x61, 0x62, 0x0, /* ".symtab" (len: 0x8) */
        //0x2e, 0x64, 0x79, 0x6e, 0x73, 0x79, 0x6d, 0x0, /* ".dymsym" (len: 0x8) */
        0x2e, 0x73, 0x74, 0x72, 0x74, 0x61, 0x62, 0x0, /* ".strtab" (len: 0x8) */
        0x2e, 0x74, 0x65, 0x78, 0x74, 0x0, /* ".text" (len: 0x6) */
        0x0, 0x0, /* padding */
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
        0x0
    };

    /* FIXME: set start address of ROM image to 0x40800000 */
    write_be32(&sh1[0xc], 0x40800000);
    /* FIXME: set ROM size to 1MB */
    write_be32(&sh1[0x14], 0x100000);    

    /* Iterate over symbols: count the length of all the strings with trailing
       nulls, plus the number of symbols */
    symcount = 0;
    symnamelen = 0;
    for (cur = sym_list; cur != NULL; cur = cur->next) {
        symcount++;
        symnamelen += strlen(cur->symname) + 1;
    }

    /* Update the offset and size of symtab */
    sym_offset = 0x34 + (5 * 0x28) + 0x2c;
    sym_size = 0x10 * (symcount + 1);
    write_be32(&sh3[0x10], sym_offset);
    write_be32(&sh3[0x14], sym_size);

    /* Update the offset and size of strtab */
    str_offset = sym_offset + sym_size;
    str_size = (symnamelen + 1);
    write_be32(&sh4[0x10], str_offset);
    write_be32(&sh4[0x14], str_size);

    /* Write out headers */
    for (i = 0; i < sizeof(gh); i++) {
        fputc(gh[i], g);
    }

    for (i = 0; i < sizeof(sh0); i++) {
        fputc(sh0[i], g);
    }
    
    for (i = 0; i < sizeof(sh1); i++) {
        fputc(sh1[i], g);
    }
    
    for (i = 0; i < sizeof(sh2); i++) {
        fputc(sh2[i], g);
    }
    
    for (i = 0; i < sizeof(sh3); i++) {
        fputc(sh3[i], g);
    }

    for (i = 0; i < sizeof(sh4); i++) {
        fputc(sh4[i], g);
    }

    for (i = 0; i < sizeof(sh5); i++) {
        fputc(sh5[i], g);
    }
    
    /* Write out dummy symtab entry */
    memset(ste, 0, sizeof(ste));
    for (i = 0; i < sizeof(ste); i++) {
        ste[0xc] = 0x12; /* Global, function */
        fputc(ste[i], g);
    }

    /* Write out symtab entries */
    symnameoffset = 1;
    for (cur = sym_list; cur != NULL; cur = cur->next) {
        memset(ste, 0, sizeof(ste));
        write_be32(&ste[0x0], symnameoffset);
        write_be32(&ste[0x4], cur->symaddr + 0x40800000);
        write_be32(&ste[0x8], cur->next ? cur->next->symaddr - cur->symaddr : 0);
        ste[0xc] = 0x12; /* Global, function */
        ste[0xd] = 0;
        write_be16(&ste[0xe], 1);  /* Section 1 (.text) */
        
        for (i = 0; i < sizeof(ste); i++) {
            fputc(ste[i], g);
        }

        symnameoffset += strlen(cur->symname) + 1;
    }
    
    /* Write out dummy string */
    fputc(0, g);

    /* Write out symbol strings */
    for (cur = sym_list; cur != NULL; cur = cur->next) {
        fputs(cur->symname, g);
        fputc(0, g);
    }
}

SymList *parse_symbol_line(char *line)
{
    /* Parse a single symbol line */
    char *p, *c, *t;
    int i = 0, j, l;
    SymList *s;
    unsigned int symaddr;

    s = malloc(sizeof(SymList));
    c = line;
    while (p = strsep(&c, "\t")) {
        switch (i) {
        case 0:
            /* Symbol name */
            l = strlen(p) + 1;
            s->symname = malloc(l);
            strncpy(s->symname, p, l - 1);
            s->symname[l] = '\0';
            break;
        case 2:
            /* Section name */
            break;
        case 3:
            /* Address field $x,$y */
            t = strrchr(p, '$');
            if (t) {
                /* Remove trailing newline */
                t++;
                j = strlen(t);
                if (t[j - 1] == '\n') {
                    t[j - 1] = '\0';
                }

                /* Parse hex address */
                sscanf(t, "%x", &symaddr);
            }
            break;
        }        
        i++;
    }

    s->symaddr = symaddr;
    s->next = NULL;
    return s;
}

void parse_file(char *list_filename, char *elf_filename)
{
    /* Read the input file for processing */
    FILE *f, *g;
    char *p, *c;
    char segname[64];
    char linebuf[255];
    int enabled = 0;
    SymList *sym_list, *cur, *next;
    
    /* Read input list file */
    f = fopen(list_filename, "r");
    if (f == NULL) {
        fprintf(stderr, "Unable to open file '%s'\n", list_filename);
        return;
    }

    /* Create output ELF file */
    g = fopen(elf_filename, "w");
    if (g == NULL) {
        fprintf(stderr, "Unable to create file '%s'\n", elf_filename);
        fclose(f);
        return; 
    }

    /* Read symbols into linked list */
    while (fgets(linebuf, sizeof(linebuf), f) != NULL) {
        /* If line starts with a space then it is metadata */
        if (linebuf[0] == 0x20) {
            /* Start of segment: copy name to segname */
            if (!strncmp(&linebuf[1], "seg", 3)) {
                c = &linebuf[5];
                p = strsep(&c, " ");
                if (p) {
                    strncpy(segname, p, sizeof(segname) - 1);
                }
                
            } else if (!strncmp(&linebuf[1], "size", 4)) {
                /* End of section */
                segname[0] = '\0';
            }
        } else {
            /* Symbol line */
            if (!strncmp(segname, "Main", 4)) {
                /* Parse symbol lines */
                next = parse_symbol_line(linebuf);

                if (sym_list == NULL) {
                    sym_list = cur = next;
                } else {
                    cur->next = next;
                    cur = next;
                }
            }
        }
    }

    /* Generate ELF file */
    generate_elf_file(g, sym_list);

    fclose(g);
    fclose(f);
}

int main(int argc, char *argv[])
{
    /* Read input filename as argument */
    if (argc != 3) {
        printf("Usage: %s <input.list> <output.elf>\n", argv[0]);
        return 0;
    }

    parse_file(argv[1], argv[2]);
}
