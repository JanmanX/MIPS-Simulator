#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "tools.h"
#include "sim.h"
#include "cpu.h"
#include "elf.h"
#include "mips32.h"
#include "disasm.h"
#include "debug.h"
#include "error.h"

#define MEMSZ 0x20000000 /* 512 MiB */

/* MACROS for less typing */
#define IF_ID (core->if_id)
#define ID_EX (core->id_ex)
#define EX_MEM (core->ex_mem)
#define MEM_WB (core->mem_wb)
#define PC (core->regs[REG_PC])
#define REGS(x) (core->regs[(x)])


/* Signals if program stopped */
bool g_finished = false;

/* Signals debugging */
bool g_debugging = false;


void interpret_if(core_t *core, memory_t *mem)
{
	/* Fetch the next instruction */
	uint32_t inst = GET_BIGWORD(mem->raw, core->regs[REG_PC]);

	/* Hazard control
	 * COD5 p. 314 */
	if(ID_EX.c_mem_read
	   && ((ID_EX.rt == GET_RS(inst))
	       || (ID_EX.rt == GET_RT(inst)))) {
		/* Insert NOP */
		IF_ID.inst = 0;
		return;
	}

	core->if_id.inst = inst;

	/* Point PC to next instruction and store in pipeline reg */
	PC += 4;
	IF_ID.next_pc = PC;
}

/* Control unit in the ID stage */
void interpret_id_control(core_t *core)
{
	uint32_t inst = IF_ID.inst;

	/* Clear all */
	ID_EX.c_reg_dst		= 0;
	ID_EX.c_alu_op		= 0x00;
	ID_EX.c_alu_src		= 0;
	ID_EX.c_beq		= 0;
	ID_EX.c_bne		= 0;
	ID_EX.c_mem_read	= 0;
	ID_EX.c_mem_write	= 0;
	ID_EX.c_reg_write	= 0;
	ID_EX.c_mem_to_reg	= 0;
	ID_EX.c_jump		= 0;


	/* COD5, page 302, fig. 4.49 */
	switch(GET_OPCODE(inst))
	{
	case OPCODE_R:
		/* If JR */
		if(GET_FUNCT(ID_EX.inst) == FUNCT_JR) {
			ID_EX.c_jump		= 1;
		} else {
			ID_EX.c_reg_dst		= 1;
			ID_EX.c_alu_op		= 0x02;
			ID_EX.c_reg_write	= 1;
		}
		break;

	case OPCODE_LBU:
	case OPCODE_LHU:
	case OPCODE_LW:
		ID_EX.c_alu_src		= 1;
		ID_EX.c_mem_read	= 1;
		ID_EX.c_reg_write	= 1;
		ID_EX.c_mem_to_reg	= 1;
		break;

	case OPCODE_LL:

	case OPCODE_LUI:
		ID_EX.c_alu_op		= 0x03;
		ID_EX.c_alu_src		= 1;
		ID_EX.c_reg_write	= 1;
		ID_EX.c_reg_dst		= 0; /* write to RT */
		break;

	case OPCODE_SB:
	case OPCODE_SH:
	case OPCODE_SW:
		ID_EX.c_alu_op		= 0x00;
		ID_EX.c_alu_src		= 1;
		ID_EX.c_mem_write	= 1;
		break;

	case OPCODE_SC:
		ERROR("INSTRUCTION NOT IMPLEMENTED: %s",
		      op_codes[GET_OPCODE(inst)]);
		break;

	case OPCODE_BEQ:
		ID_EX.c_beq		= 1;
			break;

	case OPCODE_BNE:
		ID_EX.c_bne		= 1;
		break;

	case OPCODE_SLTI:
	case OPCODE_SLTIU:
	case OPCODE_ORI:
	case OPCODE_ANDI:
	case OPCODE_ADDI:
	case OPCODE_ADDIU:
		ID_EX.c_alu_op		= 0x03;
		ID_EX.c_alu_src		= 1;
		ID_EX.c_reg_write	= 1;
		break;


	case OPCODE_J:
		ID_EX.c_jump		= 1;
		ID_EX.jump_addr = (ID_EX.next_pc & 0xF0000000)
			| (GET_ADDRESS(ID_EX.inst)<<2);
		break;

	/* Same as JUMP, but store the next address in $ra */
	case OPCODE_JAL:
		ID_EX.c_jump		= 1;
		ID_EX.jump_addr = (ID_EX.next_pc & 0xF0000000)
			| (GET_ADDRESS(ID_EX.inst)<<2);

		/* Value to store */
		ID_EX.rs_value = ID_EX.next_pc + 0x04; /* after branch delay */

		/* Save to RT register */
		ID_EX.c_reg_dst = 0;

		/* RT is return address register */
		ID_EX.rt = REG_RA;

		/* Write to register, of course */
		ID_EX.c_reg_write = 1;
		break;
	}
}

