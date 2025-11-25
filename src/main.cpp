#include <Arduino.h>          // база
#include <LittleFS.h>         // Флеш
#include <ESP8266WiFi.h>      // WiFi
#include <ESP8266WebServer.h> // WebServer
#include <ArduinoOTA.h>       // Для Over‑The‑Air аплоада
// #include <ESP8266mDNS.h>      // Для мини-днс локального
#include <Wire.h>    // Для I2C шины
#include <U8g2lib.h> // Для работы с дисплеем
#include <string.h>  // Для сравнивания С-строк
#include <stdio.h>   // Для сравнивания С-строк

// ------ Структуры (Схемы массива) и др -------

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

enum MenuType
{ // Вместо интов непонятных интов. типа мейн это ноль и тд
  NONE,
  MAIN,
  WIFI_DIR,
  STORAGE_FS,
  WEB,
  OTA,
  WIFI_SAVED,
  WIFI_SCANNED,
  StorageFS_D,
  StorageFS_R,
  StorageFS_R_F
};

struct UIDir
{
  const char *name;
  void (*edit)(MenuType); // Смена ДИРа
  MenuType type;          // Тайп для смена ДИРа
  void (*function)();     // Функциональная
};

// ---------------------------------------- РАННИЕ ОБЬЯВЛЕНИЯ ФУНКЦИЙ ДЛЯ КРАСОТЫ КОДА И РЕШЕНИЯ ПРОБЛЕМ КУРИЦА-ЯЙЦО  ---------------------------------------------------

String FlashEdit(const char *path, const char *message_string, const int message_int, char mode);
void load_saved_wifi();
void certain_wifi_link();
void checkWifiStatus();
void checkWebStatus();
void checkOTAStatus();
void wifi_connecting_debug(const char *ssid);
void editDir(MenuType name);
void webEditStatus();
void OTAEditStatus();
void scan_wifi();
void wifi_disconnect();
void openFileSystem();

// ---------------------------------------- ИМПОРТЫ -> МАКРОСЫ  ---------------------------------------------------

#define DEBUG 1 // 1 = включить вывод, 0 = выключить вывод (Просто кастомная глобальная переменная для оптимизации)
#if DEBUG
#define _print(x) Serial.print(x)
#define _println(x) Serial.println(x)
#else
#define _print(x)
#define _println(x)
#endif

#define array_length(x) (int)(sizeof(x) / sizeof((x)[0])) // Нельзя создать динамическую функцию прям для всех типов массивов

// ---------------------------------------------- МАКРОСЫ -> КОД ---------------------------------------------

// -------- Глобальные переменные  --------

MenuType activeMenu = MAIN; // Дефолт

struct DirectoryRoute
{
  MenuType previousActiveMenu;
  int activeUnitY;
};

DirectoryRoute directories[5] = { // Для истории по # и *
    {MAIN, 0}};

const int key_input_button_pins[] = {D5, D6, D7, D0};
// const int key_input_general_pins[];
// const int key_output_pins[] = {LED_BUILTIN};

long lastOpenTime[array_length(key_input_button_pins)] = {0}; // Чтобы норм регать клики без дублей

const long minOpenTime = 100; // Минимальный интервал при зажатии и будет дубликат

// int oldAW = 255;
// bool loopik_is_ended = true;

bool flash_is_avialable = false;

// Массив вайфаев

WiFi_Device wifi_devices[]{
    {"xgio2016", "smile123", 192, 168, 1, 39}, // название пароль предпочитаемый айпи
    {"password", "12348765", 10, 196, 183, 10},
};

// IPAddress local_IP(wifi_devices[choise_wifi].network_class, wifi_devices[choise_wifi].range_of_private_addreses, wifi_devices[choise_wifi].subnet, wifi_devices[choise_wifi].host); // Желаемый айпишник ESP самого (Для днс и раздетльного туннеолирования)
// IPAddress gateway(wifi_devices[choise_wifi].network_class, wifi_devices[choise_wifi].range_of_private_addreses, wifi_devices[choise_wifi].subnet, 1);                               // Роутер
// IPAddress subnet(255, 255, 255, 0);                                                                                                                                                 // Маска подсети
// IPAddress primaryDNS(8, 8, 8, 8);                                                                                                                                                   // Google DNS

ESP8266WebServer server(80); // Порт в скобках

