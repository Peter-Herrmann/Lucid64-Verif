#ifndef _COMPLIANCE_MODEL_H
#define _COMPLIANCE_MODEL_H
#define RVMODEL_DATA_SECTION                                                  \
        .pushsection .tohost,"aw",@progbits;                                  \
        .align 8; .global tohost; tohost: .dword 0;                           \
        .align 8; .global fromhost; fromhost: .dword 0;                       \
        .popsection;                                                          \
        .align 8; .global begin_regstate; begin_regstate:                     \
        .word 128;                                                            \
        .align 8; .global end_regstate; end_regstate:                         \
        .word 4;

//RV_COMPLIANCE_HALT
#define RVMODEL_HALT                                                          \
      li    x1, 0xFFFFFFF8;                                                   \
      la    x2, begin_signature;                                              \
      la    x3, end_signature;                                                \
                                                                              \
    sig_write_loop:                                                           \
      lw    x4, 0(x2);                                                        \
      sw    x4, 0(x1);                                                        \
      addi  x2, x2, 4;                                                        \
      blt   x2, x3, sig_write_loop;                                           \
                                                                              \
      li    x1, 0xF0F0F0F0;                                                   \
      sw    x1, 0(x1);                                                        \
    hang:                                                                     \
      j     hang

#define RVMODEL_BOOT      

//RV_COMPLIANCE_DATA_BEGIN      
#define RVMODEL_DATA_BEGIN                                                    \
  RVMODEL_DATA_SECTION                                                        \
  .align 4;                                                                   \
  .global begin_signature; begin_signature:

//RV_COMPLIANCE_DATA_END      
#define RVMODEL_DATA_END                                                      \
  .align 4;                                                                   \
  .global end_signature; end_signature:  

//RVTEST_IO_INIT
#define RVMODEL_IO_INIT
//RVTEST_IO_WRITE_STR
#define RVMODEL_IO_WRITE_STR(_R, _STR)
//RVTEST_IO_CHECK
#define RVMODEL_IO_CHECK()
//RVTEST_IO_ASSERT_GPR_EQ
#define RVMODEL_IO_ASSERT_GPR_EQ(_S, _R, _I)
//RVTEST_IO_ASSERT_SFPR_EQ
#define RVMODEL_IO_ASSERT_SFPR_EQ(_F, _R, _I)
//RVTEST_IO_ASSERT_DFPR_EQ
#define RVMODEL_IO_ASSERT_DFPR_EQ(_D, _R, _I)

#define RVMODEL_SET_MSW_INT                                                   \
 li t1, 1;                                                                    \
 li t2, 0x2000000;                                                            \
 sw t1, 0(t2);

#define RVMODEL_CLEAR_MSW_INT                                                 \
 li t2, 0x2000000;                                                            \
 sw x0, 0(t2);

#define RVMODEL_CLEAR_MTIMER_INT

#define RVMODEL_CLEAR_MEXT_INT


#endif // _COMPLIANCE_MODEL_H
