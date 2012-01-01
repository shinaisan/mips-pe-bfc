/* Brainf*ck compiler for MIPS (32-bit, little-endian). */
/* It generates a PE file that runs on Windows NT 4.0 for MIPS. */
/* Sample compilation (using Visual C++): */
/* > cl bfc.c */
/* > bfc hello-world.bf */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint32_t inst_t;

/* Configuration start. */
static char bin[16 * 1024] = {0};
#define STACK_SIZE 32
inst_t addr_putchar = 0x00405044;
inst_t addr_getchar = 0x00405048;
inst_t addr_data = 0x00406000;
/* Configuration end. */


/* A total of 32 registers. */
typedef enum {
    REG_ZERO = 0,
    REG_AT,
    REG_V0,
    REG_V1,
    REG_A0,
    REG_A1,
    REG_A2,
    REG_A3,
    REG_T0,
    REG_T1,
    REG_T2,
    REG_T3,
    REG_T4,
    REG_T5,
    REG_T6,
    REG_T7,
    REG_S0,
    REG_S1,
    REG_S2,
    REG_S3,
    REG_S4,
    REG_S5,
    REG_S6,
    REG_S7,
    REG_T8,
    REG_T9,
    REG_K0,
    REG_K1,
    REG_GP,
    REG_SP,
    REG_FP,
    REG_RA,
} mips_reg_t;

#define MIPS_INST_NOP						(0x00000000)
#define MIPS_INST_ADDIU(reg0, reg1, imm) 	(0x24000000 | (reg0 << 16) | (reg1 << 21) | (imm 	& 0xffff))
#define MIPS_INST_SB(reg0, reg1, disp)		(0xa0000000 | (reg0 << 16) | (reg1 << 21) | (disp	& 0xffff))
#define MIPS_INST_SW(reg0, reg1, disp) 		(0xac000000 | (reg0 << 16) | (reg1 << 21) | (disp 	& 0xffff))
#define MIPS_INST_LB(reg0, reg1, disp)		(0x80000000 | (reg0 << 16) | (reg1 << 21) | (disp	& 0xffff))
#define MIPS_INST_LW(reg0, reg1, disp) 		(0x8c000000 | (reg0 << 16) | (reg1 << 21) | (disp 	& 0xffff))
#define MIPS_INST_ADD(reg0, reg1, reg2)		(0x00000020 | (reg0 << 11) | (reg1 << 21) | (reg2 << 16))
#define MIPS_INST_AND(reg0, reg1, reg2)		(0x00000024 | (reg0 << 11) | (reg1 << 21) | (reg2 << 16))
#define MIPS_INST_ADDU(reg0, reg1, reg2) 	(0x00000021 | (reg0 << 11) | (reg1 << 21) | (reg2 << 16))
#define MIPS_INST_ADDI(reg0, reg1, imm)		(0x20000000 | (reg0 << 16) | (reg1 << 21) | (imm	& 0xffff))
#define MIPS_INST_ANDI(reg0, reg1, imm)		(0x30000000 | (reg0 << 16) | (reg1 << 21) | (imm	& 0xffff))
#define MIPS_INST_SRL(reg0, reg1, sa)		(0x00000002 | (reg0 << 11) | (reg1 << 16) | (sa << 6))
#define MIPS_INST_MOVE(reg0, reg1) 			MIPS_INST_ADDU(reg0, REG_ZERO, reg1)
#define MIPS_INST_LUI(reg0, imm) 			(0x3c000000 | (reg0 << 16) | (imm & 0xffff))
#define MIPS_INST_ORI(reg0, reg1, imm) 		(0x34000000 | (reg0 << 16) | (reg1 << 21) | (imm 	& 0xffff))
#define MIPS_INST_LI(reg0, imm) 			MIPS_INST_ORI(reg0, REG_ZERO, imm)
#define MIPS_INST_JR(reg0)					(0x00000008 | (reg0 << 21))
#define MIPS_INST_JALR(ret, dest)			(0x00000009 | (ret  << 11) | (dest << 21))
#define MIPS_INST_BEQ(reg0, reg1, dest)		(0x10000000 | (reg0 << 21) | (reg1 << 16) | (dest	& 0xffff))
#define MIPS_INST_BEQZ(reg0, dest)			MIPS_INST_BEQ(reg0, REG_ZERO, dest)
#define MIPS_INST_B(dest)					MIPS_INST_BEQ(REG_ZERO, REG_ZERO, dest)
#define MIPS_INST_BREAK						(0x0000000d)


