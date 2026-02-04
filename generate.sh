#!/bin/sh

script=$(readlink -f "$0")
root=$(dirname "$script")

inc=$root/include/smol
src=$root/src

exec smolarch gen-c -e smol64 \
    --extensions             $inc/extensions.h \
    --extensions-info        $src/extensions-info.inc \
    --regs-x                 $inc/regs-x.h \
    --operands               $inc/operands.h \
    --fields                 $inc/fields.h \
    --fields-extract         $inc/fields-extract.h \
    --insts-class            $inc/inst-class.h \
    --insts                  $inc/inst.h \
    --insts-info-extract     $inc/inst-info-extract.h \
    --insts-info             $src/inst-info.inc \
    --init                   $src/init.inc \
    --decode                 $src/decode.inc