const char *host_dns = "esp8266"; // днс домен + ОТА хост. ПОКА ОТКЛАДЫВАЕМ мДНС

U8G2_SSD1315_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
/* На русском - работа с U8G2 на модели SSD1315 (128X64) Без имени.
С режимом работы фулл буфера (Вся картинка хранится в RAM микроконтроллера),
Интерфейс подключения: Hardware I²C. Первый атрибут U8G2_R0 - без поворота те 0 градусов.
Второй U8X8_PIN_NONE - Это параметр для пина сброса (reset) дисплея.

*/

char statusBuffer[20]; // Буффер для дисплея статуса в UI

const char *wifi = nullptr;  // false/ssid  ----- Для чек статуса в мейн дире
const char *isweb = nullptr; // false/ip  ----- Для чек статуса в мейн дире
String isweb_stringed;       // false/ip  ----- Для чек статуса в мейн дире
bool isOTA = false;          // false/true  ----- Для чек статуса в мейн дире

const char *actualWifiSSID;
const char *actualWifiPassword;

String scan_wifi_stringed[51];
String file_system_stringed[12];
String read_file_stringed;

// -------- Глобальные переменные END --------

// ------------------- Функции локальные -----------------

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

// --------------  #region UI Functions + Dirs ---------------------

// #endregion
int scrollUnitY = 0;        // Сколько раз сниз клик
int scrollUnitYCounter = 0; // Сколько дисплеев прокручено

int text_height = 14;
// int maxHeight = 64;
// int maxWeight = 128;
UIDir dirs[] = {
    // MAIN
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"StorageFS", editDir, STORAGE_FS, nullptr},
    {"Web Server", editDir, WEB, checkWebStatus},
    {"OTA Upload", editDir, OTA, checkOTAStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
};

UIDir WIFI_scanned[array_length(scan_wifi_stringed)] = { // Допустим максимум 50 точек
    {"", nullptr, NONE, nullptr}};
UIDir WIFI_saved[array_length(wifi_devices) + 1] = {
    {"", nullptr, NONE, nullptr}};
UIDir WIFI[] = {
    {"", nullptr, NONE, nullptr}, // Пустые для статуса
    {"Saved WiFi", editDir, WIFI_SAVED, load_saved_wifi},
    {"Scan WiFi", editDir, WIFI_SCANNED, scan_wifi},
    {"Disconnect", nullptr, NONE, wifi_disconnect}};

UIDir StorageFS[] = {
    {"Read File", editDir, StorageFS_R, openFileSystem},
    {"Delete File", editDir, StorageFS_D, openFileSystem}};
UIDir StorageFS_Delete[12] = {
    {"N/A", nullptr, NONE, nullptr},
};
UIDir StorageFS_Read[12] = {
    {"N/A", nullptr, NONE, nullptr},
};
UIDir StorageFS_Read_File[1];

UIDir WebServer[] = {
    {"", nullptr, NONE, nullptr},
    {"Open", nullptr, NONE, webEditStatus}};
UIDir OTA_dir[] = {
    {"", nullptr, NONE, nullptr},
    {"Unlock", nullptr, NONE, OTAEditStatus}};

