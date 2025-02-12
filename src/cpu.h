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

#define FILE_NAME "ports.dat"

typedef union {
	struct {
		uint8_t NUL1:1,S:1,Z:1,K:1,AC:1,NUL:1,P:1,CY:1;
	} F;
	uint8_t x;
} Flags_t;

struct {
	uint8_t A;
	Flags_t F;
} psw;

typedef union {
	struct {
		uint8_t H,L;
	} bytes;
	uint16_t X;
} _16Bit;

_16Bit BC={.X=0}, DE={.X=0}, HL={.X=0}, SP={.X=0};

uint16_t PC=0;
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

inline void Push16(uint16_t x)
{
	ram_write(ram, SP.X--, x&255);
	ram_write(ram, SP.X--, x>>8);
}

inline uint16_t Pop16()
{
	return (fetch(ram, SP.X++)<<8)|fetch(ram, SP.X++);
}

inline _16Bit Fe16imm()
{
	_16Bit x;
	x.bytes.L = fetch(ram, PC++);
	x.bytes.H = fetch(ram, PC++);
	return x;
}

void Return()
{
	PC = Pop16();
}

void Call()
{
	_16Bit addr = Fe16imm();
	Push16(PC);
	PC = addr.X;
}

void JumpI() {
	_16Bit addr;
	addr.bytes.L = fetch(ram, PC++);
	addr.bytes.H = fetch(ram, PC++);
	PC = addr.X;
}

