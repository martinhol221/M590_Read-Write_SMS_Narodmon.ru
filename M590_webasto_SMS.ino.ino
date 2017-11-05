//https://www.drive2.ru/l/474186105906790427/
//https://www.drive2.ru/c/485387655492665696/
#include <SoftwareSerial.h>
SoftwareSerial m590(4, 5);          // RX, TX  для новой платы
#include <DallasTemperature.h>      // https://github.com/milesburton/Arduino-Temperature-Control-Library
#define ONE_WIRE_BUS 12             // https://github.com/PaulStoffregen/OneWire
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);
/*  ----------------------------------------- НАЗНАЧАЕМ ВЫВДЫ --------------------------------------------------------------------   */
#define STARTER_Pin 8               // на реле стартера, через транзистор с 8-го пина ардуино
#define ON_Pin 9                    // на реле зажигания, через транзистор с 9-го пина ардуино
#define FIRST_P_Pin 11              // на для дополнительного реле первого положения ключа зажигания
#define ACTIV_Pin 13                // на светодиод c 13-го пина для индикации активности 
#define BAT_Pin A0                  // на батарею, через делитель напряжения 39кОм / 11 кОм
#define Feedback_Pin A3             // на силовой провод замка зажигания
#define STOP_Pin A2                 // на концевик педали тормоза для отключения режима прогрева или датчик нейтральной передачи
#define DDM_Pin A1                  // на датчик давления масла
#define boot_Pin 3                  // на 19-ю ногу модема для его пробуждения
#define ring_Pin 2                  // на 10-ю ногу модема для пробуждения ардуино
/*  ----------------------------------------- ИНДИВИДУАЛЬНЫЕ НАСТРОЙКИ !!!---------------------------------------------------------   */
String call_phone = "375000000000"; // телефон входящего вызова
String SMS_phone =  "375000000000"; // телефон куда отправляем СМС
String SENS = "GSM-Sensor";         // Название устройства (придумать свое Citroen 566 или др. для отображения в программе и на карте)
String PIN = "start";               // код для ограничения доступа через SMS. 5 байт !!!
float Vstart = 12.50;               // поорог распознавания момента запуска по напряжению
int SMS_time = 2;                   // время в минутах с момента попытки запуска, черз которое отправляем СМС-отчет
int Timer_time = 15;                // таймер времени прогрева в минутах
/*  ----------------------------------------- ДАЛЕЕ НЕ ТРОГАЕМ --------------------------------------------------------------------   */
String at = "";
float TempDS0 = 20;                // переменная хранения температуры с датчика двигателя
float TempDS1 = 20;                // переменная хранения температуры с датчика на улице
float Vbat;                        // переменная хранящая напряжение бортовой сети
float m = 69.80;                   // делитель для перевода АЦП в вольты для резистров 39/11kOm
int k = 0;
int WarmUpTimer = 0;               // переменная времени прогрева двигателя по умолчанию
bool heating = false;              // переменная состояния режим прогрева двигателя
bool SMS_send = false;             // флаг разовой отправки СМС
bool SMS_report = true;            // флаг СМС отчета
unsigned long Time1 = 0;



void setup() {
  pinMode(ring_Pin,    INPUT);
  pinMode(STARTER_Pin, OUTPUT);
  pinMode(ON_Pin,      OUTPUT);
  pinMode(FIRST_P_Pin, OUTPUT);
  pinMode(boot_Pin,    OUTPUT);
  digitalWrite(boot_Pin, LOW);
  Serial.begin(9600);              // скорость порта для отладки
  m590.begin(38400);               // скорость порта модема, может быть 38400
  delay(2000);

  if (digitalRead(STOP_Pin) == HIGH) SMS_report = true;  // включаем народмон при нажатой педали тормоза при подаче питания 
  Serial.print("Starting M590, Sensor name: "+SENS+" 5.11.2017, SMS_report =  "), Serial.println(SMS_report);

/*-смена скорости модема с 9600 на 38400:
установить m590.begin(9600);, раскоментировать m590.println("AT+IPR=38400"), delay (1000);  и m590.begin(38400), прошить
вернуть  вернуть все обратно и прошить. снова.
*/
// m590.println("AT+IPR=38400"), delay (1000); // настройка скорости M590 если не завелся на 9600 но завелся на 38400
 // m590.begin(38400);
              }