int bf_compile(char *src, inst_t *dest, int inst_max) {
    char c;
    int idx = 0;
    int src_idx = 0;
    int stack_top = 0;
    int stack[STACK_SIZE];

    if (inst_max < 32) {
        /* The lower limit of 32 is not carefully chosen but it's enough for holding the prolog and epilog.   */
        printf("Output buffer is too short: %d, expected more than (or equal to) 32.\n", inst_max);
        return (-1);
    }

    /* addiu	$sp, $sp, -24 */
    dest[idx++] = MIPS_INST_ADDIU(REG_SP, REG_SP, -24);
    /* sw	$ra, 16($sp) */
    dest[idx++] = MIPS_INST_SW(REG_RA, REG_SP, 16);
    /* sw	$s3, 12($sp) */
    dest[idx++] = MIPS_INST_SW(REG_S3, REG_SP, 12);
    /* sw	$s2, 8($sp) */
    dest[idx++] = MIPS_INST_SW(REG_S2, REG_SP, 8);
    /* sw	$s1, 4($sp) */
    dest[idx++] = MIPS_INST_SW(REG_S1, REG_SP, 4);
    /* sw	$s0, 0($sp) */
    dest[idx++] = MIPS_INST_SW(REG_S0, REG_SP, 0);
    /* move	$s0, $zero */
    dest[idx++] = MIPS_INST_MOVE(REG_S0, REG_ZERO);
    /* lui	$s0, 0x0040 */
    dest[idx++] = MIPS_INST_LUI(REG_S0, (addr_data >> 16));
    /* ori	$s0, 0x6000 */
    dest[idx++] = MIPS_INST_ORI(REG_S0, REG_S0, (addr_data & 0xffff));
    /* move	$s1, $zero */
    dest[idx++] = MIPS_INST_MOVE(REG_S1, REG_ZERO);

    while (c = *src) {
        if (idx + 16 > inst_max) {
            /* The margin of 16 instructions is for the epilog and one bf-instruction.  */
            printf("Only %d instructions left in the output buffer. Compilation stopped.\n", inst_max - idx);
            return (-1);
        }
        switch (c) {
        case '>':
            /* addi	$s1, $s1, 1 */
            dest[idx++] = MIPS_INST_ADDI(REG_S1, REG_S1, 1);
            break;
        case '<':
            /* addi	$s1, $s1, -1 */
            dest[idx++] = MIPS_INST_ADDI(REG_S1, REG_S1, -1);
            break;
        case '+':
            /* add	$s2, $s0, $s1 */
            dest[idx++] = MIPS_INST_ADD(REG_S2, REG_S0, REG_S1);
            /* lb	$s3, 0($s2) */
            dest[idx++] = MIPS_INST_LB(REG_S3, REG_S2, 0);
            /* addi	$s3, $s3, 1; */
            dest[idx++] = MIPS_INST_ADDI(REG_S3, REG_S3, 1);
            /* sb	$s3, 0($s2) */
            dest[idx++] = MIPS_INST_SB(REG_S3, REG_S2, 0);
            break;
        case '-':
            /* add	$s2, $s0, $s1 */
            dest[idx++] = MIPS_INST_ADD(REG_S2, REG_S0, REG_S1);
            /* lb	$s3, 0($s2) */
            dest[idx++] = MIPS_INST_LB(REG_S3, REG_S2, 0);
            /* addi	$s3, $s3, -1;  */
            dest[idx++] = MIPS_INST_ADDI(REG_S3, REG_S3, -1);
            /* sb	$s3, 0($s2) */
            dest[idx++] = MIPS_INST_SB(REG_S3, REG_S2, 0);
            break;
        case '.':
            /* move	$v0, $zero */
            dest[idx++] = MIPS_INST_MOVE(REG_V0, REG_ZERO);
            /* lui	$v0, (putchar >> 16) */
            dest[idx++] = MIPS_INST_LUI(REG_V0, (addr_putchar >> 16));
            /* ori	$v0, $v0, (putchar & 0xffff) */
            dest[idx++] = MIPS_INST_ORI(REG_V0, REG_V0, (addr_putchar & 0xffff));
            /* lw	$v0, 0($v0) */
            dest[idx++] = MIPS_INST_LW(REG_V0, REG_V0, 0);
            /* add	$s2, $s0, $s1 */
            dest[idx++] = MIPS_INST_ADD(REG_S2, REG_S0, REG_S1);
            /* jalr	$v0 */
            dest[idx++] = MIPS_INST_JALR(REG_RA, REG_V0);
            /* lb	$a0, 0($s2) */
            dest[idx++] = MIPS_INST_LB(REG_A0, REG_S2, 0);
            break;
        case ',':
            /* move	$v0, $zero */
            dest[idx++] = MIPS_INST_MOVE(REG_V0, REG_ZERO);
            /* lui	$v0, (getchar >> 16) */
            dest[idx++] = MIPS_INST_LUI(REG_V0, (addr_getchar >> 16));
            /* ori	$v0, $v0, (getchar & 0xffff) */
            dest[idx++] = MIPS_INST_ORI(REG_V0, REG_V0, (addr_getchar & 0xffff));
            /* lw	$v0, 0($v0) */
            dest[idx++] = MIPS_INST_LW(REG_V0, REG_V0, 0);
            /* jalr	$v0 */
            dest[idx++] = MIPS_INST_JALR(REG_RA, REG_V0);
            /* add	$s2, $s0, $s1 */
            dest[idx++] = MIPS_INST_ADD(REG_S2, REG_S0, REG_S1);
            /* sb	$v0, 0($s2) */
            dest[idx++] = MIPS_INST_SB(REG_V0, REG_S2, 0);
            break;
        case '[':
            if (stack_top >= STACK_SIZE) {
                printf("Stack overflow @ %d\n", src_idx);
                return (-1);
            }
            stack[stack_top++] = idx;
            /* add	$s2, $s0, $s1 */
            dest[idx++] = MIPS_INST_ADD(REG_S2, REG_S0, REG_S1);
            /* lb	$s3, 0($s2) */
            dest[idx++] = MIPS_INST_LB(REG_S3, REG_S2, 0);
            /* beqz	$s3, 0x0000 (overwritten later) */
            dest[idx++] = MIPS_INST_BEQZ(REG_S3, 0x0000); /* Address part is overwritten later. */
            /* nop */
            dest[idx++] = MIPS_INST_NOP;
            break;
        case ']':
            if (stack_top <= 0) {
                printf("Stack underflow @ %d\n", src_idx);
                return (-1);
            }
            stack_top--;
            /* Overwrite address */
            dest[stack[stack_top] + 2] |= ((idx + 2 - (stack[stack_top] + 3)) & 0xffff);
            /* b <relative address to '['> */
            dest[idx++] = MIPS_INST_B((stack[stack_top] - (idx + 1)) & 0xffff);
            /* nop */
            dest[idx++] = MIPS_INST_NOP;
            break;
#ifdef DEBUG_BFC
        case '!':
            /* add	$s2, $s0, $s1 */
            dest[idx++] = MIPS_INST_ADD(REG_S2, REG_S0, REG_S1);
            /* lb	$s3, 0($s2) */
            dest[idx++] = MIPS_INST_LB(REG_S3, REG_S2, 0);
            /* check */
            idx = bf_compile_reg_check(REG_S3, dest, idx);
            break;
#endif  /* DEBUG_BFC */
        default:
            /* Ignore unknown characters. */
            break;
        }
        src_idx++;
        src++;
    }

	/* lw	$s0, 0($sp) */
    dest[idx++] = MIPS_INST_LW(REG_S0, REG_SP, 0);
	/* lw	$s1, 4($sp) */
    dest[idx++] = MIPS_INST_LW(REG_S1, REG_SP, 4);
	/* lw	$s2, 8($sp) */
    dest[idx++] = MIPS_INST_LW(REG_S2, REG_SP, 8);
	/* lw	$s3, 12($sp) */
    dest[idx++] = MIPS_INST_LW(REG_S3, REG_SP, 12);
	/* lw	$ra, 16($sp) */
    dest[idx++] = MIPS_INST_LW(REG_RA, REG_SP, 16);
	/* jr	$ra */
    dest[idx++] = MIPS_INST_JR(REG_RA);
	/* addiu	$sp, $sp, 24 */
    dest[idx++] = MIPS_INST_ADDIU(REG_SP, REG_SP, 24);

    return (idx);
}

