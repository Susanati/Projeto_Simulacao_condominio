

#include <SPI.h>       //Biblioteca de comunicação SPI, para comunição do módulo RFID
#include <MFRC522.h>   //Biblioteca do módulo RFID
#include <EEPROM.h>    //Biblioteca da memória EEPROM
#include <MemoryLib.h> //Biblioteca para gerenciar a EEPROM com variáveis
#include <Servo.h>     //Biblioteca do Servo Motor

#define ledGreen 2   //Led verde no pino 2
#define ledRed 3     //Led vermelho no pino 3
#define button 4     //Botão de cadastro no pino 4
#define servo 5      //Servo motor no pino 5
#define posLock 55   //Posição do servo quando fechado
#define posUnlock 160 //Posição do servo quando aberto
#define timeUnlock 7 //Tempo que o servo permance aberto (em segundos)
#define SDA_RFID 10  //SDA do RFID no pino 10
#define RST_RFID 9   //Reset do RFID no pino 9

/*----- VARIÁVEIS -----*/
MFRC522 mfrc522(SDA_RFID, RST_RFID);   //Inicializa o módulo RFID
MemoryLib memory(1, 2);                //Inicializa a biblioteca MemoryLib. Parametros: memorySize=1 (1Kb) / type=2 (LONG)
Servo myservo;                         //Inicializa o servo motor
int maxCards = memory.lastAddress / 2; //Cada cartão ocupa duas posições na memória. Para 1Kb será permitido o cadastro de 101 cartões
String cards[101] = {};                //Array com os cartões cadastrados

/*----- SETUP -----*/
void setup() {
  //Configura os pinos
  pinMode(ledGreen, OUTPUT);
  pinMode(ledRed, OUTPUT);
  pinMode(button, INPUT_PULLUP);

  //Inicia SPI
  SPI.begin();

  //Inicia o modulo RFID MFRC522
  mfrc522.PCD_Init();

  //Configura o pino do Servo Motor
  myservo.attach(servo);

  //Posiciona o servo na posição inicial
  myservo.write(posLock);

  //Retorna os cartões armazenados na memória EEPROM para o array
  ReadMemory();

  //Pisca os leds sinalizando a inicialização do circuito
  for (int i = 0; i < 10; i++) {
    digitalWrite(ledRed, HIGH);
    digitalWrite(ledGreen, HIGH);
    delay(50);
    digitalWrite(ledRed, LOW);
    digitalWrite(ledGreen, LOW);
    delay(50);
  }
}

/*----- LOOP PRINCIPAL -----*/
void loop() {
  //Variável UID recebe o valor do cartão lido
  String UID = ReadCard();

  if (UID != "") {
    boolean access = false;
    //Efetua a leitura de todas as posições do array
    for (int c = 0; c <= maxCards; c++) {
      if (UID == cards[c]) {
        //Se a posição do array for igual ao cartão lido, seta a varíavel como verdadeira e finaliza o for
        access = true;
        break;
      }
    }
    //Variável verdadeira, efetua o acesso. Caso contrário, pisca o led vermelho
    if (access) {
      Unlock();
    } else {
      for (int i = 0; i < 6; i++) {
        digitalWrite(ledRed, HIGH);
        delay(40);
        digitalWrite(ledRed, LOW);
        delay(40);
      }
      delay(400);
    }
  }

  //Verifica se o botão para castrado de novo cartão foi pressionado
  if (digitalRead(button) == LOW) { //Botão pressionado igual a LOW devido a declaração de PULL_UP
    //Grava novo cartão, ou apaga um cartão já cadastrado
    RecUser();

    //Após os leds apagarem, se o botão permanecer pressionado, apaga todos os cartões
    if (digitalRead(button) == LOW) {
      digitalWrite(ledGreen, HIGH);
      //Escreve zero em todos os enderecos da EEPROM
      for (int address = 0; address <= memory.lastAddress; address++) {
        memory.write(address, 0);
      }
      ReadMemory(); //Atualiza os valores do array
      digitalWrite(ledGreen, LOW);
    }
  }
}