void loop() {
  if (m590.available()) { // если что-то пришло от модема 
    while (m590.available()) k = m590.read(), at += char(k),delay(1);
    Serial.println(at); 
    
    if (at.indexOf("RING") > -1) { m590.println("AT+CLIP=1");                                // реакция на любой вызов , включаем АОН                              
        if (at.indexOf("CLIP: \""+call_phone+"\",") > -1 && at.indexOf("+CMGR:") == -1 ) {   // если прилетел номер телефона и он прилетел не из SMS то 
        delay(50), m590.println("ATH0");
            if (heating == false) {
                         webasto_ON();                                                      // включаем запуск если двигатьль не в прогревве  
                           } else {
                         webasto_OFF();                                                      // иначе останавливаем прогрев
                                  }
        }
    /*  --------------------------------------------------- ПРЕДНАСТРОЙКА МОДЕМА M590 ----------------------------------------------------------------------   */
    } else if (at.indexOf("+PBREADY\r\n") > -1)                    {m590.println ("ATE1"),             delay(100);      // Включить режим ЭХО
    } else if (at.indexOf("ATE1\r\r\nOK\r\n") > -1)                {m590.println ("AT+CMGF=1"),        delay(100);      // Включаем Текстовый режим СМС
    } else if (at.indexOf("AT+CMGF=1\r\r\nOK\r\n") > -1)           {m590.println ("AT+CSCS=\"gsm\""),  delay(100);      // Выбираем кодировку СМС
    } else if (at.indexOf("AT+CSCS=\"gsm\"\r\r\nOK\r\n") > -1)     {m590.println ("AT+CMGD=1,4"),      delay(2000);      // Удаляем все СМС
    } else if (at.indexOf("AT+CMGD=1,4\r\r\n") > -1)               {m590.println ("AT+CNMI=2,1,0,0,0"),delay(300);      // Разрешаем прием входящих СМС
   /*  --------------------------------------------- РЕАКЦИЯ НА ВХОДЯЩЕЕ СМС -------------------------------------------------------------------------------- */ 
   } else if (at.indexOf("\"SM\",") > -1)         { int i = at.substring(at.indexOf("\"SM\",")+5, at.indexOf("\"SM\",")+7).toInt();
                                                   m590.print("AT+CMGR="), m590.print(i), m590.println(""); // читаем СМС и удаляем их все   
                                                   
   } else if (at.indexOf(""+PIN+"") > -1 ) { Timer_time = at.substring(at.indexOf(""+PIN+"")+5,at.indexOf(""+PIN+"")+7).toInt(), webasto_ON();  
                                                  if (Timer_time == 0) Timer_time = 10;  // если вдруг вернулся ноль меняем его на 10
   } else if (at.indexOf("stop") > -1 )           { webasto_OFF();                                                                // команда остановки прогрева.
   }
     at = "";                                                                                    // очищаем переменную
}

if (millis()> Time1 + 60000) detection(), Time1 = millis(); // выполняем функцию detection () каждые 10 сек 
if (heating == true && digitalRead(STOP_Pin) == HIGH) webasto_OFF();

}   

void detection(){                           // услови проверяемые каждые 60 сек  
    sensors.requestTemperatures();          // читаем температуру с трех датчиков
    TempDS0 = sensors.getTempCByIndex(0);
    TempDS1 = sensors.getTempCByIndex(1);
    Vbat = analogRead(BAT_Pin);              // замеряем напряжение на батарее
    Vbat = Vbat / m ;                        // переводим попугаи в вольты
  Serial.print("Vbat= "), Serial.print(Vbat), Serial.print (" V.");  
  Serial.print(" || Temp : "), Serial.print(TempDS0);  
  Serial.print(" || WarmUpTimer ="), Serial.println (WarmUpTimer);
      
    if (SMS_send == true && SMS_report == true) { SMS_send = false;  // если фаг SMS_send равен 1 высылаем отчет по СМС
        Serial.print("SMS send start...");
        m590.println("AT+CMGS=\"+"+SMS_phone+"\""), delay(100);
        m590.print(""+SENS+"  v 05.11.2017 ");
        m590.print("\n Temp.Dvig: "),  m590.print(TempDS0);
        m590.print("\n Temp.Salon: "), m590.print(TempDS1);
        m590.print("\n Vbat: "),       m590.print(Vbat);
        m590.print("\n Webasto time: "), m590.print(Timer_time);
        m590.print((char)26);                   }

    if (WarmUpTimer == (Timer_time - SMS_time))         WarmUpTimer--, SMS_send = true;                   // отправляем СМС 
    if (WarmUpTimer > 0 )                               WarmUpTimer--;                  // вычитаем из таймера 1 цу каждых 60 сек.
    if (heating == true && WarmUpTimer <1)              Serial.println("End timer"),   webasto_OFF(); 
    if (heating == true && Vbat < 11.0)                 Serial.println("Low voltage"), webasto_OFF(); 
    if (heating == false)                               digitalWrite(ACTIV_Pin, HIGH), delay (100),  digitalWrite(ACTIV_Pin, LOW);
 
}             
 

/*------------------------------------------------- УПРАВЛЕНИЕ РЕЛЕ ------------------------------------------------------------------*/ 
void webasto_ON() {                                       // программа включения прогрева
    WarmUpTimer = Timer_time;                             // прогрев по умолчанию 5 минут
if (TempDS0 < -5 && TempDS0 != -127)  WarmUpTimer = 60;   // прогрев на  10 минут
if (TempDS0 <-10 && TempDS0 != -127)  WarmUpTimer = 90;   // прогрев на  15 минут 
if (TempDS0 <-15 && TempDS0 != -127)  WarmUpTimer = 120;  // прогрев на 20 минут 
if (TempDS0 <-20 && TempDS0 != -127)  WarmUpTimer = 150;  // Прогрев на 30 минут 
if (Vbat > 10.00 && digitalRead(Feedback_Pin) == LOW)  digitalWrite(ON_Pin, HIGH), heating = true;  // включаем реле на вебасту
   Serial.println ("WarmUp ON");
                }

void webasto_OFF() {  // программа остановки прогрева двигателя
   digitalWrite(ON_Pin, LOW),      delay (1000);
   Serial.println ("WarmUp OFF"), heating= false; 
                   }