int bf_compile_reg_check(int reg, inst_t *dest, int idx) {
    /* addiu	$sp, $sp, -16 */
    dest[idx++] = MIPS_INST_ADDIU(REG_SP, REG_SP, -16);
    /* sw	$s3, 12($sp) */
    dest[idx++] = MIPS_INST_SW(REG_S3, REG_SP, 12);
    /* sw	$s2, 8($sp) */
    dest[idx++] = MIPS_INST_SW(REG_S2, REG_SP, 8);
    /* sw	$s1, 4($sp) */
    dest[idx++] = MIPS_INST_SW(REG_S1, REG_SP, 4);
    /* sw	$s0, 0($sp) */
    dest[idx++] = MIPS_INST_SW(REG_S0, REG_SP, 0);

    /* move	$s0, <reg> */
    dest[idx++] = MIPS_INST_MOVE(REG_S0, reg);

    /* move	$v0, $zero */
    dest[idx++] = MIPS_INST_MOVE(REG_V0, REG_ZERO);
    /* lui	$v0, (putchar >> 16) */
    dest[idx++] = MIPS_INST_LUI(REG_V0, (addr_putchar >> 16));
    /* ori	$v0, $v0, (putchar & 0xffff) */
    dest[idx++] = MIPS_INST_ORI(REG_V0, REG_V0, (addr_putchar & 0xffff));
    /* lw	$v0, 0($v0) */
    dest[idx++] = MIPS_INST_LW(REG_V0, REG_V0, 0);
    /* srl	$a0, $s0, 4 */
    dest[idx++] = MIPS_INST_SRL(REG_A0, REG_S0, 4);
    /* andi	$a0, $a0, 0xf */
    dest[idx++] = MIPS_INST_ANDI(REG_A0, REG_A0, 0xf);
    /* jalr	$v0 */
    dest[idx++] = MIPS_INST_JALR(REG_RA, REG_V0);
    /* addi	$a0, $a0, 0x30 */
    dest[idx++] = MIPS_INST_ADDI(REG_A0, REG_A0, 0x30);

    /* move	$v0, $zero */
    dest[idx++] = MIPS_INST_MOVE(REG_V0, REG_ZERO);
    /* lui	$v0, (putchar >> 16) */
    dest[idx++] = MIPS_INST_LUI(REG_V0, (addr_putchar >> 16));
    /* ori	$v0, $v0, (putchar & 0xffff) */
    dest[idx++] = MIPS_INST_ORI(REG_V0, REG_V0, (addr_putchar & 0xffff));
    /* lw	$v0, 0($v0) */
    dest[idx++] = MIPS_INST_LW(REG_V0, REG_V0, 0);
    /* srl	$a0, $s0, 0 */
    dest[idx++] = MIPS_INST_SRL(REG_A0, REG_S0, 0);
    /* andi	$a0, $a0, 0xf */
    dest[idx++] = MIPS_INST_ANDI(REG_A0, REG_A0, 0xf);
    /* jalr	$v0 */
    dest[idx++] = MIPS_INST_JALR(REG_RA, REG_V0);
    /* addi	$a0, $a0, 0x30 */
    dest[idx++] = MIPS_INST_ADDI(REG_A0, REG_A0, 0x30);

    /* move	$v0, $zero */
    dest[idx++] = MIPS_INST_MOVE(REG_V0, REG_ZERO);
    /* lui	$v0, (putchar >> 16) */
    dest[idx++] = MIPS_INST_LUI(REG_V0, (addr_putchar >> 16));
    /* ori	$v0, $v0, (putchar & 0xffff) */
    dest[idx++] = MIPS_INST_ORI(REG_V0, REG_V0, (addr_putchar & 0xffff));
    /* lw	$v0, 0($v0) */
    dest[idx++] = MIPS_INST_LW(REG_V0, REG_V0, 0);
    /* jalr	$v0 */
    dest[idx++] = MIPS_INST_JALR(REG_RA, REG_V0);
    /* li	$a0, 0xa */
    dest[idx++] = MIPS_INST_LI(REG_A0, 0xa);

	/* lw	$s0, 0($sp) */
    dest[idx++] = MIPS_INST_LW(REG_S0, REG_SP, 0);
	/* lw	$s1, 4($sp) */
    dest[idx++] = MIPS_INST_LW(REG_S1, REG_SP, 4);
	/* lw	$s2, 8($sp) */
    dest[idx++] = MIPS_INST_LW(REG_S2, REG_SP, 8);
	/* lw	$s3, 12($sp) */
    dest[idx++] = MIPS_INST_LW(REG_S3, REG_SP, 12);
	/* addiu	$sp, $sp, 16 */
    dest[idx++] = MIPS_INST_ADDIU(REG_SP, REG_SP, 16);

    return (idx);
}

