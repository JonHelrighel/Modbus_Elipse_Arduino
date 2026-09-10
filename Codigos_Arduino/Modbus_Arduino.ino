#include <modbus.h>          // Biblioteca base do Modbus
#include <modbusDevice.h>    // Gerencia dispositivos Modbus
#include <modbusRegBank.h>   // Cria os registradores (Holding, Input, etc.)
#include <modbusSlave.h>     // Configura o Arduino como escravo Modbus
#include <DHT.h>             // Biblioteca para sensor DHT22 (temperatura/umidade)

// =======================================================================
// CONFIGURAÇÃO DO SENSOR DE TEMPERATURA
// =======================================================================
#define DHTPIN 2      // Pino onde o DHT22 está conectado
#define DHTTYPE DHT22 // Tipo do sensor: DHT22
DHT dht(DHTPIN, DHTTYPE); // Cria objeto do sensor DHT22 no pino 2

// =======================================================================
// OBJETOS MODBUS
// =======================================================================
modbusDevice regBank; // "Banco de registradores" do Modbus (memória simulada)
modbusSlave slave;    // Define o Arduino como um escravo Modbus

// =======================================================================
// VARIÁVEIS
// =======================================================================
word AI0;   // Variável para armazenar leitura do potenciômetro
word Temp;  // Variável para armazenar temperatura *10 (ex: 25,3°C → 253)

// =======================================================================
// CONFIGURAÇÃO INICIAL (setup)
// =======================================================================
void setup() {
  // Define o ID do slave (endereço Modbus)
  regBank.setId(1);

  // =====================================================================
  // CRIAÇÃO DOS REGISTRADORES NO BANCO MODBUS
  // =====================================================================
  regBank.add(10002);   // Discrete Input → botão (endereço 10002)
  regBank.add(30001);   // Input Register → potenciômetro (endereço 30001)
  regBank.add(30004);   // Input Register → sensor de temperatura (endereço 30004)

  // Holding Registers → usados como saída
  regBank.add(40010);   // LED vermelho (PWM) no pino 5
  regBank.add(40011);   // LED verde no pino 6
  regBank.add(40012);   // LED amarelo no pino 7
  regBank.add(40013);   // LED vermelho2 no pino 8
  regBank.add(40014);   // LED verde2 no pino 10
  regBank.add(40015);   // Ventoinha PWM no pino 9

  // Liga o banco de registradores ao escravo
  slave._device = &regBank;
  slave.setBaud(9600); // Define a taxa de comunicação Modbus (9600 bps)

  // =====================================================================
  // CONFIGURAÇÃO DOS PINOS DO ARDUINO
  // =====================================================================
  pinMode(3, INPUT_PULLUP); // Botão no pino D3 → entrada digital com pull-up
  pinMode(5, OUTPUT);       // LED vermelho (PWM no pino D5)
  pinMode(6, OUTPUT);       // LED verde
  pinMode(7, OUTPUT);       // LED amarelo
  pinMode(8, OUTPUT);       // LED vermelho2
  pinMode(9, OUTPUT);       // Ventoinha (PWM no pino D9)
  pinMode(10, OUTPUT);      // LED verde2

  dht.begin(); // Inicializa o sensor DHT22
}

// =======================================================================
// LOOP PRINCIPAL
// =======================================================================
void loop() {
  // =====================================================================
  // BOTÃO
  // =====================================================================
  byte DI3 = digitalRead(3);   // Lê botão no pino D3
  regBank.set(10002, DI3);     // Atualiza valor no registrador Modbus (10002)

  // =====================================================================
  // POTENCIÔMETRO
  // =====================================================================
  AI0 = analogRead(A0);        // Lê valor analógico do potenciômetro (0–1023)
  regBank.set(30001, AI0);     // Armazena no registrador Modbus (30001)

  // =====================================================================
  // SENSOR DE TEMPERATURA (DHT22)
  // =====================================================================
  float t = dht.readTemperature();  // Lê temperatura em °C
  if (!isnan(t)) {                  // Se a leitura for válida
    Temp = (word)(t * 10);          // Multiplica por 10 (25,3°C → 253)
    regBank.set(30004, Temp);       // Salva no registrador Modbus (30004)
  }

  // =====================================================================
  // LED VERMELHO PWM (pino 5)
  // =====================================================================
  word pwmVal = regBank.get(40010); // Lê valor desejado no registrador (0–255)
  if (DI3 == HIGH) {                // Se o botão for pressionado
    pwmVal = 255;                   // LED vermelho fica no máximo
  }
  if (pwmVal > 255) pwmVal = 255;   // Garante que não passe de 255
  analogWrite(5, pwmVal);           // Aplica PWM no LED vermelho

  // =====================================================================
  // LEDS DE TEMPERATURA (Verde, Amarelo, Vermelho2)
  // =====================================================================
  if (Temp >= 200 && Temp < 240) { // Entre 20,0°C e 24,0°C
    digitalWrite(6, HIGH);  // Verde ligado
    digitalWrite(7, LOW);   // Amarelo desligado
    digitalWrite(8, LOW);   // Vermelho2 desligado
    regBank.set(40011, 1);
    regBank.set(40012, 0);
    regBank.set(40013, 0);
  } else if (Temp >= 240 && Temp < 280) { // Entre 24,0°C e 28,0°C
    digitalWrite(6, LOW);
    digitalWrite(7, HIGH);  // Amarelo ligado
    digitalWrite(8, LOW);
    regBank.set(40011, 0);
    regBank.set(40012, 1);
    regBank.set(40013, 0);
  } else if (Temp >= 280) { // Acima de 28,0°C
    digitalWrite(6, LOW);
    digitalWrite(7, LOW);
    digitalWrite(8, HIGH);  // Vermelho2 ligado
    regBank.set(40011, 0);
    regBank.set(40012, 0);
    regBank.set(40013, 1);
  } else { // Abaixo de 20,0°C → todos desligados
    digitalWrite(6, LOW);
    digitalWrite(7, LOW);
    digitalWrite(8, LOW);
    regBank.set(40011, 0);
    regBank.set(40012, 0);
    regBank.set(40013, 0);
  }

  // =====================================================================
  // LED VERDE2 (pino 10)
  // =====================================================================
  word ledVerde2 = regBank.get(40014); // Lê registrador (40014)
  if (ledVerde2 == 1) {
    digitalWrite(10, HIGH); // Liga LED verde2
  } else {
    digitalWrite(10, LOW);  // Desliga LED verde2
  }

  // =====================================================================
  // VENTOINHA PWM (pino 9)
  // =====================================================================
  int pwmVentoinha = 0; // Variável para duty cycle da ventoinha
  if (Temp < 240) {
    // Abaixo de 24,0 °C → ventoinha desligada
    pwmVentoinha = 0;
  } else if (Temp >= 240 && Temp < 280) {
    // Entre 24,0 °C e 28,0 °C → ventoinha proporcional ao potenciômetro
    // Garante um valor entre 70 e 200 (não fica muito fraca)
    pwmVentoinha = map(AI0, 0, 1023, 70, 200);
  } else if (Temp >= 280) {
    // Acima de 28,0 °C → velocidade máxima
    pwmVentoinha = 255;
  }
  analogWrite(9, pwmVentoinha);       // Aplica PWM na ventoinha
  regBank.set(40015, pwmVentoinha);   // Salva duty cycle no registrador (40015)

  // =====================================================================
  // ATUALIZA O MODBUS
  // =====================================================================
  slave.run(); // Mantém comunicação Modbus funcionando
}
