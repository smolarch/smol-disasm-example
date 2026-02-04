// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <smol/disasm.h>

#include "test_data.inc"

static bool verbose = false;

static const char *marker[4] = {
#if 0
    [SMOL_INST_SHORT] = ">",
    [SMOL_INST_HEAD] = " ",
    [SMOL_INST_BODY] = " ",
    [SMOL_INST_TAIL] = ">",
#else
    [SMOL_INST_SHORT] = " ",
    [SMOL_INST_HEAD] = "╭",
    [SMOL_INST_BODY] = "│",
    [SMOL_INST_TAIL] = "╰",
#endif
};

static int take_until(char *dst, int dst_len, const char *src, char c) {
    int i, len = strlen(src);

    for (i = 0; i < len && i < (dst_len - 1); ++i) {
        if (src[i] == '\0' || src[i] == c) {
            break;
        }
        dst[i] = src[i];
    }
    dst[i] = 0;

    return i;
}

static void parse_opt_extension(smol_disasm_t *disasm, const char *optarg) {
    char buf[128];
    char *end;
    int i, major, minor, ext;
    const uint16_t *info;

    major = 1;
    minor = 0;

    i = take_until(buf, sizeof(buf), optarg, '-');
    ext = smol_find_ext_by_name(buf);
    if (ext == -1) {
        fprintf(stderr, "error: extension \"%s\" not found\n", optarg);
        exit(EXIT_FAILURE);
    }

    if (optarg[i] == '-') {
        i += 1 + take_until(buf, sizeof(buf), &optarg[i + 1], '.');
        major = strtol(buf, &end, 10);
        if (*buf == '\0' || *end != '\0') {
            fprintf(stderr, "error: invalid major version \"%s\"\n", optarg);
            exit(EXIT_FAILURE);
        }

        if (optarg[i] == '.') {
            const char *s = optarg + i + 1;
            minor = strtol(s, &end, 10);
            if (*s == '\0' || *end != '\0') {
                fprintf(stderr, "error: invalid minor version \"%s\"\n", optarg);
                exit(EXIT_FAILURE);
            }
        }
    }

    info = smol_find_ext_info_by_id(ext);
    if (info) {
        while (ext == info[0]) {
            if (major == info[1] && minor <= info[2]) {
                smol_add_ext(disasm, ext, major, minor);
                return;
            }
            info += 4 + info[3] * 3;
        }
    }

    fprintf(stderr, "error: extension \"%s\" not found\n", optarg);
    exit(EXIT_FAILURE);
}

static int parse_opts(smol_disasm_t *disasm, int argc, char *argv[]) {
    int c;

    for (;;) {
        c = getopt(argc, argv, "vus:e:");
        if (c == -1)
            break;

        switch(c) {
        case 'v':
            verbose = true;
            break;
        case 'u':
            marker[SMOL_INST_SHORT] = " ";
            marker[SMOL_INST_HEAD] = "╭";
            marker[SMOL_INST_BODY] = "│";
            marker[SMOL_INST_TAIL] = "╰";
            break;
        case 's':
            marker[SMOL_INST_SHORT] = optarg;
            marker[SMOL_INST_HEAD] = " ";
            marker[SMOL_INST_BODY] = " ";
            marker[SMOL_INST_TAIL] = optarg;
            break;
        case 'e':
            parse_opt_extension(disasm, optarg);
            break;
        default:
            exit(EXIT_FAILURE);
        }
    }

    return optind;
}

static void smol_add_ext_deps(smol_disasm_t *disasm, enum smol_ext ext, int major, int minor) {
    const uint16_t *info = smol_find_ext_info_by_id(ext);

    while (ext == info[0]) {
        int count = info[3];

        if (info[1] == major && info[2] <= minor) {
            const uint16_t *dep = &info[4];
            for (int i = 0; i < count; ++i, dep += 3) {
                smol_add_ext(disasm, dep[0], dep[1], dep[2]);
            }
        }

        info += 4 + count * 3;
    }
}