void write_padding(FILE *fp, int n) {
    while (n-- > 0) {
        fputc(0, fp);
    }
}

void write_string(FILE *fp, int len, char *str) {
    while ((*str) && (len-- > 0)) {
        fputc(*str++, fp);
    }
    while (len-- > 0) {
        fputc(0, fp);
    }
}

void write_short(FILE *fp, short v) {
    fwrite(&v, sizeof(v), 1, fp);
}

void write_pe_header(FILE *fp) {
    static unsigned char stub[] = {
        /* 00-3b: DOS Header */
        'M',  'Z',  0x50, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0f, 0x00, 0xff, 0xff, 0x00, 0x00,
        0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 3c-3f: Pointer to PE Header (=80) */
        0x80, 0x00, 0x00, 0x00,
        /* 40-7f: DOS stub */
        0xba, 0x10, 0x00, 0x0e, 0x1f, 0xb4, 0x09, 0xcd, 0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21, 0x90, 0x90,
        'T', 'h', 'i', 's', ' ', 'p', 'r', 'o', 'g', 'r', 'a', 'm', ' ', 'c', 'a', 'n',
        'n', 'o', 't', ' ', 'b', 'e', ' ', 'r', 'u', 'n', ' ', 'i', 'n', ' ', 'D', 'O',
        'S', ' ', 'm', 'o', 'd', 'e', '.', '\r', '\n', '$', 0, 0, 0, 0, 0, 0,
        /* 80-83: PE Signature */
        'P', 'E', 0, 0
    };
    IMAGE_FILE_HEADER coff = {
        0x0166, 3, 0, 0, 0, sizeof(IMAGE_OPTIONAL_HEADER32), 0x030f
    };
    IMAGE_OPTIONAL_HEADER32 opt = {
        0x010b,			/*  Magic  */
        6, 0,			/*  MajorLinkerVersion, MinorLinkerVersion  */
        sizeof(bin),	/*  SizeOfCode  */
        0, 				/*  SizeOfInitializedData  */
        65536,			/*  SizeOfUninitializedData  */
        0x1000,			/*  AddressOfEntryPoint  */
        0x1000,			/*  BaseOfCode  */
        0x6000,			/*  BaseOfData  */
        0x00400000,		/*  ImageBase  */
        0x1000,			/*  SectionAlignment  */
        0x0200,			/*  FileAlignment  */
        4, 0,			/*  MajorOperatingSystemVersion, MinorOperatingSystemVersion  */
        0, 0,			/*  MajorImageVersion, MinorImageVersion  */
        4, 0,			/*  MajorSubsystemVersion, MinorSubsystemVersion  */
        0,				/*  Win32VersionValue  */
        0x16000,		/*  SizeOfImage  */
        0x200,			/*  SizeOfHeaders  */
        0,				/*  CheckSum  */
        3,				/*  Subsystem  */
        0,				/*  DllCharacteristics  */
        1024*1024,		/*  SizeOfStackReserve  */
        8*1024,			/*  SizeOfStackCommit  */
        1024*1024,		/*  SizeOfHeapReserve  */
        4*1024,			/*  SizeOfHeapCommit  */
        0,				/*  LoaderFlags  */
        16				/*  NumberOfRvaAndSizes  */
    };
    IMAGE_SECTION_HEADER sects[3];

    fwrite(stub, sizeof(stub), 1, fp);

    fwrite(&coff, sizeof(coff), 1, fp);

    memset(opt.DataDirectory, 0, sizeof(opt.DataDirectory));
    opt.DataDirectory[1].VirtualAddress = 0x5000; /* import table */
    opt.DataDirectory[1].Size = 100;
    fwrite(&opt, sizeof(opt), 1, fp);

    memset(sects, 0, sizeof(sects));
    strcpy((char *)sects[0].Name, ".text");
    sects[0].Misc.VirtualSize = sizeof(bin);
    sects[0].VirtualAddress = 0x1000;
    sects[0].SizeOfRawData = sizeof(bin);
    sects[0].PointerToRawData = 0x400;
    sects[0].Characteristics = 0x60500060;
    strcpy((char *)sects[1].Name, ".idata");
    sects[1].Misc.VirtualSize = 100;
    sects[1].VirtualAddress = 0x5000;
    sects[1].SizeOfRawData = 512;
    sects[1].PointerToRawData = 0x200;
    sects[1].Characteristics = 0xc0300040;
    strcpy((char *)sects[2].Name, ".bss");
    sects[2].Misc.VirtualSize = 65536;
    sects[2].VirtualAddress = 0x6000;
    sects[2].Characteristics = 0xc0400080;
    fwrite(sects, sizeof(sects), 1, fp);

    write_padding(fp, 0x200 - ftell(fp));
}

