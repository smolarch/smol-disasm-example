// SPDX-License-Identifier: MIT

#ifndef SMOL_DISASM_H
#define SMOL_DISASM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define SMOL_EXT(NAME) SMOL_EXT_##NAME
#define SMOL_CLASS(NAME) CLASS_##NAME
#define SMOL_INST(NAME) SMOL_INST_##NAME
#define SMOL_INST_FLAG(NAME) SMOL_INST_FLAG_##NAME
#define SMOL_OPERAND(NAME) SMOL_OPERAND_##NAME
#define SMOL_FIELD(NAME) SMOL_FIELD_##NAME
#define SMOL_DECODE(NAME) smol_decode_##NAME

enum smol_ext {
    #define SMOL_DEFINE_EXTENSION(id, enum_name, name) SMOL_EXT(enum_name),
    #include "extensions.h"
    #undef SMOL_DEFINE_EXTENSION
    SMOL_EXT_LAST
};

enum smol_inst_class {
    #define SMOL_DEFINE_CLASS(enum_name, name, decode_name) SMOL_CLASS(enum_name),
    #include "inst-class.h"
    #undef SMOL_DEFINE_CLASS
};

enum smol_inst {
    #define SMOL_DEFINE_INST(class, enum_name, name) SMOL_INST(enum_name),
    #include "inst.h"
    #undef SMOL_DEFINE_INST
    SMOL_INST_LAST
};

enum smol_operand {
    #define SMOL_DEFINE_OPERAND(enum_name, name) SMOL_OPERAND(enum_name),
    #include "operands.h"
    #undef SMOL_DEFINE_OPERAND
};

enum smol_field {
    #define SMOL_DEFINE_FIELD(enum_name, name) SMOL_FIELD(enum_name),
    #include "fields.h"
    #undef SMOL_DEFINE_FIELD
    SMOL_FIELD_LAST
};

enum smol_inst_type {
    SMOL_INST_SHORT,
    SMOL_INST_HEAD,
    SMOL_INST_BODY,
    SMOL_INST_TAIL,
};


static inline enum smol_inst_type smol_inst_type(enum smol_inst_class bundle_class, bool stop) {
    if (bundle_class == CLASS_NONE) {
        return stop ? SMOL_INST_SHORT : SMOL_INST_HEAD;
    } else {
        return stop ? SMOL_INST_TAIL : SMOL_INST_BODY;
    }
}

typedef struct {
    enum smol_ext ext;
    uint16_t major;
    uint16_t minor;
} smol_ext_entry_t;

typedef uint32_t smol_inst_t;

#define SMOL_MAX_INST_LEN sizeof(smol_inst_t)

// Returns true if an instruction is the last in a bundle.
#define SMOL_IS_STOP(inst, len) (((inst) & (1 << ((len) * 8 - 1))) == 0)

enum smol_inst_flags {
    SMOL_INST_FLAG_NONE = 0,
    SMOL_INST_FLAG_ALIAS = 1 << 0,
    SMOL_INST_FLAG_RESERVED = 1 << 1,
};

typedef unsigned long smol_valid_t;
#define SMOL_VALID_SIZE (sizeof(smol_valid_t) * 8)
#define SMOL_VALID_LEN ((SMOL_INST_LAST + SMOL_VALID_SIZE - 1) / SMOL_VALID_SIZE)
#define SMOL_VALID_INDEX(id) ((id) / SMOL_VALID_SIZE)
#define SMOL_VALID_BIT(id) (1ULL << (id & (SMOL_VALID_SIZE - 1)))

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
    size_t offset;
    smol_inst_t inst;
    bool stop;

    enum smol_inst_class bundle_class;

    smol_valid_t valid[SMOL_VALID_LEN];

    smol_ext_entry_t *extensions;
    int extensions_len;
    int extensions_cap;
} smol_disasm_t;

typedef const uint16_t *smol_ext_info_t;

extern const char *smol_ext_name[SMOL_EXT_LAST];

smol_ext_info_t smol_find_ext_info_by_id(enum smol_ext id);
int smol_find_ext_by_name(const char *name);
void smol_add_ext(smol_disasm_t *disasm, enum smol_ext ext, int major, int minor);
bool smol_has_ext(smol_disasm_t *disasm, enum smol_ext ext, int major, int minor);

#define SMOL_EXTRACT_S(inst, offset, len) \
    (((int64_t) (inst) << (64 - (offset) - (len))) >> (64 - (len)))

#define SMOL_EXTRACT_Z(inst, offset, len) \
    (((uint64_t) (inst) << (64 - (offset) - (len))) >> (64 - (len)))

#include "fields-extract.h"

typedef const uint8_t *smol_inst_info_t;

extern uint8_t smol_inst_info[];
extern uint16_t smol_inst_info_offset[SMOL_INST_LAST];

#include "inst-info-extract.h"

void smol_init_inst_info(smol_disasm_t *disasm);

static inline smol_inst_info_t smol_get_inst_info(enum smol_inst id) {
    assert(id < SMOL_INST_LAST);
    return &smol_inst_info[smol_inst_info_offset[id]];
}

static inline smol_inst_t smol_inst_info_opcode(smol_inst_info_t inst_info) {
    smol_inst_t opcode = 0;
    for (int i = 0; i < SMOL_INST_INFO_OPCODE_LEN(inst_info); ++i) {
        opcode |= (smol_inst_t) SMOL_INST_INFO_OPCODE_PTR(inst_info)[i] << (i * 8);
    }
    return opcode;
}

#define NGPR 32
#define NFC (11 + 1) // maximum length of field name including nul-terminator
#define NRC (4 + 1) // maximum length of register name including nul-terminator

extern const char smol_field_names[SMOL_FIELD_LAST][NFC];
extern const char smol_gpr_names[NGPR][NRC];
extern const char smol_gpr_abi_names[NGPR][NRC];

void smol_reset_inst_valid(smol_disasm_t *disasm);
void smol_init_inst_valid(smol_disasm_t *disasm);

int smol_decode_read(smol_disasm_t *disasm, int from, int to);

#define SMOL_DECODE_NONE -1
#define SMOL_DECODE_READ_ERROR -2

// Declare decode functions.
#define SMOL_DEFINE_CLASS(enum_name, name, decode_name) \
    int SMOL_DECODE(decode_name)(smol_disasm_t *disasm);

#include "inst-class.h"
#undef SMOL_DEFINE_CLASS

#endif // SMOL_DISASM_H
