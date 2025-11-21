#include <Arduino.h>          // база
#include <LittleFS.h>         // Флеш
#include <ESP8266WiFi.h>      // WiFi
#include <ESP8266WebServer.h> // WebServer
#include <ArduinoOTA.h>       // Для Over‑The‑Air аплоада
// #include <ESP8266mDNS.h>      // Для мини-днс локального
#include <Wire.h>    // Для I2C шины
#include <U8g2lib.h> // Для работы с дисплеем
// ---------------------------------------- ИМПОРТЫ -> МАКРОСЫ  ---------------------------------------------------
#define DEBUG 1 // 1 = включить вывод, 0 = выключить вывод (Просто кастомная глобальная переменная для оптимизации)
#if DEBUG
#define _print(x) Serial.print(x)
#define _println(x) Serial.println(x)
#else
#define _print(x)
#define _println(x)
#endif

#define array_lenght(x) (int)(sizeof(x) / sizeof((x)[0])) // Нельзя создать динамическую функцию прям для всех типов массивов
// ---------------------------------------------- МАКРОСЫ -> КОД ---------------------------------------------

// ------ Структуры (Схемы массива) -------
struct WiFi_Device
{                   // Схема для обьекта ВайФай
  const char *ssid; // Название внешней точки доступа
  const char *password;
  const int network_class;             // (XXX.xxx.xxx.x)
  const int range_of_private_addreses; //(xxx.XXX.xxx.x)
  const int subnet;                    //(xxx.xxx.XXX.x)
  const int host;                      //(xxx.xxx.xxx.X)
  // const int host_gateway;              //(xxx.xxx.xxx.X) - для выхода в инет - итак кончается на 1 у всех тк что не нужен
};

// -------- Глобальные переменные  --------

const int key_input_button_pins[] = {D5, D6, D7, D0};
// const int key_input_general_pins[];
// const int key_output_pins[] = {LED_BUILTIN};

long lastOpenTime[array_lenght(key_input_button_pins)] = {0};

const long minOpenTime = 100;

int oldAW = 255;
bool loopik_is_ended = true;

bool flash_is_avialable = false;

// Массив вайфаев

WiFi_Device wifi_devices[]{
    {"xgio2016", "smile123", 192, 168, 1, 39},
    {"password", "12348765", 10, 196, 183, 10},
};

int choise_wifi = 0; // Предвывор активного вайфай подключения. Типо к чему будет линкаться ESP при включении (цель)
// 0 = "xgio2016"
// 1 = "password"
// ...

const char *wifi_ssid = wifi_devices[choise_wifi].ssid;
const char *wifi_password = wifi_devices[choise_wifi].password;

IPAddress local_IP(wifi_devices[choise_wifi].network_class, wifi_devices[choise_wifi].range_of_private_addreses, wifi_devices[choise_wifi].subnet, wifi_devices[choise_wifi].host); // Желаемый айпишник ESP самого (Для днс и раздетльного туннеолирования)
IPAddress gateway(wifi_devices[choise_wifi].network_class, wifi_devices[choise_wifi].range_of_private_addreses, wifi_devices[choise_wifi].subnet, 1);                               // Роутер
IPAddress subnet(255, 255, 255, 0);                                                                                                                                                 // Маска подсети
IPAddress primaryDNS(8, 8, 8, 8);                                                                                                                                                   // Google DNS

ESP8266WebServer server(80); // Порт в скобках

const char *host_dns = "esp8266"; // днс домен + ОТА хост. ПОКА ОТКЛАДЫВАЕМ мДНС

U8G2_SSD1315_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
/* На русском - работа с U8G2 на модели SSD1315 (128X64) Без имени.
С режимом работы фулл буфера (Вся картинка хранится в RAM микроконтроллера),
Интерфейс подключения: Hardware I²C. Первый атрибут U8G2_R0 - без поворота те 0 градусов.
Второй U8X8_PIN_NONE - Это параметр для пина сброса (reset) дисплея.

*/
// -------- Глобальные переменные END --------

// --------- UI ----------