void smol_add_ext(smol_disasm_t *disasm, enum smol_ext ext, int major, int minor) {
    smol_ext_entry_t *e;
    int size;

    if (disasm->extensions) {
        for (int i = 0; i < disasm->extensions_len; ++i) {
            e = &disasm->extensions[i];

            if (ext != e->ext )
                continue;
            if (major != e->major)
                continue;
            if (e->minor < minor) {
                int i = e->minor + 1;
                e->minor = minor;
                // add dependencies from old_minor to new_minor versions of this extension
                for (; i <= minor; ++i) {
                    smol_add_ext_deps(disasm, ext, major, i);
                }
            }

            return;
        }
    }

    if (!disasm->extensions) {
        disasm->extensions_cap = 32;
        disasm->extensions_len = 0;
        size = disasm->extensions_cap * sizeof(disasm->extensions[0]);
        disasm->extensions = malloc(size);
    } else if (disasm->extensions_len == disasm->extensions_cap) {
        disasm->extensions_cap *= 2;
        size = disasm->extensions_cap * sizeof(disasm->extensions[0]);
        disasm->extensions = realloc(disasm->extensions, size);
    }

    e = &disasm->extensions[disasm->extensions_len++];
    e->ext = ext;
    e->major = major;
    e->minor = minor;

    // add dependencies from 0 to minor versions of this extension
    for (int i = 0; i <= minor; ++i) {
        smol_add_ext_deps(disasm, ext, major, minor);
    }
}

bool smol_has_ext(smol_disasm_t *disasm, enum smol_ext ext, int major, int minor) {
    if (disasm->extensions) {
        for (int i = 0; i < disasm->extensions_len; ++i) {
            const smol_ext_entry_t *e = &disasm->extensions[i];
            if (ext != e->ext)
                continue;
            return major == e->major && minor <= e->minor;
        }
    }
    return false;
}

int smol_decode_read(smol_disasm_t *disasm, int start, int end) {
    if (disasm->len < disasm->offset + end) {
        return 0;
    }

    for (int i = start; i < end; ++i) {
        uint8_t byte = disasm->data[disasm->offset + i];
        int offset = i * 8;
        disasm->inst &= ~(0xffUL << offset);
        disasm->inst |= (smol_inst_t) byte << offset;
    }

    return end - start;
}

static int32_t smol_extract_field(smol_inst_t inst, enum smol_field field) {
    switch (field) {
        #define SMOL_DEFINE_FIELD(enum_name, name) \
            case SMOL_FIELD(enum_name): return SMOL_FIELD_EXTRACT_##enum_name(inst);
        #include <smol/fields.h>
        #undef SMOL_DEFINE_FIELD
        default:
            assert(0 && "unexpected field");
    }
}

static inline void print_operand_separator(int index) {
    if (index > 0) {
        printf(",");
    }
}

static void print_reg_x(smol_disasm_t *disasm, int index, enum smol_field field, int value) {
    print_operand_separator(index);
    printf("%s", smol_gpr_abi_names[value]);
}

static void print_imm(smol_disasm_t *disasm, int index, enum smol_field field, int value) {
    if (field == SMOL_FIELD_SHAMT3 && value == 0) {
        return;
    }
    print_operand_separator(index);
    printf("%d", value);
}

static void print_operand(
    smol_disasm_t *disasm,
    int index,
    enum smol_operand operand,
    enum smol_field field
) {
    int32_t value = smol_extract_field(disasm->inst, field);

    switch (operand) {
    case SMOL_OPERAND_REG_X:
        print_reg_x(disasm, index, field, value);
        break;
    case SMOL_OPERAND_IMM:
        print_imm(disasm, index, field, value);
        break;
    }
}

static void print_inst(smol_disasm_t *disasm, size_t address, enum smol_inst id, const uint8_t *info) {
    const char *inst_name = SMOL_INST_INFO_NAME(info);
    int operands_count = SMOL_INST_INFO_OPERANDS_COUNT(info);
    int inst_len = SMOL_INST_INFO_OPCODE_LEN(info);
    enum smol_inst_type inst_type;

    printf("  %4x:", (unsigned) address);

    for (int i = 0; i < inst_len; i += 2) {
        printf(" %04x", (uint16_t) (disasm->inst >> (i * 8)));
    }
    for (int i = inst_len; i < SMOL_MAX_INST_LEN; i += 2) {
        printf("     ");
    }
    printf("  ");

    inst_type = smol_inst_type(disasm->bundle_class, disasm->stop);
    printf(" %s%s", marker[inst_type], inst_name);

    if (operands_count > 0) {
        const uint8_t *p = SMOL_INST_INFO_OPERANDS_PTR(info);

        if (SMOL_INST_INFO_NAME_LEN(info) <= 3) {
            printf("\t\t");
        } else {
            printf("\t");
        }

        for (int i = 0; i < operands_count; ++i) {
            enum smol_operand operand = *p++;
            enum smol_field field = *p++;

            print_operand(disasm, i, operand, field);
        }
    }

    printf("\n");
}

