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
  WIFI_DEBUG,
  STORAGE_FS,
  WEB,
  OTA,
  WIFI_SAVED,
  WIFI_SCANNED,
  StorageFS_D,
  StorageFS_R,
  StorageFS_R_F,
};

struct UIDir
{
  const char *name;
  void (*edit)(char, MenuType); // Смена ДИРа
  MenuType type;                // Тайп для смена ДИРа
  void (*function)();           // Функциональная
  bool isServiceOption = false;
};

// ---------------------------------------- РАННИЕ ОБЬЯВЛЕНИЯ ФУНКЦИЙ ДЛЯ КРАСОТЫ КОДА И РЕШЕНИЯ ПРОБЛЕМ КУРИЦА-ЯЙЦО  ---------------------------------------------------

String FlashEdit(const char *path, const char *message_string, const int message_int, const char *mode);
void load_saved_wifi();
void certain_wifi_link();
void checkWifiStatus();
void checkWebStatus();
void checkOTAStatus();
void wifi_connecting_debug(const char *ssid);
void editDir(char mode, MenuType name = NONE);
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

DirectoryRoute directories[10] = { // Для истории по # и *
    {MAIN, 0}};

const int key_input_button_pins[] = {D7, D6, D5, D0}; // D7 = K4 = ^       D6 = K3 = Down        D5 = K2 = #          D0 = K1 = *
// const int key_input_general_pins[];
// const int key_output_pins[] = {LED_BUILTIN};

long lastOpenTime[array_length(key_input_button_pins)] = {0}; // Чтобы норм регать клики без дублей

const long minOpenTime = 200; // Минимальный интервал при зажатии и будет дубликат

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

// char statusBuffer[20]; // Буффер для дисплея статуса в UI
char statusBuffer_wifi[20];
char statusBuffer_ota[20];
char statusBuffer_file[20];

const char *wifi = nullptr;  // false/ssid  ----- Для чек статуса в мейн дире
const char *isweb = nullptr; // false/ip  ----- Для чек статуса в мейн дире
String isweb_stringed;       // false/ip  ----- Для чек статуса в мейн дире
bool isOTA = false;          // false/true  ----- Для чек статуса в мейн дире

const char *actualWifiSSID;
const char *actualWifiPassword;

String scan_wifi_stringed[51]; // Даже с стрингом если внутри че то и меняется он фрагментируется
String file_system_stringed[50];
String read_file_stringed;

const int FPS = 20;

// -------- Глобальные переменные END --------

// ------------------- Функции локальные -----------------

// ----------------------- ВЕБ СЕРВЕР ФУНКЦИИ ----------------------------------

void root_handle()
{
  String html = FlashEdit("/index.html", "", -1, "r");
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
      FlashEdit("/percentOn.txt", "", int_value, "w");
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
int scrollUnitY = 0;      // Сколько раз сниз клик
int scrollDisplayNow = 0; // Сколько дисплеев прокручено

int click_throughs = 0; // Сколько директориев прокручено

const int text_height = 14;
const int oled_height = 63;
const int oled_width = 127;
const int optionsPerViewDisplay = 4;

bool wifi_is_connecting = false;

// int maxHeight = 64;
// int maxWeight = 128;
UIDir dirs[] = {
    // MAIN
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"StorageFS", editDir, STORAGE_FS, nullptr},
    {"Web Server", editDir, WEB, checkWebStatus},
    {"OTA Upload", editDir, OTA, checkOTAStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus, true},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus, true},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus, true},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus, true},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus},
    {"WiFi", editDir, WIFI_DIR, checkWifiStatus, true},
};

UIDir WIFI_scanned[array_length(scan_wifi_stringed)] = { // Допустим максимум 50 точек
    {"*Status*", nullptr, NONE, nullptr, true}};
UIDir WIFI_saved[array_length(wifi_devices) + 1] = {
    {"*Status*", nullptr, NONE, nullptr, true}};