int text_height = 14;
int maxHeight = 64;
int maxWeight = 128;

const char *main_tools[] = {"Saved WiFI", "Scan WiFi", "Settings", "AI mode", "Camera", "StorageFS", "StorageSD", "Restart", "Reboot", "Reboot", "Reboot", "Reboot", "Reboot", "Reboot", "Reboot", "Reboot", "Reboot"};

// ------------------- FLASH  -----------------
char buffer[64]; // Для передач переменных в аргумент сообщения. Работа с адресными Чарами

String FlashEdit(const char *path, const char *message_string, const int message_int, char mode)
{
  if (flash_is_avialable)
  {
    const char *search_mode = nullptr;
    const char *search_message = nullptr;
    if (message_int != -1)
    {
      sprintf(buffer, "%d", message_int);
      search_message = buffer;
    }
    else
    {
      search_message = message_string;
    }

    switch (mode) // более проще чем elseif. Да и впринципе проба
    {
    case 'r':
      search_mode = "r";
      break;
    case 'a':
      search_mode = "a";
      break;
    case 'w':
      search_mode = "w";
      break;

    default:
      _println("Ошибка: Недопустимый режим (R, W, A)");
      break;
    }
    File file = LittleFS.open(path, search_mode);
    if (!file)
    {
      _print("Не обнаружен файл для функции ");
      _print(mode);
      _print(". По адресу: ");
      _println(path);
      return "";
    }
    if (mode == 'r')
    {
      String fileContent = ""; // Аналог буфера
      _print("Функция чтения (R) выполнена. Содержимое файла: ");
      while (file.available())
      {
        fileContent += (char)file.read(); // Аналог буфера с добавлением чаров байт за байтом. Чар нужен чтобы стрингу добавлялись ASCII символы а не число инт
      }
      _println(fileContent);
      return fileContent;
    }
    if (mode == 'w')
    {
      if (file.print(search_message))
      {
        _print("Функция записи (W) выполнена. Содержимое на данный момент: ");
        // file.close();
        // File file = LittleFS.open(path, "r");
        file.close();
        file = LittleFS.open(path, "r");
        file.seek(0);
        while (file.available())
        {
          Serial.write(file.read());
        }
        _println();
      }
      else
      {
        _println("Функция записи (W) НЕ выполнена");
      }
    }
    if (mode == 'a')
    {
      file.print(search_message);
      file.println(); // чтобы след запись была с новой строки. Для логов и подобное
      _print("Функция добавления (A) выполнена. Содержимое на данный момент: ");
      file.close();
      file = LittleFS.open(path, "r");
      file.seek(0);
      while (file.available())
      {
        Serial.write(file.read());
      }
      _println();
    }
    // if (mode == "r+")
    // {
    // }
    file.close();
  }
  else
  {
    _println("Flash isn't avialable");
  }
  return "";
}

// ------------------- Функции  -----------------

void smoothEdit(int data)
{
  loopik_is_ended = false;
  if (data > oldAW)
  {
    for (int x = oldAW; x <= data; x++)
    {
      analogWrite(2, x);
      delay(2);
    }
  }
  else
  {
    for (int x = oldAW; x >= data; x--)
    {
      analogWrite(2, x);
      delay(2);
    }
  }
  _print("Светодиод включен на ");
  _print(100 - (data * 100) / 255);
  _println("%");
  oldAW = data;
  loopik_is_ended = true;
}

