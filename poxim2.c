#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define mem_size 8*1024
//Definicao dos registradores especiais 
#define CR R[26] // registrador de causa de interrupção
#define IPC R[27] // registrador de endereço de memória da instrução que causou a interrupção
#define IR R[28] //registrador de instrucao (codigo da instrucao)
#define PC R[29] //contador de programa (endereco da instrucao)
#define SP R[30] //ponteiro de pilha (endereco da pilha)
#define SR R[31] //registrador de status

// Definicao dos registradores de status individualmente
#define CY (R[31] & 0x1) // Carry
#define IE ((R[31] >> 1) & 0x1) // Controle de interrupção 
#define IV ((R[31] >> 2) & 0x1) // Instrução inválida
#define OV ((R[31] >> 3) & 0x1) // Overflow
#define SN ((R[31] >> 4) & 0x1) // Sinal
#define ZD ((R[31] >> 5) & 0x1) // Divisao por zero
#define ZN ((R[31] >> 6) & 0x1) // Zero

/* Definição para setar ou limpar os status dos campos
do registrador especial SR. */
#define SET_CY (R[31] |= 0b1)
#define CLEAR_CY (R[31] &= ~(0b1))
#define SET_IV (R[31] |= (0b1 << 2))
#define CLEAR_IV (R[31] &= ~(0b1 << 2))
#define SET_OV (R[31] |= (0b1 << 3))
#define CLEAR_OV (R[31] &= ~(0b1 << 3))
#define SET_SN (R[31] |= (0b1 << 4))
#define CLEAR_SN (R[31] &= ~(0b1 << 4))
#define SET_ZD (R[31] |= (0b1 << 5))
#define CLEAR_ZD (R[31] &= ~(0b1 << 5))
#define SET_ZN (R[31] |= (0b1 << 6))
#define CLEAR_ZN (R[31] &= ~(0b1 << 6))

/* Definições úteis para melhor legibilidade do código */
#define RX31 (R[x] >> 31)
#define RY31 (R[y] >> 31)
#define RZ31 (R[z] >> 31)
#define CMP31 (cmp >> 31)
#define CMPI31 (cmpi >> 31)
#define I15 ((i >> 15) & 0x1)
#define SP_adress = MEM32[SP]
#define WATCHDOG_ADD 0x80808080
#define WATCHDOUNG_COUNT watchdog_reg & 0x7FFFFFFF
#define WATCHDOG_EN ((watchdog_reg >> 31) & 0b01)
#define SET_WATCHDOG (watchdog_reg |= (0b1 << 31))
#define CLEAR_WATCHDOG (watchdog_reg &= ~(0b1 << 31))
#define FPU_X_ADD 0x80808880
#define FPU_Y_ADD 0x80808884
#define FPU_Z_ADD 0x80808888
#define FPU_ST_ADD 0x8080888C
#define TERMINAL_ADD 0x88888888
#define TERMINAL_IN 0x8888888A
#define TERMINAL_OUT 0x8888888B

typedef struct interrupcao{
	uint8_t interrupcao_id;
	uint8_t prioridade;
	uint32_t endereco_pc;
} Interrupcao;

Interrupcao tabela_interrupcoes[4] = {
	{1, 1, 0x00000010},
	{2, 2, 0x00000014},
	{3, 3, 0x00000018},
	{4, 4, 0x0000001c}
};

uint32_t R[32];
uint32_t *MEM32 = NULL;
uint32_t fpu_x, fpu_y, fpu_z, fpu_st, terminal_reg;

int interrupcao = 0;
int interrupcao_pendente = 0;
int tipo_interrupcao = -1;
int fpu_ciclos = 0, fpu_in = 0;

// Para imprimir os registradores em minúsculo
void print_minus(uint32_t reg, char* buffer) {
    switch (reg) {
        case 31: sprintf(buffer, "sr"); break;
        case 30: sprintf(buffer, "sp"); break;
        case 29: sprintf(buffer, "pc"); break;
        case 28: sprintf(buffer, "ir"); break;
		case 27: sprintf(buffer, "ipc"); break;
		case 26: sprintf(buffer, "cr"); break;
        default: sprintf(buffer, "r%u", reg); break;
    }
}

// Para imprimir os registradores em maiúsculo
void print_maius(uint32_t reg, char* buffer) {
    switch (reg) {
        case 31: sprintf(buffer, "SR"); break;
        case 30: sprintf(buffer, "SP"); break;
        case 29: sprintf(buffer, "PC"); break;
        case 28: sprintf(buffer, "IR"); break;
		case 27: sprintf(buffer, "IPC"); break;
		case 26: sprintf(buffer, "CR"); break;
        default: sprintf(buffer, "R%u", reg); break;
    }
}

void convert_to_uppercase(char* str) {
    while (*str) {
        *str = toupper(*str);
        str++;
    }
}

void salvar_contexto(uint32_t *pc, uint32_t *sp, uint32_t *ipc, uint32_t *cr, uint32_t MEM[]) {
    // Salva o PC na pilha (PC + 4, pois a próxima instrução será após a atual)
    if (*sp < 4) {
        printf("Erro: ponteiro de pilha fora dos limites x.\n");
        return;
    }

    MEM[*sp] = *pc + 4;
    *sp = *sp - 4;

    if (*sp < 4) {
        printf("Erro: ponteiro de pilha fora dos limites a.\n");
        return;
    }

    // Salva o CR na pilha
    MEM[*sp] = *cr;
    *sp = *sp - 4;

    if (*sp < 4) {
        printf("Erro: ponteiro de pilha fora dos limites b.\n");
        return;
    }

    // Salva o IPC na pilha
    MEM[*sp] = *ipc;
    *sp = *sp - 4;
}

void interrupcao_hardware(uint8_t interrupcao, uint32_t *pc, uint32_t *sp, uint32_t *ipc, uint32_t *cr, uint32_t *MEM32, FILE *output) {

	uint8_t max_prioridade = 0;
	uint8_t interrupcao_ativa = 0;
	uint32_t endereco_pc = 0;

	for (int i = 0; i < 4; i++) {
		if (tabela_interrupcoes[i].interrupcao_id == interrupcao) {
			if (tabela_interrupcoes[i].prioridade > max_prioridade) {
				max_prioridade = tabela_interrupcoes[i].prioridade;
				interrupcao_ativa = tabela_interrupcoes[i].interrupcao_id;
				endereco_pc = tabela_interrupcoes[i].endereco_pc;
			}
		}
	}

	if (interrupcao_ativa) {

		uint32_t pc_inicial = *pc;
		salvar_contexto(pc, sp, ipc, cr, MEM32);
		
		
		switch(interrupcao) {
			case 1: {
				*pc = 0x00000010;
				break;
			}
			case 2: {
				*pc = 0x00000014;
				break;
			}
			case 3: {
				*pc = 0x00000018;
				break;
			}
			case 4: {
				*pc = 0x0000001C;
				break;
			}
			default: {
				break;
			}
		}

		*pc = *pc - 4;
		fprintf(output, "[HARDWARE INTERRUPTION %d]\n", interrupcao);
	} else {
		printf("Sem interrupcao ativa\tPC = 0x%08X\n", *pc);
	}
}