void interpret_id(core_t *core)
{
	uint32_t inst = IF_ID.inst;

	ID_EX.rt		= GET_RT(inst);
	ID_EX.rd		= GET_RD(inst);
	ID_EX.rs		= GET_RS(inst);
	ID_EX.rs_value		= core->regs[GET_RS(inst)];
	ID_EX.rt_value		= core->regs[GET_RT(inst)];
	ID_EX.sign_ext_imm	= SIGN_EXTEND(GET_IMM(inst));
	ID_EX.funct		= GET_FUNCT(inst);
	ID_EX.shamt		= GET_SHAMT(inst);
	ID_EX.inst		= IF_ID.inst;
	ID_EX.next_pc		= IF_ID.next_pc;

	/* Control unit */
	interpret_id_control(core);
}

/* ALU in EX stage
 * COD5, page 301, fig. 4.47 */
void interpret_ex_alu(core_t *core)
{
	uint32_t a = ID_EX.rs_value;

	/* MUX B for alu_src */
	uint32_t b = ID_EX.c_alu_src == 0 ? ID_EX.rt_value : ID_EX.sign_ext_imm ;

	/* LW and SW */
	if(ID_EX.c_alu_op == 0x00) {
		DEBUG("LW a: %d\t b: %d", a, b);
		EX_MEM.alu_res = a + b;
		return;
	}

	/* BEQ */
	if(ID_EX.c_alu_op == 0x01) {
		/* UNUSED */
		return;
	}

	/* R-Type */
	if(ID_EX.c_alu_op == 0x02) {
		switch(ID_EX.funct) {
		case FUNCT_ADD:
			DEBUG("A: %d   B: %d",a,b);
			EX_MEM.alu_res = (int32_t)a + (int32_t)b;
			break;

		case FUNCT_ADDU:
			EX_MEM.alu_res = a + b;
			break;

		case FUNCT_SUB:
			EX_MEM.alu_res = (int32_t)a + (int32_t)b;
			break;

		case FUNCT_SUBU:
			EX_MEM.alu_res = a - b;
			break;

		case FUNCT_AND:
			EX_MEM.alu_res = a & b;
			break;

		case FUNCT_OR:
			EX_MEM.alu_res = a | b;
			break;

		case FUNCT_NOR:
			EX_MEM.alu_res = ~(uint32_t)((uint32_t)a | (uint32_t)b);
			DEBUG("NOR: %08x", EX_MEM.alu_res);
			break;

		case FUNCT_SLT:
			EX_MEM.alu_res = ((int32_t)a < (int32_t)b) ? 1 : 0;
			break;

		case FUNCT_SLTU:
			EX_MEM.alu_res = a < b ? 1 : 0;
			break;

		case FUNCT_SLL:
			EX_MEM.alu_res = b << ID_EX.shamt;
			DEBUG("SHIFTING %d << %d = %d", b, ID_EX.shamt,
			      EX_MEM.alu_res);
			break;

		case FUNCT_SRL:
			EX_MEM.alu_res = b >> ID_EX.shamt;
			break;

		case FUNCT_SYSCALL:
			LOG("SYSCALL Caught");
			//	g_finished = true;
			break;
		default:
			ERROR("Unknown funct: 0x%x", ID_EX.funct);
			break;
		}
	}

	/* I-Type */
	if(ID_EX.c_alu_op == 0x03) {
		switch(GET_OPCODE(ID_EX.inst)) {
		case OPCODE_ADDI:
			EX_MEM.alu_res = (int32_t)a + (int32_t)b;
			break;
		case OPCODE_ADDIU:
			EX_MEM.alu_res = a + b;
			break;

		case OPCODE_SLTI:
			EX_MEM.alu_res = (int32_t)a < (int32_t) b ? 1 : 0;
			break;

		case OPCODE_SLTIU:
			EX_MEM.alu_res = (uint32_t)a < (uint32_t) b ? 1 : 0;

			break;

		case OPCODE_ANDI:
			EX_MEM.alu_res = a & b;
			break;

		case OPCODE_ORI:
			EX_MEM.alu_res = a | b;
			break;

		case OPCODE_LUI:
			EX_MEM.alu_res = (uint32_t)b << 16;
			break;
		}
	}
}

