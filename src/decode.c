// SPDX-License-Identifier: MIT

#include <string.h>
#include <smol/disasm.h>

const char *smol_ext_name[SMOL_EXT_LAST] = {
    #define SMOL_DEFINE_EXTENSION(id, enum_name, name) name,
    #include "smol/extensions.h"
    #undef SMOL_DEFINE_EXTENSION
};

static uint16_t smol_ext_info[] = {
    #include "extensions-info.inc"
    -1
};

smol_ext_info_t smol_find_ext_info_by_id(enum smol_ext id) {
    const uint16_t *p = smol_ext_info;
    while (p[0] != (uint16_t) -1) {
        if (p[0] == id) {
            return p;
        }
        p += 4 + p[3] * 3;
    }
    return NULL;
}

int smol_find_ext_by_name(const char *s) {
    for (int i = 0; i < SMOL_EXT_LAST; ++i) {
        if (strcmp(smol_ext_name[i], s) == 0) {
            return i;
        }
    }
    return -1;
}

const char smol_field_names[SMOL_FIELD_LAST][NFC] = {
    #define SMOL_DEFINE_FIELD(enum_name, name) name,
    #include "smol/fields.h"
    #undef SMOL_DEFINE_FIELD
};

const char smol_gpr_names[NGPR][NRC] = {
    #define SMOL_DEFINE_REG_X(i, enum_name, enum_abi_name, name, abi_name, desc) name,
    #include "smol/regs-x.h"
    #undef SMOL_DEFINE_REG_X
};

const char smol_gpr_abi_names[NGPR][NRC] = {
    #define SMOL_DEFINE_REG_X(i, enum_name, enum_abi_name, name, abi_name, desc) abi_name,
    #include "smol/regs-x.h"
    #undef SMOL_DEFINE_REG_X
};

uint8_t smol_inst_info[] = {
    #include "inst-info.inc"
};

uint16_t smol_inst_info_offset[SMOL_INST_LAST] = { 0 };

void smol_init_inst_info(smol_disasm_t *disasm) {
    uint16_t offset = 0;
    for (int i = 0; i < SMOL_INST_LAST; ++i) {
        smol_inst_info_offset[i] = offset;
        offset += SMOL_INST_INFO_LEN(&smol_inst_info[offset]);
    }
}

void smol_reset_inst_valid(smol_disasm_t *disasm) {
    memset(disasm->valid, 0, sizeof(disasm->valid));
}

void smol_init_inst_valid(smol_disasm_t *disasm) {
    #define SMOL_HAS_EXT(ext, major, minor) \
        smol_has_ext(disasm, ext, major, minor)

    #define SMOL_INST_ENABLE(id) \
        disasm->valid[SMOL_VALID_INDEX(id)] |= SMOL_VALID_BIT(id)

    #include "init.inc"

    #undef SMOL_HAS_EXT
    #undef SMOL_INST_ENABLE
}

#define SMOL_DEFINE_DECODE(name) \
    int SMOL_DECODE(name)(smol_disasm_t *disasm)

#define SMOL_DECODE_READ(from, to) \
    if (!smol_decode_read(disasm, from, to)) \
        return SMOL_DECODE_READ_ERROR

#define SMOL_DECODE_INST() (disasm->inst)

#define SMOL_DECODE_HIT(NAME) \
    if (disasm->valid[SMOL_VALID_INDEX(SMOL_INST(NAME))] & SMOL_VALID_BIT(SMOL_INST(NAME))) \
        return SMOL_INST(NAME)

#define SMOL_DECODE_CALL(func) \
    do { \
        int result = SMOL_DECODE(func)(disasm); \
        if (result != SMOL_DECODE_NONE) { \
            return result; \
        } \
    } while(0)

#define SMOL_DECODE_END(name) \
    return SMOL_DECODE_NONE

#include "decode.inc"