UIDir WIFI[] = {
    {"*Status*", nullptr, NONE, nullptr, true}, // Пустые для статуса
    {"Saved WiFi", editDir, WIFI_SAVED, load_saved_wifi},
    {"Scan WiFi", editDir, WIFI_SCANNED, scan_wifi},
    {"Disconnect", nullptr, NONE, wifi_disconnect}};
UIDir WIFI_debug[] = {
    {"Connect to: ", nullptr, NONE, nullptr, true}, // Если везде служебка то не отображаем
    {"*WiFI_Name*", nullptr, NONE, nullptr, true},
    {"...", nullptr, NONE, nullptr, true},
};

UIDir StorageFS[] = {
    {"Read File", editDir, StorageFS_R, openFileSystem},
    {"Delete File", editDir, StorageFS_D, openFileSystem}};
UIDir StorageFS_Delete[array_length(file_system_stringed)] = {
    {"*File*", nullptr, NONE, nullptr},
};
UIDir StorageFS_Read[array_length(file_system_stringed)] = {
    {"*File*", nullptr, NONE, nullptr},
};
UIDir StorageFS_Read_File[1];

UIDir WebServer[] = {
    {"*Status_name*", nullptr, NONE, nullptr, true},
    {"*Status_ip*", nullptr, NONE, nullptr, true},
    {"Open", nullptr, NONE, webEditStatus}};
UIDir OTA_dir[] = {
    {"*Status_nameSSID*", nullptr, NONE, nullptr, true},
    {"*Status_nameHostDNS*", nullptr, NONE, nullptr, true},
    {"Unlock", nullptr, NONE, OTAEditStatus}};

