#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

//Definicao do tamanho da memoria
#define MEM32_SIZE 8*1024

//Definicao dos registradores especiais 
#define IR R[28] //registrador de instrucao (codigo da instrucao)
#define PC R[29] //contador de programa (endereco da instrucao)
#define SP R[30] //ponteiro de pilha (endereco da pilha)
#define SR R[31] //registrador de status
// Definicao dos registradores de status individualmente
#define CY (R[31] & 0x1) // Carry
#define IV ((R[31] >> 2) & 0x1) // Instrução inválida
#define OV ((R[31] >> 3) & 0x1) // Overflow
#define SN ((R[31] >> 4) & 0x1) // Sinal
#define ZD ((R[31] >> 5) & 0x1) // Divisao por zero (divisor B = 0) 
#define ZN ((R[31] >> 6) & 0x1) // Zero

/* Definição para setar ou limpar os status dos campos
do registrador especial SR. Cada campo possui dois comandos:
set e clear, que é calculado com base nas operações or (|), ou and (&).
Desloca-se um bit com valor 1 de acordo com o índice de cada status 
do registrador, e a operação gera um resultado que é definido com SET ou CLEAR. */
#define SET_CY() (R[31] |= (1 << 0))
#define CLEAR_CY() (R[31] &= ~(1 << 0))
#define SET_IV() (R[31] |= (1 << 2))
#define CLEAR_IV() (R[31] &= ~(1 << 2))
#define SET_OV() (R[31] |= (1 << 3))
#define CLEAR_OV() (R[31] &= ~(1 << 3))
#define SET_SN() (R[31] |= (1 << 4))
#define CLEAR_SN() (R[31] &= ~(1 << 4))
#define SET_ZD() (R[31] |= (1 << 5))
#define CLEAR_ZD() (R[31] &= ~(1 << 5))
#define SET_ZN() (R[31] |= (1 << 6))
#define CLEAR_ZN() (R[31] &= ~(1 << 6))

/* Definições úteis para melhor legibilidade do código */
#define RX31 (R[x] & (1 << 31))
#define RY31 (R[y] & (1 << 31))
#define RZ31 (R[z] & (1 << 31))
#define RZ32 ((R[z] & ((uint64_t) 1 << 32)) // Revisar essa macro
#define CMP31 (cmp & (1 << 31))
#define CMP32 (cmp & ((uint64_t) 1 << 32))
#define CMPI31 (cmpi & (1 << 31))
#define CMPI32 (cmpi & ((uint64_t) 1 << 32))
#define I15 ((i >> 15) & 0x1)

