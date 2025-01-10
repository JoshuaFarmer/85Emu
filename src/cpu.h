#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<stdbool.h>
#include<unistd.h>
#include<termios.h>

#ifdef __JGUI
#include<SDL2/SDL.h>
#endif

#define RESET_VECTOR 0xFFFF
#define RAM_SIZE	   0x10000

#pragma pack(1)

#define X(x)  *((uint16_t*)&x)
#define XF(x) *((uint8_t*)&x)

#define FILE_NAME "ports.dat"

typedef struct {
	uint8_t NUL1:1,S:1,Z:1,K:1,AC:1,NUL:1,P:1,CY:1;
} Flags_t;

struct {
	uint8_t A;
	Flags_t F;
} psw;

typedef struct {
	uint8_t H,L;
} _16Bit;

_16Bit BC, DE, HL, SP;

uint16_t PC;
uint8_t  ram[RAM_SIZE];

bool running = true;

bool interrupts = false;
bool interrupt_mask[8];
bool redraw = false;

FILE* LOG_FILE;

const char* msgs[] = {
	"ok\n",
	"invalid org\n",
	"file not found or cannot be opened\n",
	"file too large\n",
};

inline void reset()
{
	PC = RAM_SIZE - 1;
}

int get_raw_scancode() {
	struct termios oldt, newt;
	int ch;
	
	// Get the current terminal settings
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	
	// Disable canonical mode and echo
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	
	// Read a single character from standard input
	ch = getchar();
	
	// Restore the old terminal settings
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	
	return ch;
}

// Function to read a byte from a specific port
int read_port(int port_number) {
	FILE *file = fopen(FILE_NAME, "rb");
	if (!file) {
		perror("Failed to open ports.dat");
		return 0;
	}

	fseek(file, port_number, SEEK_SET);
	int byte = fgetc(file);
	if (byte == EOF) {
	}

	fclose(file);
	return byte;
}

// Function to write a byte to a specific port
void write_port(int port_number, unsigned char value) {
	FILE *file = fopen(FILE_NAME, "rb+");
	if (!file) {
		perror("Failed to open ports.dat");
		return;
	}

	fseek(file, port_number, SEEK_SET);
	if (fputc(value, file) == EOF) {
		perror("Failed to write to ports.dat");
	}

	fclose(file);
}

const char* INST_NAME[] = {
	"NOP", "LXI_BC_DATA", "STAX_B", "INX_B", "INR_B", "DCR_B", "MVI_B", "RLC",
	"DSUB", "DAD_B", "LDAX_B", "DCX_B", "INR_C", "DCR_C", "MVI_C", "RRC",

	"ARHL", "LXI_DE_DATA", "STAX_D", "INX_D", "INR_D", "DCR_D", "MVI_D", "RAL",
	"RDEL", "DAD_D", "LDAX_D", "DCX_D", "INR_E", "DCR_E", "MVI_E", "RAR",

	"RIM", "LXI_HL_DATA", "SHLD_A16", "INX_H", "INR_H", "DCR_H", "MVI_H", "DAA",
	"LDHI_D8", "DAD_H", "LHLD_A16", "DCX_H", "INR_L", "DCR_L", "MVI_L", "CMA",

	"SIM", "LXI_SP_DATA", "STA_A16", "INX_SP", "INR_M", "DCR_M", "MVI_M", "STC",
	"LDSI_D8", "DAD_SP", "LDA_A16", "DCX_SP", "INR_A", "DCR_A", "MVI_A", "CMC",

	"MOV_B_B", "MOV_B_C", "MOV_B_D", "MOV_B_E", "MOV_B_H", "MOV_B_L", "MOV_B_M", "MOV_B_A",
	"MOV_C_B", "MOV_C_C", "MOV_C_D", "MOV_C_E", "MOV_C_H", "MOV_C_L", "MOV_C_M", "MOV_C_A",

	"MOV_D_B", "MOV_D_C", "MOV_D_D", "MOV_D_E", "MOV_D_H", "MOV_D_L", "MOV_D_M", "MOV_D_A",
	"MOV_E_B", "MOV_E_C", "MOV_E_D", "MOV_E_E", "MOV_E_H", "MOV_E_L", "MOV_E_M", "MOV_E_A",

	"MOV_H_B", "MOV_H_C", "MOV_H_D", "MOV_H_E", "MOV_H_H", "MOV_H_L", "MOV_H_M", "MOV_H_A",
	"MOV_L_B", "MOV_L_C", "MOV_L_D", "MOV_L_E", "MOV_L_H", "MOV_L_L", "MOV_L_M", "MOV_L_A",

	"MOV_M_B", "MOV_M_C", "MOV_M_D", "MOV_M_E", "MOV_M_H", "MOV_M_L", "HLT", "MOV_M_A",
	"MOV_A_B", "MOV_A_C", "MOV_A_D", "MOV_A_E", "MOV_A_H", "MOV_A_L", "MOV_A_M", "MOV_A_A",

	"ADD_B", "ADD_C", "ADD_D", "ADD_E", "ADD_H", "ADD_L", "ADD_M", "ADD_A",
	"ADC_B", "ADC_C", "ADC_D", "ADC_E", "ADC_H", "ADC_L", "ADC_M", "ADC_A",

	"SUB_B", "SUB_C", "SUB_D", "SUB_E", "SUB_H", "SUB_L", "SUB_M", "SUB_A",
	"SBB_B", "SBB_C", "SBB_D", "SBB_E", "SBB_H", "SBB_L", "SBB_M", "SBB_A",

	"ANA_B", "ANA_C", "ANA_D", "ANA_E", "ANA_H", "ANA_L", "ANA_M", "ANA_A",
	"XRA_B", "XRA_C", "XRA_D", "XRA_E", "XRA_H", "XRA_L", "XRA_M", "XRA_A",

	"ORA_B", "ORA_C", "ORA_D", "ORA_E", "ORA_H", "ORA_L", "ORA_M", "ORA_A",
	"CMP_B", "CMP_C", "CMP_D", "CMP_E", "CMP_H", "CMP_L", "CMP_M", "CMP_A",

	"RNZ", "POP_B", "JNZ_A16", "JMP_A16", "CNZ_A16", "PUSH_B", "ADI_D8", "RST_0",
	"RZ", "RET", "JZ_A16", "RSTV", "CZ_A16", "CALL_A16", "ACI_D8", "RST_1",

	"RNC", "POP_D", "JNC_A16", "OUT_D8", "CNC_A16", "PUSH_D", "SUI_D8", "RST_2",
	"RC", "SHLX", "JC_A16", "IN_D8", "CC_A16", "JNK_A16", "SBI_D8", "RST_3",

	"RPO", "POP_H", "JPO_A16", "XTHL", "CPO_A16", "PUSH_H", "ANI_D8", "RST_4",
	"RPE", "PCHL", "JPE_A16", "XCHG", "CPE_A16", "LHLX", "XRI_D8", "RST_5",

	"RP", "POP_PSW", "JP_A16", "DI", "CP_A16", "PUSH_PSW", "ORI_D8", "RST_6",
	"RM", "SPHL", "JM_A16", "EI", "CM_A16", "JK_A16", "CPI_D8", "RST_7"
};