void editDir(char mode, MenuType name)
{
  if (mode == '*')
  {
    directories[click_throughs].activeUnitY = scrollUnitY;
    click_throughs++;
    directories[click_throughs] = {name, 0};
    scrollUnitY = 0;
    scrollDisplayNow = 0;
    activeMenu = name;
  }
  else if (mode == '#')
  {
    activeMenu = directories[click_throughs - 1].previousActiveMenu;
    scrollUnitY = directories[click_throughs - 1].activeUnitY;
    scrollDisplayNow = scrollUnitY / optionsPerViewDisplay;
    click_throughs--;
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
      wifi_is_connecting = true;
      editDir('*', WIFI_DEBUG);
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
      wifi_is_connecting = true;
      editDir('*', WIFI_DEBUG);
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
  const char *statusAssemledText = nullptr;
  if (wifi)
  {
    strcpy(statusBuffer_wifi, "S: "); // Перезаписать первую строку
    strcat(statusBuffer_wifi, wifi);  // Добавить. Цель - соединить 2 переменные чара
    statusAssemledText = statusBuffer_wifi;
  };
  const char *status = wifi ? statusAssemledText : "S: No WiFI";
  WIFI[0].name = status;
  WIFI_saved[0].name = status;
  WIFI_scanned[0].name = status;
  WebServer[0].name = status;
  OTA_dir[0].name = status;
}
int count_of_scanned = 2; // Для статуса и лоадинга

void upload_scanned_wifi(int Async_count_of_scanned)
{
  count_of_scanned = Async_count_of_scanned;
  if (count_of_scanned != 0)
  {
    for (int i = 0; i < count_of_scanned; i++)
    {
      scan_wifi_stringed[i] = WiFi.SSID(i); // Новые ставим
      WIFI_scanned[i + 1] = {scan_wifi_stringed[i].c_str(), nullptr, NONE, tryToSome_wifi_link};
    };
  }
  WiFi.begin(actualWifiSSID, actualWifiPassword);
  checkWifiStatus();
}

void scan_wifi()
{
  count_of_scanned = 2;
  WiFi.scanNetworksAsync(upload_scanned_wifi);
  WIFI_scanned[1].name = "Loading...";
}

void webEditStatus()
{
  if (WiFi.localIP() != IPAddress(0, 0, 0, 0))
  {
    if (strcmp(WebServer[1].name, "S: Closed") == 0)
    {
      server.begin();
      isweb_stringed = WiFi.localIP().toString(); // Стринг должен жить вечно тк c_str просто указывет на него
      isweb = isweb_stringed.c_str();
      WebServer[2].name = "Close";
    }
    else
    {
      server.stop();
      isweb = nullptr;
      WebServer[2].name = "Open";
    }
    checkWebStatus();
  }
}

void checkWebStatus()
{
  const char *statusNAME = isweb ? isweb : "S: Closed";
  WebServer[1].name = statusNAME;
}

void OTAEditStatus()
{
  if (WiFi.localIP() != IPAddress(0, 0, 0, 0))
  {
    if (strcmp(OTA_dir[1].name, "S: Locked") == 0)
    {
      ArduinoOTA.begin();
      isOTA = true;
      OTA_dir[2].name = "Lock";
    }
    else
    {
      ArduinoOTA.end();
      isOTA = false;
      OTA_dir[2].name = "Unlock";
    }
    checkOTAStatus();
  }
}

void checkOTAStatus()
{
  const char *statusAssemledText = nullptr;
  if (isOTA)
  {
    strcpy(statusBuffer_ota, "S: "); // Перезаписать первую строку
    strcat(statusBuffer_ota, host_dns);  // Добавить. Цель - соединить 2 переменные чара
    statusAssemledText = statusBuffer_ota;
  };
  const char* statusOTA = isOTA ? statusAssemledText : "S: Locked";
  OTA_dir[1].name = statusOTA;
}

// Animation vars
const int framesAmount = 20; // Колво этапов анимации / прогрессия уменьшения скорости. Чем больше тем хуже тк высота экранчика то маленькая

struct animationData
{
  int coordinates[framesAmount];
};

unsigned long millis_necessary_to_frame = 500 / framesAmount; // Для неблок делеек и синхрона с общим ФПС
unsigned long millis_spent = 0;
// const float amountOfSpeedAtFirstStage = 100.0 / 100.0 + 1.0; // Сколько процентов хавает на первый шаг + 100% тк нам не нужен ноль
animationData /* Указатель на массив */ smoothAnimateCoordinatesReturner(int x1, int x2, char mode = 'o', float amountOfSpeedAtFirstStage = 100.0 / 100.0 + 1.0)
{ // Из точки x1 в точку x2. Мод - i = ease-in. o = ease-out, q - ease-in-out
  animationData data;
  int length = x2 - x1;
  const float soleUnitOfMotion = (float)length / (float)framesAmount; // Указываем, иначе может произойти целочисленное деление
  float theChangeOfSpeedPerStage = (amountOfSpeedAtFirstStage - 1) / (framesAmount / 2);
  bool wasOnce = false;
  float accumulated_motion = 0.0;
  switch (mode)
  {
  case 'i':
    for (int i = 0; i < framesAmount; i++)
    {
      data.coordinates[i] = soleUnitOfMotion;
    }
    break;
  case 'o':
    for (int i = 0; i < framesAmount; i++)
    {

      float toForward = soleUnitOfMotion;
      if ((int)round((amountOfSpeedAtFirstStage - theChangeOfSpeedPerStage * i) * 1000) == 1000)
      {
        wasOnce = true;
      }
      toForward *= wasOnce ? (amountOfSpeedAtFirstStage - (theChangeOfSpeedPerStage * i) - theChangeOfSpeedPerStage)
                           : (amountOfSpeedAtFirstStage - theChangeOfSpeedPerStage * i);
      accumulated_motion += toForward;
      float absolute_position_float = (float)x1 + accumulated_motion;
      int calculated_y = round(absolute_position_float);
      calculated_y = calculated_y >= 0 ? calculated_y : 0;
      // data.coordinates[i] = startPosition + toForward;
      // data.coordinates[i] = data.coordinates[i] >= 0 ? data.coordinates[i] : 0;

      if (x2 > x1)
      {
        data.coordinates[i] = calculated_y <= x2 ? calculated_y : x2;
      }
      else
      {
        data.coordinates[i] = calculated_y >= x2 ? calculated_y : x2;
      }

    } // Не можем сравнивать флот с флотом из за фундаментальных ограничений. Сравниваем по сути с единицей но если округлить 1700/1000 то будет 1 что true... Поэтому сравниваем с 1000
    // data.coordinates[framesAmount - 1] = x2;
    break;
  case 'q':
    for (int i = 0; i < framesAmount; i++)
    {
      data.coordinates[i] = soleUnitOfMotion;
    }
    break;
  }
  return data;
}

void animation_andRender_scrollbar(int y1, int y2, char mode = 'o', float amountOfSpeedAtFirstStage = 100.0 / 100.0 + 1.0)
{
  static int oldY1 = -9999;
  static int oldY2;
  static int previousY1_forAnimation;
  static int previousY2_forAnimation;
  if (oldY1 == -9999)
  {
    oldY1 = y1;
    previousY1_forAnimation = y1;
    oldY2 = y2;
    previousY2_forAnimation = y2;
  }
  static int nowFrame = 0;
  static bool isSteps = false;
  static animationData coordsForTopSide;
  static animationData coordsForBottomSide;
  if (oldY1 == y1 && oldY2 == y2 && !isSteps)
  {
    u8g2.drawLine(oled_width, oldY1, oled_width, oldY2);
  }
  else if (oldY1 != y1 || oldY2 != y2)
  {

    coordsForTopSide = smoothAnimateCoordinatesReturner(previousY1_forAnimation, y1);
    coordsForBottomSide = smoothAnimateCoordinatesReturner(previousY2_forAnimation, y2);
    // for (int i = 0; i < array_length(coordsForTopSide.coordinates); i++)
    // {
    //   _print(coordsForTopSide.coordinates[i]);
    //   _println(" - top");
    // }
    // for (int i = 0; i < array_length(coordsForBottomSide.coordinates); i++)
    // {
    //   _print(coordsForBottomSide.coordinates[i]);
    //   _println(" - bottom");
    // }
    oldY1 = y1;
    oldY2 = y2;

    nowFrame = 0;
    isSteps = true;
  }
  if (millis() - millis_spent >= millis_necessary_to_frame && isSteps)
  { // Каждый например 4 фпс делаем фрейм (Чтобы по времени все было)
    millis_spent = millis();
    if (nowFrame < framesAmount)
    {
      previousY1_forAnimation = coordsForTopSide.coordinates[nowFrame];
      previousY2_forAnimation = coordsForBottomSide.coordinates[nowFrame];
      u8g2.drawLine(oled_width, coordsForTopSide.coordinates[nowFrame], oled_width, coordsForBottomSide.coordinates[nowFrame]);
      // _print(previousY1_forAnimation);
      // _print(" (nowFrame < maxSteps) ");
      // _println(previousY2_forAnimation);
      nowFrame++;
    }
    else
    {
      u8g2.drawLine(oled_width, previousY1_forAnimation, oled_width, previousY2_forAnimation);
      // _print(previousY1_forAnimation);
      // _print(" (is Steps = false) ");
      // _println(previousY2_forAnimation);
      isSteps = false;
    }
  }
  else if (isSteps)
  {
    u8g2.drawLine(oled_width, previousY1_forAnimation, oled_width, previousY2_forAnimation);
    // _print(previousY1_forAnimation);
    // _print(" (else if isSteps, but not necessary FPS) ");
    // _println(previousY2_forAnimation);
  }
}
unsigned long millis_spent_pointBar = 0;

struct animationData_disappearing
{
  animationData coord_wrapper;
  int y = NONE;
  int dir_now = 0;
  int display_now = 0;
  int now_frame = 0;
  unsigned long previousMillis = 0;
  int previousX2_forAnimation = 0;
};

animationData_disappearing disappearing_pointBars[20]{};
// int amount_disappearing_pointBars = 0;
void processing_disappearing_pointBars(int index)
{
  int y_coord = disappearing_pointBars[index].y;
  int x_coord_now = disappearing_pointBars[index].coord_wrapper.coordinates[disappearing_pointBars[index].now_frame];
  if (millis() - disappearing_pointBars[index].previousMillis >= millis_necessary_to_frame)
  {
    disappearing_pointBars[index].previousMillis = millis();
    if (disappearing_pointBars[index].now_frame < framesAmount)
    {
      if (scrollDisplayNow == disappearing_pointBars[index].display_now && click_throughs == disappearing_pointBars[index].dir_now)
      {
        u8g2.drawLine(0, y_coord, x_coord_now, y_coord);
      }
      disappearing_pointBars[index].previousX2_forAnimation = x_coord_now;
      disappearing_pointBars[index].now_frame++;
    }
    else
    {
      disappearing_pointBars[index].y = NONE;
    }
  }
  else
  {
    u8g2.drawLine(0, y_coord, disappearing_pointBars[index].coord_wrapper.coordinates[disappearing_pointBars[index].now_frame], y_coord);
  }
}
void animation_andRender_pointBar(int y, int x2, char mode = 'o', float amountOfSpeedAtFirstStage = 100.0 / 100.0 + 1.0)
{
  static int oldY = -9999;
  static int oldscrollDisplayNow;
  static int oldСlick_throughs;
  static int oldX;
  static int previousX2_forAnimation_2;
  if (oldY == -9999)
  {
    oldY = y;
    oldscrollDisplayNow = scrollDisplayNow;
    oldСlick_throughs = click_throughs;
    oldX = x2;
    previousX2_forAnimation_2 = x2;
    // oldX2 = x2;
  }
  static int nowFrame = 0;
  static bool isSteps = false;
  static animationData coordsForRightSide_main;
  if (oldY == y && oldX == x2 && !isSteps)
  {
    u8g2.drawLine(0, y, x2, y);
  }
  else if (oldY != y || oldX != x2)
  {
    coordsForRightSide_main = smoothAnimateCoordinatesReturner(0, x2);
    for (int i = 0; i < array_length(disappearing_pointBars); i++)
    {
      if (disappearing_pointBars[i].y == y)
      {
        disappearing_pointBars[i].y = NONE;
        coordsForRightSide_main = smoothAnimateCoordinatesReturner(disappearing_pointBars[i].previousX2_forAnimation, x2);
        break;
      }
    }

    for (int i = 0; i < array_length(disappearing_pointBars); i++)
    {
      if (disappearing_pointBars[i].y == NONE)
      {
        disappearing_pointBars[i] = {smoothAnimateCoordinatesReturner(previousX2_forAnimation_2, 0), oldY, oldСlick_throughs, oldscrollDisplayNow};
        break;
        // amount_disappearing_pointBars++;
      }
    }
    // for (int i = 0; i < array_length(disappearing_pointBars); i++)
    // {
    //   _print(disappearing_pointBars[i].coord_wrapper.coordinates[0]);
    //   _print(" - ");
    //   _print(disappearing_pointBars[i].y);
    //   _print(" - ");
    //   _print(disappearing_pointBars[i].previousMillis);
    //   _print(" - ");
    //   _print(disappearing_pointBars[i].now_frame);
    //   _println();
    // }
    oldY = y;
    oldX = x2;
    oldscrollDisplayNow = scrollDisplayNow;
    oldСlick_throughs = click_throughs;
    nowFrame = 0;
    isSteps = true;
  }
  if (millis() - millis_spent_pointBar >= millis_necessary_to_frame && isSteps)
  { // Каждый например 4 фпс делаем фрейм (Чтобы по времени все было)
    millis_spent_pointBar = millis();
    if (nowFrame < framesAmount)
    {

      previousX2_forAnimation_2 = coordsForRightSide_main.coordinates[nowFrame];
      u8g2.drawLine(0, y, previousX2_forAnimation_2, y);
      // _print(" (nowFrame < maxSteps) ");
      // _println(previousX2_forAnimation_2);
      nowFrame++;
    }
    else
    {
      u8g2.drawLine(0, y, previousX2_forAnimation_2, y);
      // _print(" (nowFrame < maxSteps) ");
      // _println(previousX2_forAnimation_2);

      isSteps = false;
    }
  }
  else if (isSteps)
  {
    u8g2.drawLine(0, y, previousX2_forAnimation_2, y);
    // _print(" (nowFrame < maxSteps) ");
    // _println(previousX2_forAnimation_2);
  }
  for (int i = 0; i < array_length(disappearing_pointBars); i++)
  {
    if (disappearing_pointBars[i].y != NONE)
    {
      processing_disappearing_pointBars(i);
    }
  }
}

void displayTools(UIDir array[], int length)
{ // Если не передавать длину то при счете функцией legnth-array будет указатель на указатель что не есть хорошо
  u8g2.clearBuffer();
  bool isServiseDisplay = false;
  if (array[scrollUnitY].isServiceOption)
  {
    bool isFreeFinded = false;
    for (int j = 0; j < length; j++)
    {
      if (!array[j].isServiceOption)
      {
        scrollUnitY = j;
        isFreeFinded = true;
        break;
      }
    }
    if (!isFreeFinded)
    {
      isServiseDisplay = true;
    }
  }
  if (!isServiseDisplay)
  {
    animation_andRender_pointBar(text_height * (scrollUnitY % optionsPerViewDisplay + 1) + 2 * (scrollUnitY % optionsPerViewDisplay), u8g2.getStrWidth(array[scrollUnitY].name));
  }
  for (int i = 0; i < (length - optionsPerViewDisplay * scrollDisplayNow); i++)
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
    u8g2.print(array[i + optionsPerViewDisplay * scrollDisplayNow].name);
  }
  animation_andRender_scrollbar(scrollDisplayNow * (oled_height * optionsPerViewDisplay / length), scrollDisplayNow * (oled_height * optionsPerViewDisplay / length) + (oled_height * optionsPerViewDisplay / length) > oled_height ? oled_height : scrollDisplayNow * (oled_height * optionsPerViewDisplay / length) + (oled_height * optionsPerViewDisplay / length)); // Скролл бар
  u8g2.sendBuffer();

  if (isPressedKey(0)) // *
  {
    if (array[scrollUnitY].function)
    {
      array[scrollUnitY].function();
    }
    if (array[scrollUnitY].edit)
    {
      array[scrollUnitY].edit('*', array[scrollUnitY].type);
    }
  }
  if (isPressedKey(1)) // #
  {
    if (click_throughs > 0)
    {
      editDir('#');
    }
  }
  if (isPressedKey(2) && !isServiseDisplay && scrollUnitY < length - 1) // ˅
  {
    if (array[scrollUnitY + 1].isServiceOption)
    {
      for (int i = scrollUnitY + 1; i < length; i++)
      {
        if (!array[i].isServiceOption)
        {
          scrollUnitY = i;
          break;
        }
      }
    }
    else
    {
      scrollUnitY++;
    }
    scrollDisplayNow = scrollUnitY / optionsPerViewDisplay;
  }
  if (isPressedKey(3) && !isServiseDisplay && scrollUnitY > 0) // ^
  {
    if (array[scrollUnitY - 1].isServiceOption)
    {
      for (int i = scrollUnitY - 1; i > 0; i--)
      {
        if (!array[i].isServiceOption)
        {
          scrollUnitY = i;
          break;
        }
      }
    }
    else
    {
      scrollUnitY--;
    }
    scrollDisplayNow = scrollUnitY / optionsPerViewDisplay;
  }
}