void editDir(MenuType name)
{
  scrollUnitY = 0;
  scrollUnitYCounter = 0;
  activeMenu = name;
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

void certain_wifi_link()
{
  for (int i = 0; i < array_length(wifi_devices); i++)
  {
    if (strcmp(wifi_devices[i].ssid, WIFI_saved[scrollUnitY].name) == 0 /* cmp - для сравнивания. Если 0 = то что надо*/)
    {
      WiFi.disconnect(true);
      actualWifiSSID = wifi_devices[i].ssid;
      actualWifiPassword = wifi_devices[i].password;
      WiFi.begin(actualWifiSSID, actualWifiPassword);
      wifi_connecting_debug(wifi_devices[i].ssid);
    }
  }
}

void tryToSome_wifi_link()
{
  for (int i = 0; i < array_length(wifi_devices); i++)
  {
    if (strcmp(wifi_devices[i].ssid, WIFI_scanned[scrollUnitY].name) == 0 /* cmp - для сравнивания. Если 0 = то что надо*/)
    {
      WiFi.disconnect(true);
      actualWifiSSID = wifi_devices[i].ssid;
      actualWifiPassword = wifi_devices[i].password;
      WiFi.begin(actualWifiSSID, actualWifiPassword);
      wifi = actualWifiSSID;
      checkWifiStatus();
      wifi_connecting_debug(wifi_devices[i].ssid);
    }
  }
}

void load_saved_wifi()
{
  for (int i = 0; i < array_length(wifi_devices); i++)
  {
    WIFI_saved[i + 1] = {wifi_devices[i].ssid, nullptr, NONE, certain_wifi_link}; // Так как 0 - статус
  }
  checkWifiStatus();
} // wifi_connecting_debug

void wifi_disconnect()
{
  WiFi.disconnect(true);
  wifi = nullptr;
  checkWifiStatus();
}

void checkWifiStatus()
{
  const char *statusAssemleText = nullptr;
  if (wifi)
  {
    strcpy(statusBuffer, "S: "); // Перезаписать первую строку
    strcat(statusBuffer, wifi);  // Добавить. Цель - соединить 2 переменные чара
    statusAssemleText = statusBuffer;
  };
  const char *status = wifi ? statusAssemleText : "S: No WiFI";
  WIFI[0] = {status, nullptr, NONE, nullptr};
  WIFI_saved[0] = {status, nullptr, NONE, nullptr};
  WIFI_scanned[0] = {status, nullptr, NONE, nullptr};
}

void scan_wifi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA); // Станция
  delay(100);
  int count_of_scanned = WiFi.scanNetworks();
  if (count_of_scanned != 0)
  {
    for (int i = 0; i < count_of_scanned; i++)
    {
      scan_wifi_stringed[i] = WiFi.SSID(i); // Новые ставим
      WIFI_scanned[i + 1] = {scan_wifi_stringed[i].c_str(), nullptr, NONE, tryToSome_wifi_link};
    };
    for (int i = count_of_scanned; i < array_length(WIFI_scanned) - 1; i++)
    {
      WIFI_scanned[i + 1] = {"N/A", nullptr, NONE, nullptr};
    };
  }
  WiFi.begin(actualWifiSSID, actualWifiPassword);
  checkWifiStatus();
}

void webEditStatus()
{
  if (WiFi.localIP() != IPAddress(0, 0, 0, 0))
  {
    if (strcmp(WebServer[0].name, "S: Closed") == 0)
    {
      server.begin();
      isweb_stringed = WiFi.localIP().toString(); // Стринг должен жить вечно тк c_str просто указывет на него
      isweb = isweb_stringed.c_str();
      WebServer[1] = {"Close", nullptr, NONE, webEditStatus};
    }
    else
    {
      server.stop();
      isweb = nullptr;
      WebServer[1] = {"Open", nullptr, NONE, webEditStatus};
    }
    checkWebStatus();
  }
}

void checkWebStatus()
{
  const char *status = isweb ? isweb : "S: Closed";
  WebServer[0] = {status, nullptr, NONE, nullptr};
}

void OTAEditStatus()
{
  if (WiFi.localIP() != IPAddress(0, 0, 0, 0))
  {
    if (strcmp(OTA_dir[0].name, "S: Disabled") == 0)
    {
      ArduinoOTA.begin();
      isweb_stringed = WiFi.localIP().toString(); // Стринг должен жить вечно тк c_str просто указывет на него
      isOTA = true;
      OTA_dir[1] = {"Lock", nullptr, NONE, OTAEditStatus};
    }
    else
    {
      ArduinoOTA.end();
      isOTA = false;
      OTA_dir[1] = {"Unlock", nullptr, NONE, OTAEditStatus};
    }
    checkOTAStatus();
  }
}

void checkOTAStatus()
{
  const char *status = isOTA ? "S: Enabled" : "S: Disabled";
  OTA_dir[0] = {status, nullptr, NONE, nullptr};
}