void interpret_ex(core_t *core)
{
	/* Pipe to next pipeline registers */
	/* MEM */
	EX_MEM.c_mem_read	= ID_EX.c_mem_read;
	EX_MEM.c_mem_write	= ID_EX.c_mem_write;
	EX_MEM.rt_value		= ID_EX.rt_value;
	/* WB */
	EX_MEM.c_reg_write	= ID_EX.c_reg_write;
	EX_MEM.c_mem_to_reg	= ID_EX.c_mem_to_reg;


	EX_MEM.inst		= ID_EX.inst;

	/* On J and JR */
	if(ID_EX.c_jump == 1) {
		if(GET_OPCODE(ID_EX.inst) == OPCODE_R
		   && GET_FUNCT(ID_EX.inst) == FUNCT_JR) {
			ID_EX.jump_addr = ID_EX.rs_value;
		}

		DEBUG("JUMPING TO: %08x", ID_EX.jump_addr);
		PC = ID_EX.jump_addr;
	}
	/* On BEQ */
	if(ID_EX.c_beq == 1) {
		if(ID_EX.rs_value == ID_EX.rt_value) {
			/* Calculate branch address */
			PC = ID_EX.next_pc + (ID_EX.sign_ext_imm << 2);
			DEBUG("BRANCHING TO: %08x", PC);
		}
	}
	/* On BNE */
	if(ID_EX.c_bne == 1) {
		if(ID_EX.rs_value != ID_EX.rt_value) {
			/* Calculate branch address */
			PC = ID_EX.next_pc + (ID_EX.sign_ext_imm << 2);
			DEBUG("BRANCHING TO: %08x", PC);
		}
	}

	/* MUX for RegDST */
	EX_MEM.reg_dst = ID_EX.c_reg_dst == 0 ? ID_EX.rt : ID_EX.rd;

	/* ALU */
	interpret_ex_alu(core);
}


void interpret_mem(core_t *core, memory_t *mem)
{
	MEM_WB.c_reg_write	= EX_MEM.c_reg_write;
	MEM_WB.reg_dst		= EX_MEM.reg_dst;
	MEM_WB.c_mem_to_reg	= EX_MEM.c_mem_to_reg;
	MEM_WB.alu_res		= EX_MEM.alu_res;

	MEM_WB.inst		= EX_MEM.inst;

	/* If read */
	if(EX_MEM.c_mem_read) {
		DEBUG("READING DATA AT: 0x%08x", EX_MEM.alu_res);

		switch(GET_OPCODE(MEM_WB.inst)) {
		case OPCODE_LW:
			MEM_WB.read_data = GET_BIGWORD(mem->raw, EX_MEM.alu_res);
			break;
		case OPCODE_LBU:
			MEM_WB.read_data = GET_BIGBYTE(mem->raw, EX_MEM.alu_res);

			break;
		case OPCODE_LHU:
			MEM_WB.read_data = GET_BIGHALF(mem->raw, EX_MEM.alu_res);
			break;
		}

		LOG("alu_res = %d\nread_data = %d", EX_MEM.alu_res, MEM_WB.read_data);

	/* If write */
	} else if(EX_MEM.c_mem_write) {
		DEBUG("writing %d to addr: 0x%08x", EX_MEM.rt_value,
		      EX_MEM.alu_res);
		switch(GET_OPCODE(MEM_WB.inst)) {
		case OPCODE_SW:
			SET_BIGWORD(mem->raw, EX_MEM.alu_res, EX_MEM.rt_value);
			break;
		case OPCODE_SB:
			SET_BIGBYTE(mem->raw, EX_MEM.alu_res, EX_MEM.rt_value);
			break;
		case OPCODE_SH:
			SET_BIGHALF(mem->raw, EX_MEM.alu_res, EX_MEM.rt_value);
			break;
		}

		LOG("alu_res = %d\nrt_value = %d", EX_MEM.alu_res,
		    EX_MEM.rt_value);
	}
}