typedef enum {
	// 0x0
	NOP, LXI_BC_DATA, STAX_B, INX_B, INR_B, DCR_B, MVI_B, RLC,
	DSUB, DAD_B, LDAX_B, DCX_B, INR_C, DCR_C, MVI_C, RRC,
	
	// 0x1
	ARHL, LXI_DE_DATA, STAX_D, INX_D, INR_D, DCR_D, MVI_D, RAL,
	RDEL, DAD_D, LDAX_D, DCX_D, INR_E, DCR_E, MVI_E, RAR,

	// 0x2
	RIM, LXI_HL_DATA, SHLD_A16, INX_H, INR_H, DCR_H, MVI_H, DAA,
	LDHI_D8, DAD_H, LHLD_A16, DCX_H, INR_L, DCR_L, MVI_L, CMA,

	// 0x3
	SIM, LXI_SP_DATA, STA_A16, INX_SP, INR_M, DCR_M, MVI_M, STC,
	LDSI_D8, DAD_SP, LDA_A16, DCX_SP, INR_A, DCR_A, MVI_A, CMC,

	// 0x4
	MOV_B_B, MOV_B_C, MOV_B_D, MOV_B_E, MOV_B_H, MOV_B_L, MOV_B_M, MOV_B_A,
	MOV_C_B, MOV_C_C, MOV_C_D, MOV_C_E, MOV_C_H, MOV_C_L, MOV_C_M, MOV_C_A,

	// 0x5
	MOV_D_B, MOV_D_C, MOV_D_D, MOV_D_E, MOV_D_H, MOV_D_L, MOV_D_M, MOV_D_A,
	MOV_E_B, MOV_E_C, MOV_E_D, MOV_E_E, MOV_E_H, MOV_E_L, MOV_E_M, MOV_E_A,

	// 0x6
	MOV_H_B, MOV_H_C, MOV_H_D, MOV_H_E, MOV_H_H, MOV_H_L, MOV_H_M, MOV_H_A,
	MOV_L_B, MOV_L_C, MOV_L_D, MOV_L_E, MOV_L_H, MOV_L_L, MOV_L_M, MOV_L_A,

	// 0x7
	MOV_M_B, MOV_M_C, MOV_M_D, MOV_M_E, MOV_M_H, MOV_M_L, HLT, MOV_M_A,
	MOV_A_B, MOV_A_C, MOV_A_D, MOV_A_E, MOV_A_H, MOV_A_L, MOV_A_M, MOV_A_A,

	// 0x8
	ADD_B, ADD_C, ADD_D, ADD_E, ADD_H, ADD_L, ADD_M, ADD_A,
	ADC_B, ADC_C, ADC_D, ADC_E, ADC_H, ADC_L, ADC_M, ADC_A,

	// 0x9
	SUB_B, SUB_C, SUB_D, SUB_E, SUB_H, SUB_L, SUB_M, SUB_A,
	SBB_B, SBB_C, SBB_D, SBB_E, SBB_H, SBB_L, SBB_M, SBB_A,

	// 0xA
	ANA_B, ANA_C, ANA_D, ANA_E, ANA_H, ANA_L, ANA_M, ANA_A,
	XRA_B, XRA_C, XRA_D, XRA_E, XRA_H, XRA_L, XRA_M, XRA_A,

	// 0xB
	ORA_B, ORA_C, ORA_D, ORA_E, ORA_H, ORA_L, ORA_M, ORA_A,
	CMP_B, CMP_C, CMP_D, CMP_E, CMP_H, CMP_L, CMP_M, CMP_A,

	// 0xC
	RNZ, POP_B, JNZ_A16, JMP_A16, CNZ_A16, PUSH_B, ADI_D8, RST_0,
	RZ, RET, JZ_A16, RSTV, CZ_A16, CALL_A16, ACI_D8, RST_1,

	// 0xD
	RNC, POP_D, JNC_A16, OUT_D8, CNC_A16, PUSH_D, SUI_D8, RST_2,
	RC, SHLX, JC_A16, IN_D8, CC_A16, JNK_A16, SBI_D8, RST_3,

	// 0xE
	RPO, POP_H, JPO_A16, XTHL, CPO_A16, PUSH_H, ANI_D8, RST_4,
	RPE, PCHL, JPE_A16, XCHG, CPE_A16, LHLX, XRI_D8, RST_5,

	// 0xF
	RP, POP_PSW, JP_A16, DI, CP_A16, PUSH_PSW, ORI_D8, RST_6,
	RM, SPHL, JM_A16, EI, CM_A16, JK_A16, CPI_D8, RST_7
} Inst_t;