int click_throughs = 0;
void displayTools(UIDir array[], int length)
{ // Если не передавать длину то при счете функцией legnth-array будет указатель на указатель что не есть хорошо
  u8g2.clearBuffer();
  u8g2.drawLine(0, text_height * (scrollUnitY % 4 + 1) + 2 * (scrollUnitY % 4), u8g2.getStrWidth(array[scrollUnitY].name), text_height * (scrollUnitY % 4 + 1) + 2 * (scrollUnitY % 4));
  for (int i = 0; i < (length - 4 * scrollUnitYCounter); i++)
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
    u8g2.print(array[i + 4 * scrollUnitYCounter].name);
  }
  u8g2.drawLine(127, scrollUnitYCounter * (63 * 4 / length), 127, (63 * 4 / length) + scrollUnitYCounter * (63 * 4 / length)); // Скролл бар
  u8g2.sendBuffer();

  if (isPressedKey(0)) // *
  {
    if (array[scrollUnitY].function)
    {
      array[scrollUnitY].function();
    }
    if (array[scrollUnitY].edit)
    {
      directories[click_throughs].activeUnitY = scrollUnitY;
      click_throughs++;
      array[scrollUnitY].edit(array[scrollUnitY].type);
      directories[click_throughs] = {activeMenu, 0};
    }
  }
  if (isPressedKey(1)) // #
  {
    if (click_throughs > 0)
    {
      activeMenu = directories[click_throughs - 1].previousActiveMenu;
      scrollUnitY = directories[click_throughs - 1].activeUnitY;
      int scrollUnitYCopy = scrollUnitY;
      int x = 0;
      while (scrollUnitYCopy >= 4)
      {
        if (scrollUnitYCopy % 4 == 0)
        {
          x++;
        }
        scrollUnitYCopy--;
      }
      scrollUnitYCounter = x;
      click_throughs--;
    }
  }
  if (isPressedKey(2)) // ˅
  {
    if (scrollUnitY < length - 1) // из за индексации
    {
      if (scrollUnitY != 0 && (scrollUnitY + 1) % 4 == 0)
      {
        scrollUnitYCounter++;
      }
      scrollUnitY++;
    }
  }
  if (isPressedKey(3)) // ^
  {
    if (scrollUnitY > 0)
    {
      scrollUnitY--;
      if (scrollUnitY != 0 && (scrollUnitY + 1) % 4 == 0)
      {
        scrollUnitYCounter--;
      }
    }
  }
}

void displayMenu(MenuType menu)
{
  switch (menu)
  {
  case MAIN:
    displayTools(dirs, array_length(dirs));
    break;
  case WIFI_DIR:
    displayTools(WIFI, array_length(WIFI));
    break;
  case STORAGE_FS:
    displayTools(StorageFS, array_length(StorageFS));
    break;
  case WEB:
    displayTools(WebServer, array_length(WebServer));
    break;
  case OTA:
    displayTools(OTA_dir, array_length(OTA_dir));
    break;
  case WIFI_SAVED:
    displayTools(WIFI_saved, array_length(WIFI_saved));
    break;
  case WIFI_SCANNED:
    displayTools(WIFI_scanned, array_length(WIFI_scanned));
    break;
  case StorageFS_D:
    displayTools(StorageFS_Delete, array_length(StorageFS_Delete));
    break;
  case StorageFS_R:
    displayTools(StorageFS_Read, array_length(StorageFS_Read));
    break;
  case StorageFS_R_F:
    displayTools(StorageFS_Read_File, array_length(StorageFS_Read_File));
    break;
  case NONE:
    break;
  }
}

bool wifi_is_finding = false;
void wifi_connecting_debug(const char *ssid)
{
  if (!wifi_is_finding)
  {
    wifi_is_finding = true;

    int counter = 0;
    float timer = 0;
    while (WiFi.status() != WL_CONNECTED && timer != 10)
    {
      u8g2.clearBuffer();
      u8g2.setCursor(0, 14);
      u8g2.println("Connecting: ");
      u8g2.setCursor(0, 30);
      u8g2.println(ssid);
      u8g2.setCursor(0, 46);
      counter++;
      for (int i = counter % 3 + 1; i > 0; i--)
      {
        u8g2.print('.');
      }
      u8g2.sendBuffer();
      _println("Connecting...");
      digitalWrite(2, LOW);
      delay(250);
      digitalWrite(2, HIGH);
      delay(250);
      timer += 0.5;
    }

    wifi = timer == 10 ? nullptr : ssid;
    checkWifiStatus();
    _print("IP адрес: ");
    _println(WiFi.localIP());
    _print("GateWay адрес: ");
    _println(WiFi.gatewayIP());
    wifi_is_finding = false;
  }
}

void delete_file()
{
  FlashEdit(StorageFS_Delete[scrollUnitY].name, "", -1, 'd');
  activeMenu = directories[click_throughs - 1].previousActiveMenu;
  scrollUnitY = directories[click_throughs - 1].activeUnitY;
  int scrollUnitYCopy = scrollUnitY;
  int x = 0;
  while (scrollUnitYCopy >= 4)
  {
    if (scrollUnitYCopy % 4 == 0)
    {
      x++;
    }
    scrollUnitYCopy--;
  }
  scrollUnitYCounter = x;
  click_throughs--;
  click_throughs--;
}

