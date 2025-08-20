#include <Keypad.h>

const byte LINHAS = 4;
const byte COLUNAS = 4;

char teclas[LINHAS][COLUNAS] = {
  {'1','2','3','4'},
  {'5','6','7','8'},
  {'9','A','B','C'},
  {'D','E','F','G'}
};

byte pinosLinhas[LINHAS] = {27, 14, 12, 13};  // R1 → R4
byte pinosColunas[COLUNAS] = {26, 25, 33, 32}; // C1 → C4

Keypad teclado = Keypad(makeKeymap(teclas), pinosLinhas, pinosColunas, LINHAS, COLUNAS);

// Nomes das teclas (S1–S16)
const char* nomesTeclas[16] = {
  "S1", "S2", "S3", "S4",
  "S5", "S6", "S7", "S8",
  "S9", "S10", "S11", "S12",
  "S13", "S14", "S15", "S16"
};

// Variáveis para detectar o tempo de pressionamento
char teclaAtual = NO_KEY;
unsigned long tempoInicio = 0;
bool pressionando = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Leitor de Teclado com clique curto e longo");
}

void loop() {
  char tecla = teclado.getKey();

  if (tecla != NO_KEY) {
    // Nova tecla pressionada
    teclaAtual = tecla;
    tempoInicio = millis();
    pressionando = true;
  }

  if (pressionando && teclado.getState() == HOLD) {
    // Exibir barra de progresso no Serial
    unsigned long duracao = millis() - tempoInicio;
    int progresso = map(duracao, 0, 2000, 0, 20); // 2 segundos = barra cheia
    if (progresso > 20) progresso = 20;
    Serial.print("\r[");
    for (int i = 0; i < 20; i++) {
      if (i < progresso) Serial.print("#");
      else Serial.print(" ");
    }
    Serial.print("] ");
    Serial.print(duracao / 1000.0, 2);
    Serial.print("s   "); // Espaço extra para limpar resíduos
    Serial.flush();
    delay(100); // Atualiza a barra a cada 100ms
  }

  if (pressionando && teclado.getState() == RELEASED) {
    // Apaga a barra de progresso
    Serial.print("\r");
    for (int i = 0; i < 30; i++) Serial.print(" "); // Limpa a linha
    Serial.print("\r");
    pressionando = false;
    unsigned long duracao = millis() - tempoInicio;

    int index = mapearTecla(teclaAtual);

    if (index != -1) {
      Serial.println(); // Garante que a mensagem fique em nova linha
      Serial.print("Tecla ");
      Serial.print(nomesTeclas[index]);

      if (duracao >= 1000) {
        Serial.println(" → Clique longo");
      } else {
        Serial.println(" → Clique curto");
      }
    }

    teclaAtual = NO_KEY;
  }
}

// Função auxiliar para mapear a tecla para o índice S1–S16
int mapearTecla(char tecla) {
  switch(tecla) {
    case '1': return 0;
    case '2': return 1;
    case '3': return 2;
    case '4': return 3;
    case '5': return 4;
    case '6': return 5;
    case '7': return 6;
    case '8': return 7;
    case '9': return 8;
    case 'A': return 9;
    case 'B': return 10;
    case 'C': return 11;
    case 'D': return 12;
    case 'E': return 13;
    case 'F': return 14;
    case 'G': return 15;
    default: return -1;
  }
}
