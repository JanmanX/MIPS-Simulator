#ifndef _CP0_H
#define _CP0_H
#include <stdint.h>

#define CP0_NUM_REGS	40
extern char *cp0_reg_names[CP0_NUM_REGS];

#define REG_UNUSED      0	/* Offset due to forwarding hazard to reg 0 */
#define	REG_CONTEXT	1
#define	REG_RANDOM	2
#define	REG_ENTRYLO0	3
#define	REG_ENTRYLO1	4
#define	REG_CONTEXT2	5
#define	REG_PAGEMASK	6
#define	REG_WIRED	7
#define	REG_ENTRYHI	8
#define	REG_HWRENA	9
#define	REG_BADVADDR	10
#define	REG_COUNT	11
#define	REG_COMPARE	12
#define	REG_SR		13
#define	REG_INTCTL	14
#define	REG_EPC		15
#define	REG_SRSMAP	16
#define	REG_CAUSE	17
#define	REG_SRSCTL	18
#define	REG_PRID	19
#define	REG_EBASE	20
#define	REG_CONFIG	21
#define	REG_CONFIG1	22
#define	REG_CONFIG2	23
#define	REG_CONFIG3	24
#define	REG_LLADDR	25
#define	REG_DEBUG	26
#define	REG_DEPC	27
#define	REG_DESAVE	28
#define	REG_CACHEERR	29
#define	REG_ECC		30
#define	REG_ERROREPC	31
#define	REG_TAGLO	32
#define	REG_DATALO	33
#define	REG_TAGHI	34
#define	REG_DATAHI	35
#define	REG_WATCHLO	36
#define	REG_WATCHHI	37
#define	REG_PERFCTL	38
#define REG_PERFCNT	39


/* XXX: Where is the LLVAL really stored? */
#define REG_LLVAL	40


/* Status register bits
 * See Mips Run page 60. */
#define SR_IE	(uint32_t)(1 << 0)
#define SR_EXL	(uint32_t)(1 << 1)
#define SR_ERL	(uint32_t)(1 << 2)
#define SR_KSU  (uint32_t)(2 << 3)	/* 2 BITS! */
#define SR_UX	(uint32_t)(1 << 5)
#define SR_SX	(uint32_t)(1 << 6)
#define SR_KX	(uint32_t)(1 << 7)
/* Some space here */
#define SR_NMI	(uint32_t)(1 << 19)
#define SR_SR	(uint32_t)(1 << 20)
#define SR_TS	(uint32_t)(1 << 21)
#define SR_BEV	(uint32_t)(1 << 22)
#define SR_PX	(uint32_t)(1 << 23)
#define SR_MX	(uint32_t)(1 << 24)
#define SR_RE	(uint32_t)(1 << 25)
#define SR_FR	(uint32_t)(1 << 26)
#define SR_RP	(uint32_t)(1 << 27)

/* CAUSE register bits
 * See Mips Run page 64 */
#define CAUSE_BD (uint32_t)(1 << 31)
#define CAUSE_ExcCode (uint32_t)(31 << 1) /* 5 bits, bit 2 - 6 */


/* Cause Exception Codes */
#define CAUSE_ExcCode_Interrupt 0
#define CAUSE_ExcCode_Mod	1
#define CAUSE_ExcCode_TLBL	2
#define CAUSE_ExcCode_TLBS	3
#define CAUSE_ExcCode_AdEL	4
#define CAUSE_ExcCode_AdES	5
#define CAUSE_ExcCode_IBE	6
#define CAUSE_ExcCode_DBE	7
#define CAUSE_ExcCode_Syscall	8
#define CAUSE_ExcCode_Bp	9
#define CAUSE_ExcCode_RI	10
#define CAUSE_ExcCode_CpU	11
#define CAUSE_ExcCode_Ov	12
#define CAUSE_ExcCode_TRAP	13
#define CAUSE_ExcCode_UNUSED	14
#define CAUSE_ExcCode_FPE	15



typedef struct cp0 {
	uint32_t regs[CP0_NUM_REGS + 1]; /* Offset from 0, due to forwarding
					    collisions */
} cp0_t;


void cp0_dump_registers(cp0_t *cp0);

#endif /* _CP0_H */