void Reset(int x) {
	SP.X = SP.X - 2;
	ram_write(ram, SP.X + 1,  PC>>8);
	ram_write(ram, SP.X, PC);
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
	psw.F.F.Z = (a == 0);
	psw.F.F.CY = (a < b);
	psw.F.F.S = (a & 0x80) != 0;
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
		case MVI_B: BC.bytes.H = fetch(ram, PC++); break;
		case MVI_C: BC.bytes.L = fetch(ram, PC++); break;
		case MVI_D: DE.bytes.H = fetch(ram, PC++); break;
		case MVI_E: DE.bytes.L = fetch(ram, PC++); break;
		case MVI_H: HL.bytes.H = fetch(ram, PC++); break;
		case MVI_L: HL.bytes.L = fetch(ram, PC++); break;
		case MVI_M: ram_write(ram, HL.X, fetch(ram, PC++)); break;

		case LXI_BC_DATA: BC.bytes.L = fetch(ram, PC++); BC.bytes.H = fetch(ram, PC++); break;
		case STAX_B: ram_write(ram, BC.X, psw.A); break;

		case MOV_B_B: BC.bytes.H = BC.bytes.H; break;
		case MOV_B_C: BC.bytes.H = BC.bytes.L; break;
		case MOV_B_D: BC.bytes.H = DE.bytes.H; break;
		case MOV_B_E: BC.bytes.H = DE.bytes.L; break;
		case MOV_B_H: BC.bytes.H = HL.bytes.H; break;
		case MOV_B_L: BC.bytes.H = HL.bytes.L; break;
		case MOV_B_M: BC.bytes.H = fetch(ram, HL.X); break;
		case MOV_B_A: BC.bytes.H = psw.A; break;

		case MOV_C_B: BC.bytes.L = BC.bytes.H; break;
		case MOV_C_C: BC.bytes.L = BC.bytes.L; break;
		case MOV_C_D: BC.bytes.L = DE.bytes.H; break;
		case MOV_C_E: BC.bytes.L = DE.bytes.L; break;
		case MOV_C_H: BC.bytes.L = HL.bytes.H; break;
		case MOV_C_L: BC.bytes.L = HL.bytes.L; break;
		case MOV_C_M: BC.bytes.L = fetch(ram, HL.X); break;
		case MOV_C_A: BC.bytes.L = psw.A; break;

		case MOV_D_B: DE.bytes.H = BC.bytes.H; break;
		case MOV_D_C: DE.bytes.H = BC.bytes.L; break;
		case MOV_D_D: DE.bytes.H = DE.bytes.H; break;
		case MOV_D_E: DE.bytes.H = DE.bytes.L; break;
		case MOV_D_H: DE.bytes.H = HL.bytes.H; break;
		case MOV_D_L: DE.bytes.H = HL.bytes.L; break;
		case MOV_D_M: DE.bytes.H = fetch(ram, HL.X); break;
		case MOV_D_A: DE.bytes.H = psw.A; break;

		case MOV_E_B: DE.bytes.L = BC.bytes.H; break;
		case MOV_E_C: DE.bytes.L = BC.bytes.L; break;
		case MOV_E_D: DE.bytes.L = DE.bytes.H; break;
		case MOV_E_E: DE.bytes.L = DE.bytes.L; break;
		case MOV_E_H: DE.bytes.L = HL.bytes.H; break;
		case MOV_E_L: DE.bytes.L = HL.bytes.L; break;
		case MOV_E_M: DE.bytes.L = fetch(ram, HL.X); break;
		case MOV_E_A: DE.bytes.L = psw.A; break;

		case MOV_H_B: HL.bytes.H = BC.bytes.H; break;
		case MOV_H_C: HL.bytes.H = BC.bytes.L; break;
		case MOV_H_D: HL.bytes.H = DE.bytes.H; break;
		case MOV_H_E: HL.bytes.H = DE.bytes.L; break;
		case MOV_H_H: HL.bytes.H = HL.bytes.H; break;
		case MOV_H_L: HL.bytes.H = HL.bytes.L; break;
		case MOV_H_M: HL.bytes.H = fetch(ram, HL.X); break;
		case MOV_H_A: HL.bytes.H = psw.A; break;

		case MOV_L_B: HL.bytes.L = BC.bytes.H; break;
		case MOV_L_C: HL.bytes.L = BC.bytes.L; break;
		case MOV_L_D: HL.bytes.L = DE.bytes.H; break;
		case MOV_L_E: HL.bytes.L = DE.bytes.L; break;
		case MOV_L_H: HL.bytes.L = HL.bytes.H; break;
		case MOV_L_L: HL.bytes.L = HL.bytes.L; break;
		case MOV_L_M: HL.bytes.L = fetch(ram, HL.X); break;
		case MOV_L_A: HL.bytes.L = psw.A; break;

		case MOV_M_B: ram_write(ram, HL.X, BC.bytes.H); break;
		case MOV_M_C: ram_write(ram, HL.X, BC.bytes.L); break;
		case MOV_M_D: ram_write(ram, HL.X, DE.bytes.H); break;
		case MOV_M_E: ram_write(ram, HL.X, DE.bytes.L); break;
		case MOV_M_H: ram_write(ram, HL.X, HL.bytes.H); break;
		case MOV_M_L: ram_write(ram, HL.X, HL.bytes.L); break;

		case MOV_M_A: ram_write(ram, HL.X, psw.A); break;

		case MOV_A_B: psw.A = BC.bytes.H; break;
		case MOV_A_C: psw.A = BC.bytes.L; break;
		case MOV_A_D: psw.A = DE.bytes.H; break;
		case MOV_A_E: psw.A = DE.bytes.L; break;
		case MOV_A_H: psw.A = HL.bytes.H; break;
		case MOV_A_L: psw.A = HL.bytes.L; break;
		case MOV_A_M: psw.A = fetch(ram, HL.X); break;
		case MOV_A_A: psw.A = psw.A; break;

		// Part 2, Basic Arithmetic
		case ADD_B: psw.A += BC.bytes.H; Flags(psw.A, BC.bytes.H); break;
		case ADD_C: psw.A += BC.bytes.L; Flags(psw.A, BC.bytes.L); break;
		case ADD_D: psw.A += DE.bytes.H; Flags(psw.A, DE.bytes.H); break;
		case ADD_E: psw.A += DE.bytes.L; Flags(psw.A, DE.bytes.L); break;
		case ADD_H: psw.A += HL.bytes.H; Flags(psw.A, HL.bytes.H); break;
		case ADD_L: psw.A += HL.bytes.L; Flags(psw.A, HL.bytes.L); break;
		case ADI_D8: psw.A += fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;
		case ADD_M: { uint8_t value = fetch(ram, HL.X); psw.A += value; Flags(psw.A, value); } break;
		case ADD_A: psw.A += psw.A; psw.F.F.Z = (psw.A == 0); psw.F.F.CY = 1; psw.F.F.S = (psw.A & 0x80) != 0; break;

		case ADC_A: psw.A += psw.A + psw.F.F.CY; Flags(psw.A, psw.A + psw.F.F.CY); break;
		case ADC_B: psw.A += BC.bytes.H + psw.F.F.CY; Flags(psw.A, BC.bytes.H + psw.F.F.CY); break;
		case ADC_C: psw.A += BC.bytes.L + psw.F.F.CY; Flags(psw.A, BC.bytes.L + psw.F.F.CY); break;
		case ADC_D: psw.A += DE.bytes.H + psw.F.F.CY; Flags(psw.A, DE.bytes.H + psw.F.F.CY); break;
		case ADC_E: psw.A += DE.bytes.L + psw.F.F.CY; Flags(psw.A, DE.bytes.L + psw.F.F.CY); break;
		case ADC_H: psw.A += HL.bytes.H + psw.F.F.CY; Flags(psw.A, HL.bytes.H + psw.F.F.CY); break;
		case ADC_L: psw.A += HL.bytes.L + psw.F.F.CY; Flags(psw.A, HL.bytes.L + psw.F.F.CY); break;
		case ADC_M: { uint8_t value = fetch(ram, HL.X); psw.A += value + psw.F.F.CY; Flags(psw.A, value + psw.F.F.CY); } break;
		case ACI_D8: psw.A += fetch(ram, PC) + psw.F.F.CY; Flags(psw.A, fetch(ram, PC++) + psw.F.F.CY); break;

		case SUB_A: psw.A = 0; Flags(psw.A, psw.A); break;
		case SUB_B: psw.A -= BC.bytes.H; Flags(psw.A, BC.bytes.H); break;
		case SUB_C: psw.A -= BC.bytes.L; Flags(psw.A, BC.bytes.L); break;
		case SUB_D: psw.A -= DE.bytes.H; Flags(psw.A, DE.bytes.H); break;
		case SUB_E: psw.A -= DE.bytes.L; Flags(psw.A, DE.bytes.L); break;
		case SUB_H: psw.A -= HL.bytes.H; Flags(psw.A, HL.bytes.H); break;
		case SUB_L: psw.A -= HL.bytes.L; Flags(psw.A, HL.bytes.L); break;
		case SUB_M: psw.A -= fetch(ram, HL.X); Flags(psw.A, fetch(ram, HL.X)); break;
		case SUI_D8: psw.A -= fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;

		case SBB_A: { uint8_t borrow = psw.A + psw.F.F.CY; psw.F.F.Z = (borrow == psw.A); psw.F.F.S = 0; psw.F.F.CY = 0; psw.A = 0; } break;
		case SBB_B: { uint8_t borrow = BC.bytes.H + psw.F.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_C: { uint8_t borrow = BC.bytes.L + psw.F.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_D: { uint8_t borrow = DE.bytes.H + psw.F.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_E: { uint8_t borrow = DE.bytes.L + psw.F.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_H: { uint8_t borrow = HL.bytes.H + psw.F.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_L: { uint8_t borrow = HL.bytes.L + psw.F.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBB_M: { uint8_t borrow = fetch(ram, HL.X) + psw.F.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;
		case SBI_D8: { uint8_t borrow = fetch(ram, PC++) + psw.F.F.CY; psw.A -= borrow; Flags(psw.A, borrow); } break;

		// Part 3, Logical Operations
		case ANA_A: break;
		case ANA_B: psw.A &= BC.bytes.H; Flags(psw.A, BC.bytes.H); break;
		case ANA_C: psw.A &= BC.bytes.L; Flags(psw.A, BC.bytes.H); break;
		case ANA_D: psw.A &= DE.bytes.H; Flags(psw.A, DE.bytes.H); break;
		case ANA_E: psw.A &= DE.bytes.L; Flags(psw.A, DE.bytes.H); break;
		case ANA_H: psw.A &= HL.bytes.H; Flags(psw.A, HL.bytes.H); break;
		case ANA_L: psw.A &= HL.bytes.L; Flags(psw.A, HL.bytes.H); break;
		case ANA_M: psw.A &= fetch(ram, HL.X); Flags(psw.A, fetch(ram, HL.X)); break;
		case ANI_D8: psw.A &= fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;

		case XRA_A: psw.A = 0; Flags(psw.A, psw.A); break;
		case XRA_B: psw.A ^= BC.bytes.H; Flags(psw.A, BC.bytes.H); break;
		case XRA_C: psw.A ^= BC.bytes.L; Flags(psw.A, BC.bytes.L); break;
		case XRA_D: psw.A ^= DE.bytes.H; Flags(psw.A, DE.bytes.H); break;
		case XRA_E: psw.A ^= DE.bytes.L; Flags(psw.A, DE.bytes.L); break;
		case XRA_H: psw.A ^= HL.bytes.H; Flags(psw.A, HL.bytes.H); break;
		case XRA_L: psw.A ^= HL.bytes.L; Flags(psw.A, HL.bytes.L); break;
		case XRA_M: psw.A ^= fetch(ram, HL.X); Flags(psw.A, fetch(ram, HL.X)); break;
		case XRI_D8: psw.A ^= fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;

		case ORA_A: psw.A = 0; Flags(psw.A, psw.A); break;
		case ORA_B: psw.A |= BC.bytes.H; Flags(psw.A, BC.bytes.H); break;
		case ORA_C: psw.A |= BC.bytes.L; Flags(psw.A, BC.bytes.L); break;
		case ORA_D: psw.A |= DE.bytes.H; Flags(psw.A, DE.bytes.H); break;
		case ORA_E: psw.A |= DE.bytes.L; Flags(psw.A, DE.bytes.L); break;
		case ORA_H: psw.A |= HL.bytes.H; Flags(psw.A, HL.bytes.H); break;
		case ORA_L: psw.A |= HL.bytes.L; Flags(psw.A, HL.bytes.L); break;
		case ORA_M: psw.A |= fetch(ram, HL.X); Flags(psw.A, fetch(ram, HL.X)); break;
		case ORI_D8: psw.A |= fetch(ram, PC); Flags(psw.A, fetch(ram, PC++)); break;

		case CMP_A: { psw.F.F.Z = 1; psw.F.F.S = 0; psw.F.F.CY = 0; } break;
		case CMP_B: { int tmp = psw.A; tmp -= BC.bytes.H; Flags(tmp, BC.bytes.H); } break;
		case CMP_C: { int tmp = psw.A; tmp -= BC.bytes.L; Flags(tmp, BC.bytes.L); } break;
		case CMP_D: { int tmp = psw.A; tmp -= DE.bytes.H; Flags(tmp, DE.bytes.H); } break;
		case CMP_E: { int tmp = psw.A; tmp -= DE.bytes.L; Flags(tmp, DE.bytes.L); } break;
		case CMP_H: { int tmp = psw.A; tmp -= HL.bytes.H; Flags(tmp, HL.bytes.H); } break;
		case CMP_L: { int tmp = psw.A; tmp -= HL.bytes.L; Flags(tmp, HL.bytes.L); } break;
		case CMP_M: { int tmp = psw.A; tmp -= fetch(ram, HL.X); Flags(tmp, fetch(ram, HL.X)); } break;
		case CPI_D8: { int tmp = psw.A; tmp -= fetch(ram, PC); Flags(tmp, fetch(ram, PC++)); } break;

		// Part 4, Wide Operations
		case INX_B: BC.X = BC.X + 1; break;
		case INX_D: DE.X = DE.X + 1; break;
		case INX_H: HL.X = HL.X + 1; break;
		case INX_SP: SP.X = SP.X + 1; break;

		case DCX_B: BC.X = BC.X - 1; break;
		case DCX_D: DE.X = DE.X - 1; break;
		case DCX_H: HL.X= HL.X - 1; break;
		case DCX_SP: SP.X = SP.X - 1; break;

		case LDAX_B: psw.A = fetch(ram, BC.X); break;
		case LDAX_D: psw.A = fetch(ram, DE.X); break;
		case STAX_D: ram_write(ram, DE.X, psw.A); break;

		case INR_A:
			psw.A+=1;
			psw.F.F.Z = (psw.A == 0);  // Zero flag
			psw.F.F.CY = (psw.A == 0xFF);  // Carry flag
			psw.F.F.S = (psw.A & 0x80) != 0;  // Sign flag
			break;

		case INR_B:
			BC.bytes.H++;
			psw.F.F.Z = (BC.bytes.H == 0);  // Zero flag
			psw.F.F.CY = (BC.bytes.H == 0xFF);  // Carry flag
			psw.F.F.S = (BC.bytes.H & 0x80) != 0;  // Sign flag
			break;

		case DCR_B:
			BC.bytes.H--;
			psw.F.F.Z = (BC.bytes.H == 0);  // Zero flag
			psw.F.F.S = (BC.bytes.H & 0x80) != 0;  // Sign flag
			break;
		
		case DCR_A:
			psw.A--;
			psw.F.F.Z = (psw.A == 0);  // Zero flag
			psw.F.F.S = (psw.A & 0x80) != 0;  // Sign flag
			break;

		case RLC: 
			{
				uint8_t carry = psw.A & 0x80;
				psw.A = (psw.A << 1) | (carry >> 7);
				psw.F.F.CY = (carry != 0);
			}
			break;

		case DAD_B:
			{
				uint32_t HL_pair = HL.X;
				uint32_t BC_pair = BC.X;
				uint32_t result = HL_pair + BC_pair;
				HL.X = result & 0xFFFF;
				psw.F.F.CY = (result > 0xFFFF);  // Set carry if overflow
			}
			break;

		case INR_C:
			BC.bytes.L++;
			psw.F.F.Z = (BC.bytes.L == 0);  // Zero flag
			psw.F.F.S = (BC.bytes.L & 0x80) != 0;  // Sign flag
			break;

		case DCR_C:
			BC.bytes.L--;
			psw.F.F.Z = (BC.bytes.L == 0);  // Zero flag
			psw.F.F.S = (BC.bytes.L & 0x80) != 0;  // Sign flag
			break;

		case RRC:
			{
				uint8_t carry = psw.A & 0x01;
				psw.A = (psw.A >> 1) | (carry << 7);
				psw.F.F.CY = (carry != 0);
			}
			break;

		case LXI_DE_DATA:
			DE.bytes.L = fetch(ram, PC++);
			DE.bytes.H = fetch(ram, PC++);
			break;


		case INR_D:
			DE.bytes.H++;
			psw.F.F.Z = (DE.bytes.H == 0);  // Zero flag
			psw.F.F.S = (DE.bytes.H & 0x80) != 0;  // Sign flag
			break;

		case DCR_D:
			DE.bytes.H--;
			psw.F.F.Z = (DE.bytes.H == 0);  // Zero flag
			psw.F.F.S = (DE.bytes.H & 0x80) != 0;  // Sign flag
			break;


		case RAL: 
			{
				uint8_t carry = psw.F.F.CY;
				psw.F.F.CY = (psw.A & 0x80) != 0;
				psw.A = (psw.A << 1) | carry;
			}
			break;

		case DAD_D:
			{
				uint32_t HL_pair = HL.X;
				uint32_t DE_pair = DE.X;
				uint32_t result = HL_pair + DE_pair;
				HL.X = result & 0xFFFF;
				psw.F.F.CY = (result > 0xFFFF);  // Set carry if overflow
			}
			break;

		case DAD_SP:
			{
				uint32_t HL_pair = HL.X;
				uint32_t SP_pair = SP.X;
				uint32_t result = HL_pair + SP_pair;
				HL.X = result & 0xFFFF;
				psw.F.F.CY = (result > 0xFFFF);  // Set carry if overflow
			}
			break;

		case DAD_H:
			{
				uint32_t HL_pair = HL.X;
				uint32_t result = HL_pair + HL_pair;
				HL.X = result & 0xFFFF;
				psw.F.F.CY = (result > 0xFFFF);  // Set carry if overflow
			}
			break;


		case INR_E:
			DE.bytes.L++;
			psw.F.F.Z = (DE.bytes.L == 0);  // Zero flag
			psw.F.F.S = (DE.bytes.L & 0x80) != 0;  // Sign flag
			break;

		case DCR_E:
			DE.bytes.L--;
			psw.F.F.Z = (DE.bytes.L == 0);  // Zero flag
			psw.F.F.S = (DE.bytes.L & 0x80) != 0;  // Sign flag
			break;


		case RAR:
			{
				uint8_t carry = psw.F.F.CY;
				psw.F.F.CY = (psw.A & 0x01) != 0;
				psw.A = (psw.A >> 1) | (carry << 7);
			}
			break;

		case LXI_HL_DATA:
			HL.bytes.L = fetch(ram, PC++);
			HL.bytes.H = fetch(ram, PC++);
			break;

		case SHLD_A16:
			{
				uint16_t addr = fetch(ram, PC++) << 8;
				addr |= fetch(ram, PC++);
				ram_write(ram, addr, HL.bytes.L);
				ram_write(ram, addr + 1, HL.bytes.H);
			}
			break;


		case INR_H:
			HL.bytes.H++;
			psw.F.F.Z = (HL.bytes.H == 0);  // Zero flag
			psw.F.F.S = (HL.bytes.H & 0x80) != 0;  // Sign flag
			break;

		case DCR_H:
			HL.bytes.H--;
			psw.F.F.Z = (HL.bytes.H == 0);  // Zero flag
			psw.F.F.S = (HL.bytes.H & 0x80) != 0;  // Sign flag
			break;


		case DAA: 
			// Adjust A for BCD operations
			break;

		case LHLD_A16:
			{
				uint16_t addr = fetch(ram, PC++) << 8;
				addr |= fetch(ram, PC++);
				HL.bytes.L = fetch(ram, addr);
				HL.bytes.H = fetch(ram, addr + 1);
			}
			break;


		case INR_L:
			HL.bytes.L++;
			psw.F.F.Z = (HL.bytes.L == 0);  // Zero flag
			psw.F.F.S = (HL.bytes.L & 0x80) != 0;  // Sign flag
			break;

		case DCR_L:
			HL.bytes.L--;
			psw.F.F.Z = (HL.bytes.L == 0);  // Zero flag
			psw.F.F.S = (HL.bytes.L & 0x80) != 0;  // Sign flag
			break;


		case CMA: psw.A = ~psw.A; break;
		case LXI_SP_DATA: SP.bytes.L = fetch(ram, PC++); SP.bytes.H = fetch(ram, PC++); break;

		case STA_A16:
			{
				uint16_t addr = fetch(ram, PC++) << 8;
				addr |= fetch(ram, PC++);
				ram_write(ram, addr, psw.A);
			}
			break;


		case INR_M:
			{
				uint16_t addr = HL.X;
				uint8_t value = fetch(ram, addr);
				value++;
				ram_write(ram, addr, value);
				psw.F.F.Z = (value == 0);
				psw.F.F.S = (value & 0x80) != 0;
			}
			break;

		case DCR_M:
			{
				uint16_t addr = HL.X;
				uint8_t value = fetch(ram, addr);
				value--;
				ram_write(ram, addr, value);
				psw.F.F.Z = (value == 0);
				psw.F.F.S = (value & 0x80) != 0;
			}
			break;

		case STC: psw.F.F.CY = 1; break;

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
			psw.F.F.CY = ~psw.F.F.CY;
			break;

		case PUSH_B:
			SP.X = SP.X - 2;
			ram_write(ram, SP.X + 1, BC.bytes.H);
			ram_write(ram, SP.X, BC.bytes.L);
			break;

		case PUSH_D:
			SP.X = SP.X - 2;
			ram_write(ram, SP.X + 1, DE.bytes.H);
			ram_write(ram, SP.X, DE.bytes.L);
			break;
		
		case PUSH_H:
			SP.X = SP.X - 2;
			ram_write(ram, SP.X + 1, HL.bytes.H);
			ram_write(ram, SP.X, HL.bytes.L);
			break;

		case PUSH_PSW:
			SP.X = SP.X - 2;
			ram_write(ram, SP.X + 1, psw.A);
			ram_write(ram, SP.X, psw.F.x);
			break;

		case POP_B:
			{
				BC.X = Pop16();
			}
			break;

		case POP_D:
			{
				DE.X = Pop16();
			}
			break;

		case POP_H:
			{
				HL.X = Pop16();
			}
			break;

		case POP_PSW:
			{
				uint16_t x = Pop16();
				psw.A   = x&255;
				psw.F.x = x >> 8;
			}
			break;

		case RZ:
			if (psw.F.F.Z) {
				Return();
			}
			break;
		case RNZ:
			if (!psw.F.F.Z) {
				Return();
			}
			break;
		case RC:
			if (psw.F.F.CY) {
				Return();
			}
			break;
		case RNC:
			if (!psw.F.F.CY) {
				Return();
			}
			break;
		case RPE:
			if (psw.F.F.P) {
				Return();
			}
			break;
		case RPO:
			if (!psw.F.F.P) {
				Return();
			}
			break;
		case RM:
			if (psw.F.F.S) {
				Return();
			}
			break;
		case RP:
			if (!psw.F.F.S) {
				Return();
			}
			break;
		case RET:
			Return();
			break;

		case JZ_A16:
			if (psw.F.F.Z)
				JumpI();
			else PC += 2;
			break;
		case JNZ_A16:
			if (!psw.F.F.Z)
				JumpI();
			else PC += 2;
			break;
		case JC_A16:
			if (psw.F.F.CY)
				JumpI();
			else PC += 2;
			break;
		case JNC_A16:
			if (!psw.F.F.CY)
				JumpI();
			else PC += 2;
			break;
		case JPE_A16:
			if (psw.F.F.P)
				JumpI();
			else PC += 2;
			break;
		case JPO_A16:
			if (!psw.F.F.P)
				JumpI();
			else PC += 2;
			break;
		case JM_A16:
			if (psw.F.F.S)
				JumpI();
			else PC += 2;
			break;
		case JP_A16:
			if (!psw.F.F.S)
				JumpI();
			else PC += 2;
			break;
		case JMP_A16:
			JumpI();
			break;
		case CZ_A16:
			if (psw.F.F.Z) {
				Call();
			} else PC += 2;
			break;
		case CNZ_A16:
			if (!psw.F.F.Z) {
				Call();
			} else PC += 2;
			break;
		case CC_A16:
			if (psw.F.F.CY) {
				Call();
			} else PC += 2;
			break;
		case CNC_A16:
			if (!psw.F.F.CY) {
				Call();
			} else PC += 2;
			break;
		case CPE_A16:
			if (psw.F.F.P) {
				Call();
			} else PC += 2;
			break;
		case CPO_A16:
			if (!psw.F.F.P) {
				Call();
			} else PC += 2;
			break;
		case CM_A16:
			if (psw.F.F.S) {
				Call();
			} else PC += 2;
			break;
		case CP_A16:	
			if (!psw.F.F.S) {
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
				int tmp0 = HL.bytes.L;
				int tmp1 = HL.bytes.H;
				HL.bytes.L = fetch(ram, SP.X);
				HL.bytes.H = fetch(ram, SP.X+1);
				ram_write(ram, SP.X, tmp0);
				ram_write(ram, SP.X+1, tmp1);
			}
			break;
	
		case PCHL:
			PC = HL.X;
			break;
		
		case XCHG:
			{
				int x = DE.X;
				DE.X = HL.X;
				HL.X = x;
			}
			break;
		
		case SPHL:
			SP.X = HL.X;
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
