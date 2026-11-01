#include <IRremote.hpp>

const uint8_t CMD_BOTAO_0 = 0x19;
const uint8_t CMD_BOTAO_1 = 0x46;
const uint8_t CMD_BOTAO_2 = 0x45;
const uint8_t CMD_BOTAO_4 = 0x44;
const uint8_t CMD_BOTAO_6 = 0x43; // Botão 6 mapeado para a estratégia combinada em looping
const uint8_t CMD_BOTAO_OK = 0x1C;

const int SENS_DETECTADO = LOW;
const int BORDA_DETECTADA = LOW;

// --- TEMPOS OTIMIZADOS PARA DOJO DE AÇO ---
const unsigned long TEMPO_ESPERA_START = 5000; // 5 segundos de segurança obrigatórios[cite: 1]
const unsigned long TEMPO_TRECHO_CURTO = 150;
const unsigned long TEMPO_GIRO_DIR = 120;
const unsigned long TEMPO_GIRO_ESQ = 120;
const unsigned long TEMPO_ARCO = 250;         
const unsigned long TEMPO_ESQUIVA_RECUO = 220; 
const unsigned long TEMPO_PERSISTENCIA = 300;
const unsigned long TEMPO_VARREDURA = 500;
const unsigned long TEMPO_RECUO_BORDA = 180;
const unsigned long TEMPO_GIRO_BORDA = 200;
const unsigned long TEMPO_SAIDA_BORDA = 180;

// Tempos específicos para os passos da estratégia combinada do Botão 6
const unsigned long TEMPO_PASSO_FRENTE_6 = 300;
const unsigned long TEMPO_PASSO_GIRO_6 = 200;

// Proteção para evitar chaveamento frenético dos relés
const unsigned long TEMPO_MIN_TROCA_ESTADO = 180; 
const unsigned long TEMPO_MIN_ESTADO_FIXO = 120;
const unsigned long TEMPO_MIN_MUDANCA_PED = 50;

// Pinagem mantida exatamente como no original
const int PINO_IR = 12;
const int SOFD = 3;
const int SLE = A1;
const int SLD = A2;
const int CHAVE_START = 4;
const int SFD = 5;
const int SFE = 6;
const int MDF = 8;
const int MDT = 9;
const int MEF = 10;
const int MET = 11;
const int BORDA_LED = A0;
const int OPONENTE_LED = 13;

enum EstadoMotor { PARADO, FRENTE, TRAS, GIRO_ESQ, GIRO_DIR };
enum Estrategia { NENHUMA = -1, ATAQUE_DIRETO = 0, ATAQUE_CIRCULAR = 1, ATAQUE_ESPERA_ESQ = 2, ATAQUE_ESPERA_DIR = 4, COMBINADA_BOTAO_6 = 6 };

EstadoMotor estadoAtual = PARADO;
EstadoMotor estadoPedido = PARADO;
Estrategia estrategiaAtual = NENHUMA;

bool roboLigado = false;
bool esperandoStart = false;
bool executandoSequencia = false;

int passoSequencia = 0;
int estadoBorda = 0;
int ladoBorda = 0;

unsigned long tempoInicioStart = 0;
unsigned long tempoInicioPasso = 0;
unsigned long tempoBorda = 0;
unsigned long tempoUltimoIR = 0;
unsigned long tempoUltimoAtaqueFrontal = 0;
unsigned long tempoUltimaVarredura = 0;
unsigned long tempoUltimaTrocaMotor = 0;
unsigned long tempoUltimoEstadoFixo = 0;
unsigned long tempoMudancaPedido = 0;

bool sentidoVarredura = false;

void escreverMotor(EstadoMotor novoEstado) {
  switch (novoEstado) {
    case FRENTE:
      digitalWrite(MEF, LOW); digitalWrite(MET, HIGH);
      digitalWrite(MDF, LOW); digitalWrite(MDT, HIGH);
      break;

    case TRAS:
      digitalWrite(MEF, HIGH); digitalWrite(MET, LOW);
      digitalWrite(MDF, HIGH); digitalWrite(MDT, LOW);
      break;

    case GIRO_ESQ:
      digitalWrite(MEF, HIGH); digitalWrite(MET, HIGH);
      digitalWrite(MDF, LOW); digitalWrite(MDT, HIGH);
      break;

    case GIRO_DIR:
      digitalWrite(MEF, LOW); digitalWrite(MET, HIGH);
      digitalWrite(MDF, HIGH); digitalWrite(MDT, HIGH);
      break;

    case PARADO:
    default:
      digitalWrite(MEF, HIGH); digitalWrite(MET, HIGH);
      digitalWrite(MDF, HIGH); digitalWrite(MDT, HIGH);
      break;
  }
}