bool wifi_is_founding = false;
void wifi_connecting_debug()
{
  if (!wifi_is_founding)
  {
    wifi_is_founding = true;
    // int vector = 1; // Меняем говнокод на лаконичный остаток от деления
    int counter = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      u8g2.clearBuffer();
      u8g2.setCursor(0, 10);
      u8g2.println("Connecting to: ");
      u8g2.setCursor(0, 25);
      u8g2.println(wifi_ssid);
      u8g2.setCursor(0, 40);
      // if (counter >= 1 && vector == 1)
      // {
      //   counter++;
      // }
      // else if (counter <= 3 && vector == -1)
      // {
      //   counter--;
      // }
      counter++;
      for (int i = counter % 3 + 1; i > 0; i--)
      {
        u8g2.print('.');
      }
      // if (counter == 1)
      // {
      //   vector = 1;
      // }
      // else if (counter == 3)
      // {
      //   vector = -1;
      // }
      u8g2.sendBuffer();
      _println("Connecting...");
      digitalWrite(2, LOW);
      delay(250);
      digitalWrite(2, HIGH);
      delay(250);
    }
    // IPAddress ip = WiFi.localIP();
    // for (int i = ip[3]; i > 0; i--) {
    //   digitalWrite(2, LOW);
    //   delay(500);
    //   digitalWrite(2, HIGH);
    //   delay(500);
    // };
    u8g2.clearBuffer();
    u8g2.setCursor(0, 10);
    u8g2.println("Connected!");
    u8g2.setCursor(0, 25);
    u8g2.println("Your local IP:");
    u8g2.setCursor(0, 40);
    u8g2.println(WiFi.localIP());
    u8g2.sendBuffer();
    _print("IP адрес: ");
    _println(WiFi.localIP());
    _print("GateWay адрес: ");
    _println(WiFi.gatewayIP());
    wifi_is_founding = false;
  }
}

bool isPressedKey(int index)
{
  // LOW = нажата тк заземляется
  int stateReading = digitalRead(key_input_button_pins[index]);
  if (stateReading == LOW)
  {
    if ((millis() - lastOpenTime[index]) > minOpenTime)
    {
      lastOpenTime[index] = millis();
      return true;
    }
  }
  else
  {
    return false;
  }
  return false;
}

// ----------------------- ВЕБ СЕРВЕР ФУНКЦИИ ----------------------------------

void root_handle()
{
  String html = FlashEdit("/index.html", "", -1, 'r');
  if (html != "")
  {
    server.send(200, "text/html", html);
  }
  else
  {
    server.send(404, "text/plain", "Ошибка: Файл index.html не найден в LittleFS!");
  }
}

void bright_handle()
{
  if (server.hasArg("value"))
  {
    String value = server.arg("value");
    int int_value = value.toInt();
    if (int_value >= 0 && int_value <= 100)
    {
      analogWrite(2, 255 - (255 * int_value) / 100);
      FlashEdit("/percentOn.txt", "", int_value, 'w');
      server.send(200, "text/plain", "Установлена яркость: " + value + "%"); // Отправляем для отладки
    }
  }
  else
  {
    _println("Нету аргумента");
  }
}

// ----------------------- ВЕБ СЕРВЕР ФУНКЦИИ END ----------------------------------

// ------------------- Начало программы и луп  -----------------
// Учитываем, что стринг может забивать РАМ в долгосроке. Вместо них в будущем для оптимизации юзать С-строки. Плюсом потом еще неблок. код юзать
void setup()
{
  u8g2.begin();                       // OLED
  u8g2.setFont(u8g2_font_ncenB14_tr); // Фонт - ncen → New Century Schoolbook (семейство шрифта). B - bold. 14 - size. tr - прозрачный фон transparent
  // u8g2_font_micro_tr.  u8g2_font_unifont_t_symbols. u8g2_font_4x6_tr. u8g2_font_ncenB14_tr
  // text_height = u8g2.getMaxCharHeight();
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < array_lenght(key_input_button_pins); i++)
  {
    pinMode(key_input_button_pins[i], INPUT_PULLUP);
  }
  if (LittleFS.begin())
  {
    _println("LittleFS смонтирован и готов работать");
    flash_is_avialable = true;
  }
  _print("Подключаемся к WiFI: ");
  _println(wifi_ssid);
  // WiFi.config(local_IP);

  // if(MDNS.begin(host_dns)) { ОТЛОЖЕНО
  //   _print("А также доступно по адресу: http://");
  //   _print(host_dns);
  //   _println(".local");
  // }
  server.on("/", root_handle);
  server.on("/bright", bright_handle);
  server.begin(); // Слушаем на порту 80 к слову

  ArduinoOTA.setHostname(host_dns);
  ArduinoOTA.begin();
}