void write_idata(FILE *fp) {
    int idt[] = {
        // IDT 1
        0x5028, 0, 0, 0x5034, 0x5044,
        // IDT (Terminator)
        0, 0, 0, 0, 0
    };
    int ilt_iat[] = {0x5050, 0x505a, 0};

    fwrite(idt, sizeof(idt), 1 ,fp);
    fwrite(ilt_iat, sizeof(ilt_iat), 1, fp); // ILT

    write_string(fp, 16, "msvcrt.dll");

    fwrite(ilt_iat, sizeof(ilt_iat), 1, fp); // IAT

    // putchar
    write_short(fp, 0);
    write_string(fp, 8, "putchar");

    // getchar
    write_short(fp, 0);
    write_string(fp, 8, "getchar");

    write_padding(fp, 0x400 - ftell(fp));
}

void make_exe_path(char *src, char *dest) {
    int i, len;
    strcpy(dest, src);
    i = len = strlen(dest);
    while (i-- > 0) {
        if (dest[i] == '.') {
            strcpy(&dest[i], ".exe");
            return;
        }
        if ((dest[i] == '/') || (dest[i] == '\\')) {
            break;
        }
    }
    strcpy(&dest[len], ".exe");
}

int bf_pegen(FILE *out) {
    write_pe_header(out);
    write_idata(out);
    fwrite(bin, sizeof(bin), 1, out);
    return (0);
}