/*----- ABRE O ACESSO -----*/
void Unlock() {
  int posServo = myservo.read();
  //Abre o acesso
  digitalWrite(ledGreen, HIGH);
  digitalWrite(ledRed, LOW);
  for (int i = posServo; i <= posUnlock; i++) {
    myservo.write(i);
    delay(10);
  }
  delay(timeUnlock * 1000);
  digitalWrite(ledGreen, LOW);
  //Fecha o acesso
  digitalWrite(ledRed, HIGH);
  for (int i = posUnlock; i >= posLock; i--) {
    myservo.write(i);
    delay(10);
  }
  delay(400);
  digitalWrite(ledRed, LOW);
}

/*----- EFETUA A LEITURA DO CARTAO RFID -----*/
String ReadCard() {
  String UID = "";
  //Verifica a presença de um novo cartao e efetua a leitura
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    //Retorna o UID para a variavel UIDcard
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      UID += String(mfrc522.uid.uidByte[i]);
    }
  }
  return UID;
}

/*----- CADASTRA CARTÃO DE ACESSO -----*/
void RecUser() {
  String UID = "";
  boolean deleteCard = false;

  //Efetua a leitura do cartao para cadastro
  //Aprox 5 segundos para cadastrar o cartao
  for (int t = 0; t < 150; t++) {
    delay(10);
    //Acende os leds verde e vermelho
    digitalWrite(ledGreen, HIGH);
    digitalWrite(ledRed, HIGH);
    //Faz a leitura o cartao
    UID = ReadCard();
    if (UID != "") {
      //Se leu um cartão, mantém somente o led verde acesso enquanto segue executando o procedimento
      digitalWrite(ledRed, LOW);
      //Verifica se o cartao ja esta cadastrado
      for (int c = 0; c < maxCards; c++) {
        //Se já estiver cadastrado, exclui o cartão do array e também da memória
        if (cards[c] == UID) {
          digitalWrite(ledRed, HIGH);
          digitalWrite(ledGreen, LOW);
          cards[c] = "0";
          deleteCard = true;
        }
      }
      break; //finaliza o for
    }
  }

  //Cancela o cadastro se nenhum cartao foi lido
  if (UID == "") {
    //Apaga os leds verde e vermelho
    digitalWrite(ledGreen, LOW);
    digitalWrite(ledRed, LOW);
    return;
  }

  if (deleteCard == false) {
    //Se for inclusão de novo cartão, verifica se ainda existe espaco para novos cadastros
    //Se a última posição da memória for diferente de zero, pisca o led vermelho sinalizando
    //que não existe mais espaço para novos cartões, e finaliza o procedimento
    if (cards[maxCards - 1].toInt()  != 0) {
      digitalWrite(ledGreen, LOW);
      for (int i = 0; i < 10; i++) {
        digitalWrite(ledRed, HIGH);
        delay(100);
        digitalWrite(ledRed, LOW);
        delay(100);
      }
      return;
    }
  }

  //Adiciona o cartão no array, somente se for inclusão de novo cartão
  if (deleteCard == false) {
    for (int c = 0; c < maxCards; c++) {
      if (cards[c].toInt() == 0) { //Posicao livre
        cards[c] = UID;
        break; //finaliza o for
      }
    }
  }

  //Grava na memória os cartões do array
  //Cada cartão ocupa duas posições da memória
  for (int e = 0; e <= memory.lastAddress; e++) { //Limpa os valores da memória
    memory.write(e, 0);
  }
  int a = 0;
  for (int c = 0; c < maxCards; c++) {
    if (cards[c].toInt() != 0) {
      memory.write(a, cards[c].substring(0, 6).toInt());
      memory.write(a + 1, cards[c].substring(6, cards[c].length()).toInt());
      a += 2;
    }
  }

  //Retorna os valores da memória para o array, para ajustar as posições do cartão no array como está na memória
  ReadMemory();

  if (deleteCard == false) {
    Unlock();
  } else {
    //Apaga os leds verde e vermelho
    digitalWrite(ledGreen, LOW);
    digitalWrite(ledRed, LOW);
  }
}

/*----- RETORNA OS DADOS DA MEMÓRIA PARA O ARRAY -----*/
void ReadMemory() {
  int a = 0;
  for (int c = 0; c < maxCards; c++) {
    cards[c] = String(memory.read(a)) + String(memory.read(a + 1));
    a += 2;
  }
}