void read_file()
{
  read_file_stringed = FlashEdit(StorageFS_Read[scrollUnitY].name, "", -1, 'r');
  StorageFS_Read_File[0] = {read_file_stringed.c_str(), nullptr, NONE, nullptr};
}

void openFileSystem()
{
  Dir dir = LittleFS.openDir("/");
  int count_of_files = 0;
  while (dir.next() && count_of_files < 12)
  {
    file_system_stringed[count_of_files] = dir.fileName();
    count_of_files++;
  }
  if (count_of_files != 0)
  {
    for (int i = 0; i < count_of_files; i++)
    {
      StorageFS_Delete[i] = {file_system_stringed[i].c_str(), nullptr, NONE, delete_file};
      StorageFS_Read[i] = {file_system_stringed[i].c_str(), editDir, StorageFS_R_F, read_file};
    };
    for (int i = count_of_files; i < array_length(StorageFS_Delete); i++)
    {
      StorageFS_Delete[i] = {"N/A", nullptr, NONE, nullptr};
      StorageFS_Read[i] = {"N/A", nullptr, NONE, nullptr};
    };
  }
}

// ------------------- Начало программы и луп  -----------------

// ------------------- FLASH  -----------------
char buffer[64]; // Для передач переменных в аргумент сообщения. Работа с адресными Чарами. Учитываем что 64 - максимальный размер передаваемого инта

String FlashEdit(const char *path, const char *message_string, const int message_int, char mode)
{
  if (flash_is_avialable)
  {
    if (path[0] != '/')
    {
      if (StorageFS_Delete[scrollUnitY].name)
      {
        strcpy(statusBuffer, "/");                                // Перезаписать первую строку
        strcat(statusBuffer, StorageFS_Delete[scrollUnitY].name); // Добавить. Цель - соединить 2 переменные чара
        path = statusBuffer;
      };
    }
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
    case 'd':
      LittleFS.remove(path);
      return "";
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
      _print("Функция чтения (R) выполнена. Содержимое файла (");
      _print(path);
      _print("): ");

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
    return "";
  }
  else
  {
    _println("Flash isn't avialable");
  }
  return "";
}

// Учитываем, что стринг может забивать РАМ в долгосроке. Вместо них в будущем для оптимизации юзать С-строки. Плюсом потом еще неблок. код юзать
void setup()
{
  u8g2.begin();                       // OLED
  u8g2.setFont(u8g2_font_ncenB14_tr); // Фонт - ncen → New Century Schoolbook (семейство шрифта). B - bold. 14 - size. tr - прозрачный фон transparent
  // u8g2_font_micro_tr.  u8g2_font_unifont_t_symbols. u8g2_font_4x6_tr. u8g2_font_ncenB14_tr
  // text_height = u8g2.getMaxCharHeight();
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Off
  for (int i = 0; i < array_length(key_input_button_pins); i++)
  {
    pinMode(key_input_button_pins[i], INPUT_PULLUP);
  }
  if (LittleFS.begin())
  {
    _println("LittleFS смонтирован и готов работать");
    flash_is_avialable = true;
  }
  // _print("Подключаемся к WiFI: ");
  // _println(wifi_ssid);
  // WiFi.config(local_IP);
  // if(MDNS.begin(host_dns)) { ОТЛОЖЕНО
  //   _print("А также доступно по адресу: http://");
  //   _print(host_dns);
  //   _println(".local");
  // }
  server.on("/", root_handle);
  server.on("/bright", bright_handle);
  // server.begin(); // Слушаем на порту 80 к слову
  // for (int j = 0; j < 4; j++)
  // {
  //   FlashEdit("/Some", "Hello", -1, 'w');
  // }
  ArduinoOTA.setHostname(host_dns);
}

// String* name_tools = other_tools;
void loop()
{
  displayMenu(activeMenu);
  // MDNS.update();         // Обработка ДНС. ОТЛОЖЕНО
  if (isOTA)
  {
    ArduinoOTA.handle(); // Обработка ОТА аплоадов
  }
  if (isweb)
  {
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

6 - Аналоговый райт и светодиод

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

*/