/* Buffer(*pbuf) must be freed manually. */
int bf_read_source(FILE *fp, char **pbuf) {
    int size = 0;
    if (fseek(fp, 0, SEEK_END) == 0) {
        size = ftell(fp);
        if (fseek(fp, 0, SEEK_SET) == 0) {
            *pbuf = malloc(size + 1);
            if (!(*pbuf)) {
                return (-1);
            }
            (*pbuf)[size] = 0;
            return ((int)fread(*pbuf, 1, size, fp));
        } else {
            return (-1);
        }
    } else {
        return (-1);
    }
}

#ifndef TEST_BFC

int main(int argc, char *argv[]) {
    char *src_path;
    char exe_path[256];
    FILE *src_file = NULL;
    FILE *exe_file = NULL;
    char *src_buf = NULL;
    int ret;

    if (argc < 2) {
        printf("[USAGE] %s <bf file name>\n", argv[0]);
        return (-1);
    }

    src_path = argv[1];
    make_exe_path(src_path, exe_path);
    
    src_file = fopen(src_path, "rb");
    if (!src_file) {
        printf("Cannot open source file: %s\n", src_path);
        ret = -1;
        goto main_end;
    }
    
    exe_file = fopen(exe_path, "wb");
    if (!exe_file) {
        printf("Cannot create file: %s\n", exe_path);
        ret = -1;
        goto main_end;
    }

    if (bf_read_source(src_file, &src_buf) < 0) {
        printf("File read error: %s\n", src_path);
        ret = -1;
        goto main_end;
    }

    bf_compile(src_buf, (inst_t *)(&bin[0]), sizeof(bin) / sizeof(inst_t));
    
    bf_pegen(exe_file);

 main_end:

    if (src_file) {
        fclose(src_file);
    }
    if (exe_file) {
        fclose(exe_file);
    }
    if (src_buf) {
        free(src_buf);
    }

    return (0);
}

#else  /* TEST_BFC */

#define ALEN(x) (sizeof(x) / sizeof(x[0]))

static inst_t test_prolog[] = {
    /*
      $ cat | mips-elf-as
      .set	noreorder
      addiu	$sp, $sp, -24
      sw	$ra, 16($sp)
      sw	$s3, 12($sp)
      sw	$s2, 8($sp)
      sw	$s1, 4($sp)
      sw	$s0, 0($sp)
      move	$s0, $zero
      lui	$s0, 0x0040
      ori	$s0, 0x6000
      move	$s1, $zero
    */
    0x27bdffe8,
    0xafbf0010,
    0xafb3000c,
    0xafb20008,
    0xafb10004,
    0xafb00000,
    0x00008021,
    0x3c100040,
    0x36106000,
    0x00008821,
};

static inst_t test_epilog[] = {
    /*
      $ cat | mips-elf-as
      .set	noreorder
      lw	$s0, 0($sp)
      lw	$s1, 4($sp)
      lw	$s2, 8($sp)
      lw	$s3, 12($sp)
      lw	$ra, 16($sp)
      jr	$ra
      addiu	$sp, $sp, 24
    */
    0x8fb00000,
    0x8fb10004,
    0x8fb20008,
    0x8fb3000c,
    0x8fbf0010,
    0x03e00008,
    0x27bd0018,
};

static inst_t test_basic[] = {
    /* "<+->"
      $ cat | mips-elf-as
      .set	noreorder
      addi	$s1, $s1, -1
      
      add	$s2, $s0, $s1
      lb	$s3, 0($s2)
      addi	$s3, $s3, 1;
      sb	$s3, 0($s2)
      
      add	$s2, $s0, $s1
      lb	$s3, 0($s2)
      addi	$s3, $s3, -1;
      sb	$s3, 0($s2)

      addi	$s1, $s1, 1
    */
    0x2231ffff,
    0x02119020,
    0x82530000,
    0x22730001,
    0xa2530000,
    0x02119020,
    0x82530000,
    0x2273ffff,
    0xa2530000,
    0x22310001,
};

static inst_t test_jump1[] = {
    /* []
       $ cat | mips-elf-as
       .set	noreorder
       begin1:
       add	$s2, $s0, $s1
       lb	$s3, 0($s2)
       beqz $s3, end1
       nop

       b	begin1
       nop

       end1:
     */
    0x02119020,
    0x82530000,
    0x12600003,
    0x00000000,
    0x1000fffb,
    0x00000000,
};

