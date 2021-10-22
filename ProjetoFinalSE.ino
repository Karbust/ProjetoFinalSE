#define BLYNK_PRINT Serial

//Bibliotecas
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Servo.h>

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "";

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "";
char pass[] = "";

#define samp_siz 4
#define rise_threshold 4

//----------------------------------
//Sensor Ultrasonico
const int pinTrigger = 13;
const int pinEcho = 15;
int cm = 0;

//----------------------------------
//Servo Motor
Servo servoPorta;
Servo servoAlcool;
const int pinServoPorta = 2;
const int pinServoAlcool = 0;
int pos = 0;

//----------------------------------
//Sensor do Alcool
int alcool = 0;
int valorMinAlcool = 510;

//----------------------------------
//Porta
volatile int porta = LOW;

//----------------------------------
//CD74HC4051
const int selectPins[3] = {12, 14, 16};
const int analogInput = A0;

//----------------------------------
//LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

byte sym[3][8] = {
  {
    B00000, B01010, B11111, B11111, B01110, B00100, B00000, B00000
  },{
    B00000, B00000, B00000, B11000, B00100, B01000, B10000, B11100
  },{
    B00000, B00100, B01010, B00010, B00100, B00100, B00000, B00100
  }
};

BLYNK_WRITE(V3) {
  if(param.asInt() == 1)
    abre_porta();
}

void setup() {
  Serial.begin(9600);
  
  Blynk.begin(auth, ssid, pass);

  servoPorta.attach(pinServoPorta);
  servoPorta.write(0);
  
  servoAlcool.attach(pinServoAlcool);
  servoAlcool.write(0);
  
  lcd.init();   // INICIALIZA O DISPLAY LCD
  lcd.backlight(); // HABILITA O BACKLIGHT (LUZ DE FUNDO) 
  
  for(int i=0;i<8;i++) 
    lcd.createChar(i, sym[i]);

  for (int i=0; i < 3; i++) {
    pinMode(selectPins[i], OUTPUT);
    digitalWrite(selectPins[i], HIGH);
  }
}

void loop ()
{  
  Blynk.run();
  lcd.clear();
  
  if (porta == HIGH) {
    porta = LOW;
    
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("Bem-vindo a");
    lcd.setCursor(0, 1);
    lcd.print("Sala de Detecao");
    delay(1000);
    lcd.clear();

    lcd.setCursor(3, 0);
    lcd.print("Aproxime-se");
    lcd.setCursor(7, 1);
    lcd.print(":)");
    delay(1000);
    lcd.clear();

    do {
      cm = 0.01723 * readUltrasonicDistance(pinTrigger, pinEcho);
      Serial.print("cm.: [" + String(cm) + "]cm" + "\n\n");
    } while(cm > 60);

    if (cm <= 60) {
      lcd.setCursor(1, 0);
      lcd.print("Pode iniciar a");
      lcd.setCursor(1, 1);
      lcd.print("sua leitura !!!");
      delay(3000);
      lcd.clear();

      /*...Sensor gas..........................................*/
      for (int i=0; i < 3; i++) {
          digitalWrite(selectPins[i], 1 & (1 << i) ? HIGH : LOW);
      }
      alcool = analogRead(analogInput);
      Serial.print("GAS.: [" + String(alcool) + "] alcool" + "\n");
      //delay(1000);
      /*.......................................................*/

      //print valor do sensor
      lcd.setCursor(1, 0);
      lcd.print("Valores Alcool ");
      lcd.setCursor(6, 1);
      lcd.print(alcool);
      delay(1000);
      lcd.clear();

      //Verificação das mãos desinfetadas sensor de álcool
      if (alcool >= valorMinAlcool) {
        lcd.setCursor(3, 0);
        lcd.print("Tem as maos ");
        lcd.setCursor(0, 1);
        lcd.print("desinfetadas !!! ");
        delay(2000);
        lcd.clear();
      } else { //A pessoa não tem as mãos desinfetadas
        lcd.setCursor(3, 0);
        lcd.print("Desinfecao");
        lcd.setCursor(4, 1);
        lcd.print("das maos");
        delay(2000);
        lcd.clear();

        deitar_alcool();
      }

      delay(2000);
      lcd.print("Prossiga para a ");
      lcd.setCursor(0, 1);
      lcd.print("proxima mesa 2.");
      delay(2000);
      lcd.clear();

      //--------------------------
      //Oximetro
      oximetro();
      
      delay(1000);
    }
  } else {
    Serial.print("Sleeping\n");
    lcd.setCursor(4, 0);
    lcd.print("Sleeping");
    lcd.setCursor(7, 1);
    lcd.print("~_~");
    delay(500);
  }
}