int scrollUnitY = 0;
int scrollUnitYCounter = 0;
void loop()
{
  u8g2.clearBuffer();

  u8g2.drawLine(0, text_height * (scrollUnitY % 4 + 1) + 2 * (scrollUnitY % 4), u8g2.getStrWidth(main_tools[scrollUnitY]), text_height * (scrollUnitY % 4 + 1) + 2 * (scrollUnitY % 4));

  for (int i = 0; i < (array_lenght(main_tools) - 4 * scrollUnitYCounter); i++)
  {
    int separator = 0;
    if (i != 0)
    {
      separator = 2;
    }
    if (i == 0)
    {
      u8g2.setCursor(0, text_height * (i + 1));
    }
    else
    {
      u8g2.setCursor(0, text_height * (i + 1) + separator * i);
    }
    u8g2.print(main_tools[i+4*scrollUnitYCounter]);
  }
  u8g2.drawLine(127, 0, 127, 63);
  u8g2.sendBuffer();
  if (isPressedKey(0)) // *
  {
  }
  if (isPressedKey(1)) // #
  {
  }
  if (isPressedKey(2)) // ˅
  {
    if (scrollUnitY < array_lenght(main_tools) - 1) // из за индексации
    {
      if (scrollUnitY != 0 && (scrollUnitY + 1) % 4 == 0) {
        scrollUnitYCounter++;
        _println(scrollUnitYCounter);
      }
      scrollUnitY++; 
    }
  }
  if (isPressedKey(3)) // ^
  {
    if (scrollUnitY > 0)
    {
      scrollUnitY--;
      if (scrollUnitY != 0 && (scrollUnitY + 1) % 4 == 0) {
        scrollUnitYCounter--;
        _println(scrollUnitYCounter);
      }
    }
  }
  if (WiFi.status() != WL_CONNECTED)
  {

    // wifi_connecting_debug();
  }
  else
  {
    // MDNS.update();         // Обработка ДНС. ОТЛОЖЕНО
    ArduinoOTA.handle();   // Обработка ОТА аплоадов
    server.handleClient(); // Обработка веб сервера
  }

  delay(100);
}

/* ----------------------- Unuse Commented Blocks of code  ----------------------------------

1 - Дыхание
 // while (true)
  // {
  //   for (int i = 0; i < 255; i++)
  //   {
  //     analogWrite(2, i);
  //     delay(10);
  //   }
  //   for (int i = 255; i > 0; i--)
  //   {
  //     analogWrite(2, i);
  //     delay(10);
  //   }
  // }

2 - Заметка к ASCII
  // if(Serial.available() && loopik_is_ended == true) {
  //   char x = Serial.read();
  //   if(x >= '0' && x <= '9') {
  //     loopik(x - '0'); // Так как в ASCII коде '0' = 48. В проверках он видит так Число больше 48 >= 48...
  //   }
  // }

3 - Чек на флэше директорий
Dir dir = LittleFS.openDir("/");
  int count = 0;
  while (dir.next())
  {
    count++;
    _print("Найден файл: ");
    _println(dir.fileName());
  }
  if (count == 0) {
    _println("Пусто брат");
  }

  4 - Работа с консолью

 if (Serial.available() && loopik_is_ended == true)
  {
    // _print(Serial.read() - '0');
    int x = Serial.parseInt();

    if (x >= 0 && x <= 100)
    {
      smoothEdit(255 - (255 * x) / 100);
      // FlashEdit("/percentOn.txt", String(x), 'w');
      FlashEdit("/percentOn.txt", "", x, 'w');
      // FlashEdit("/percentOn.txt", "", -1, 'r');
    }
  }
  while (Serial.available() > 0)
  {
    Serial.read();
  }

  5 - работа с флешем

 String percent_data = FlashEdit("/percentOn.txt", "", -1, 'r');
 if (percent_data != "")
 {
   int int_data = percent_data.toInt();
   oldAW = int_data;
   analogWrite(2, (255 - int_data * 255 / 100));
 }

*/