static int disassemble_inst(smol_disasm_t *disasm) {
    enum smol_inst id;
    const uint8_t *info;
    enum smol_inst_class inst_class;
    int inst_len, result;

    switch (disasm->bundle_class) {
    #define SMOL_DEFINE_CLASS(enum_name, name, decode_name) \
        case SMOL_CLASS(enum_name): \
            result = SMOL_DECODE(decode_name)(disasm); \
            break;
    #include <smol/inst-class.h>
    #undef SMOL_DEFINE_CLASS
    default:
        assert(0 && "unexpected bundle class");
    }

    if (result < 0) {
        return result;
    }

    id = result;
    info = smol_get_inst_info(id);
    inst_len = SMOL_INST_INFO_OPCODE_LEN(info);
    inst_class = SMOL_INST_INFO_CLASS(info);

    disasm->stop = SMOL_IS_STOP(disasm->inst, inst_len);
    print_inst(disasm, disasm->offset, id, info);
    disasm->offset += inst_len;

    if (disasm->stop) {
        disasm->bundle_class = CLASS_NONE;
        return 0;
    }

    switch (disasm->bundle_class) {
    case CLASS_NONE:
        disasm->bundle_class = inst_class;
        break;
    case CLASS_LOAD:
        if (inst_class == CLASS_LOAD) {
            disasm->bundle_class = CLASS_INT;
        }
        break;
    default:
        break;
    }

    return 0;
}

void disassemble(smol_disasm_t *disasm) {
    disasm->bundle_class = CLASS_NONE;
    disasm->offset = 0;
    while (disasm->offset < disasm->len) {
        int status = disassemble_inst(disasm);
        if (status != 0) {
            if (status == SMOL_DECODE_NONE) {
                printf("failed to decode\n");
                disasm->offset += 2;
            } else if (status == SMOL_DECODE_READ_ERROR) {
                printf("unexpected end\n");
                break;
            } else {
                printf("unknown error %d\n", status);
                break;
            }
        }
    }
}

static int read_file(smol_disasm_t *disasm, const char *path) {
    FILE *f = fopen(path, "r");
    size_t n;

    if (!f) {
        perror("fopen");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    n = n < 4096 ? 4096 : n;

    disasm->len = 0;
    while (!feof(f)) {
        if (disasm->cap < disasm->len + n) {
            disasm->cap *= disasm->cap * 2;
            if (disasm->cap < n) {
                disasm->cap = n;
            }
            disasm->data = realloc(disasm->data, disasm->cap);
        }
        n = fread(disasm->data + disasm->len, 1, n, f);
        if (ferror(f)) {
            perror("fread");
            fclose(f);
            return -1;
        } if (n == 0) {
            break;
        }
        disasm->len += n;
        n = 4096;
    }

    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    smol_disasm_t *disasm;
    int args;

    disasm = calloc(1, sizeof(*disasm));
    args = parse_opts(disasm, argc, argv);

    if (!disasm->extensions_len) {
        smol_add_ext(disasm, SMOL_EXT_SMOL64, 0, 0);
    }

    smol_init_inst_info(disasm);
    smol_init_inst_valid(disasm);

    if (verbose && disasm->extensions_len) {
        printf("extensions:\n");
        for (int i = 0; i < disasm->extensions_len; ++i) {
            const smol_ext_entry_t *e = &disasm->extensions[i];
            printf("  %s %d.%d\n", smol_ext_name[e->ext], e->major, e->minor);
        }
        printf("\n");
    }

    if (args < argc) {
        for (int i = args; i < argc; ++i) {
            const char *file = argv[i];
            printf("file: %s\n", file);
            if (read_file(disasm, file) == 0) {
                disassemble(disasm);
            }
        }
    } else {
        disasm->data = (uint8_t *) test_data;
        disasm->len = sizeof(test_data);
        printf("test data:\n");
        disassemble(disasm);
    }

    if (disasm->cap) {
        free(disasm->data);
    }

    return 0;
}