void interpret_wb(core_t *core)
{
	if(GET_OPCODE(MEM_WB.inst) == 0 &&
	   GET_FUNCT(MEM_WB.inst) == FUNCT_SYSCALL) {
		g_finished = true;
	}


	/* mem_to_reg MUX */
	uint32_t data = MEM_WB.c_mem_to_reg ? MEM_WB.read_data : MEM_WB.alu_res;

	/* Write back */
	if(MEM_WB.c_reg_write && MEM_WB.reg_dst != 0) {
		DEBUG("Writing %d to destionation register: %d", data, MEM_WB.reg_dst);
		core->regs[MEM_WB.reg_dst] = data;
	}
}

void forwarding_unit(core_t *core)
{
	/* Forward to A MUX */
	/* MEM */
	if(EX_MEM.c_reg_write == 1
	   && EX_MEM.reg_dst != 0
	   && EX_MEM.reg_dst == ID_EX.rs) {
		ID_EX.rs_value = EX_MEM.alu_res;
		DEBUG("Forwarding from MEM MUX A");

	}

	/* Forward to B MUX */
	/* MEM */
	if(EX_MEM.c_reg_write == 1
	   && EX_MEM.reg_dst != 0
	   && EX_MEM.reg_dst == ID_EX.rt) {
		ID_EX.rt_value = EX_MEM.alu_res;
		DEBUG("Forwarding from MEM to MUX B");

	}

	/* WB */
	if(MEM_WB.c_reg_write == 1
	   && MEM_WB.reg_dst != 0
	   && !(EX_MEM.c_reg_write == 1
		&& (EX_MEM.reg_dst != 0)
		&& (EX_MEM.reg_dst == ID_EX.rs))
	   && MEM_WB.reg_dst == ID_EX.rs) {
		/* WB MUX */
		ID_EX.rs_value = MEM_WB.c_mem_to_reg ?
			MEM_WB.read_data : MEM_WB.alu_res;
		DEBUG("Forwarding from WB to MUX A");
	}

	/* WB */
	if(MEM_WB.c_reg_write == 1
	   && MEM_WB.reg_dst != 0
	   && !(EX_MEM.c_reg_write == 1
		&& (EX_MEM.reg_dst != 0)
		&& (EX_MEM.reg_dst == ID_EX.rt))
	   && MEM_WB.reg_dst == ID_EX.rt) {
		/* WB MUX */
		ID_EX.rt_value = MEM_WB.c_mem_to_reg ?
			MEM_WB.read_data : MEM_WB.alu_res;
		DEBUG("Forwarding from WB to MUX A");
	}
}

/* Simulates a clock-tick */
void tick(hardware_t *hw)
{
	LOG("tick ... ");

	cpu_t* cpu = hw->cpu;
	memory_t* mem = hw->mem;

	/* Iterate over each core */
	int i;
	for(i = 0; i < cpu->num_cores; i++) {
		interpret_wb(cpu->core+i);
		interpret_mem(cpu->core+i, mem);
		interpret_ex(cpu->core+i);
		interpret_id(cpu->core+i);
		interpret_if(cpu->core+i, mem);

		forwarding_unit(cpu->core+i);
	}
}

int run(hardware_t *hw)
{
	while(g_finished == false) {
		/* XXX: Assumes one core */
		if(g_debugging) {
			debug(GET_BIGWORD(hw->mem->raw, hw->cpu->core[0].regs[REG_PC]),
			      &hw->cpu->core[0]);
		}
		tick(hw);
	}

	DEBUG("RETURNED: %d", hw->cpu->core[0].regs[REG_V0]);

	/* XXX */
	return hw->cpu->core[0].regs[REG_V0];
}

int simulate(char *program, bool debug)
{
	/* Set debugging */
	g_debugging = debug;

	/* Hardware to simulate */
	hardware_t hardware;

	/* Initialize the memory */
	hardware.mem = mem_init(MEMSZ);

	/* Create a new CPU */
	hardware.cpu = cpu_init(1);

	/* Set stack pointer to top of memory */
	hardware.cpu->core[0].regs[REG_SP] = MIPS_RESERVE + MEMSZ - 4;

	/* Load the program into memory */
	if(elf_dump(program, &(hardware.cpu->core[0].regs[REG_PC]),
		    hardware.mem->raw, MEMSZ) != 0) {
		ERROR("Elf file could not be read.");
		exit(0);
	}

	int ret = run(&hardware);

	cpu_free(hardware.cpu);
	mem_free(hardware.mem);

	return ret;
}