int fetch(uint8_t* ram, uint16_t addr)
{
	return ram[addr % (RAM_SIZE)];
}

void ram_write(uint8_t* ram, uint16_t addr, uint8_t value)
{
	ram[addr % (RAM_SIZE)] = value;
}

void Return() {
	PC  = fetch(ram, X(SP) - 2);
	PC |= fetch(ram, X(SP) - 1) << 8;
	X(SP) = X(SP) + 2;
}

void Call() {
	_16Bit addr;
	addr.L = fetch(ram, PC++);
	addr.H = fetch(ram, PC++);
	
	X(SP) = X(SP) - 2;
	ram_write(ram, X(SP) + 1,  PC>>8);
	ram_write(ram, X(SP), PC);

	PC = X(addr);
}

void JumpI() {
	_16Bit addr;
	addr.L = fetch(ram, PC++);
	addr.H = fetch(ram, PC++);
	PC = X(addr);
}

// Helper Functions
void Reset(int x) {
	X(SP) = X(SP) - 2;
	ram_write(ram, X(SP) + 1,  PC>>8);
	ram_write(ram, X(SP), PC);
	PC = x * 8; // 0, 8, etc
}

void In() {
	int port = fetch(ram, PC++);
	switch (port) {
		case 0x0:
			break;
		case 0x1:
			psw.A = get_raw_scancode();
			break;
		case 0x2:
			break;
		default:
			psw.A = read_port(port);
			break;
	}
}

void Out() {
	int port = fetch(ram, PC++);
	switch (port) {
		case 0x0:
			fprintf(LOG_FILE, "\'%c\' or 0x%x", psw.A, psw.A);
			break;
		case 0x1:
			printf("%c", psw.A);
			break;
		case 0x2:
			redraw = true;
			printf("redraw\n");
			break;
		default:
			write_port(port, psw.A);
			break;
	}
}

void Flags(int a, int b) {
	psw.F.Z = (a == 0);
	psw.F.CY = (a < b);
	psw.F.S = (a & 0x80) != 0;
}

// little endian means the little byte is FIRST in memory (a bit weird if u think about the name.)