/*..........Oximetro..........*/
//Ler valores do Oxigenio e Batimentos Cardiacos
void oximetro()
{
  lcd.setCursor(5, 0);
  lcd.print("Oximetro");
  
  delay(1000);
  
  float reads[samp_siz], sum;
  long int now, ptr;
  float last, reader, start;
  float first, second, third, before, print_value;
  bool rising;
  int rise_count;
  int n, i = 0;
  int whileCount = 0;
  long int last_beat;

  for (int i = 0; i < samp_siz; i++)
    reads[i] = 0;
    
  sum = 0;
  ptr = 0;

  while(i < 1000) {
    // calculate an average of the sensor
    // during a 20 ms period (this will eliminate
    // the 50 Hz noise caused by electric light
    n = 0;
    start = millis();
    reader = 0.;
    do {
      for (int i=0; i < 3; i++) {
          digitalWrite(selectPins[i], 0 & (1 << i) ? HIGH : LOW);
      }
      reader += analogRead(analogInput);
      n++;
      now = millis();
      yield();
    } while (now < start + 20);  
    reader /= n;  // we got an average
    
    // Add the newest measurement to an array
    // and subtract the oldest measurement from the array
    // to maintain a sum of last measurements
    sum -= reads[ptr];
    sum += reader;
    reads[ptr] = reader;
    last = sum / samp_siz;
    // now last holds the average of the values in the array

    // check for a rising curve (= a heart beat)
    if (last > before) {
      rise_count++;
      if (!rising && rise_count > rise_threshold) {
        // Ok, we have detected a rising curve, which implies a heartbeat.
        // Record the time since last beat, keep track of the two previous
        // times (first, second, third) to get a weighed average.
        // The rising flag prevents us from detecting the same rise more than once.
        rising = true;
        first = millis() - last_beat;
        last_beat = millis();

        // Calculate the weighed average of heartbeat rate
        // according to the three last beats
        print_value = 60000. / (0.4 * first + 0.3 * second + 0.3 * third);
        
        Serial.print(print_value);
        Serial.print('\n');
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(print_value);
        
        third = second;
        second = first;
      }
    } else {
      // Ok, the curve is falling
      rising = false;
      rise_count = 0;
    }
    before = last;
    
    ptr++;
    ptr %= samp_siz;

    i++;
  }
  
  delay(1000);
}

/*..........Rotação do Servo Motor..........*/
void deitar_alcool()
{
  lcd.setCursor(2, 0);
  lcd.print("Vou deitar o");
  lcd.setCursor(5, 1);
  lcd.print("Alcool");
  delay(1000);
  lcd.clear();

  servoAlcool.write(90);
  delay(1000); // Wait for 1 segundo(s)

  servoAlcool.write(0);
  delay(1000); // Wait for 1 segundo(s)
}

/*..........Leitura da distancia para o Sensor ultra..........*/
long readUltrasonicDistance(int triggerPin, int echoPin)
{
  pinMode(triggerPin, OUTPUT);  // Clear the trigger
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  // Sets the trigger pin to HIGH state for 10 microseconds
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);
  pinMode(echoPin, INPUT);
  // Reads the echo pin, and returns the sound wave travel time in microseconds
  return pulseIn(echoPin, HIGH);
}

void abre_porta()
{
    porta = HIGH;
    
    Serial.print("Entrei\n");
    
    servoPorta.write(180);
    delay(1000); // Wait for 1 segundo(s)
 
    servoPorta.write(0);
    delay(1000); // Wait for 1 segundo(s)
}