static inst_t test_jump2[] = {
    /* [[]]
       $ cat | mips-elf-as
       .set	noreorder
       begin1:
       add	$s2, $s0, $s1
       lb	$s3, 0($s2)
       beqz $s3, end1
       nop

       begin2:
       add	$s2, $s0, $s1
       lb	$s3, 0($s2)
       beqz $s3, end2
       nop

       b	begin2
       nop

       end2:

       b	begin1
       nop

       end1:
     */
    0x02119020,
    0x82530000,
    0x12600009,
    0x00000000,
    0x02119020,
    0x82530000,
    0x12600003,
    0x00000000,
    0x1000fffb,
    0x00000000,
    0x1000fff5,
    0x00000000,
};

static inst_t test_jump3[] = {
    /* []>[]
       $ cat | mips-elf-as
       .set	noreorder
       begin1:
       add	$s2, $s0, $s1
       lb	$s3, 0($s2)
       beqz $s3, end1
       nop

       b	begin1
       nop

       end1:

       addi	$s1, $s1, 1

       begin2:
       add	$s2, $s0, $s1
       lb	$s3, 0($s2)
       beqz $s3, end2
       nop

       b	begin2
       nop

       end2:
     */
    0x02119020,
    0x82530000,
    0x12600003,
    0x00000000,
    0x1000fffb,
    0x00000000,
    0x22310001,
    0x02119020,
    0x82530000,
    0x12600003,
    0x00000000,
    0x1000fffb,
    0x00000000,
};

static inst_t test_echo[] = {
    /*
      +[>,.<]
      $ cat | misp-elf-as
      .set	noreorder

      add	$s2, $s0, $s1
      lb	$s3, 0($s2)
      addi	$s3, $s3, 1;
      sb	$s3, 0($s2)

      begin:
      add	$s2, $s0, $s1
      lb	$s3, 0($s2)
      beqz	$s3, end
      nop
      
      addi	$s1, $s1, 1

      move	$v0, $zero
      lui	$v0, 0x0040
      ori	$v0, $v0, 0x5048
      lw	$v0, 0($v0)
      jalr	$v0
      add	$s2, $s0, $s1
      sb	$v0, 0($s2)
      
      move	$v0, $zero
      lui	$v0, 0x0040
      ori	$v0, $v0, 0x5044
      lw	$v0, 0($v0)
      add	$s2, $s0, $s1
      jalr	$v0
      lb	$a0, 0($s2)

      addi	$s1, $s1, -1

      b		begin
      nop

      end:
    */
    0x02119020,
    0x82530000,
    0x22730001,
    0xa2530000,
    0x02119020,
    0x82530000,
    0x12600013,
    0x00000000,
    0x22310001,
    0x00001021,
    0x3c020040,
    0x34425048,
    0x8c420000,
    0x0040f809,
    0x02119020,
    0xa2420000,
    0x00001021,
    0x3c020040,
    0x34425044,
    0x8c420000,
    0x02119020,
    0x0040f809,
    0x82440000,
    0x2231ffff,
    0x1000ffeb,
    0x00000000,
};

void test_dump_insts(inst_t expected[], inst_t actual[], int n) {
    int i;
    printf("\tEXPECTED\tACTUAL\n");
    for (i = 0; i < n; i++) {
        printf("\t%08x\t%08x\n", expected[i], actual[i]);
    }
}

void test_compile_empty() {
    inst_t buf[64];
    int len;
    int len_expected;
    len = bf_compile("", buf, ALEN(buf));
    len_expected = ALEN(test_prolog) + ALEN(test_epilog);
    if (len == len_expected) {
        printf("[%08d] Length OK: %d\n", __LINE__, len);
    } else {
        printf("[%08d] Length ERROR: %d, %d expected.\n", __LINE__, len, len_expected);
        return;
    }
    if (memcmp(test_prolog, buf, sizeof(test_prolog)) == 0) {
        printf("[%08d] Prolog OK\n", __LINE__);
    } else {
        printf("[%08d] Prolog ERROR\n", __LINE__);
        test_dump_insts(test_prolog, buf, ALEN(test_prolog));
        return;
    }
    if (memcmp(test_epilog, &buf[ALEN(test_prolog)], sizeof(test_epilog)) == 0) {
        printf("[%08d] Epilog OK\n", __LINE__);
    } else {
        printf("[%08d] Epilog ERROR\n", __LINE__);
        test_dump_insts(test_epilog, &buf[ALEN(test_prolog)], ALEN(test_epilog));
        return;
    }
}

