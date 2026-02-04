static const uint16_t test_data[] = {
#if 1
    0x8108, //  seqb a0,a0
    0x8bff, //  addi -1
    0x87ff, //  sori 2047
    0x87ff, //  sori 2047
    0xa029, //  add a1
    0x212a, // >add a2,2
    0x0000, // >illop
    0x97ff, //  bal
    0x87ff, // >sori 2047
    0xfc08, //  breakz.eq a0,a1
    0x0009,
    0x052a, // >mv a1,a2
#endif

#if 0
    0x8800, //  seqw
    0x0800, // >add
    0x8c00, //  seqd
    0x0800, // >add
#endif

#if 1
    0x1d01, // >li a0,1
    0x9d02, //  li a0,2
    0x0003, // >sori15 3
    0x9d04, //  li a0,4
    0x8005, //  sori15 5
    0x0006, // >sori15 6
    0x9d10, //  li a0,-16
    0xffff, //  sori15 32767
    0x7fff, // >sori15 32767
#endif
};