int count_of_files = 0;
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
    displayTools(WIFI_scanned, count_of_scanned);
    break;
  case StorageFS_D:
    displayTools(StorageFS_Delete, count_of_files);
    break;
  case StorageFS_R:
    displayTools(StorageFS_Read, count_of_files);
    break;
  case StorageFS_R_F:
    displayTools(StorageFS_Read_File, array_length(StorageFS_Read_File));
    break;
  case WIFI_DEBUG:
    displayTools(WIFI_debug, array_length(WIFI_debug));
    break;
  case NONE:
    break;
  }
}
void wifi_connecting_debug(const char *ssid)
{
  static int point_counter = 0;
  static unsigned long start_timer = millis();
  if (start_timer == 0)
  {
    start_timer = millis();
  }
  static unsigned long previousMillis_start_timer = 0;
  static char download_point_buffer[10];
  if (millis() - previousMillis_start_timer >= 500 && WiFi.status() != WL_CONNECTED && millis() - start_timer < 10000)
  {
    point_counter = point_counter % 3;
    previousMillis_start_timer = millis();
    // WIFI_debug[0].name = "Connecting to: ";
    WIFI_debug[1].name = ssid;
    // WIFI_debug[2].name = ".";
    strcpy(download_point_buffer, "."); // функции ориентируются на то, где стоит первый \0.
    for (int i = point_counter % 3; i > 0; i--)
    {
      strcat(download_point_buffer, ".");
    }
    WIFI_debug[2].name = download_point_buffer;
    point_counter++;
  }
  else if (WiFi.status() == WL_CONNECTED)
  {
    wifi_is_connecting = false;
    if (activeMenu == WIFI_DEBUG)
    {
      editDir('#');
    }
    start_timer = 0;
    wifi = ssid;
    checkWifiStatus();
  }
  else if (millis() - start_timer >= 10000)
  {
    if (activeMenu == WIFI_DEBUG)
    {
      editDir('#');
    }
    wifi_is_connecting = false;
    start_timer = 0;
    wifi = nullptr;
    checkWifiStatus();
  }
}