void execute(uint8_t X)
{
	switch(X)
	{
		case NOP: 
			break;
		
		// Part 1, Basic Data
		case MVI_A: psw.A = fetch(ram, PC++); break;
		case MVI_B: BC.H = fetch(ram, PC++); break;
		case MVI_C: BC.L = fetch(ram, PC++); break;
		case MVI_D: DE.H = fetch(ram, PC++); break;
		case MVI_E: DE.L = fetch(ram, PC++); break;
		case MVI_H: HL.H = fetch(ram, PC++); break;
		case MVI_L: HL.L = fetch(ram, PC++); break;
		case MVI_M: ram_write(ram, X(HL), fetch(ram, PC++)); break;

		case LXI_BC_DATA: BC.L = fetch(ram, PC++); BC.H = fetch(ram, PC++); break;
		case STAX_B: ram_write(ram, X(BC), psw.A); break;

		case MOV_B_B: BC.H = BC.H; break;
		case MOV_B_C: BC.H = BC.L; break;
		case MOV_B_D: BC.H = DE.H; break;
		case MOV_B_E: BC.H = DE.L; break;
		case MOV_B_H: BC.H = HL.H; break;
		case MOV_B_L: BC.H = HL.L; break;
		case MOV_B_M: BC.H = fetch(ram, X(HL)); break;
		case MOV_B_A: BC.H = psw.A; break;

		case MOV_C_B: BC.L = BC.H; break;
		case MOV_C_C: BC.L = BC.L; break;
		case MOV_C_D: BC.L = DE.H; break;
		case MOV_C_E: BC.L = DE.L; break;
		case MOV_C_H: BC.L = HL.H; break;
		case MOV_C_L: BC.L = HL.L; break;
		case MOV_C_M: BC.L = fetch(ram, X(HL)); break;
		case MOV_C_A: BC.L = psw.A; break;

		case MOV_D_B: DE.H = BC.H; break;
		case MOV_D_C: DE.H = BC.L; break;
		case MOV_D_D: DE.H = DE.H; break;
		case MOV_D_E: DE.H = DE.L; break;
		case MOV_D_H: DE.H = HL.H; break;
		case MOV_D_L: DE.H = HL.L; break;
		case MOV_D_M: DE.H = fetch(ram, X(HL)); break;
		case MOV_D_A: DE.H = psw.A; break;

		case MOV_E_B: DE.L = BC.H; break;
		case MOV_E_C: DE.L = BC.L; break;
		case MOV_E_D: DE.L = DE.H; break;
		case MOV_E_E: DE.L = DE.L; break;
		case MOV_E_H: DE.L = HL.H; break;
		case MOV_E_L: DE.L = HL.L; break;
		case MOV_E_M: DE.L = fetch(ram, X(HL)); break;
		case MOV_E_A: DE.L = psw.A; break;

		case MOV_H_B: HL.H = BC.H; break;
		case MOV_H_C: HL.H = BC.L; break;
		case MOV_H_D: HL.H = DE.H; break;
		case MOV_H_E: HL.H = DE.L; break;
		case MOV_H_H: HL.H = HL.H; break;
		case MOV_H_L: HL.H = HL.L; break;
		case MOV_H_M: HL.H = fetch(ram, X(HL)); break;
		case MOV_H_A: HL.H = psw.A; break;

		case MOV_L_B: HL.L = BC.H; break;
		case MOV_L_C: HL.L = BC.L; break;
		case MOV_L_D: HL.L = DE.H; break;
		case MOV_L_E: HL.L = DE.L; break;
		case MOV_L_H: HL.L = HL.H; break;
		case MOV_L_L: HL.L = HL.L; break;
		case MOV_L_M: HL.L = fetch(ram, X(HL)); break;
		case MOV_L_A: HL.L = psw.A; break;

		case MOV_M_B: ram_write(ram, X(HL), BC.H); break;
		case MOV_M_C: ram_write(ram, X(HL), BC.L); break;
		case MOV_M_D: ram_write(ram, X(HL), DE.H); break;
		case MOV_M_E: ram_write(ram, X(HL), DE.L); break;
		case MOV_M_H: ram_write(ram, X(HL), HL.H); break;
		case MOV_M_L: ram_write(ram, X(HL), HL.L); break;

		case MOV_M_A: ram_write(ram, X(HL), psw.A); break;

		case MOV_A_B: psw.A = BC.H; break;
		case MOV_A_C: psw.A = BC.L; break;
		case MOV_A_D: psw.A = DE.H; break;
		case MOV_A_E: psw.A = DE.L; break;
		case MOV_A_H: psw.A = HL.H; break;
		case MOV_A_L: psw.A = HL.L; break;
		case MOV_A_M: psw.A = fetch(ram, X(HL)); break;
		case MOV_A_A: psw.A = psw.A; break;

		// Part 2, Basic Arithmetic
		case ADD_B: psw.A += BC.H; Flags(psw.A, BC.H); break;
		case ADD_C: psw.A += BC.L; Flags(psw.A, BC.L); break;
		case ADD_D: psw.A += DE.H; Flags(psw.A, DE.H); break;
		case ADD_E: psw.A += DE.L; Flags(psw.A, DE.L); break;
		case ADD_H: psw.A += HL.H; Flags(psw.A, HL.H); break;
		case ADD_L: psw.A += HL.L; Flags(psw.A, HL.L); break;
		case ADI_D8: psw.A += fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;
		case ADD_M: { uint8_t value = fetch(ram, X(HL)); psw.A += value; Flags(psw.A, value); } break;
		case ADD_A: psw.A += psw.A; psw.F.Z = (psw.A == 0); psw.F.CY = 1; psw.F.S = (psw.A & 0x80) != 0; break;

		case ADC_A: psw.A += psw.A + psw.F.CY; Flags(psw.A, psw.A + psw.F.CY); break;
		case ADC_B: psw.A += BC.H + psw.F.CY; Flags(psw.A, BC.H + psw.F.CY); break;
		case ADC_C: psw.A += BC.L + psw.F.CY; Flags(psw.A, BC.L + psw.F.CY); break;
		case ADC_D: psw.A += DE.H + psw.F.CY; Flags(psw.A, DE.H + psw.F.CY); break;
		case ADC_E: psw.A += DE.L + psw.F.CY; Flags(psw.A, DE.L + psw.F.CY); break;
		case ADC_H: psw.A += HL.H + psw.F.CY; Flags(psw.A, HL.H + psw.F.CY); break;
		case ADC_L: psw.A += HL.L + psw.F.CY; Flags(psw.A, HL.L + psw.F.CY); break;
		case ADC_M: { uint8_t value = fetch(ram, X(HL)); psw.A += value + psw.F.CY; Flags(psw.A, value + psw.F.CY); } break;
		case ACI_D8: psw.A += fetch(ram, PC) + psw.F.CY; Flags(psw.A, fetch(ram, PC++) + psw.F.CY); break;

		case SUB_A: psw.A = 0; Flags(psw.A, psw.A); break;
		case SUB_B: psw.A -= BC.H; Flags(psw.A, BC.H); break;
		case SUB_C: psw.A -= BC.L; Flags(psw.A, BC.L); break;
		case SUB_D: psw.A -= DE.H; Flags(psw.A, DE.H); break;
		case SUB_E: psw.A -= DE.L; Flags(psw.A, DE.L); break;
		case SUB_H: psw.A -= HL.H; Flags(psw.A, HL.H); break;
		case SUB_L: psw.A -= HL.L; Flags(psw.A, HL.L); break;
		case SUB_M: psw.A -= fetch(ram, X(HL)); Flags(psw.A, fetch(ram, X(HL))); break;
		case SUI_D8: psw.A -= fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;

		case SBB_A: { uint8_t borrow = psw.A + psw.F.CY; psw.F.Z = (borrow == psw.A); psw.F.S = 0; psw.F.CY = 0; psw.A = 0; } break;
		case SBB_B: { uint8_t borrow = BC.H + psw.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_C: { uint8_t borrow = BC.L + psw.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_D: { uint8_t borrow = DE.H + psw.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_E: { uint8_t borrow = DE.L + psw.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_H: { uint8_t borrow = HL.H + psw.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_L: { uint8_t borrow = HL.L + psw.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_M: { uint8_t borrow = fetch(ram, X(HL)) + psw.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBI_D8: { uint8_t borrow = fetch(ram, PC++) + psw.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;

		// Part 3, Logical Operations
		case ANA_A: break;
		case ANA_B: psw.A &= BC.H; Flags(psw.A, BC.H); break;
		case ANA_C: psw.A &= BC.L; Flags(psw.A, BC.H); break;
		case ANA_D: psw.A &= DE.H; Flags(psw.A, DE.H); break;
		case ANA_E: psw.A &= DE.L; Flags(psw.A, DE.H); break;
		case ANA_H: psw.A &= HL.H; Flags(psw.A, HL.H); break;
		case ANA_L: psw.A &= HL.L; Flags(psw.A, HL.H); break;
		case ANA_M: psw.A &= fetch(ram, X(HL)); Flags(psw.A, fetch(ram, X(HL))); break;
		case ANI_D8: psw.A &= fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;

		case XRA_A: psw.A = 0; Flags(psw.A, psw.A); break;
		case XRA_B: psw.A ^= BC.H; Flags(psw.A, BC.H); break;
		case XRA_C: psw.A ^= BC.L; Flags(psw.A, BC.L); break;
		case XRA_D: psw.A ^= DE.H; Flags(psw.A, DE.H); break;
		case XRA_E: psw.A ^= DE.L; Flags(psw.A, DE.L); break;
		case XRA_H: psw.A ^= HL.H; Flags(psw.A, HL.H); break;
		case XRA_L: psw.A ^= HL.L; Flags(psw.A, HL.L); break;
		case XRA_M: psw.A ^= fetch(ram, X(HL)); Flags(psw.A, fetch(ram, X(HL))); break;
		case XRI_D8: psw.A ^= fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;

		case ORA_A: psw.A = 0; Flags(psw.A, psw.A); break;
		case ORA_B: psw.A |= BC.H; Flags(psw.A, BC.H); break;
		case ORA_C: psw.A |= BC.L; Flags(psw.A, BC.L); break;
		case ORA_D: psw.A |= DE.H; Flags(psw.A, DE.H); break;
		case ORA_E: psw.A |= DE.L; Flags(psw.A, DE.L); break;
		case ORA_H: psw.A |= HL.H; Flags(psw.A, HL.H); break;
		case ORA_L: psw.A |= HL.L; Flags(psw.A, HL.L); break;
		case ORA_M: psw.A |= fetch(ram, X(HL)); Flags(psw.A, fetch(ram, X(HL))); break;
		case ORI_D8: psw.A |= fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;

		case CMP_A: { psw.F.Z = 1; psw.F.S = 0; psw.F.CY = 0; } break;
		case CMP_B: { int tmp = psw.A; tmp -= BC.H; Flags(tmp, BC.H); } break;
		case CMP_C: { int tmp = psw.A; tmp -= BC.L; Flags(tmp, BC.L); } break;
		case CMP_D: { int tmp = psw.A; tmp -= DE.H; Flags(tmp, DE.H); } break;
		case CMP_E: { int tmp = psw.A; tmp -= DE.L; Flags(tmp, DE.L); } break;
		case CMP_H: { int tmp = psw.A; tmp -= HL.H; Flags(tmp, HL.H); } break;
		case CMP_L: { int tmp = psw.A; tmp -= HL.L; Flags(tmp, HL.L); } break;
		case CMP_M: { int tmp = psw.A; tmp -= fetch(ram, X(HL)); Flags(tmp, fetch(ram, X(HL))); } break;
		case CPI_D8: { int tmp = psw.A; tmp -= fetch(ram, PC); Flags(tmp, fetch(ram, PC++)); } break;

		// Part 4, Wide Operations
		case INX_B: X(BC) = X(BC) + 1; break;
		case INX_D: X(DE) = X(DE) + 1; break;
		case INX_H: X(HL) = X(HL) + 1; break;
		case INX_SP: X(SP) = X(SP) + 1; break;

		case DCX_B: X(BC) = X(BC) - 1; break;
		case DCX_D: X(DE) = X(DE) - 1; break;
		case DCX_H: X(HL)= X(HL) - 1; break;
		case DCX_SP: X(SP) = X(SP) - 1; break;

		case LDAX_B: psw.A = fetch(ram, X(BC)); break;
		case LDAX_D: psw.A = fetch(ram, X(DE)); break;
		case STAX_D: ram_write(ram, X(DE), psw.A); break;

		case INR_A:
			psw.A+=1;
			psw.F.Z = (psw.A == 0);  // Zero flag
			psw.F.CY = (psw.A == 0xFF);  // Carry flag
			psw.F.S = (psw.A & 0x80) != 0;  // Sign flag
			break;

		case INR_B:
			BC.H++;
			psw.F.Z = (BC.H == 0);  // Zero flag
			psw.F.CY = (BC.H == 0xFF);  // Carry flag
			psw.F.S = (BC.H & 0x80) != 0;  // Sign flag
			break;

		case DCR_B:
			BC.H--;
			psw.F.Z = (BC.H == 0);  // Zero flag
			psw.F.S = (BC.H & 0x80) != 0;  // Sign flag
			break;
		
		case DCR_A:
			psw.A--;
			psw.F.Z = (psw.A == 0);  // Zero flag
			psw.F.S = (psw.A & 0x80) != 0;  // Sign flag
			break;

		case RLC: 
			{
				uint8_t carry = psw.A & 0x80;
				psw.A = (psw.A << 1) | (carry >> 7);
				psw.F.CY = (carry != 0);
			}
			break;

		case DAD_B:
			{
				uint32_t HL_pair = X(HL);
				uint32_t BC_pair = X(BC);
				uint32_t result = HL_pair + BC_pair;
				X(HL) = result & 0xFFFF;
				psw.F.CY = (result > 0xFFFF);  // Set carry if overflow
			}
			break;

		case INR_C:
			BC.L++;
			psw.F.Z = (BC.L == 0);  // Zero flag
			psw.F.S = (BC.L & 0x80) != 0;  // Sign flag
			break;

		case DCR_C:
			BC.L--;
			psw.F.Z = (BC.L == 0);  // Zero flag
			psw.F.S = (BC.L & 0x80) != 0;  // Sign flag
			break;

		case RRC:
			{
				uint8_t carry = psw.A & 0x01;
				psw.A = (psw.A >> 1) | (carry << 7);
				psw.F.CY = (carry != 0);
			}
			break;

		case LXI_DE_DATA:
			DE.L = fetch(ram, PC++);
			DE.H = fetch(ram, PC++);
			break;


		case INR_D:
			DE.H++;
			psw.F.Z = (DE.H == 0);  // Zero flag
			psw.F.S = (DE.H & 0x80) != 0;  // Sign flag
			break;

		case DCR_D:
			DE.H--;
			psw.F.Z = (DE.H == 0);  // Zero flag
			psw.F.S = (DE.H & 0x80) != 0;  // Sign flag
			break;


		case RAL: 
			{
				uint8_t carry = psw.F.CY;
				psw.F.CY = (psw.A & 0x80) != 0;
				psw.A = (psw.A << 1) | carry;
			}
			break;

		case DAD_D:
			{
				uint32_t HL_pair = X(HL);
				uint32_t DE_pair = X(DE);
				uint32_t result = HL_pair + DE_pair;
				X(HL) = result & 0xFFFF;
				psw.F.CY = (result > 0xFFFF);  // Set carry if overflow
			}
			break;

		case DAD_SP:
			{
				uint32_t HL_pair = X(HL);
				uint32_t SP_pair = X(SP);
				uint32_t result = HL_pair + SP_pair;
				X(HL) = result & 0xFFFF;
				psw.F.CY = (result > 0xFFFF);  // Set carry if overflow
			}
			break;

		case DAD_H:
			{
				uint32_t HL_pair = X(HL);
				uint32_t result = HL_pair + HL_pair;
				X(HL) = result & 0xFFFF;
				psw.F.CY = (result > 0xFFFF);  // Set carry if overflow
			}
			break;


		case INR_E:
			DE.L++;
			psw.F.Z = (DE.L == 0);  // Zero flag
			psw.F.S = (DE.L & 0x80) != 0;  // Sign flag
			break;

		case DCR_E:
			DE.L--;
			psw.F.Z = (DE.L == 0);  // Zero flag
			psw.F.S = (DE.L & 0x80) != 0;  // Sign flag
			break;


		case RAR:
			{
				uint8_t carry = psw.F.CY;
				psw.F.CY = (psw.A & 0x01) != 0;
				psw.A = (psw.A >> 1) | (carry << 7);
			}
			break;

		case LXI_HL_DATA:
			HL.L = fetch(ram, PC++);
			HL.H = fetch(ram, PC++);
			break;

		case SHLD_A16:
			{
				uint16_t addr = fetch(ram, PC++) << 8;
				addr |= fetch(ram, PC++);
				ram_write(ram, addr, HL.L);
				ram_write(ram, addr + 1, HL.H);
			}
			break;


		case INR_H:
			HL.H++;
			psw.F.Z = (HL.H == 0);  // Zero flag
			psw.F.S = (HL.H & 0x80) != 0;  // Sign flag
			break;

		case DCR_H:
			HL.H--;
			psw.F.Z = (HL.H == 0);  // Zero flag
			psw.F.S = (HL.H & 0x80) != 0;  // Sign flag
			break;


		case DAA: 
			// Adjust A for BCD operations
			break;

		case LHLD_A16:
			{
				uint16_t addr = fetch(ram, PC++) << 8;
				addr |= fetch(ram, PC++);
				HL.L = fetch(ram, addr);
				HL.H = fetch(ram, addr + 1);
			}
			break;


		case INR_L:
			HL.L++;
			psw.F.Z = (HL.L == 0);  // Zero flag
			psw.F.S = (HL.L & 0x80) != 0;  // Sign flag
			break;

		case DCR_L:
			HL.L--;
			psw.F.Z = (HL.L == 0);  // Zero flag
			psw.F.S = (HL.L & 0x80) != 0;  // Sign flag
			break;


		case CMA: psw.A = ~psw.A; break;
		case LXI_SP_DATA: SP.L = fetch(ram, PC++); SP.H = fetch(ram, PC++); break;

		case STA_A16:
			{
				uint16_t addr = fetch(ram, PC++) << 8;
				addr |= fetch(ram, PC++);
				ram_write(ram, addr, psw.A);
			}
			break;


		case INR_M:
			{
				uint16_t addr = X(HL);
				uint8_t value = fetch(ram, addr);
				value++;
				ram_write(ram, addr, value);
				psw.F.Z = (value == 0);
				psw.F.S = (value & 0x80) != 0;
			}
			break;

		case DCR_M:
			{
				uint16_t addr = X(HL);
				uint8_t value = fetch(ram, addr);
				value--;
				ram_write(ram, addr, value);
				psw.F.Z = (value == 0);
				psw.F.S = (value & 0x80) != 0;
			}
			break;

		case STC: psw.F.CY = 1; break;

		case LDA_A16:
			{
				uint16_t addr = fetch(ram, PC++) << 8;
				addr |= fetch(ram, PC++);
				psw.A = fetch(ram, addr);
			}
			break;

		case HLT:
			fprintf(LOG_FILE, "CPU halted @%.4x\n", PC);
			running = false;
			break;

		case CMC:
			psw.F.CY = ~psw.F.CY;
			break;

		case PUSH_B:
			X(SP) = X(SP) - 2;
			ram_write(ram, X(SP) + 1, BC.H);
			ram_write(ram, X(SP), BC.L);
			break;

		case PUSH_D:
			X(SP) = X(SP) - 2;
			ram_write(ram, X(SP) + 1, DE.H);
			ram_write(ram, X(SP), DE.L);
			break;
		
		case PUSH_H:
			X(SP) = X(SP) - 2;
			ram_write(ram, X(SP) + 1, HL.H);
			ram_write(ram, X(SP), HL.L);
			break;

		case PUSH_PSW:
			X(SP) = X(SP) - 2;
			ram_write(ram, X(SP) + 1, psw.A);
			ram_write(ram, X(SP), XF(psw.F));
			break;

		case POP_B:
			BC.L = fetch(ram, X(SP) - 2);
			BC.H = fetch(ram, X(SP) - 1);
			X(SP) = X(SP) + 2;
			break;

		case POP_D:
			DE.L = fetch(ram, X(SP) - 2);
			DE.H = fetch(ram, X(SP) - 1);
			X(SP) = X(SP) + 2;
			break;

		case POP_H:
			HL.L = fetch(ram, X(SP) - 2);
			HL.H = fetch(ram, X(SP) - 1);
			X(SP) = X(SP) + 2;
			break;

		case POP_PSW:
			XF(psw.F) = fetch(ram, X(SP) - 2);
			psw.A = fetch(ram, X(SP) - 1);
			X(SP) = X(SP) + 2;
			break;

		case RZ:
			if (psw.F.Z) {
				Return();
			}
			break;
		case RNZ:
			if (!psw.F.Z) {
				Return();
			}
			break;
		case RC:
			if (psw.F.CY) {
				Return();
			}
			break;
		case RNC:
			if (!psw.F.CY) {
				Return();
			}
			break;
		case RPE:
			if (psw.F.P) {
				Return();
			}
			break;
		case RPO:
			if (!psw.F.P) {
				Return();
			}
			break;
		case RM:
			if (psw.F.S) {
				Return();
			}
			break;
		case RP:
			if (!psw.F.S) {
				Return();
			}
			break;
		case RET:
			Return();
			break;

		case JZ_A16:
			if (psw.F.Z)
				JumpI();
			else PC += 2;
			break;
		case JNZ_A16:
			if (!psw.F.Z)
				JumpI();
			else PC += 2;
			break;
		case JC_A16:
			if (psw.F.CY)
				JumpI();
			else PC += 2;
			break;
		case JNC_A16:
			if (!psw.F.CY)
				JumpI();
			else PC += 2;
			break;
		case JPE_A16:
			if (psw.F.P)
				JumpI();
			else PC += 2;
			break;
		case JPO_A16:
			if (!psw.F.P)
				JumpI();
			else PC += 2;
			break;
		case JM_A16:
			if (psw.F.S)
				JumpI();
			else PC += 2;
			break;
		case JP_A16:
			if (!psw.F.S)
				JumpI();
			else PC += 2;
			break;
		case JMP_A16:
			JumpI();
			break;
		case CZ_A16:
			if (psw.F.Z) {
				Call();
			} else PC += 2;
			break;
		case CNZ_A16:
			if (!psw.F.Z) {
				Call();
			} else PC += 2;
			break;
		case CC_A16:
			if (psw.F.CY) {
				Call();
			} else PC += 2;
			break;
		case CNC_A16:
			if (!psw.F.CY) {
				Call();
			} else PC += 2;
			break;
		case CPE_A16:
			if (psw.F.P) {
				Call();
			} else PC += 2;
			break;
		case CPO_A16:
			if (!psw.F.P) {
				Call();
			} else PC += 2;
			break;
		case CM_A16:
			if (psw.F.S) {
				Call();
			} else PC += 2;
			break;
		case CP_A16:	
			if (!psw.F.S) {
				Call();
			} else PC += 2;
			break;
		case CALL_A16:
			Call();
			break;
		case RST_0:
			Reset(0);
			break;
		case RST_1:
			Reset(1);
			break;
		case RST_2:
			Reset(2);
			break;
		case RST_3:
			Reset(3);
			break;
		case RST_4:
			Reset(4);
			break;
		case RST_5:
			Reset(5);
			break;
		case RST_6:
			Reset(6);
			break;
		case RST_7:
			Reset(7);
			break;

		case XTHL:
			{
				int tmp0 = HL.L;
				int tmp1 = HL.H;
				HL.L = fetch(ram, X(SP));
				HL.H = fetch(ram, X(SP)+1);
				ram_write(ram, X(SP), tmp0);
				ram_write(ram, X(SP)+1, tmp1);
			}
			break;
	
		case PCHL:
			PC = X(HL);
			break;
		
		case XCHG:
			{
				int x = X(DE);
				X(DE) = X(HL);
				X(HL) = x;
			}
			break;
		
		case SPHL:
			X(SP) = X(HL);
			break;

		case EI:
			interrupts = true;
			break;
		case DI:
			interrupts = false;
			break;
		
		case OUT_D8:
			Out();
			break;

		case IN_D8:
			In();
			break;

		case RIM:
			psw.A = 0;
			for (int i = 0; i < 8; ++i) {
				psw.A |= (interrupt_mask[i] << i);
			}
			break;
		
		case SIM:
			for (int i = 0; i < 8; ++i)
				interrupt_mask[i] = (psw.A >> i) & i;
			break;

		// undocumented/not bothering/illegal
		case DSUB:
		case ARHL:
		case RDEL:
		case LDHI_D8:
		case LDSI_D8:
		case RSTV:
		case SHLX:
		case JNK_A16:
		case LHLX:
		case JK_A16:
			break;
		default:
			fprintf(LOG_FILE, "Unknown instruction %.2X\n", X);
			running = false;
			break;
	}
}

int bytes_read = 0;
int load_file(const char* path, int org) {
	if (org < 0 || org >= RAM_SIZE) {
		return 1;  // Error: Origin out of bounds
	}
	
	FILE* fp = fopen(path, "rb");
	if (fp == NULL) {
		return 2;  // Error: File not found or cannot be opened
	}
	
	// Move to the specified origin in memory
	fseek(fp, 0, SEEK_END);
	size_t file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	if (file_size + org > RAM_SIZE) {
		fclose(fp);
		return 3;  // Error: File too large for memory at specified origin
	}
	
	size_t read_size = fread(ram + org, 1, file_size, fp);
	bytes_read = read_size;
	fclose(fp);
	
	return 0;  // Success
}

void check_what_we_have()
{
	running = true;
	for (int i = 0; i < 256; ++i) {
		execute(i);
	}
}