void test_compile_basic() {
    inst_t buf[128];
    int len;
    int len_expected;
    len = bf_compile("<+->", buf, ALEN(buf));
    len_expected = ALEN(test_prolog) + ALEN(test_basic) + ALEN(test_epilog);
    if (len == len_expected) {
        printf("[%08d] Length OK: %d\n", __LINE__, len);
    } else {
        printf("[%08d] Length ERROR: %d, %d expected.\n", __LINE__, len, len_expected);
        return;
    }
    if (memcmp(test_basic, &buf[ALEN(test_prolog)], sizeof(test_basic)) == 0) {
        printf("[%08d] Basic instructions OK\n", __LINE__);
    } else {
        printf("[%08d] Basic instructions ERROR\n", __LINE__);
        test_dump_insts(test_basic, &buf[ALEN(test_prolog)], ALEN(test_basic));
        return;
    }
}

void test_compile_jump1() {
    inst_t buf[128];
    int len;
    int len_expected;
    len = bf_compile("[]", buf, ALEN(buf));
    len_expected = ALEN(test_prolog) + ALEN(test_jump1) + ALEN(test_epilog);
    if (len == len_expected) {
        printf("[%08d] Length OK: %d\n", __LINE__, len);
    } else {
        printf("[%08d] Length ERROR: %d, %d expected.\n", __LINE__, len, len_expected);
        return;
    }
    if (memcmp(test_jump1, &buf[ALEN(test_prolog)], sizeof(test_jump1)) == 0) {
        printf("[%08d] Jump1 OK\n", __LINE__);
    } else {
        printf("[%08d] Jump1 ERROR\n", __LINE__);
        test_dump_insts(test_jump1, &buf[ALEN(test_prolog)], ALEN(test_jump1));
        return;
    }
}

void test_compile_jump2() {
    inst_t buf[128];
    int len;
    int len_expected;
    len = bf_compile("[[]]", buf, ALEN(buf));
    len_expected = ALEN(test_prolog) + ALEN(test_jump2) + ALEN(test_epilog);
    if (len == len_expected) {
        printf("[%08d] Length OK: %d\n", __LINE__, len);
    } else {
        printf("[%08d] Length ERROR: %d, %d expected.\n", __LINE__, len, len_expected);
        return;
    }
    if (memcmp(test_jump2, &buf[ALEN(test_prolog)], sizeof(test_jump2)) == 0) {
        printf("[%08d] Jump2 OK\n", __LINE__);
    } else {
        printf("[%08d] Jump2 ERROR\n", __LINE__);
        test_dump_insts(test_jump2, &buf[ALEN(test_prolog)], ALEN(test_jump2));
        return;
    }
}

void test_compile_jump3() {
    inst_t buf[128];
    int len;
    int len_expected;
    len = bf_compile("[]>[]", buf, ALEN(buf));
    len_expected = ALEN(test_prolog) + ALEN(test_jump3) + ALEN(test_epilog);
    if (len == len_expected) {
        printf("[%08d] Length OK: %d\n", __LINE__, len);
    } else {
        printf("[%08d] Length ERROR: %d, %d expected.\n", __LINE__, len, len_expected);
        return;
    }
    if (memcmp(test_jump3, &buf[ALEN(test_prolog)], sizeof(test_jump3)) == 0) {
        printf("[%08d] Jump3 OK\n", __LINE__);
    } else {
        printf("[%08d] Jump3 ERROR\n", __LINE__);
        test_dump_insts(test_jump3, &buf[ALEN(test_prolog)], ALEN(test_jump3));
        return;
    }
}

void test_compile_echo() {
    inst_t buf[128];
    int len;
    int len_expected;
    len = bf_compile("+[>,.<]", buf, ALEN(buf));
    len_expected = ALEN(test_prolog) + ALEN(test_echo) + ALEN(test_epilog);
    if (len == len_expected) {
        printf("[%08d] Length OK: %d\n", __LINE__, len);
    } else {
        printf("[%08d] Length ERROR: %d, %d expected.\n", __LINE__, len, len_expected);
        return;
    }
    if (memcmp(test_echo, &buf[ALEN(test_prolog)], sizeof(test_echo)) == 0) {
        printf("[%08d] Echo OK\n", __LINE__);
    } else {
        printf("[%08d] Echo ERROR\n", __LINE__);
        test_dump_insts(test_echo, &buf[ALEN(test_prolog)], ALEN(test_echo));
        return;
    }
}

int main(int argc, char *argv[]) {
    test_compile_empty();
    test_compile_basic();
    test_compile_jump1();
    test_compile_jump2();
    test_compile_jump3();
    test_compile_echo();
    return (0);
}
#endif /* TEST_BFC */