void aplicarEstado(EstadoMotor novoEstado) {
  estadoAtual = novoEstado;
  escreverMotor(novoEstado);
  tempoUltimaTrocaMotor = millis();
  tempoUltimoEstadoFixo = millis();
}

void pararMotores() {
  escreverMotor(PARADO);
  estadoAtual = PARADO;
  estadoPedido = PARADO;
  tempoUltimaTrocaMotor = millis();
  tempoUltimoEstadoFixo = millis();
}

bool ehInversao(EstadoMotor a, EstadoMotor b) {
  return (a == FRENTE && b == TRAS) || (a == TRAS && b == FRENTE);
}

void solicitarEstado(EstadoMotor novoEstado) {
  if (novoEstado != estadoPedido) {
    estadoPedido = novoEstado;
    tempoMudancaPedido = millis();
  }
}

void atualizarMotor() {
  unsigned long agora = millis();

  if (estadoAtual == estadoPedido) return;
  if (agora - tempoMudancaPedido < TEMPO_MIN_MUDANCA_PED) return;
  if (agora - tempoUltimaTrocaMotor < TEMPO_MIN_TROCA_ESTADO) return;
  if (agora - tempoUltimoEstadoFixo < TEMPO_MIN_ESTADO_FIXO) return;

  if (ehInversao(estadoAtual, estadoPedido)) {
    pararMotores();
    return;
  }

  aplicarEstado(estadoPedido);
}

void resetRobo() {
  roboLigado = false;
  esperandoStart = false;
  executandoSequencia = false;
  estrategiaAtual = NENHUMA;
  passoSequencia = 0;
  estadoBorda = 0;
  ladoBorda = 0;
  tempoUltimoAtaqueFrontal = 0;
  tempoUltimaVarredura = 0;
  sentidoVarredura = false;
  digitalWrite(BORDA_LED, LOW);
  digitalWrite(OPONENTE_LED, LOW);
  pararMotores();
}

void iniciarEstrategia(Estrategia est) {
  estrategiaAtual = est;
  roboLigado = true;
  esperandoStart = true;
  executandoSequencia = false;
  passoSequencia = 0;
  estadoBorda = 0;
  ladoBorda = 0;
  tempoInicioStart = millis();
  tempoUltimoAtaqueFrontal = 0;
  tempoUltimaVarredura = millis();
  digitalWrite(BORDA_LED, LOW);
  digitalWrite(OPONENTE_LED, LOW);
  pararMotores();
}

bool bordaEsquerda() { return digitalRead(SFE) == BORDA_DETECTADA; }
bool bordaDireita() { return digitalRead(SFD) == BORDA_DETECTADA; }
bool bordaDetectada() { return bordaEsquerda() || bordaDireita(); }

void iniciarFugaBorda() {
  if (bordaEsquerda() && bordaDireita()) ladoBorda = 0;
  else if (bordaEsquerda()) ladoBorda = -1;
  else ladoBorda = 1;

  estadoBorda = 1;
  tempoBorda = millis();
  executandoSequencia = false;
  digitalWrite(BORDA_LED, HIGH);
  solicitarEstado(TRAS);
}

void tratarFugaBorda() {
  unsigned long dt = millis() - tempoBorda;

  if (estadoBorda == 1) {
    solicitarEstado(TRAS);
    if (dt >= TEMPO_RECUO_BORDA) {
      estadoBorda = 2;
      tempoBorda = millis();
      solicitarEstado(PARADO);
    }
    return;
  }

  if (estadoBorda == 2) {
    solicitarEstado(ladoBorda == -1 ? GIRO_DIR : GIRO_ESQ);
    if (dt >= TEMPO_GIRO_BORDA) {
      estadoBorda = 3;
      tempoBorda = millis();
      solicitarEstado(PARADO);
    }
    return;
  }

  if (estadoBorda == 3) {
    solicitarEstado(FRENTE);
    if (dt >= TEMPO_SAIDA_BORDA) {
      estadoBorda = 0;
      ladoBorda = 0;
      digitalWrite(BORDA_LED, LOW);
    }
    return;
  }
}