void delete_file()
{
  FlashEdit(StorageFS_Delete[scrollUnitY].name, "", -1, "d");
  editDir('#');
}

void read_file()
{
  read_file_stringed = FlashEdit(StorageFS_Read[scrollUnitY].name, "", -1, "r");
  StorageFS_Read_File[0] = {read_file_stringed.c_str(), nullptr, NONE, nullptr};
}

void openFileSystem()
{
  Dir dir = LittleFS.openDir("/");
  count_of_files = 0;
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
  }
}

// ------------------- Начало программы и луп  -----------------

// ------------------- FLASH  -----------------
char buffer[64]; // Для передач переменных в аргумент сообщения. Работа с адресными Чарами. Учитываем что 64 - максимальный размер передаваемого инта

String FlashEdit(const char *path, const char *message_string, const int message_int, const char *mode)
{
  if (flash_is_avialable)
  {
    if (path[0] != '/')
    {
      if (StorageFS_Delete[scrollUnitY].name)
      {
        strcpy(statusBuffer_file, "/");                                // Перезаписать первую строку
        strcat(statusBuffer_file, StorageFS_Delete[scrollUnitY].name); // Добавить. Цель - соединить 2 переменные чара
        path = statusBuffer_file;
      };
    }
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
    File file = LittleFS.open(path, mode);
    if (!file)
    {
      _print("Не обнаружен файл для функции ");
      _print(mode);
      _print(". По адресу: ");
      _println(path);
      return "";
    }
    if (strcmp(mode, "r") == 0) // Сравнивание
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
    if (strcmp(mode, "w") == 0)
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
    if (strcmp(mode, "a") == 0)
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
    _println();
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
  checkWifiStatus();
}
// String* name_tools = other_tools;
const long FPScounterIntervalCheck = 1000;
long FPScounterIntervalCheckPreviousMillis = 0;
int FPSCounter = 0;
void loop()
{
  FPSCounter++;
  if (millis() - FPScounterIntervalCheckPreviousMillis >= FPScounterIntervalCheck)
  {
    FPScounterIntervalCheckPreviousMillis = millis();
    for (int i = 0; i < array_length(directories); i++)
    {
      _print("previousActiveMenu - ");
      _println(directories[i].previousActiveMenu);
      _print("activeUnitY - ");
      _println(directories[i].activeUnitY);
    }
    _print(FPSCounter);
    _println(" - FPS");
    FPSCounter = 0;
  }
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
  if (wifi_is_connecting)
  {
    wifi_connecting_debug(actualWifiSSID);
  }
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