void processar_fpu (uint32_t *MEM32) {
	uint32_t op = MEM32[FPU_ST_ADD & 0x1F];
	uint32_t x = MEM32[FPU_X_ADD];
	uint32_t y = MEM32[FPU_Y_ADD];
	uint32_t z;

	if (fpu_in) {
		if (fpu_ciclos > 0) {
			fpu_ciclos--;
			if(fpu_ciclos == 0) {
				fpu_in = 0;
				printf("FPU completa\n");
			} 
			return;
		}
	}

	switch (op) {
	// sem operação
	case 00000: {
		break;
	}
	// adição
	case 00001: {
		z = x + y;
		break;
	}
	// subtração
	case 00010: {
		z = x - y;
		break;
	}
	// multiplicação
	case 00011: {
		z = x * y;
		break;
	}
	// divisão
	case 00100: {
		if (y != 0) {
			z = x / y;
		} else {
			MEM32[FPU_ST_ADD] |= (1 << 31);
			return;
		}
		break;
	}
	// atritbuição
	case 00101: {
		MEM32[FPU_X_ADD] = MEM32[FPU_Z_ADD];
		return;
	}
	// atribuição
	case 00110: {
		MEM32[FPU_Y_ADD] = MEM32[FPU_Z_ADD];
		return;
	}
	// teto
	case 00111: {
		z = ceil(MEM32[FPU_Z_ADD]);
		break;
	}
	// piso
	case 01000: {
		z = floor(MEM32[FPU_Z_ADD]);
		break;
	}
	// arrendondamento
	case 01001: {
		z = roundf(*(float*)&MEM32[FPU_Z_ADD]);
		break;
	}
	default: {
		MEM32[FPU_ST_ADD] |= (1 << 31);
		return;
	}
	}

	MEM32[FPU_Z_ADD] = z;
	MEM32[FPU_ST_ADD] = 0;
}