void combateAgressivo() {
  bool frontal = (digitalRead(SOFD) == SENS_DETECTADO);
  bool esq = (digitalRead(SLE) == SENS_DETECTADO);
  bool dir = (digitalRead(SLD) == SENS_DETECTADO);

  if (frontal) {
    tempoUltimoAtaqueFrontal = millis();
    digitalWrite(OPONENTE_LED, HIGH);
    solicitarEstado(FRENTE);
    return;
  }

  if (millis() - tempoUltimoAtaqueFrontal < TEMPO_PERSISTENCIA) {
    digitalWrite(OPONENTE_LED, HIGH);
    solicitarEstado(FRENTE);
    return;
  }

  if (esq && !dir) {
    digitalWrite(OPONENTE_LED, HIGH);
    solicitarEstado(GIRO_ESQ);
    return;
  }

  if (dir && !esq) {
    digitalWrite(OPONENTE_LED, HIGH);
    solicitarEstado(GIRO_DIR);
    return;
  }

  if (esq && dir) {
    digitalWrite(OPONENTE_LED, HIGH);
    solicitarEstado(FRENTE);
    return;
  }

  if (millis() - tempoUltimaVarredura >= TEMPO_VARREDURA) {
    tempoUltimaVarredura = millis();
    sentidoVarredura = !sentidoVarredura;
  }

  solicitarEstado(sentidoVarredura ? GIRO_ESQ : GIRO_DIR);
  digitalWrite(OPONENTE_LED, LOW);
}

void setup() {
  pinMode(MEF, OUTPUT);
  pinMode(MET, OUTPUT);
  pinMode(MDF, OUTPUT);
  pinMode(MDT, OUTPUT);

  pinMode(BORDA_LED, OUTPUT);
  pinMode(OPONENTE_LED, OUTPUT);

  pinMode(SOFD, INPUT_PULLUP);
  pinMode(SLE, INPUT_PULLUP);
  pinMode(SLD, INPUT_PULLUP);
  pinMode(SFD, INPUT_PULLUP);
  pinMode(SFE, INPUT_PULLUP);
  pinMode(CHAVE_START, INPUT_PULLUP);

  pararMotores();
  IrReceiver.begin(PINO_IR, DISABLE_LED_FEEDBACK);
}