int main(int argc, char* argv[]) {

	FILE* input = fopen(argv[1], "r");
	FILE* output = fopen(argv[2], "w");

	// Inicializa os registradores com valor inicial 0
	uint32_t R[32] = { 0 };
	// Inicializa a memórias
	uint32_t* MEM32 = (uint32_t*)(calloc(8, 1024));
	uint32_t valor;
	size_t index = 0;
	// Armazena o conteúdo do arquivo dentro da memória
	while (fscanf(input, "%x", &valor) != EOF && index < (MEM32_SIZE)) {
		MEM32[index++] = valor;
    }

	printf("[START OF SIMULATION]\n");
	PC = 0;
	uint8_t executa = 1;
	while(executa) {
		// Cadeia de caracteres da instrucao
		char instrucao[30] = { 0 };
		// Declarando operandos
		uint8_t z = 0, x = 0, y = 0, l = 0, i = 0;
		uint32_t pc = 0, xyl = 0;
		// Carregando a instrucao de 32 bits (4 bytes) da memoria indexada pelo PC (R29) no registrador IR (R28)
		R[28] = MEM32[R[29] >> 2];
		// Obtendo o codigo da operacao (6 bits mais significativos)
		/* A máscara é aplicada para isolar apenas os 6 dígitos que correspondem ao código da operação;
		É realizada uma operação and bit a bit, por meio do deslocamento em 26 bits;
		Isso faz com que os demais dígitos sejam zerados, e em seguida desloca-se em 26 casas os dígitos que sobraram;
		Os dígitos que sobraram são os dígitos equivalentes ao opcode. */
		// Decodificando a instrucao buscada na memoria
		uint8_t opcode = (R[28] & (0b111111 << 26)) >> 26; 

		// Implementação das instruções definidas na documentação
		
		switch(opcode) {

		// Instruções básicas

			// mov (sem sinal)
			case 0b000000: {
				// Obtendo operandos
				/*É aplicada uma máscara para isolar os 5 dígitos que estão no registrador 28
				A máscara é aplicada após deslocar 21 casas à direita, o que faz com que os primeiros dígitos
				que correspondem ao opcode não sejam afetados, e que os demais valores sejam zerados.
				O valor que sobrou é deslocado 21 casas à esquerda, mantendo apenas os valores que correspondem ao índice z. */
				z = (R[28] & (0b11111 << 21)) >> 21;
				xyl = R[28] & 0x1FFFFF;
				// Execucao do comportamento
				R[z] = xyl;
				// Formatacao da instrucao
				sprintf(instrucao, "mov r%u,%u", z, xyl);
				
				// Formatacao de saida no output
				// 0x%08x representa que o valor deve ser formatado em hexadecimal com 0 a esquerda,
				// e com ao menos 8 dígitos, e o X representa que as letras serão maiúsculas
				// \t insere um espaço de tabulacao
				// -25s indica que a string terá 25 caracteres alinhada a esquerda
				// R[29] indicará o registrador contador de programa (PC)
				// Cada instrução ocupa 4 bytes, ou seja, PC será acrescido de 4 em 4 no output

				//0x00000020:	mov r1,1193046           	R1=0x00123456
				fprintf(output, "0x%08X:\t%-25s\tR%u=0x%08X\n", R[29], instrucao, z, xyl);
				break;
			}
			// movs (com extensão de sinal) - Analisar como aplicar a extensão de sinal
			case 0b000001: {
				// Operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				xyl = R[28] & 0x1FFFFF;
				x = (R[28] & (0b11111 << 16)) >> 16;
				// Execucao do comportamento
				if (xyl & (1 << 20)) {
				    // Extensão de sinal para 32 bits
				    xyl |= 0xFFF00000; // Define os 12 bits mais significativos como 1
				}
				R[z] = xyl;
				// Formatacao da instrucao
				sprintf(instrucao, "movs r%u,%u", z, xyl);
				// Formatacao da saida no output
				// 0x00000024:	movs r2,-1048576         	R2=0xFFF00000
				fprintf(output, "0x%08X:\t%-25s\tR%u=0x%08X\n", R[29], instrucao, z, xyl);
				break;
			}
			// add 
			case 0b000010: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;
				// Executando a adicao
				R[z] = R[x] + R[y];
				//Condição para setar ZN caso os números sejam iguais
				// Valida se o valor armazenado em R[z] é 0
				if (R[z] == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (RZ31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				} if ((R[x] & (1 << 31)) == (R[y] & (1 << 31)) 
					&& ((R[z] & (1 << 31)) != (R[x] & (1 << 31)))) {
					SET_OV();
				} else {
					CLEAR_OV();
				} if ((R[z] & (1 << 31)) == 1) {
					SET_CY();
				} else {
					CLEAR_CY();
				}

				sprintf(instrucao, "add r%u,r%u,r%u", z, x, y);
				//Modelo: R3=R1+R2=0x00023456,SR=0x00000001
				fprintf(output, "0x%08X:\t%-25s\tR%u=R%u+R%u=0x%08X", R[29], instrucao, z, x, y, R[z]);
				break;
			} 

			// sub
			case 0b000011: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;
				// Executando a subtracao
				R[z] = R[x] - R[y];

				//Condição para setar ZN caso os números sejam iguais
				// Valida se o valor armazenado em R[z] é 0
				if (R[z] == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (RZ31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				} if ((R[x] & (1 << 31)) != (R[y] & (1 << 31)) 
					&& ((R[z] & (1 << 31)) != (R[x] & (1 << 31)))) {
					SET_OV();
				} else {
					CLEAR_OV();
				} if ((R[z] & (1 << 31)) == 1) {
					SET_CY();
				} else {
					CLEAR_CY();
				}

				sprintf(instrucao, "sub r%u,r%u,r%u", z, x, y);
				// 0x00000030:	sub r4,r2,r3             	R4=R2-R3=0x7FFDCBAA,SR=0x00000008
				fprintf(output, "0x%08X:\t%-25s\tR%u=R%u+R%u=0x%08X", R[29], instrucao, z, x, y, R[z]); 
				break;
			}

			// cmp 
			case 0b000101: {
				// Obtendo operandos
				x = (R[28] & (0b11111) << 16);
				y = (R[28] & (0b11111) << 11);
				uint32_t cmp = R[x] - R[y];

				if (cmp	== 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (CMP31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				} if ((RX31 != RY31) && (CMP31 != RX31)) {
					SET_OV();
				} else {
					CLEAR_OV();
				} if (CMP32 == 1) {
					SET_CY();
				} else {
					CLEAR_CY();
				}

				sprintf(instrucao, "cmp r%u, r%u", x, y);
				// 0x00000054:	cmp ir,pc  SR=0x00000020
				fprintf(output, "0x%08X:\t%-25s\tSR=0x%08X", R[29], instrucao, SR);
				break;
			}

		// Instrucoes especificas (opcode: 0b000100)

			case 000100: {
				// pegar o código da subfunção e implementar o controle
				uint8_t subf = (R[28] & (0b111 << 5)) >> 5;
				switch(subf) {
					// mul (sem sinal)
					case 0b000: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						//aplicando uma extensão para caso o resultado tenha 64 bits
						uint64_t mult = (uint64_t)R[x] * (uint64_t)R[y];

						/* Desloca os bits L em 31 posições, deixando-os nas posições mais significativas.*/
						// Validar essa concatenação e como tratar R[L]
						uint64_t lz = (R[l] << 31) | (R[z]);

						if (R[l] == 0) {
							SET_ZN();
						} else {
							CLEAR_ZN();
						} if ((R[l] & (0b11111 << 26)) != 0){
							SET_CY();
						} else {
							CLEAR_CY();
						}

						sprintf(instrucao, "mul r%u,r%u,r%u, r%u", l, z, x, y);
						//0x00000034:	mul r0,r5,r4,r3          	R0:R5=R4*R3=0x0000000023F4F31C,SR=0x00000008
						//fprintf(output, "0x%08X:\t%-25s\tR%u:R%u=R%u*R%u=0x%016X,SR=0x%08X\n", R[29], instrucao, l, z, x, y, lz, SR); 
						break;
					}
    				// muls (com sinal)
    				// precisa fazer os ajustes necessários
    				case 0b010: {
    					z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						int64_t mult = (uint64_t) R[x] * (uint64_t) R[y];
						int64_t lz = (R[l] << 31) | (R[z]);		
						
   						if (lz == 0) {
    						SET_ZN();
    					} else {
    						CLEAR_ZN();
    					} if (R[l] != 0) {
    						SET_OV();
    					} else {
    						CLEAR_OV();
    					}	

    					sprintf(instrucao, "muls r%u,r%u,r%u,r%u", l, z, x, y);
    					// Implementar o print
    					// 0x0000003C:	muls r0,r7,r6,r5         	R0:R7=R6*R5=0x0000000000000000,SR=0x00000040	
 						//fprintf(output, "0x%08X:\t%-25s\tR%u:R%u=R%u*R%u=0x%08X,SR=0x%08X\n", R[29], instrucao, l, z, x, y, lz, SR);    												
						break;
					}

					//sll (deslocamento para esquerda - lógico sem sinal)
					case 0b001: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						uint64_t concat = ((uint64_t) R[z] << 32) | R[y];
						uint64_t multi = concat * ((uint64_t) 1 << (l +1));
						R[z] = (multi >> 32) & 0xFFFFFFFF; // 32 bits superiores
    					R[x] = multi & 0xFFFFFFFF;         // 32 bits inferiores

    					uint64_t zx = ((uint64_t) R[z] << 32 | R[x]);

    					if (zx == 0) {
    						SET_ZN();
    					} else {
    						CLEAR_ZN();
    					} if (R[z] != 0) {
    						SET_CY();
    					} else {
    						CLEAR_CY();
    					}

    					//Implementar o print
    					sprintf(instrucao, "sll r%u,r%u,r%u,r%u", z, x, z, l);
    					// 0x00000038:	sll r6,r5,r5,0           	R6:R5=R6:R5<<1=0x0000000047E9E638,SR=0x00000008
    					break;
    				}

    				// sla (deslocamento para esquerda - aritmético com sinal)
    				// precisa fazer os ajustes necessários
    				case 0b011: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						/* Implementar o comportamento

    					if (zx == 0) {
    						SET_ZN();
    					} else {
    						CLEAR_ZN();
    					} if (R[z] != 0) {
    						SET_OV();
    					} else {
    						CLEAR_OV();
    					}

    					// Implementar o print
    					sprintf(instrucao, "sll r%u,r%u,r%u,r%u", z, x, z, l);
    					// 0x00000040:	sla r8,r7,r7,1           	R8:R7=R8:R7<<2=0x0000000000000000,SR=0x00000040 */
    					break;
    				}
					// div (sem sinal)
    				case 0b100: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						R[z] = R[x] / R[y];
						// Salvando o resto da divisão em R[l]
						R[l] = R[x] % R[y];

						if (R[z] == 0) {
							SET_ZN();
						} else {
							CLEAR_ZN();
						}
						if (R[y] == 0) {
							SET_ZD();
						} else {
							CLEAR_ZD();
						}
						if (R[l] != 0) {
							SET_CY();
						} else {
							CLEAR_CY();
						}

						sprintf(instrucao, "div r%u,r%u,r%u,r%u", l, z, x, y);
						//0x00000044:	div r0,r9,r8,r7          	R0=R8%R7=0x00000000,R9=R8/R7=0x00000000,SR=0x00000060
						fprintf(output, "0x%08X,\t%-25s\tR%u=R%uR%u=0x%08X,R%u=R%u/R%u=0x%08X,SR=0x%08X", R[29], instrucao, l, x, y, R[l], z, x, y, R[z], SR);
    					break;
    				}
					// srl (deslocamento para direita - lógico sem sinal)
    				case 0b101: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						uint64_t concat = ((uint64_t) R[z] << 32) | R[y];
						uint64_t div = concat / ((uint64_t) 1 << (l +1));
						R[z] = (div >> 32) & 0xFFFFFFFF; // 32 bits superiores
    					R[x] = div & 0xFFFFFFFF;         // 32 bits inferiores

    					uint64_t zx = ((uint64_t) R[z] << 32 | R[x]);

    					if (zx == 0) {
    						SET_ZN();
    					} else {
    						CLEAR_ZN();
    					} if (R[z] != 0) {
    						SET_CY();
    					} else {
    						CLEAR_CY();
    					}

    					//Implementar o print
    					sprintf(instrucao, "sll r%u,r%u,r%u,r%u", z, x, z, l);
    					// 0x00000048:	srl r10,r9,r9,2          	R10:R9=R10:R9>>3=0x0000000000000000,SR=0x00000060
    					break;
    				}
					// divs (com sinal)
					// precisa fazer os ajustes necessários 
    				case 0b110: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						/* Implementar o comportamento 

						if (R[z] == 0) {
							SET_ZN();
						} else {
							CLEAR_ZN();
						}
						if (R[y] == 0) {
							SET_ZD();
						} else {
							CLEAR_ZD();
						}
						if (R[l] != 0) {
							SET_CY();
						} else {
							CLEAR_CY();
						}

						sprintf(instrucao, "div r%u,r%u,r%u,r%u", l, z, x, y);
						//0x00000044:	div r0,r9,r8,r7          	R0=R8%R7=0x00000000,R9=R8/R7=0x00000000,SR=0x00000060
						fprintf(output, "0x%08X,\t-25s\tR%u=R%u%R%u=0x%08X,R%u=R%u/R%u=0x%08X,SR=0x%08X", R[29], instrucao, l, x, y, , R[l], z, x, y, , R[z], SR);    					
    					*/ 
    					break;
    				}
					// sra (deslocamento para direita - artimético com sinal)
					// precisa fazer os ajustes necessários
    				case 0b111: {
						z = (R[28] & (0b11111 << 21)) >> 21;
						x = (R[28] & (0b11111 << 16)) >> 16;
						y = (R[28] & (0b11111 << 11)) >> 11;
						l = (R[28] & (0b11111));

						/* Impĺementar o comportamento

    					if (zx == 0) {
    						SET_ZN();
    					} else {
    						CLEAR_ZN();
    					} if (R[z] != 0) {
    						SET_OV();
    					} else {
    						CLEAR_OV();
    					}

    					// Implementar o print
    					sprintf(instrucao, "sll r%u,r%u,r%u,r%u", z, x, z, l);
    					*/
    					break;
    				}
				} // fim das subfunções
				break;
			} // fim do case

		// Instruções bit a bit

			// and
			case 0b000110: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				y = (R[28] & (0b11111 << 11)) >> 11;	

				R[z] = R[x] & R[y];

				if (R[z] == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (RZ31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				}

				// Implementar a impressão
				sprintf(instrucao, "and r%u,r%u,r%u", z, x, y);
				// 0x00000058:	and r13,r1,r5            	R13=R1&R5=0x00002410,SR=0x00000020
				fprintf(output, "0x%08X\t%-25s\tR%u=R%u&R%u=0x%08X,SR=0x%08X",R[29], instrucao, z, x, y, R[z], SR);
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
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (RZ31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				}	

				// Implementar a impressão
				sprintf(instrucao, "or r%u,r%u,r%u", z, x, y);
				// 0x0000005C:	or r14,r2,r6             	R14=R2|R6=0x80000000,SR=0x00000030
				fprintf(output, "0x%08X\t%-25s\tR%u=R%u|R%u=0x%08X,SR=0x%08X", R[29], instrucao, z, x, y, R[z], SR);
				break;
			}
			// not
			case 0b001000: {
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;	

				// Implementando o comportamento
				R[z] = ~R[x];
				
				if (R[z] == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (RZ31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				}	

				// Implementar a impressão 
				sprintf(instrucao, "not r%u,r%u", z, x);
				// 0x00000060:	not r15,r7               	R15=~R7=0xFFFFFFFF,SR=0x00000030
				fprintf(output, "0x%08X\t%-25s\tR%u=~R%u=0x%08X,SR=0x%08X", R[29], instrucao, z, x, R[z], SR);				
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
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (RZ31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				}

				// Implementar a impressão 
				sprintf(instrucao, "xor r%u,r%u,r%u", z, x, y);
				// 0x00000064:	xor r16,r16,r8           	R16=R16^R8=0x00000000,SR=0x00000060
				fprintf(output, "0x%08X\t%-25s\tR%u=R%u^R%u=0x%08X,SR=0x%08X", R[29], instrucao, z, x, y, R[z], SR);					
				break;
			}					
				//formatar a impressão

		// Instruções imediatas		

			// addi
			// ajustar as condições de setar e limpar
			case 0b010010: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				// Essa validação analisa se o bit 15 de i é igual a 1 ou 0
				// Se for igual a 1, ele repete esse valor 16 vezes
				// Se for igual a 0, repete o 0 por 16 vezes
				uint32_t bit15 = I15 ? 0xFFFF : 0x0000;
				uint32_t concat_i = ((bit15 << 16) | i);

				R[z] = R[x] + concat_i;

				if (R[z] == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				}; if (RZ31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				}; if ((RX31 == I15) && (RZ31 != RX31)) {
					SET_OV();
				} else {
					CLEAR_OV();
				}; /*if (RZ32 == 1) {
					SET_CY();
				} else {
					CLEAR_CY();
				};*/

				// Implementar a impressão 
				sprintf(instrucao, "addi, r%u, r%u, %d", z, x, i);
				// 0x00000068:	addi r17,r17,1           	R17=R17+0x00000001=0x00000001,SR=0x00000020
				fprintf(output, "0x%08X\t%-25s\tR%u=R%u+0x%08X=0x%08X,SR=0x%08X", R[29], instrucao, z, x, i, R[z], SR);
				break;
			}
			// subi
			// ajustar as condições de setar e limpar			
			case 0b010011: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				// Essa validação analisa se o bit 15 de i é igual a 1 ou 0
				// Se for igual a 1, ele repete esse valor 16 vezes
				// Se for igual a 0, repete o 0 por 16 vezes
				uint32_t bit15 = I15 ? 0xFFFF : 0x0000;
				uint32_t concat_i = ((bit15 << 16) | i);

				R[z] = R[x] - concat_i;

				if (R[z] == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} 

				if (RZ31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				} 

				if ((RX31 != I15) && (RZ31 != RX31)) {
					SET_OV();
				} else {
					CLEAR_OV();
				} 

				/*if (RZ32 == 1) {
					SET_CY();
				} else {
					CLEAR_CY();
				}*/

				// Implementar a impressão 
				sprintf(instrucao, "subi, r%u, r%u, %d", z, x, i);
				// 0x0000006C:	subi r18,r18,-1          	R18=R18-0xFFFFFFFF=0x00000001,SR=0x00000020
				fprintf(output, "0x%08X\t%-25s\tR%u=R%u-0x%08X=0x%08X,SR=0x%08X", R[29], instrucao, z, x, i, R[z], SR);
				break;
			}
			// muli (com sinal)
			// ajustar por ser com sinal
			case 0b010100: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				// Essa validação analisa se o bit 15 de i é igual a 1 ou 0
				// Se for igual a 1, ele repete esse valor 16 vezes
				// Se for igual a 0, repete o 0 por 16 vezes
				uint32_t bit15 = I15 ? 0xFFFF : 0x0000;
				uint32_t concat_i = ((bit15 << 16) | i);

				R[z] = R[x] * concat_i;

				if (R[z] == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (i == 0) {
					SET_ZD();
				} else {
					CLEAR_ZD();
				} if (0) {
					SET_OV();
				} else {
					CLEAR_OV();
				}

				// Implementar a impressão 
				sprintf(instrucao, "muli, r%u, r%u, %d", z, x, i);
				// 0x00000070:	muli r19,r17,2           	R19=R17*0x00000002=0x00000002,SR=0x00000020
				fprintf(output, "0x%08X\t%-25s\tR%u=R%u*0x%08X=0x%08X,SR=0x%08X", R[29], instrucao, z, x, i, R[z], SR);
				break;
			}

			// divi (com sinal)
			// ajustar por ser com sinal
			case 0b010101: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				// Essa validação analisa se o bit 15 de i é igual a 1 ou 0
				// Se for igual a 1, ele repete esse valor 16 vezes
				// Se for igual a 0, repete o 0 por 16 vezes
				uint32_t bit15 = I15 ? 0xFFFF : 0x0000;
				uint32_t concat_i = ((bit15 << 16) | i);

				R[z] = R[x] / concat_i;

				if (R[z] == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (i == 0) {
					SET_ZD();
				} else {
					CLEAR_ZD();
				} if (0) {
					SET_OV();
				} else {
					CLEAR_OV();
				}

				// Implementar a impressão 
				sprintf(instrucao, "divi, r%u, r%u, %d", z, x, i);
				// 0x00000074:	divi r20,r19,2           	R20=R19/0x00000002=0x00000001,SR=0x00000000
				fprintf(output, "0x%08X\t%-25s\tR%u=R%u/0x%08X=0x%08X,SR=0x%08X", R[29], instrucao, z, x, i, R[z], SR);
				break;
			}

			// modi (com sinal)
			// ajustar por ser com sinal
			case 0b010110: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				// Essa validação analisa se o bit 15 de i é igual a 1 ou 0
				// Se for igual a 1, ele repete esse valor 16 vezes
				// Se for igual a 0, repete o 0 por 16 vezes
				uint32_t bit15 = I15 ? 0xFFFF : 0x0000;
				uint32_t concat_i = ((bit15 << 16) | i);

				R[z] = R[x] % concat_i;

				if (R[z] == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (i == 0) {
					SET_ZD();
				} else {
					CLEAR_ZD();
				} if (0) {
					SET_OV();
				} else {
					CLEAR_OV();
				}

				// Implementar a impressão 
				sprintf(instrucao, "modi, r%u, r%u, %d", z, x, i);
				// 0x00000078:	modi r21,r19,3           	R21=R19%0x00000003=0x00000002,SR=0x00000000
				fprintf(output, "0x%08X\t%-25s\tR%u=R%u%%0x%08X=0x%08X,SR=0x%08X", R[29], instrucao, z, x, i, R[z], SR);
				break;
			} 

			// compi
			case 0b010111: {
				// Obtendo operandos
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = (R[28] & 0xFFFF);

				// Implementar o comportamento
				// Essa validação analisa se o bit 15 de i é igual a 1 ou 0
				// Se for igual a 1, ele repete esse valor 16 vezes
				// Se for igual a 0, repete o 0 por 16 vezes
				uint32_t bit15 = I15 ? 0xFFFF : 0x0000;
				uint32_t concat_i = ((bit15 << 16) | i);

				uint32_t cmpi = R[x] - concat_i;

				if (cmpi == 0) {
					SET_ZN();
				} else {
					CLEAR_ZN();
				} if (CMPI31 == 1) {
					SET_SN();
				} else {
					CLEAR_SN();
				} if ((RX31 != I15) && (CMPI31 != RX31)) {
					SET_OV();
				} else {
					CLEAR_OV();
				} if (CMPI32 == 1) {
					SET_CY();
				} else {
					CLEAR_CY();
				}

				// Implementar a impressão 
				sprintf(instrucao, "cmpi, r%u, %d", x, i);
				// 0x0000007C:	cmpi r21,32              	SR=0x00000011
				fprintf(output, "0x%08X\t%-25s\tSR=0x%08X", R[29], instrucao, SR);				

				break;
			}

				// formatar a impressão										

		// Instruções de leitura/escrita da memória
			
			// l8
			case 0b011000: { 
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = R[28] & 0xFFFF;
				// Execucao do comportamento com MEM32 (calculo do indice da palavra e selecao do byte big-endian)
				R[z] = (((uint8_t*)(&MEM32[(R[x] + i) >> 2]))[3 - ((R[x] + i) % 4)]);
				// Formatacao da instrucao
				fprintf(output, instrucao, "l8 r%u,[r%u%s%i]", z, x, (i >= 0) ? ("+") : (""), i);
				// 0x00000080:	l8 r22,[r0+35]           	R22=MEM[0x00000023]=0x56
				fprintf(output, "0x%08X:\t%-25s\tR%u=MEM[0x%08X]=0x%02X\n", R[29], instrucao, z, R[x] + i, R[z]);
				break;
			}
			// l16
			case 0b011001: {
				// Obtendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = R[28] & 0xFFFF;
				// Implementar o comportamento

				// Implementar a impressão
				fprintf(output, instrucao, "l16 r%u,[r%u%s%i]", z, x, (i >= 0) ? ("+") : (""), i);
				// 0x00000084:	l16 r23,[r0+17]          	R23=MEM[0x00000022]=0x3456
				fprintf(output, "0x%08X:\t%-25s\tR%u=MEM[0x%08X]=0x%02X\n", R[29], instrucao, z, R[x] + i, R[z]);
				break;
				break;
			}
			// l32
			case 0b011010: {
				// Otendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = R[28] & 0xFFFF;
				// Implementar o comportamento
				R[z] = MEM32[R[x] + i];
				// Implementar a impressão
				fprintf(output, instrucao, "l32 r%u,[r%u%s%i]", z, x, (i >= 0) ? ("+") : (""), i);
				// 0x00000088:	l32 r24,[r0+8]           	R24=MEM[0x00000020]=0x00323456
				fprintf(output, "0x%08X:\t%-25s\tR%u=MEM[0x%08X]=0x%08X\n", R[29], instrucao, z, (R[x] + i) << 2, R[z]);
				break;
			}
			// s8
			case 0b011011: {
				// Otendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = R[28] & 0xFFFF;
				// Implementar o comportamento

				// Implementar a impressão
				fprintf(output, instrucao, "l32 r%u,[r%u%s%i]", z, x, (i >= 0) ? ("+") : (""), i);
				// 0x0000008C:	s8 [r0+35],r22           	MEM[0x00000023]=R22=0x56
				fprintf(output, "0x%08X:\t%-25s\tR%u=MEM[0x%08X]=0x%08X\n", R[29], instrucao, z, (R[x] + i) << 2, R[z]);
				break;				
			}
			// s16
			case 0b011100: {
				// Otendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = R[28] & 0xFFFF;
				// Implementar o comportamento

				// Implementar a impressão
				fprintf(output, instrucao, "l32 r%u,[r%u%s%i]", z, x, (i >= 0) ? ("+") : (""), i);
				// 0x00000090:	s16 [r0+17],r23          	MEM[0x00000022]=R23=0x3456
				fprintf(output, "0x%08X:\t%-25s\tR%u=MEM[0x%08X]=0x%08X\n", R[29], instrucao, z, (R[x] + i) << 2, R[z]);
				break;
			}
			// s32
			case 0b011101: {
				// Otendo operandos
				z = (R[28] & (0b11111 << 21)) >> 21;
				x = (R[28] & (0b11111 << 16)) >> 16;
				i = R[28] & 0xFFFF;
				// Implementar o comportamento

				// Implementar a impressão
				fprintf(output, instrucao, "l32 r%u,[r%u%s%i]", z, x, (i >= 0) ? ("+") : (""), i);
				// 0x00000094:	s32 [r0+8],r24           	MEM[0x00000020]=R24=0x00323456
				fprintf(output, "0x%08X:\t%-25s\tR%u=MEM[0x%08X]=0x%08X\n", R[29], instrucao, z, (R[x] + i) << 2, R[z]);				
				break;
			}

		// Instruções de controle de fluxo

			// bae (desvio incondicional: AE - sem sinal)
			case 0b101010: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A ≥ B → A − B ≥ 0 ≡ AE ← CY = 0

				// Implementar a impressão 
				sprintf(instrucao, "bae %u",i);
				// 0x00000098:	bae 0                    	PC=0x0000009C
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);
				break;
			}
			// bat (desvio condicional: AT - sem sinal)
			case 0b101011: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A > B → A − B > 0 ≡ AT ← (ZN = 0 ∧ CY = 0)

				// Implementar a impressão 
				sprintf(instrucao, "bat %u",i);
				// 0x0000009C:	bat 0                    	PC=0x000000A0
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);				
				break;
			}
			// bbe (desvio condicional: BE - sem sinal)
			case 0b101100: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A ≤ B → A − B ≤ 0 ≡ BE ← (ZN = 1 ∨ CY = 1)

				// Implementar a impressão 
				sprintf(instrucao, "bbe %u",i);
				// 0x000000A0:	bbe 0                    	PC=0x000000A4
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);				
				break;
			}
			// bbt (desvio condicional: BT - sem sinal)
			case 0b101101: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A < B → A − B < 0 ≡ BT ← CY = 1

				// Implementar a impressão 
				sprintf(instrucao, "bbt %u",i);
				// 0x000000A4:	bbt 0                    	PC=0x000000A8
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);					
				break;
			}
			// beq (desvio condicional: EQ)				
			case 0b101110: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A = B → A − B = 0 ≡ EQ ← ZN = 1

				// Implementar a impressão 
				sprintf(instrucao, "beq %u",i);
				// 0x000000A8:	beq 0                    	PC=0x000000AC
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);						
				break;
			}
			// bge (desvio condicional: GE - com sinal)
			case 0b101111: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A ≥ B → A − B ≥ 0 ≡ GE ← SN = OV

				// Implementar a impressão 
				sprintf(instrucao, "bge %u",i);
				// 0x000000AC:	bge 0                    	PC=0x000000B0
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);						
				break;
			}
			// bgt (desvio condicional: GT - com sinal)
			case 0b110000: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A > B → A − B > 0 ≡ GT ← (ZN = 0 ∧ SN = OV )

				// Implementar a impressão 
				sprintf(instrucao, "bgt %u",i);
				// 0x000000B0:	bgt 0                    	PC=0x000000B4
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);
				break;
			}
			// biv (desvio condicional: IV)
			case 0b110001: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);

				// Implementar a impressão 
				sprintf(instrucao, "biv %u",i);
				// 0x000000B4:	biv 0                    	PC=0x000000B8
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);
				break;
			}
			// ble (desvio condicional: LE - com sinal)
			case 0b110010: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A ≤ B → A − B ≤ 0 ≡ LE ← (ZN = 1 ∨ SN != OV )

				// Implementar a impressão 
				sprintf(instrucao, "ble %u",i);
				// 0x000000B8:	ble 0                    	PC=0x000000BC
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);				
				break;
			}
			// blt (desvio condicional: LT - com sinal)
			case 0b110011: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A < B → A − B < 0 ≡ LT ← SN != OV

				// Implementar a impressão 
				sprintf(instrucao, "blt %u",i);
				// 0x000000BC:	blt 0                    	PC=0x000000C0
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);				
				break;
			}
			// bne (desvio condicional: NE)
			case 0b110100: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// A != B → A − B != 0 ≡ NE ← ZN = 0

				// Implementar a impressão 
				sprintf(instrucao, "bne %u",i);
				// 0x000000C0:	bne 0                    	PC=0x000000C4
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);	
				break;
			}
			// bni (desvio condicional: NI)
			case 0b110101: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// NI ← IV = 0

				// Implementar a impressão 
				sprintf(instrucao, "bni %u",i);
				// 0x000000C4:	bni 0                    	PC=0x000000C8
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);					
				break;
			}
			// bnz (desvio condicional: NZ)
			case 0b110110: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;

				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Condições para setar:
				// NZ ← ZD = 0

				// Implementar a impressão 
				sprintf(instrucao, "bnz %u",i);
				// 0x000000C8:	bnz 0                    	PC=0x000000CC
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);					
				break;
			}
			// bun
			case 0b110111: {
				// Armazenando o PC antigo
				pc = R[29];
				// Implementar o comportamento
				R[29] = R[29] + ((R[28] & 0x3FFFFFF) << 2);
				// Implementar a impressão
				fprintf(output, instrucao, "bun %i", R[28] & 0x3FFFFFF);
				// 0x000000CC:	bun 0                    	PC=0x000000D0a
				fprintf(output, "0x%08X:\t%-25s\tPC=0x%08X\n", pc, instrucao, R[29] + 4);
				break;
			}
			// bzd
			case 0b111000: {
				// Obtendo operandos
				i = R[28] & 0x3FFFFFF;
				// Implementar o comportamento
				PC = PC + 4 + (i << 2);
				// Implementar a impressão 
				sprintf(instrucao, "bzd %u",i);
				// 0x000000D0:	bzd 0                    	PC=0x000000D4
				fprintf(output, "0x%08X\t%-25s\tPC=0xx%08X", R[29], instrucao, PC);		
				break;
			}
			// int
			case 0b111111: {
				// Parar a execucao
				executa = 0;
				// Implementar a impressão
				fprintf(output, instrucao, "int 0");
				// 0x000000D4:	int 0                    	CR=0x00000000,PC=0x00000000
				fprintf(output, "0x%08X:\t%-25s\tCR=0x00000000,PC=0x00000000\n", R[29], instrucao);
				break;
			}
		// Instrucao desconhecida
			default: {
				// Exibindo mensagem de erro
				printf("Instrucao desconhecida!\n");
				// Parar a execucao
				executa = 0;
			}
		}
		PC = PC + 4; //(proxima instrucao)
		R[29] = R[29] + 4;
	}
	// Exibindo a finalizacao da execucao
	printf("[END OF SIMULATION]\n");
	// Fechando os arquivos
	fclose(input);
	fclose(output);
	// Liberando a memória alocada 
	free(MEM32);
	// Finalizando programa
	return 0;
}