// main
int main(int argc, char* argv[]) {

	FILE* input = fopen(argv[1], "r");
	FILE* output = fopen(argv[2], "w");

	// Inicializa os registradores com valor inicial 0
	uint32_t R[32] = { 0 };
	// Inicializa a memórias
	uint32_t* MEM32 = (uint32_t*)calloc(8 * 1024, sizeof(uint32_t));
		if (MEM32 == NULL) {
    printf("Erro ao alocar memória");
    return 1;
	}
	uint32_t valor;
	size_t index = 0;
	// Armazena o conteúdo do arquivo dentro da memória
	while (fscanf(input, "%x", &valor) != EOF && index < (8 * 1024 * 1024)) {
		MEM32[index++] = valor;
    }

	fprintf(output, "[START OF SIMULATION]\n");
	PC = 0;
	uint8_t executa = 1;

	uint32_t watchdog_reg;

	// Implementando o terminal
	char buffer[mem_size];
	uint8_t buffer_index = 0;

	while(executa) {

	uint32_t watchdog_en = (watchdog_reg >> 31) & 0b01;
	uint32_t watchdog_c = watchdog_reg & 0x7FFFFFFF;

	if (interrupcao_pendente && IE) {
		interrupcao_hardware(interrupcao_pendente, &PC, &SP, &IPC, &CR, MEM32, output);
		interrupcao_pendente = 0;
	}

		if (watchdog_en == 1) {
			
			if (watchdog_c == 0) {
				watchdog_reg = 0;
				
				if (IE) {
					salvar_contexto(&PC, &SP, &IPC, &CR, MEM32);
					uint32_t PC_atual = PC;

					CR = 1;
					IPC = PC;

					PC = 0x00000010;
				} else {
					interrupcao_pendente = 1;
				}
			}

			watchdog_reg--; 
			//printf("watch: %d\n", watchdog_c);
		}

		uint8_t terminal_out;
		uint8_t terminal_in;	

		// Cadeia de caracteres da instrucao
		char instrucao[1024] = { 0 };
		// Declarando operandos
		uint8_t z = 0, x = 0, y = 0, l = 0, v, w;
		uint32_t xyl = 0;

		// Carregando a instrucao de 32 bits (4 bytes) da memoria indexada pelo PC (R29) no registrador IR (R28)
		R[28] = MEM32[R[29] >> 2];
		// Decodificando a instrucao buscada na memoria
		uint8_t opcode = (R[28] & (0b111111 << 26)) >> 26; 

		// Implementação das instruções definidas na documentação
		
		switch(opcode) {

		// Instruções básicas

			// mov (sem sinal)
			case 0b000000: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				xyl = R[28] & 0x1FFFFF;
				// Execucao do comportamento
				R[z] = xyl;
				// Formatacao da instrucao
				char z_minus[10], z_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				sprintf(instrucao, "mov %s,%u", z_minus, xyl);
				// Formatacao de saida em tela (deve mudar para o arquivo de saida)
				fprintf(output, "0x%08X:\t%-25s\t%s=0x%08X\n", R[29], instrucao, z_maius, xyl);
				break;
			}
			// movs (com extensão de sinal) - Analisar como aplicar a extensão de sinal
			case 0b000001: {
				// Operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				int32_t xyl = R[28] & 0x1FFFFF;
				if (xyl & 0x100000) {
				    // Se estiver definido, faz a extensão de sinal
				    xyl |= 0xFFE00000; 
				}
				// Execucao do comportamento
				xyl = (xyl << 11) >> 11;
				R[z] = xyl;
				// Formatacao da instrucao
				char z_minus[10], z_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				sprintf(instrucao, "movs %s,%d", z_minus, xyl);
				// Formatacao da saida no output
				// 0x00000024:	movs r2,-1048576         	R2=0xFFF00000
				fprintf(output, "0x%08X:\t%-25s\t%s=0x%08X\n", R[29], instrucao, z_maius, R[z]);
				break;
			}
			// add 
			case 0b000010: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;
				// Executando a adicao
				uint64_t rx = R[x];
				uint64_t ry = R[y];
				uint64_t rz = rx + ry;
				R[z] = R[x] + R[y];

				//Condição para setar ZN caso os números sejam iguais
				// Valida se o valor armazenado em R[z] é 0
				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
  				} if (RZ31 == 1) {
					SET_SN;
				} else {
					CLEAR_SN;
				} if ((RX31 == RY31) && (RZ31 != RX31)) {
					SET_OV;
				} else {
					CLEAR_OV;
				}
				if ((rz & (1ULL << 32)) != 0) {
				    SET_CY;
				} else {
				    CLEAR_CY;
				}

				char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);
				print_minus(y, y_minus);
				print_maius(y, y_maius);								
				sprintf(instrucao, "add %s,%s,%s", z_minus, x_minus, y_minus);
				//Modelo: R3=R1+R2=0x00023456,SR=0x00000001
				fprintf(output, "0x%08X:\t%-25s\t%s=%s+%s=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, y_maius, R[z], SR);
				break;
			} 

			// sub
			case 0b000011: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;
				// Executando a subtracao
				uint64_t rx = R[x];
				uint64_t ry = R[y];
				uint64_t rz = rx - ry;				
				R[z] = R[x] - R[y];

				//Condição para setar ZN caso os números sejam iguais
				// Valida se o valor armazenado em R[z] é 0
				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				} if (RZ31 == 1) {
					SET_SN;
				} else {
					CLEAR_SN;
				} if ((RX31 != RY31) && (RZ31 != RX31)) {
					SET_OV;
				} else {
					CLEAR_OV;
				}
				if ((rz & (1ULL << 32)) != 0) {
				    SET_CY;
				} else {
				    CLEAR_CY;
				}
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);
				print_minus(y, y_minus);
				print_maius(y, y_maius);	
				sprintf(instrucao, "sub %s,%s,%s", z_minus, x_minus, y_minus);
				// 0x00000030:	sub r4,r2,r3             	R4=R2-R3=0x7FFDCBAA,SR=0x00000008
				fprintf(output, "0x%08X:\t%-25s\t%s=%s-%s=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, y_maius, R[z],SR); 
				break;
			}

			// cmp 
			case 0b000101: {
				// Obtendo operandos
				x = ((R[28] & (0b11111) << 16) >> 16);
				y = ((R[28] & (0b11111) << 11) >> 11);

				uint32_t cmp = R[x] - R[y];
				

				if (cmp	== 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				} if (CMP31 == 1) {
					SET_SN;
				} else {
					CLEAR_SN;
				} if ((RX31 != RY31) && (CMP31 != RX31)) {
					SET_OV;
				} else {
					CLEAR_OV;
				}
				if ((uint64_t)cmp >> 31) {
				    SET_CY;
				} else {
				    CLEAR_CY;
				}
				char x_minus[10], x_maius[10], y_minus[10], y_maius[10];

				print_minus(x, x_minus);
				print_maius(x, x_maius);
				print_minus(y, y_minus);
				print_maius(y, y_maius);	
				sprintf(instrucao, "cmp %s,%s", x_minus, y_minus);
				// 0x00000054:	cmp ir,pc  SR=0x00000020
				fprintf(output, "0x%08X:\t%-25s\tSR=0x%08X\n", R[29], instrucao, SR);
				break;
			}

		// Instrucoes especificas (opcode: 0b000100)

			case 0b000100: {
				// pegar o código da subfunção e implementar o controle
				uint8_t subf = (R[28] & (0b111 << 8)) >> 8;
				switch(subf) {
					// mul (sem sinal)
					case 0b000: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						uint64_t auxRx = R[x];
						uint64_t auxRy = R[y]; 
						uint64_t mult = auxRx * auxRy;
						R[z] = (uint32_t)mult;  
						R[l] = (uint64_t)(mult >> 32);

						uint64_t lz = ((uint64_t)R[l] << 32) | (R[z]);

						if (lz == 0) {
							SET_ZN;
						} else {
							CLEAR_ZN;
						} if (R[l] != 0){
							SET_CY;
						} else {
							CLEAR_CY;
						}
						char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10], l_minus[10], l_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);
						print_minus(y, y_minus);
						print_maius(y, y_maius);	
						print_minus(l, l_minus);
						print_maius(l, l_maius);							
						sprintf(instrucao, "mul %s,%s,%s,%s", l_minus, z_minus, x_minus, y_minus);
						//0x00000034:	mul r0,r5,r4,r3          	R0:R5=R4*R3=0x0000000023F4F31C,SR=0x00000008
						fprintf(output, "0x%08X:\t%-25s\t%s:%s=%s*%s=0x%016lX,SR=0x%08X\n", R[29], instrucao, l_maius, z_maius, x_maius, y_maius, lz, SR); 
						break;
					}
    				// muls (com sinal)
    				case 0b010: {
    					z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						int32_t auxRx = (int32_t) R[x];
						int32_t auxRy = (int32_t) R[y];
						int64_t mult = auxRx * auxRy;
					    R[z] = (mult);
					    R[l] = (mult >> 32);
						int64_t lz = ((int64_t)R[l] << 32) | (R[z]);	
							
						
   						if (lz == 0) {
    						SET_ZN;
    					} else {
    						CLEAR_ZN;
    					} if (R[l] != 0) {
    						SET_OV;
    					} else {
    						CLEAR_OV;
    					}	
						
						char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10], l_minus[10], l_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);
						print_minus(y, y_minus);
						print_maius(y, y_maius);	
						print_minus(l, l_minus);
						print_maius(l, l_maius);	
    					sprintf(instrucao, "muls %s,%s,%s,%s", l_minus, z_minus, x_minus, y_minus);
    					// Implementar o print
    					// 0x0000003C:	muls r0,r7,r6,r5         	R0:R7=R6*R5=0x0000000000000000,SR=0x00000040	
 						fprintf(output, "0x%08X:\t%-25s\t%s:%s=%s*%s=0x%016lX,SR=0x%08X\n", R[29], instrucao, l_maius, z_maius, x_maius, y_maius, lz, SR);    												
						break;
					}
					//sll (deslocamento para esquerda - lógico sem sinal)
					case 0b001: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						
						uint64_t concat = ((uint64_t)R[z] << 32) | R[x];
						uint64_t multi = concat << (l +1);
						R[z] = (multi >> 32) & 0xFFFFFFFF; 
    					R[y] = multi & 0xFFFFFFFF;

    					uint64_t zy = ((uint64_t) R[z] << 32 | R[y]);

    					if (zy == 0) {
    						SET_ZN;
    					} else {
    						CLEAR_ZN;
    					} if (R[z] != 0) {
    						SET_CY;
    					} else {
    						CLEAR_CY;
    					}

						char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);	
    					//Implementar o print
    					sprintf(instrucao, "sll %s,%s,%s,%d", z_minus, x_minus, x_minus, l);
    					// 0x00000038:	sll r6,r5,r5,0           	R6:R5=R6:R5<<1=0x0000000047E9E638,SR=0x00000008
    					fprintf(output, "0x%08X:\t%-25s\t%s:%s=%s:%s<<%d=0x%016lX,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, z_maius, x_maius, l + 1, multi, SR);
    					break;
    				}
    				// sla (deslocamento para esquerda - aritmético com sinal)
    				case 0b011: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						//int32_t auxRz = (int32_t)R[z];
						//int32_t auxRy = (int32_t)R[y];
						int64_t concat = (R[z] << 31) | R[y];
						int64_t multi = concat << (l + 1);
						
    					R[x] = multi & 0xFFFFFFFF;
    					if (z == 0) {
    						R[z] = 0x0000000;
    					} else {
							R[z] = (multi >> 32); 
    					}

						int64_t zx = (R[z] << 31 | R[x]);

    					if (zx == 0) {
    						SET_ZN;
    					} else {
    						CLEAR_ZN;
    					} if (R[z] != 0) {
    						SET_OV;
    					} else {
    						CLEAR_OV;
    					}
						char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);
						print_minus(y, y_minus);
						print_maius(y, y_maius);	
	
    					// Implementar o print
    					sprintf(instrucao, "sla %s,%s,%s,%d", z_minus, x_minus, y_minus, l);
    					// 0x0000002C:	sla r0,r2,r2,10          	R0:R2=R0:R2<<11=0x0000000080000000,SR=0x00000001 
    					// Saída: 0x0000002C:	sla r0,r2,r2,10          	R0:R2=R0:R2<<11=0xffffffff80000000,SR=0x00000008
    					fprintf(output, "0x%08X:\t%-25s\t%s:%s=%s:%s<<%d=0x%016lX,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, z_maius, x_maius, l + 1, zx, SR);
    					break;
    				}
					// div (sem sinal)
    				case 0b100: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						// Condição especial caso o denominador seja zero
						// Evita que o programa retorne "arithmetic exception"
						if (R[y] == 0) {
							salvar_contexto(&PC, &SP, &IPC, &CR, MEM32);
							uint32_t i = 0;							
							CR = i;							
							IPC = PC;

							PC = 0x00000008;
							PC = PC - 4;

							SET_ZD;
						char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10], l_minus[10], l_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);
						print_minus(y, y_minus);
						print_maius(y, y_maius);	
						print_minus(l, l_minus);
						print_maius(l, l_maius);								
						sprintf(instrucao, "div %s,%s,%s,%s", l_minus, z_minus, x_minus, y_minus);
						//0x00000044:	div r0,r9,r8,r7          	R0=R8%R7=0x00000000,R9=R8/R7=0x00000000,SR=0x00000060
						fprintf(output, "0x%08X:\t%-25s\t%s=%s%%%s=0x%08X,%s=%s/%s=0x%08X,SR=0x%08X\n", R[29], instrucao, l_maius, x_maius, y_maius, R[l], z_maius, x_maius, y_maius, R[z], SR);
						fprintf(output, "[SOFTWARE INTERRUPTION]\n");
						break;		

						// Caso não seja zero, continua normalmente					
						} else {
							CLEAR_ZD;
						R[z] = R[x] / R[y];
						// Salvando o resto da divisão em R[l]
						R[l] = R[x] % R[y];

						if (R[z] == 0) {
							SET_ZN;
						} else {
							CLEAR_ZN;
						}
						if (R[l] != 0) {
							SET_CY;
						} else {
							CLEAR_CY;
						}
						char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10], l_minus[10], l_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);
						print_minus(y, y_minus);
						print_maius(y, y_maius);	
						print_minus(l, l_minus);
						print_maius(l, l_maius);	
						sprintf(instrucao, "div %s,%s,%s,%s", l_minus, z_minus, x_minus, y_minus);
						//0x00000044:	div r0,r9,r8,r7          	R0=R8%R7=0x00000000,R9=R8/R7=0x00000000,SR=0x00000060
						fprintf(output, "0x%08X:\t%-25s\t%s=%s%%%s=0x%08X,%s=%s/%s=0x%08X,SR=0x%08X\n", R[29], instrucao, l_maius, x_maius, y_maius, R[l], z_maius, x_maius, y_maius, R[z], SR);
    					break;
    				}
    				}
					// srl (deslocamento para direita - lógico sem sinal)
    				case 0b101: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						uint64_t concat = ( R[z] << 31) | R[x];
						uint64_t div = concat >> (l +1);
						R[z] = (div >> 32) & 0xFFFFFFFF;
    					R[y] = div & 0xFFFFFFFF;

    					uint64_t zy = (R[z] << 31 | R[y]);

    					if (zy == 0) {
    						SET_ZN;
    					} else {
    						CLEAR_ZN;
    					} if (R[z] != 0) {
    						SET_CY;
    					} else {
    						CLEAR_CY;
    					}
						char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);
						print_minus(y, y_minus);
						print_maius(y, y_maius);	

    					//Implementar o print
    					sprintf(instrucao, "srl %s,%s,%s,%d", z_minus, x_minus, x_minus, l);
    					// 0x00000048:	srl r10,r9,r9,2          	R10:R9=R10:R9>>3=0x0000000000000000,SR=0x00000060
						fprintf(output, "0x%08X:\t%-25s\t%s:%s=%s:%s>>%d=0x%016lx,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, z_maius, x_maius, l + 1, zy, SR);
    					break;
    				}
					// divs (com sinal)
    				case 0b110: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));
						// Condição especial caso o denominador seja zero
						// Evita que o programa retorne "arithmetic exception"
						if (R[y] == 0) {
							R[z] = 0;
							R[l] = 0;
							SET_ZD;
						char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10], l_minus[10], l_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);
						print_minus(y, y_minus);
						print_maius(y, y_maius);	
						print_minus(l, l_minus);
						print_maius(l, l_maius);								
						sprintf(instrucao, "divs %s,%s,%s,%s", l_minus, z_minus, x_minus, y_minus);
						//0x00000044:	div r0,r9,r8,r7          	R0=R8%R7=0x00000000,R9=R8/R7=0x00000000,SR=0x00000060
						fprintf(output, "0x%08X:\t%-25s\t%s=%s%s=0x%08X,%s=%s/%s=0x%08X,SR=0x%08X\n", R[29], instrucao, l_maius, x_maius, y_maius, R[l], z_maius, x_maius, y_maius, R[z], SR);
						break;		

						// Caso não seja zero, continua normalmente					
						} else {
							CLEAR_ZD;

						int32_t auxRx = (int32_t) R[x];
						int32_t auxRy = (int32_t) R[y];

						R[z] = auxRx/auxRy;
						// Salvando o resto da divisão em R[l]
						R[l] = auxRx % auxRy;	

						if (R[z] == 0) {
							SET_ZN;
						} else {
							CLEAR_ZN;
						}
						if (R[l] != 0) {
							SET_CY;
						} else {
							CLEAR_CY;
						}
						char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10], l_minus[10], l_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);
						print_minus(y, y_minus);
						print_maius(y, y_maius);	
						print_minus(l, l_minus);
						print_maius(l, l_maius);	
						sprintf(instrucao, "divs %s,%s,%s,%s", l_minus, z_minus, x_minus, y_minus);
						//0x00000044:	div r0,r9,r8,r7          	R0=R8%R7=0x00000000,R9=R8/R7=0x00000000,SR=0x00000060
						fprintf(output, "0x%08X:\t%-25s\t%s=%s%%%s=0x%08X,%s=%s/%s=0x%08X,SR=0x%08X\n", R[29], instrucao, l_maius, x_maius, y_maius, R[l], z_maius, x_maius, y_maius, R[z], SR);    					
    					break;
    				}
    				}
					// sra (deslocamento para direita - artimético com sinal)
    				case 0b111: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						uint32_t auxRz = R[z];
						uint32_t auxRy = R[y];

						int64_t concat = ((int64_t)auxRz << 32) | auxRy;
						int64_t div = concat >> (l + 1);

						if (z == 0) {
							R[z] = 0;
						} else {
							R[z] = (div >> 32) & 0xFFFFFFFF;
						}

    					R[x] = div & 0xFFFFFFFF;

    					int64_t zy = ((int64_t)R[z] << 32) | R[x];

    					if (zy == 0) {
    						SET_ZN;
    					} else {
    						CLEAR_ZN;
    					} if (R[z] != 0) {
    						SET_OV;
    					} else {
    						CLEAR_OV;
    					}
						char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10];
						print_minus(z, z_minus);
						print_maius(z, z_maius);
						print_minus(x, x_minus);
						print_maius(x, x_maius);
						print_minus(y, y_minus);
						print_maius(y, y_maius);	

    					//Implementar o print
    					sprintf(instrucao, "sra %s,%s,%s,%d", z_minus, x_minus, x_minus, l);
    					// 0x00000050:	sra r12,r10,r10,3        	R12:R10=R12:R10>>4=0x0000000000000000,SR=0x00000060
						fprintf(output, "0x%08X:\t%-25s\t%s:%s=%s:%s>>%d=0x%016lX,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, z_maius, x_maius, l + 1, zy, SR);
    					break;
    				}
    			}
    			break;
    		}

		// Instruções bit a bit

			// and
			case 0b000110: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;	

				R[z] = R[x] & R[y];

				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				} if (RZ31 == 1) {
					SET_SN;
				} else {
					CLEAR_SN;
				}

				char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);
				print_minus(y, y_minus);
				print_maius(y, y_maius);	
				// Implementar a impressão
				sprintf(instrucao, "and %s,%s,%s", z_minus, x_minus, y_minus);
				// 0x00000058:	and r13,r1,r5            	R13=R1&R5=0x00002410,SR=0x00000020
				fprintf(output, "0x%08X:\t%-25s\t%s=%s&%s=0x%08X,SR=0x%08X\n",R[29], instrucao, z_maius, x_maius, y_maius, R[z], SR);
				break;
			}
			// or
			case 0b000111: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;	

				// Implementando o comportamento
				R[z] = R[x] | R[y];

				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				} if (RZ31 == 1) {
					SET_SN;
				} else {
					CLEAR_SN;
				}	
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);
				print_minus(y, y_minus);
				print_maius(y, y_maius);
				// Implementar a impressão
				sprintf(instrucao, "or %s,%s,%s", z_minus, x_minus, y_minus);
				// 0x0000005C:	or r14,r2,r6             	R14=R2|R6=0x80000000,SR=0x00000030
				fprintf(output, "0x%08X:\t%-25s\t%s=%s|%s=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, y_maius, R[z], SR);
				break;
			}
			// not
			case 0b001000: {
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;	

				// Implementando o comportamento
				R[z] = ~R[x];
				
				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				} if (RZ31 == 1) {
					SET_SN;
				} else {
					CLEAR_SN;
				}	
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);
				// Implementar a impressão 
				sprintf(instrucao, "not %s,%s", z_minus, x_minus);
				// 0x00000060:	not r15,r7               	R15=~R7=0xFFFFFFFF,SR=0x00000030
				fprintf(output, "0x%08X:\t%-25s\t%s=~%s=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, R[z], SR);				
				break;
			}
			// xor
			case 0b001001: {
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;					

				// Implementar o comportamento
				R[z] = R[x] ^ R[y];
				
				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				} if (RZ31 == 1) {
					SET_SN;
				} else {
					CLEAR_SN;
				}
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10], y_minus[10], y_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);
				print_minus(y, y_minus);
				print_maius(y, y_maius);
				// Implementar a impressão 
				sprintf(instrucao, "xor %s,%s,%s", z_minus, x_minus, y_minus);
				// 0x00000064:	xor r16,r16,r8           	R16=R16^R8=0x00000000,SR=0x00000060
				fprintf(output, "0x%08X:\t%-25s\t%s=%s^%s=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, y_maius, R[z], SR);					
				break;
			}					
		// Instruções imediatas		

			// addi
			case 0b010010: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = (R[28] & 0xFFFF);

				uint64_t bit15 = I15 ? 0xFFFF : 0x0000;
				uint64_t concat_i = ((bit15 << 16) | i);

				uint64_t add = R[x] + concat_i;

				R[z] = (int32_t)add;

				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				}; if (RZ31 == 1) {
					SET_SN;
				} else {
					CLEAR_SN;
				}; if ((RX31 == I15) && (RZ31 != RX31)) {
					SET_OV;
				} else {
					CLEAR_OV;
				}; if (add & (1ULL << 32)) {
					SET_CY;
				} else {
					CLEAR_CY;
				};
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);
				// Implementar a impressão 
				sprintf(instrucao, "addi %s,%s,%d", z_minus, x_minus, i);
				// 0x00000068:	addi r17,r17,1           	R17=R17+0x00000001=0x00000001,SR=0x00000020
				fprintf(output, "0x%08X:\t%-25s\t%s=%s+0x%08X=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, i, R[z], SR);
				break;
			}
			// subi
			// ajustar as condições de setar e limpar			
			case 0b010011: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				// Essa validação analisa se o bit 15 de i é igual a 1 ou 0
				// Se for igual a 1, ele repete esse valor 16 vezes
				// Se for igual a 0, repete o 0 por 16 vezes
				uint32_t bit15 = I15 ? 0xFFFF : 0x0000;
				uint32_t concat_i = ((bit15 << 16) | i);

				R[z] = R[x] - concat_i;

				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				} if (RZ31 == 1) {
					SET_SN;
				} else {
					CLEAR_SN;
				} if ((RX31 != I15) && (RZ31 != RX31)) {
					SET_OV;
				} else {
					CLEAR_OV;
				} if ((uint64_t)R[z] & (1ULL << 32)) {
					SET_CY;
				} else {
					CLEAR_CY;
				}
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);
				// Implementar a impressão 
				sprintf(instrucao, "subi %s,%s,%d", z_minus, x_minus, i);
				// 0x0000006C:	subi r18,r18,-1          	R18=R18-0xFFFFFFFF=0x00000001,SR=0x00000020
				fprintf(output, "0x%08X:\t%-25s\t%s=%s-0x%08X=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, i, R[z], SR);
				break;
			}
			// muli (com sinal)
			// ajustar por ser com sinal
			case 0b010100: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				int32_t i32 =  (int32_t) i;
				int32_t auxRx = (int32_t) R[x];

				int64_t resultado = auxRx * i32;
				int64_t sinal = resultado >> 32;
				R[z] = resultado;

				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				} if (sinal != 0 ) {
					SET_OV;
				} else {
					CLEAR_OV;
				}
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);				
				// Implementar a impressão 
				sprintf(instrucao, "muli %s,%s,%d", z_minus, x_minus, i32);
				// 0x00000070:	muli r19,r17,2           	R19=R17*0x00000002=0x00000002,SR=0x00000020
				fprintf(output, "0x%08X:\t%-25s\t%s=%s*0x%08X=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, i, R[z], SR);
				break;
			}

			// divi (com sinal)
			// ajustar por ser com sinal
			case 0b010101: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				int32_t i32 =  (int32_t) i;
				int32_t auxRx = (int32_t) R[x];
				int64_t resultado;
				if (i32 == 0) {
					SET_ZD;
				} else {
					CLEAR_ZD;
					resultado = auxRx / i32;
					R[z] = resultado;

					if (R[z] == 0) {
						SET_ZN;
					} else {
						CLEAR_ZN;
					}
				}
					CLEAR_OV;
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);					
				// Implementar a impressão 
				sprintf(instrucao, "divi %s,%s,%d", z_minus, x_minus, i32);
				// 0x00000074:	divi r20,r19,2           	R20=R19/0x00000002=0x00000001,SR=0x00000000
				fprintf(output, "0x%08X:\t%-25s\t%s=%s/0x%08X=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, i, R[z], SR);
				break;
			}

			// modi (com sinal)
			// ajustar por ser com sinal
			case 0b010110: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				int32_t i32 =  (int32_t) i;
				int32_t auxRx = (int32_t) R[x];

				int32_t resultado = auxRx % i32;
				R[z] = resultado;

				if (R[z] == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;
				} if (i == 0) {
					SET_ZD;
				} else {
					CLEAR_ZD;
				}
				CLEAR_OV;
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);
				// Implementar a impressão 
				sprintf(instrucao, "modi %s,%s,%d", z_minus, x_minus, i32);
				// 0x00000078:	modi r21,r19,3           	R21=R19%0x00000003=0x00000002,SR=0x00000000
				fprintf(output, "0x%08X:\t%-25s\t%s=%s%%0x%08X=0x%08X,SR=0x%08X\n", R[29], instrucao, z_maius, x_maius, i, R[z], SR);
				break;
			} 

			// compi
			case 0b010111: {
				// Obtendo operandos
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = (R[28] & 0xFFFF);

				int32_t cmpi = R[x] - i;

				if (cmpi == 0) {
					SET_ZN;
				} else {
					CLEAR_ZN;

				} if ((cmpi >> 31) != 0) {
				    SET_SN;  // Sign flag (bit 31)
				} else {
				    CLEAR_SN;
				} if ((RX31 != (I15)) && (CMPI31 != RX31)) {
					SET_OV;
				} else {
					CLEAR_OV;
				} if ((((uint64_t)cmpi >> 32) & 0b1) != 0)  {
					SET_CY;
				} else {
					CLEAR_CY;
				}
				char x_minus[10];
				print_minus(x, x_minus);
				// Implementar a impressão 
				sprintf(instrucao, "cmpi %s,%d", x_minus, i);
				// 0x0000007C:	cmpi r21,32              	SR=0x00000011
				fprintf(output, "0x%08X:\t%-25s\tSR=0x%08X\n", R[29], instrucao, SR);				
				break;
			}

				// formatar a impressão										

		// Instruções de leitura/escrita da memória
			
			// l8
			case 0b011000: { 
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = R[28] & 0xFFFF;
				// Execucao do comportamento com MEM32 (calculo do indice da palavra e selecao do byte big-endian)
								
				// Implementar o comportamento
				uint32_t index = R[x] + i;
					switch(index) {
						case (WATCHDOG_ADD): 
							return watchdog_reg;
							break;
						case (FPU_Z_ADD):
							return fpu_z;
							break;
						case (FPU_Y_ADD):
							return fpu_y;
							break;
						case (FPU_X_ADD):
							return fpu_x;
							break;
						case (FPU_ST_ADD):
							return fpu_st;
							break;
						case (TERMINAL_ADD):
							return terminal_reg;
							break;
						case (TERMINAL_IN):
							return terminal_reg >> 8;
							break;
						default:
							R[z] = (((uint8_t*)(&MEM32[(R[x] + i) >> 2]))[3 - ((R[x] + i) % 4)]);
					} 

				// Formatacao da instrucao
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);				
				sprintf(instrucao, "l8 %s,[%s%s%i]", z_minus, x_minus, (i >= 0) ? ("+") : (""), i);
				// 0x00000080:	l8 r22,[r0+35]           	R22=MEM[0x00000023]=0x56
				fprintf(output, "0x%08X:\t%-25s\t%s=MEM[0x%08X]=0x%02X\n", R[29], instrucao, z_maius, R[x] + i, R[z]);
				break;
			}
			// l16
			case 0b011001: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = R[28] & 0xFFFF;
				// Implementar o comportamento
				uint32_t index = R[x] + i;
					switch(index) {
						case (WATCHDOG_ADD): 
							return watchdog_reg;
							break;
						case (FPU_Z_ADD):
							return fpu_z;
							break;
						case (FPU_Y_ADD):
							return fpu_y;
							break;
						case (FPU_X_ADD):
							return fpu_x;
							break;
						case (FPU_ST_ADD):
							return fpu_st;
							break;
						case (TERMINAL_ADD):
							return terminal_reg;
							break;
						case (TERMINAL_IN):
							return terminal_reg >> 8;
							break;
						default:
							R[z] = (((uint16_t*)(&MEM32[(R[x] + i) >> 1]))[1 - ((R[x] + i) % 2)]);		
					} 

				// Implementar a impressão
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);				
				sprintf(instrucao, "l16 %s,[%s%s%i]", z_minus, x_minus, (i >= 0) ? ("+") : (""), i);
				// 0x00000084:	l16 r23,[r0+17]          	R23=MEM[0x00000022]=0x3456
				fprintf(output, "0x%08X:\t%-25s\t%s=MEM[0x%08X]=0x%04X\n", R[29], instrucao, z_maius, (R[x] + i) << 1, R[z]);
				break;
			}
			// l32
			case 0b011010: {
				// Otendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = R[28] & 0xFFFF;
				
				// Implementar o comportamento
				uint32_t index = R[x] + i;
					switch(index) {
						case (WATCHDOG_ADD): 
							return watchdog_reg;
							break;
						case (FPU_Z_ADD):
							return fpu_z;
							break;
						case (FPU_Y_ADD):
							return fpu_y;
							break;
						case (FPU_X_ADD):
							return fpu_x;
							break;
						case (FPU_ST_ADD):
							return fpu_st;
							break;
						case (TERMINAL_ADD):
							return terminal_reg;
							break;
						case (TERMINAL_IN):
							return terminal_reg >> 8;
							break;
						default:
							R[z] = MEM32[index];
					} 

				// Implementar a impressão
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);				
				sprintf(instrucao, "l32 %s,[%s%s%i]", z_minus, x_minus, (i >= 0) ? ("+") : (""), i);
				// 0x00000088:	l32 r24,[r0+8]           	R24=MEM[0x00000020]=0x00323456
				fprintf(output, "0x%08X:\t%-25s\t%s=MEM[0x%08X]=0x%08X\n", R[29], instrucao, z_maius, (R[x] + i) << 2, R[z]);
				break;
			}
			// s8
			case 0b011011: {
				// Otendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				uint16_t i = R[28] & 0xFFFF;
				int32_t i32 = (int32_t) i;
				// Implementar o comportamento
				uint32_t index = (R[x] + i32);
					switch(index) {
						case (WATCHDOG_ADD): 
							watchdog_reg = R[z];
							break;
						case (FPU_Z_ADD):
							fpu_z = R[z];
							break;
						case (FPU_Y_ADD):
							fpu_y = R[z];
							break;
						case (FPU_X_ADD):
							fpu_x = R[z];
							break;
						case (FPU_ST_ADD):
							fpu_st = R[z];
							break;
						case (TERMINAL_ADD):
							terminal_reg = R[z];
							break;	
						case (TERMINAL_OUT):
							terminal_out = R[z] & 0xFF;
							buffer[buffer_index++] = terminal_out;
							break;
						default:
							MEM32[(index) % (8 * 1024)]  = R[z];
					}
			
				// Implementar a impressão
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);				
				sprintf(instrucao, "s8 [%s%s%i],%s", x_minus, (i >= 0) ? ("+") : (""), i, z_minus);
				// 0x0000008C:	s8 [r0+35],r22           	MEM[0x00000023]=R22=0x56
				fprintf(output, "0x%08X:\t%-25s\tMEM[0x%08X]=%s=0x%02X\n", R[29], instrucao, R[x] + i32, z_maius, R[z]);
				break;				
			}
			// s16
			case 0b011100: {
				// Otendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				uint16_t i = R[28] & 0xFFFF;
				uint32_t i32 = (uint32_t) i;
				// Implementar o comportamento
				uint32_t index = (R[x] + i32) << 1;
					switch(index) {
						case (WATCHDOG_ADD): 
							watchdog_reg = R[z];
							break;
						case (FPU_Z_ADD):
							fpu_z = R[z];
							break;
						case (FPU_Y_ADD):
							fpu_y = R[z];
							break;
						case (FPU_X_ADD):
							fpu_x = R[z];
							break;
						case (FPU_ST_ADD):
							fpu_st = R[z];
							break;
						case (TERMINAL_ADD):
							terminal_reg = R[z];
							break;	
						case (TERMINAL_OUT):
							terminal_out = R[z] & 0xFF;
							buffer[buffer_index] = terminal_out;
							break;
						default:
							MEM32[(index)]  = R[z];
					}

				// Implementar a impressão
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);				
				sprintf(instrucao, "s16 [%s%s%i],%s", x_minus, (i >= 0) ? ("+") : (""), i, z_minus);
				// 0x0000008C:	s8 [r0+35],r22           	MEM[0x00000023]=R22=0x56
				fprintf(output, "0x%08X:\t%-25s\tMEM[0x%08X]=%s=0x%04X\n", R[29], instrucao, (R[x] + i) << 1, z_maius, R[z]);
				break;
			}
			// s32
			case 0b011101: {
				// Otendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				uint16_t i = R[28] & 0xFFFF;
				// Implementar o comportamento

				uint32_t index = (R[x] + i) << 2;
					switch(index) {
						case (WATCHDOG_ADD): 
							watchdog_reg = R[z];
							break;
						case (FPU_Z_ADD):
							fpu_z = R[z];
							break;
						case (FPU_Y_ADD):
							fpu_y = R[z];
							break;
						case (FPU_X_ADD):
							fpu_x = R[z];
							break;
						case (FPU_ST_ADD):
							fpu_st = R[z];
							break;
						case (TERMINAL_ADD):
							terminal_reg = R[z];
							break;	
						case (TERMINAL_OUT):
							terminal_out = R[z] & 0xFF;
							buffer[buffer_index] = terminal_out;
							break;
						default:
							MEM32[index] = R[z];
					}
			
				// Implementar a impressão
				char z_minus[10], z_maius[10], x_minus[10], x_maius[10];
				print_minus(z, z_minus);
				print_maius(z, z_maius);
				print_minus(x, x_minus);
				print_maius(x, x_maius);			
				sprintf(instrucao, "s32 [%s%s%i],%s", x_minus, (i >= 0) ? ("+") : (""), i, z_minus);
				// 0x0000008C:	s8 [r0+35],r22           	MEM[0x00000023]=R22=0x56
				fprintf(output, "0x%08X:\t%-25s\tMEM[0x%08X]=%s=0x%08X\n", R[29], instrucao, (R[x] + i) << 2,z_maius, R[z]);
				//return 0;
				break;
			}

		// Instruções de controle de fluxo

			// bae (desvio condicional: AE - sem sinal)
				// Branch if Above or Equal: Desvia se a comparação A ≥ B for verdadeira
			case 0b101010: {
				// Obtendo operandos
				uint32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A ≥ B → A − B ≥ 0 ≡ AE ← CY = 0
				if (CY == 0) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "bae %d",i);
				// 0x00000098:	bae 0                    	PC=0x0000009C
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);
				break;
			}
			// bat (desvio condicional: AT - sem sinal)
				// Branch if Above or Top: Desvia se A > B for verdadeiro
			case 0b101011: {
				// Obtendo operandos
				uint32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A > B → A − B > 0 ≡ AT ← (ZN = 0 ∧ CY = 0)
				if ((ZN == 0) && (CY == 0)) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "bat %d",i);
				// 0x0000009C:	bat 0                    	PC=0x000000A0
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n",pc_antigo, instrucao, PC + 4);			
				break;
			}
			// bbe (desvio condicional: BE - sem sinal)
				// Branch if Below or Equal: Desvia se a comparação A ≤ B for verdadeira
			case 0b101100: {
				// Obtendo operandos
				uint32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A ≤ B → A − B ≤ 0 ≡ BE ← (ZN = 1 ∨ CY = 1)
				if ((ZN == 1) || (CY == 1)) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "bbe %d", i);
				// 0x000000A0:	bbe 0                    	PC=0x000000A4
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);					
				break;
			}
			// bbt (desvio condicional: BT - sem sinal)
				// Branch if Below or Top: Desvia se A < B for verdadeiro
			case 0b101101: {
				// Obtendo operandos
				uint32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A < B → A − B < 0 ≡ BT ← CY = 1
				if (CY == 1) {
					PC = PC + (i << 2);	
				}
				// Implementar a impressão 
				sprintf(instrucao, "bbt %d",i);
				// 0x000000A4:	bbt 0                    	PC=0x000000A8
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);					
				break;
			}
			// beq (desvio condicional: EQ)	
				// Branch if Equal: Desvia se A == B			
			case 0b101110: {
				// Obtendo operandos
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A = B → A − B = 0 ≡ EQ ← ZN = 1
				if (ZN == 1) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "beq %d", i);
				// 0x000000A8:	beq 0                    	PC=0x000000AC
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);			
				break;
			}
			// bge (desvio condicional: GE - com sinal)
				// Branch if Greater or Equal: Desvia se A ≥ B
			case 0b101111: {
				// Obtendo operandos
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A ≥ B → A − B ≥ 0 ≡ GE ← SN = OV
				if (SN == OV) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "bge %d", i);
				// 0x000000AC:	bge 0                    	PC=0x000000B0
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n",pc_antigo, instrucao, PC + 4);				
				break;
			}
			// bgt (desvio condicional: GT - com sinal)
				// Branch if Greater Than: Desvia se A > B
			case 0b110000: {
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A > B → A − B > 0 ≡ GT ← (ZN = 0 ∧ SN = OV )
				if ((ZN == 0) && (SN == OV)) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "bgt %d", i);
				// 0x000000B0:	bgt 0                    	PC=0x000000B4
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);
				break;
			}
			// biv (desvio condicional: IV)
				// Branch if Invalid: Desvia se uma condição de erro ou estado inválido for detectada
			case 0b110001: {
				// Obtendo operandos
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// IV == 1
				if (IV == 1) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "biv %d", i);
				// 0x000000B4:	biv 0                    	PC=0x000000B8
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);
				break;
			}
			// ble (desvio condicional: LE - com sinal)
				// Branch if Less or Equal: Desvia se A ≤ B
			case 0b110010: {
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A ≤ B → A − B ≤ 0 ≡ LE ← (ZN = 1 ∨ SN != OV )
				if ((ZN == 1) || (SN != OV)) {
					PC = PC + (i << 2);	
				}
				// Implementar a impressão 
				sprintf(instrucao, "ble %d", i);
				// 0x000000B8:	ble 0                    	PC=0x000000BC
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);				
				break;
			}
			// blt (desvio condicional: LT - com sinal)
				// Branch if Less Than: Desvia se A < B
			case 0b110011: {
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A < B → A − B < 0 ≡ LT ← SN != OV
				if (SN != OV) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "blt %d", i);
				// 0x000000BC:	blt 0                    	PC=0x000000C0
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);				
				break;
			}
			// bne (desvio condicional: NE)
				// Branch if Not Equal: Desvia se A != B
			case 0b110100: {
				// Obtendo operandos
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// A̸ = B → A − B̸ = 0 ≡ NE ← ZN = 0
				if (ZN == 0) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "bne %d", i);
				// 0x000000C0:	bne 0                    	PC=0x000000C4
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);	
				PC = PC - 4;
				break;
			}
			// bni (desvio condicional: NI)
				// Branch if Not Invalid: Desvia se não houver uma condição de erro ou estado inválido
			case 0b110101: {
				// Obtendo operandos
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// NI ← IV = 0
				if (IV == 0) {
					PC = PC + (i << 2);	
				}

				// Implementar a impressão 
				sprintf(instrucao, "bni %d", i);
				// 0x000000C4:	bni 0                    	PC=0x000000C8
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);					
				break;
			}
			// bnz (desvio condicional: NZ)
				// Branch if Not Zero: Desvia se o resultado da última operação não for zero
			case 0b110110: {
				// Obtendo operandos
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// NZ ← ZD = 0
				if (ZD == 0) {
					PC = PC + (i << 2);	
				}
				// Implementar a impressão 
				sprintf(instrucao, "bnz %d", i);
				// 0x000000C8:	bnz 0                    	PC=0x000000CC
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);					
				break;
			}
			// bun
				// Branch Unconditionally: Desvia para um endereço específico sem considerar nenhuma condição
			case 0b110111: {
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				if (i & 0x02000000) {
			        i |= 0xFC000000;  
   				}


				// Implementar o comportamento

				PC = PC + (i << 2);	
				
				// Implementar a impressão
				sprintf(instrucao, "bun %d", i);
				// 0x000000CC:	bun 0                    	PC=0x000000D0a
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);
				break;
			}
			// bzd
				// Branch if Zeroed: Desvia se o resultado da última operação for zero
			case 0b111000: {
				// Obtendo operandos
				int32_t i = R[28] & 0x03FFFFFF;
				int32_t pc_antigo = PC;

				// Implementar o comportamento
				// Condições para executar:
				// ZD == 1
				if (ZD == 1) {
					PC = PC + (i << 2);	
				}
				// Implementar a impressão 
				sprintf(instrucao, "bzd %d", i);
				// 0x000000D0:	bzd 0                    	PC=0x000000D4
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc_antigo, instrucao, PC + 4);	
				break;
			}

		// Instruções de sub-rotina e empilhamento

			//call (tipo F)
			case 0b011110: {
				// Obtendo operandos
				x = (R[28] & (0b11111 << 16)) >> 16;
				int16_t i = R[28] & 0xFFFF;

				uint32_t pc_antigo = PC;
				MEM32[SP] = PC + 4;
			
				PC = (R[x] + (uint32_t)i) << 2;

				// Implementar a impressão
				char x_minus[10], x_maius[10];
				print_minus(x, x_minus);
				print_maius(x, x_maius);			
				sprintf(instrucao, "call [%s+%d]", x_minus, i);
				// 0x00000050:	call -13                 	PC=0x00000020,MEM[0x00007FFC]=0x00000054
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X,MEM[0x%08X]=0x%08X\n", pc_antigo, instrucao, PC,SP,MEM32[SP]);
				SP = SP - 4;
				PC = PC - 4;
				break;
			}

			// call (tipo S)
			case 0b111001: {
				// Obtendo operandos
			    int32_t i = R[28] & 0x01FFFFFF; // Obtém os 25 bits de imediato
			    
			    // Extensão de sinal se o bit 24 (i25) for 1
			    if (i & 0x01000000) { // Verifica o bit mais significativo de 25 bits
			        i |= 0xFE000000; // Extensão de sinal para 32 bits
			    }
				uint32_t auxSp = SP;
				uint32_t auxPc = PC;	
				MEM32[SP] = PC + 4;
				SP = SP - 4;
				PC += (int64_t)(i << 2);
				sprintf(instrucao, "call %d", i);
				// 0x00000050:	call -13                 	PC=0x00000020,MEM[0x00007FFC]=0x00000054
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X,MEM[0x%08X]=0x%08X\n", auxPc, instrucao, PC + 4,auxSp, MEM32[auxSp]);

				break; 
			}


			// ret
			case 0b011111: {
			// Atualiza o ponteiro da pilha para o endereço anterior 
			uint32_t auxPc = PC;
			//printf("MEM[SP] = 0x%08X\n", MEM32[SP]);
			SP = SP + 4;
			uint32_t retorno_PC = MEM32[SP];
			PC = retorno_PC - 4;
			// Implementar a impressão
			sprintf(instrucao, "ret");
			// 0x00000044:	ret                      	PC=MEM[0x00007FE4]=0x0000003C
			fprintf(output, "0x%08X:\t%-25s\tPC=MEM[0x%08X]=0x%08X\n", auxPc,instrucao, SP, retorno_PC);
			break;
			}

			// push 
			case 0b001010: {
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;
				v = (R[28] & (0b11111 << 6)) >> 6;
				w = (R[28] & (0b11111));	

				uint32_t sp_antigox = SP;
				uint8_t indices[5] = {v, w, x, y, z};
				char registros_minus[200] = "";
				char valores[200] = "";
				char reg_name[10];
				
				for (int i = 0; i < 5; i++) {
					int reg = indices[i];
					if (reg == 0) {
						continue; // Se for 0, apenas ignoramos, sem quebrar o loop.
					} 
					SP = SP - 4; // Decrementar SP antes de armazenar.
					MEM32[SP] = R[reg];

					char valor[50];
					print_minus(reg, reg_name); // Obter o nome do registro em minúsculas.
					sprintf(valor, "0x%08X", MEM32[SP]); // Formatar o valor.

					strcat(registros_minus, reg_name);
					strcat(registros_minus, ",");
					strcat(valores, valor);
					strcat(valores, ",");
				}

				if (strlen(registros_minus) > 0) {
					registros_minus[strlen(registros_minus) - 1] = '\0'; // Remover última vírgula.
				}
				if (strlen(valores) > 0) {
					valores[strlen(valores) - 1] = '\0'; // Remover última vírgula.
				}
				
				char registros_maius[200];
				strcpy(registros_maius, registros_minus);
				convert_to_uppercase(registros_maius); // Converter registros para maiúsculas.

				if (v == 0) {
					sprintf(instrucao, "push -");
				} else {
					sprintf(instrucao, "push %s", registros_minus);
				}

				// 0x00000030:	push r1                  	MEM[0x00007FF0]{0x00000002}={R1}
				fprintf(output, "0x%08X:\t%-25s\tMEM[0x%08X]{%s}={%s}\n", PC, instrucao, sp_antigox, valores, registros_maius);
				break;	
			} 

			// pop
			case 0b001011: {
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;
				v = (R[28] & (0b11111 << 6)) >> 6;
				w = (R[28] & (0b11111));

			
				uint32_t sp_antigox = SP;
				uint8_t indices[5] = {v, w, x, y, z};
				char registros_minus[200] = "";
				char valores[200] = "";
				char reg_name[10];
				
					for (int i = 0; i < 5; i++) {
						int reg = indices[i];
						if (reg == 0) {
							break;
						} else {
						
						R[reg] = MEM32[SP];
							char valor[50];
							print_minus(reg, reg_name);
							sprintf(valor, "0x%08X,", MEM32[SP]);
							strcat(registros_minus, reg_name);
							strcat(registros_minus, ",");
							strcat(valores, valor);

							SP = SP + 4;
						}
					}
				registros_minus[strlen(registros_minus) - 1] = '\0';
				valores[strlen(valores) - 1] = '\0';
				char registros_maius[200];
				strcpy(registros_maius, registros_minus);
				convert_to_uppercase(registros_maius);
				// Implementar a impressão
				if (v == 0) {
					sprintf(instrucao, "pop -");
				} else {
					sprintf(instrucao, "pop %s", registros_minus);
				}
				// 0x00000030:	push r1                  	MEM[0x00007FF0]{0x00000002}={R1}
				fprintf(output, "0x%08X:\t%-25s\t{%s}=MEM[0x%08X]{%s}\n", PC, instrucao, registros_maius, sp_antigox, valores);
				break;		      
			}

			// reti
			case 0b100000: {
				uint32_t PC_atual = PC;

				SP += 4;
				uint32_t sp_ipc = SP;
				IPC = MEM32[sp_ipc];

				SP += 4;
				uint32_t sp_cr = SP;
				CR = MEM32[SP];

				SP += 4;
				uint32_t sp_pc = SP;
				PC = MEM32[sp_pc];
				PC = PC - 4;

				sprintf(instrucao, "reti");
				// 0x00000028:	reti          IPC=MEM[0x00007FF4]=0x00000000,CR=MEM[0x00007FF8]=0x00000000,PC=MEM[0x00007FFC]=0x00000034
				fprintf(output, "0x%08X:\t%-25s\tIPC=MEM[0x%08X]=0x%08X,CR=MEM[0x%08X]=0x%08X,PC=MEM[0x%08X]=0x%08X\n", 
				PC_atual, instrucao, sp_ipc, MEM32[sp_ipc], sp_cr, MEM32[sp_cr], sp_pc, MEM32[sp_pc]);
				break;
			}
			
			// cbr & sbr (limpeza de bit do registrador)
			case 0b100001: {

				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;	
				uint8_t i = R[28] & 0b1;

				if (i == 0) {
					R[z] &= (~(0b1) << x);
				} else {
					R[z] |= (0b1 << x);
					char z_minus[10], z_maius[10];
					print_minus(z, z_minus);
					print_maius(z, z_maius);

					//0x0000005C:	sbr sr[1]                	SR=0x00000002
					sprintf(instrucao, "sbr %s[%d]", z_minus, x);
					fprintf(output, "0x%08X:\t%-25s\t%s=0x%08X\n", PC, instrucao, z_maius, R[z]);
				}
				break;
			}
			// int
			case 0b111111: {
				int32_t i = R[28] & 0x03FFFFFF;

				if (i == 0) {
					// Parar a execucao
					executa = 0;
					sprintf(instrucao, "int 0");
					// 0x000000D4:	int 0                    	CR=0x00000000,PC=0x00000000
					fprintf(output, "0x%08X:\t%-25s\tCR=0x00000000,PC=0x00000000\n", R[29], instrucao);
				} else {
					uint32_t PC_inicial = PC;
					salvar_contexto(&PC_inicial, &SP, &IPC, &CR, MEM32);


					CR = i;
					IPC = PC;
					PC = 0x0000000C;
					PC = PC - 4;


					sprintf(instrucao, "int %d", i);
					fprintf(output, "0x%08X:\t%-25s\tCR=0x%08X, PC=0x%08X\n", PC_inicial, instrucao, CR, PC + 4);
					fprintf(output, "[SOFTWARE INTERRUPTION]\n");
				}
				break;
			}
		// Instrucao desconhecida
			default: {
				fprintf(output, "[INVALID INSTRUCTION @ 0x%08X]\n", PC);
				fprintf(output, "[SOFTWARE INTERRUPTION]\n");
				SET_IV;

				uint32_t PC_atual = PC;

				salvar_contexto(&PC_atual, &SP, &IPC, &CR, MEM32);
				CR = PC;
				IPC = PC;

				PC = 0x00000004;
				PC = PC - 4;

				break;
			}
		}
		PC = PC + 4; //(proxima instrucao)
	}

	if (buffer_index != 0) {
		fprintf(output, "[TERMINAL]\n");

		for (int i = 0; i < buffer_index; i++) {
            fprintf(output, "%c", buffer[i]);
        }
		fprintf(output, "\n");
	}


	// Exibindo a finalizacao da execucao
	fprintf(output, "[END OF SIMULATION]\n");
	// Fechando os arquivos
	fclose(input);
	fclose(output);
	// Liberando a memória alocada 
	free(MEM32);
	// Finalizando programa
	return 0;
}