void loop() {
  if (digitalRead(CHAVE_START) == LOW) {
    resetRobo();
    if (IrReceiver.decode()) IrReceiver.resume();
    atualizarMotor();
    return;
  }

  if (IrReceiver.decode()) {
    uint8_t comando = IrReceiver.decodedIRData.command;
    bool repetido = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT);

    if (!repetido && comando != 0x00) {
      if (comando == CMD_BOTAO_OK) {
        resetRobo();
        IrReceiver.resume();
        atualizarMotor();
        return;
      }

      if (!roboLigado) {
        if (millis() - tempoUltimoIR > 300) {
          if (comando == CMD_BOTAO_0) iniciarEstrategia(ATAQUE_DIRETO);
          else if (comando == CMD_BOTAO_1) iniciarEstrategia(ATAQUE_CIRCULAR);
          else if (comando == CMD_BOTAO_2) iniciarEstrategia(ATAQUE_ESPERA_ESQ);
          else if (comando == CMD_BOTAO_4) iniciarEstrategia(ATAQUE_ESPERA_DIR);
          else if (comando == CMD_BOTAO_6) iniciarEstrategia(COMBINADA_BOTAO_6);
          tempoUltimoIR = millis();
        }
      } else {
        if (millis() - tempoUltimoIR > 800) {
          resetRobo();
          tempoUltimoIR = millis();
        }
      }
    }
    IrReceiver.resume();
  }

  if (!roboLigado) {
    pararMotores();
    atualizarMotor();
    return;
  }

  if (esperandoStart) {
    if (millis() - tempoInicioStart < TEMPO_ESPERA_START) {
      pararMotores();
      digitalWrite(OPONENTE_LED, (millis() / 250) % 2);
      atualizarMotor();
      return;
    } else {
      esperandoStart = false;
      digitalWrite(OPONENTE_LED, LOW);
      executandoSequencia = true;
      passoSequencia = 1;
      tempoInicioPasso = millis();
    }
  }

  if (estadoBorda != 0) {
    tratarFugaBorda();
    atualizarMotor();
    return;
  }

  if (bordaDetectada()) {
    iniciarFugaBorda();
    atualizarMotor();
    return;
  }

  if (digitalRead(SOFD) == SENS_DETECTADO) {
    executandoSequencia = false; 
    combateAgressivo();
    atualizarMotor();
    return;
  }

  if (estrategiaAtual == ATAQUE_DIRETO) {
    solicitarEstado(FRENTE);
    atualizarMotor();
    return;
  }

  if (estrategiaAtual == ATAQUE_CIRCULAR && executandoSequencia) {
    unsigned long dt = millis() - tempoInicioPasso;

    if (passoSequencia == 1) {
      solicitarEstado(GIRO_ESQ);
      if (dt >= TEMPO_GIRO_ESQ) {
        passoSequencia = 2;
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }

    if (passoSequencia == 2) {
      solicitarEstado(FRENTE);
      if (dt >= TEMPO_ARCO) {
        passoSequencia = 3;
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }

    if (passoSequencia == 3) {
      combateAgressivo();
      atualizarMotor();
      return;
    }
  }

  if (estrategiaAtual == ATAQUE_ESPERA_ESQ && executandoSequencia) {
    unsigned long dt = millis() - tempoInicioPasso;

    if (passoSequencia == 1) {
      solicitarEstado(TRAS);
      if (dt >= TEMPO_ESQUIVA_RECUO) {
        passoSequencia = 2;
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }

    if (passoSequencia == 2) {
      solicitarEstado(GIRO_ESQ);
      if (dt >= TEMPO_GIRO_ESQ) {
        passoSequencia = 3;
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }

    if (passoSequencia == 3) {
      combateAgressivo();
      atualizarMotor();
      return;
    }
  }

  if (estrategiaAtual == ATAQUE_ESPERA_DIR && executandoSequencia) {
    unsigned long dt = millis() - tempoInicioPasso;

    if (passoSequencia == 1) {
      solicitarEstado(TRAS);
      if (dt >= TEMPO_ESQUIVA_RECUO) {
        passoSequencia = 2;
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }

    if (passoSequencia == 2) {
      solicitarEstado(GIRO_DIR);
      if (dt >= TEMPO_GIRO_DIR) {
        passoSequencia = 3;
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }

    if (passoSequencia == 3) {
      combateAgressivo();
      atualizarMotor();
      return;
    }
  }

  // ESTRATEGIA COMBINADA DO BOTÃO 6 EM LOOPING:
  // Passo 1: Vai pra frente (FRENTE)
  // Passo 2: Vira pra esquerda (GIRO_ESQ)
  // Passo 3: Vai pra frente (FRENTE)
  // Passo 4: Vira pra direita (GIRO_DIR) -> Retorna ao Passo 1
  if (estrategiaAtual == COMBINADA_BOTAO_6 && executandoSequencia) {
    unsigned long dt = millis() - tempoInicioPasso;

    if (passoSequencia == 1) {
      solicitarEstado(FRENTE);
      if (dt >= TEMPO_PASSO_FRENTE_6) {
        passoSequencia = 2;
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }

    if (passoSequencia == 2) {
      solicitarEstado(GIRO_ESQ);
      if (dt >= TEMPO_PASSO_GIRO_6) {
        passoSequencia = 3;
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }

    if (passoSequencia == 3) {
      solicitarEstado(FRENTE);
      if (dt >= TEMPO_PASSO_FRENTE_6) {
        passoSequencia = 4;
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }

    if (passoSequencia == 4) {
      solicitarEstado(GIRO_DIR);
      if (dt >= TEMPO_PASSO_GIRO_6) {
        passoSequencia = 1; // Reseta para o passo 1 criando o looping contínuo
        tempoInicioPasso = millis();
      }
      atualizarMotor();
      return;
    }
  }

  combateAgressivo();
  atualizarMotor();
}