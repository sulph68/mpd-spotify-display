// Wifi network setup
#include <WiFi.h>
const char* ssid = "******"; // WiFi name
const char* password = "******"; // WiFi password

// MPD host information
uint16_t port = 6600;
char * host = "xxx.xxx.xxx.xxx"; // ip or dns

// Use WiFiClient class to create TCP connections
WiFiClient client;

// TFT display library and setup
#include <Arduino_GFX_Library.h>

#define GFX_BL 38 // default backlight pin, you may replace DF_GFX_BL to actual backlight pin
Arduino_DataBus *bus = new Arduino_SWPAR8(7 /* DC */, 6 /* CS */, 8 /* WR */, 9 /* RD */, 39 /* D0 */, 40 /* D1 */, 41 /* D2 */, 42 /* D3 */, 45 /* D4 */, 46 /* D5 */, 47 /* D6 */, 48 /* D7 */);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 5 /* RST */, 1 /* rotation */, true /* IPS */, 170 /* width */, 320 /* height */, 0, 0, 35 ,0);

// include HTTPClient
#include <HTTPClient.h>
const char *HTTP_HOST = "xxx.xxx.xxx.xxx";   
const uint16_t HTTP_PORT = 6680;   
const char *HTTP_PATH_TEMPLATE = "/local/current.jpg";
const uint16_t HTTP_TIMEOUT = 1000; // in ms, wait a while for server processing
char http_path[1024];

const char *DECODE_HOST = "xxx.xxx.xxx.xxx"; // unidecode server
const uint16_t DECODE_PORT = 5555;  
const uint16_t SPOTIPY_PORT = 5566; // assumes same address as unidecode server. 

WiFiClient http_client;
HTTPClient http;

// include JPEG library
#include "JpegFunc.h"

// Pin declare
#define BOOT_PIN GPIO_NUM_0
#define KEY_PIN GPIO_NUM_14
#define BAT_VOLT 4

// position of log bottom text
static int INACTIVITY_TIMEOUT = 300; // defined device screen timeout to sleep in ~seconds

// Internal setup
long lastMillis = 0;

// storage variables
char  old_title[256];
char  old_album[256];
char  old_artist[256];
char  old_state[40];
char  old_nextsongid[40];
char  old_songid[40];
int   old_artsize = -1;
int   update_art = 0;
int   title_offset = 0;
int   scroll_wait = 0;
long  pause_counter = 0;
bool  decoded = false;
bool  mpd_available = false;

/* JPEG callback Function
*/

// pixel drawing callback
static int jpegDrawCallback(JPEGDRAW *pDraw) {
  gfx->draw16bitRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

/*
 * MPD Connection functions
 */
// MPD client functions
int mpc_connect(char * host, int port) {
  char smsg[40];
  char rmsg[40];

  if (!client.connect(host, port)) {
      return 0;
  }

  String line;
  client.setTimeout(1); // setTimeout is in seconds.
  line = client.readStringUntil('\0');
  line.toCharArray(rmsg, line.length()+1);
  rmsg[line.length()-1] = 0;
  if (strncmp(rmsg,"OK",2) == 0) return 1;  
  return 0;
}

int mpc_command(char * buf) {
  char smsg[40];
  char rmsg[40];
  sprintf(smsg,"%s\n",buf);
  client.print(smsg);

  String line;
  client.setTimeout(1);
  line = client.readStringUntil('\0');
  line.toCharArray(rmsg, line.length()+1);
  rmsg[line.length()-1] = 0;
  if (strcmp(rmsg,"OK") == 0) return 1;
  return 0;
}

void mpc_error(char * buf) {
  while(1) {}
}

int getItem(String line, char * item, char * value, int len) {
  int pos1,pos2,pos3;
  pos1=line.indexOf(item);
  String line2;
  line2 = line.substring(pos1);
  pos2=line2.indexOf(":");
  pos3=line2.indexOf(0x0a);
  String line3;
  line3 = line2.substring(pos2+1,pos3);
  string2char(line3, value, len);
  return(strlen(value));
}

void string2char(String line, char * cstr4, int len) {
  char cstr3[256];
  line.toCharArray(cstr3, line.length()+1);
  int pos4 = 0;
  for (int i=0;i<strlen(cstr3);i++) {
    if (cstr3[i] == ' ' && pos4 == 0) continue;
    cstr4[pos4++] = cstr3[i];
    cstr4[pos4] = 0;
    if (pos4 == (len-1)) break;
  }
}

// Helper string functions
void substr(char *s, int a, int b, char *t) {
  // s as the string to manipulate, 
  // a as the initial offset 
  // b as the length of the string you want to extract
  // t char variable to hold the outcome 
  strncpy(t, s+a, b);
}

String IpAddress2String(const IPAddress& ipAddress) {
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ; 
}


/*
 * Drawing functions
 */

void draw_default_state(char *s) {
  // clear the update area
  gfx->fillRect(0, 0, 320, 158, BLACK);
  // Set the text size of the currently playing song
  gfx->setTextColor(WHITE, BLACK); // set a default text color before handing off to loop
  gfx->setTextSize(2 /* x scale */, 2 /* y scale */, 5 /* pixel_margin */);
  gfx->setCursor(0, 0);
  gfx->println(String(s));
  gfx->setTextSize(0 /* x scale */, 0 /* y scale */, 5 /* pixel_margin */);
}

void draw_log(String logtype, String s) {
  static int LOG_X = 0;
  static int LOG_Y = 160;
  gfx->setCursor(LOG_X, LOG_Y);
  gfx->setTextSize(0 /* x scale */, 0 /* y scale */, 5 /* pixel_margin */);   
  if (logtype == "spotify") {
    gfx->setTextColor(LIGHTGREY, DARKGREEN);
    gfx->fillRect(LOG_X, LOG_Y, 320, 9, DARKGREEN);
  } else if (logtype == "mpd") {
    gfx->setTextColor(DARKGREY, BLACK);
    gfx->fillRect(LOG_X, LOG_Y, 320, 9, BLACK);
  } else {
    gfx->setTextColor(DARKGREY, BLACK);
    gfx->fillRect(LOG_X, LOG_Y, 320, 9, BLACK);    
  }
  gfx->println(s);
  return;
}

void draw_battery_level() {
  gfx->fillRect(126, 150, 194, 9, BLACK);
  gfx->setCursor(127, 150);
  if (get_battery_level() >= 4300) {
    gfx->setTextColor(GREEN, BLACK);
  } else if (get_battery_level() >= 3900) {
    gfx->setTextColor(GREENYELLOW, BLACK);
  } else {
    gfx->setTextColor(RED, BLACK);
  }
  gfx->println("Voltage: " + String(get_battery_level()));
}

void draw_time_bar(char *p) {
  char currtime[40]; memset(currtime,0,sizeof(currtime));
  char fulltime[40]; memset(fulltime,0,sizeof(fulltime));
  // get the current and full song time, calculate time_bar
  int pos = String(p).indexOf(":");
  substr(p,0,pos,currtime);
  substr(p,pos+1,strlen(p),fulltime);
  int current_time = atoi(currtime);
  int song_time = atoi(fulltime);
  int time_bar = 0;
  if (song_time != 0) { // make sure no illegal divide by zero
    time_bar = (int)((float) current_time / (float) song_time * 194.0);
  }
  // draw the time bar
  gfx->fillRect(126, 42, 194, 13, BLACK);
  gfx->setTextSize(0 /* x scale */, 0 /* y scale */, 5 /* pixel_margin */);
  gfx->setTextColor(YELLOW, BLACK);
  gfx->setCursor(126, 44);
  gfx->println("Time: " + String(currtime) + "s / " + String(fulltime) + "s");
  // draw time bar
  gfx->drawFastHLine( 126 /* x */, 56 /* y */, 194 /* width */, MAROON/* color */);
  gfx->drawFastHLine( 126 /* x */, 56 /* y */, time_bar /* width */, RED/* color */);        
}

void draw_next_song(char *title, char *album, char *artist) {
  gfx->fillRect(126, 57, 194, 90, BLACK);
  gfx->setTextColor(PINK, BLACK);
  gfx->setCursor(126, 62);
  gfx->println("Next Track");
  gfx->setTextColor(PINK, BLACK);
  gfx->setCursor(126, 78);
  gfx->println(String(title));
  gfx->setCursor(126, 90);
  gfx->println(String(album));
  gfx->setCursor(126, 102);
  gfx->println(String(artist));
}

void draw_current_song(char *title, char *album, char *artist) {
  gfx->fillRect(0, 0, 320, 16, BLACK);
  // Set the text size of the currently playing song
  gfx->setTextColor(WHITE, BLACK);
  gfx->setTextSize(2 /* x scale */, 2 /* y scale */, 5 /* pixel_margin */);
  gfx->setCursor(0, 0);
  gfx->println(String(title));
  
  gfx->fillRect(0, 16, 320, 22, BLACK);
  gfx->setTextColor(LIGHTGREY, BLACK);
  gfx->setTextSize(1 /* x scale */, 1 /* y scale */, 5 /* pixel_margin */);
  gfx->setCursor(0, 19);
  gfx->println(String(album));
  
  gfx->setTextSize(0 /* x scale */, 0 /* y scale */, 5 /* pixel_margin */);
  gfx->setCursor(0, 30);
  gfx->println(String(artist));
}

/*
 * Simple HTTP fetching functions
 */

// http unidecode
String http_unidecode(String s /* decode endpoint path */) {
  HTTPClient unidecode;
  WiFiClient unidecode_http;
  char unidecode_path[128];
  unidecode.setTimeout(1000);
  s = s + '\0';
  strncpy(unidecode_path, s.c_str(), strlen(s.c_str()));
  unidecode.begin(unidecode_http, DECODE_HOST, DECODE_PORT, unidecode_path);
  int httpCode = unidecode.GET();
  if (httpCode == 200) {
    return unidecode.getString();
  } else {
    return "";
  }
}

// get spotify status
String spotify_status(String s /* decode endpoint path */) {
  HTTPClient http;
  WiFiClient wifi_http;
  char path[128];
  http.setTimeout(2000);
  s = s + '\0';
  strncpy(path, s.c_str(), strlen(s.c_str()));
  http.begin(wifi_http, DECODE_HOST, SPOTIPY_PORT, path);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String result = http.getString();
    return result;
  } else {
    return "";
  } 
}

/*
 * Button and sleep/wake functions
 */
 
// helper button functions
int buttonPressed(uint8_t button) {
  static uint16_t lastStates = 0;
  uint8_t state = digitalRead(button);
  if (state != ((lastStates >> button) & 1)) {
    lastStates ^= 1 << button;
    return state == LOW;
  }
  return false;
}

// sleep management functions
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0 : 
      // draw_default_state("Wakeup caused by external signal using RTC_IO"); 
      draw_default_state("Resuming...");
      break;
    case ESP_SLEEP_WAKEUP_EXT1 : 
      // draw_default_state("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER : 
      // draw_default_state("Wakeup caused by timer"); 
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : 
      // draw_default_state("Wakeup caused by touchpad"); 
      break;
    case ESP_SLEEP_WAKEUP_ULP : 
      // draw_default_state("Wakeup caused by ULP program"); 
      break;
    default : 
      draw_default_state("Starting...");
      // draw_default_state("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); 
      break;
  }
}

// get battery voltage
uint32_t get_battery_level() {
  return (analogRead(BAT_VOLT) * 2 * 3.3 * 1000) / 4096; // in volts
}

// Main setup 
void setup() {
  // setup all the pins
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH); // HIGH = On, LOW = Off
  // setup button 
  pinMode(KEY_PIN, INPUT); 
  esp_sleep_enable_ext0_wakeup(KEY_PIN,0); // 1 = high, 0 = low

  // init the display to black
  gfx->begin();
  gfx->fillScreen(BLACK);
  
  // Set the pin to wake up
  print_wakeup_reason();
  
  // We start by connecting to a WiFi network
  WiFi.begin(ssid, password);

  int cnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    cnt++;
    if ((cnt % 10) >= 5) {
      draw_log("default", "WiFi: Connecting to [" + String(ssid) + "] ....");
    } else {
      draw_log("default", "WiFi: Connecting to [" + String(ssid) + "] ..");
    }
  }
  draw_log("default", "IP: " + IpAddress2String(WiFi.localIP()) + " Server: " + String(host) + ":" + String(port) + " - ??");
  
  static int MPD_MAX_RETRY = 3;
  int mpd_retry = 0;
  while(1) {
    if (mpc_connect(host, port) == 1) {
      mpd_available = true;
      break;
    }
    delay(1000);
    if (mpd_retry > MPD_MAX_RETRY) {
      break;
    } else {
      mpd_retry++;
    }
  }
  if (mpd_available) {
    draw_log("default", "IP: " + IpAddress2String(WiFi.localIP()) + " Server: " + String(host) + ":" + String(port) + " - OK");
  } else {
    draw_log("default", "IP: " + IpAddress2String(WiFi.localIP()) + " MPD Server: None " + " - Only Spotify");
  }

  // setup album artwork http query path
  sprintf(http_path, HTTP_PATH_TEMPLATE);
  pause_counter = 0;
}

// Loop function
void loop() {
  // static int counter = 0;
  String line;
  char state[40];
  char smsg[40];
  
  char tft_title[64]; memset(tft_title,0,sizeof(tft_title));
  char tft_album[64]; memset(tft_album,0,sizeof(tft_album));
  char tft_artist[64]; memset(tft_artist,0,sizeof(tft_artist));
  char tft_next_title[64]; memset(tft_next_title,0,sizeof(tft_next_title));
  char tft_next_album[64]; memset(tft_next_album,0,sizeof(tft_next_album));
  char tft_next_artist[64]; memset(tft_next_artist,0,sizeof(tft_next_artist));

  // Check if still connected to server
  if (!client.connected()) {    
    client.stop();
    mpd_available = false;
  } else {
    mpd_available = true;
  }

  // check button state for trigger sleep
  if (buttonPressed(KEY_PIN)) {
    draw_default_state("Deep Sleep... zz");
    client.stop();
    WiFi.disconnect();
    delay(1000);
    esp_deep_sleep_start();
  }

  // compute inactivity timeout
  if (pause_counter > INACTIVITY_TIMEOUT) {
    draw_default_state("Going to sleep... zz");
    client.stop();
    WiFi.disconnect();
    delay(1000);
    esp_deep_sleep_start();
  }
  
  // code updates once X second
  long now = millis();
  if (now < lastMillis) lastMillis = now; // millis is overflow
  if (now - lastMillis > 900) {
    lastMillis = now;
    
    // do this section only if mpd is available and client is connected
    if (mpd_available && client.connected()) {
      // get the status of the music server
      sprintf(smsg,"status\n");
      client.print(smsg);
      client.setTimeout(1);
      line = client.readStringUntil('\0');
      getItem(line, "state:", state, sizeof(state));
  
      // get the current song id
      char songid[40]; memset(songid,0,sizeof(songid));
      getItem(line, "songid:", songid, sizeof(songid));
      
      // get the next song id
      char nextsongid[40]; memset(nextsongid,0,sizeof(nextsongid));
      getItem(line, "nextsongid:", nextsongid, sizeof(nextsongid));
      
      // get the time bar
      int draw_timebar = 0;
      char playtime[40]; memset(playtime,0,sizeof(playtime));
      int playtimeLen = getItem(line, "time:", playtime, sizeof(playtime));
  
      // if playing, get current song
      if (strcmp(state,"play") == 0) {
        pause_counter = 0;
        // update the play time every second
        // playtime
        if (playtimeLen != 0) draw_timebar = 1;
  
        // prefetch unidecoded information if there is any song change.
        String decoded_line = "";
        if ((strcmp(old_songid, songid) != 0 && atoi(songid) != 0) || (strcmp(old_nextsongid, nextsongid) != 0 && atoi(nextsongid) != 0) || decoded == false)  {
          // Do a prefetch HTTP fallback
          String endpoint = "/currentnext";
          decoded_line = http_unidecode(endpoint);  
          // End HTTP fallback
          if (decoded_line.length() == 0) {
            draw_log("mpd", "[U] No data from Unidecode server. Will use MPDSocket.");
            decoded = false;
          } else {
            draw_log("mpd", "[U] Unicode data decoded.");
            decoded = true;
          }
        } 
  
        // get the current song information from server
        int artistLen = 0;
        int albumLen = 0;
        int titleLen = 0;
        int draw_currentsong = 0;
        
        if (strcmp(old_songid, songid) != 0 && atoi(songid) != 0) {
          strcpy(old_songid, songid); // updates the songid.
  
          char artist[256]; memset(artist,0,sizeof(artist));
          char title[256]; memset(title,0,sizeof(title));
          char album[256]; memset(album,0,sizeof(album));
  
          // if information already from http, read. Otherwise Socket to MPD.
          if (decoded_line.length() != 0) {
            artistLen = getItem(decoded_line, "Artist:", artist, sizeof(artist));
            albumLen = getItem(decoded_line, "Album:", album, sizeof(album));
            titleLen = getItem(decoded_line, "Title:", title, sizeof(title));
          } else {
            sprintf(smsg,"currentsong\n");
            client.print(smsg);
            client.setTimeout(1);
            line = client.readStringUntil('\0');
            artistLen = getItem(line, "Artist:", artist, sizeof(artist));
            albumLen = getItem(line, "Album:", album, sizeof(album));
            titleLen = getItem(line, "Title:", title, sizeof(title));       
          }
          if (titleLen == 0) strcpy(title, "-");
          if (artistLen == 0) strcpy(artist, "-");
          if (albumLen == 0) strcpy(album, "-");
          // format strings so display doesn't wrap
          if (titleLen >= 25) { 
            substr(title, 0, 24, tft_title);
            strcat(tft_title,"..");
          } else {
            strcpy(tft_title, title);
          }
          if (albumLen >= 53) { 
            substr(album, 0, 51, tft_album);
            strcat(tft_album,"..");
          } else {
            strcpy(tft_album, album);            
          }
          if (artistLen >= 53) { 
            substr(artist, 0, 51, tft_artist);
            strcat(tft_artist,"..");
          } else {
            strcpy(tft_artist, artist);
          }
          strcpy(old_title,title); // keep the old title for scrolling
          strcpy(old_album,album); 
          strcpy(old_artist,artist); 
          // will make songid update in next cycle as it should never be -1
          if (titleLen == 0 && artistLen == 0 && albumLen == 0) {
            strcpy(old_songid, "-1");
          }
          title_offset = 0;
          scroll_wait = 0;
          // flag to update artwork and titles
          update_art = 0;
          draw_currentsong = 1;
        } else {
          // update the tft title value to scroll. song hasn't changed.
          // waits for scroll_wait seconds before scrolling begins
          if (strlen(old_title) >= 25 && scroll_wait > 7) {           
            int end_str = strlen(old_title);
            if ((title_offset + 24) < end_str) {
              end_str = title_offset + 24;
            } else {
              end_str--;
            }
            substr(old_title, title_offset, end_str, tft_title);
            // make sure that the scorll text does not overflow
            if (strlen(tft_title) > 25) {
              char tmp[27] = "";
              strncpy(tmp,tft_title,25);
              memset(tft_title,0,sizeof(tft_title));
              strncpy(tft_title,tmp,25);
            }
            if (title_offset >= (strlen(old_title) - 24)) {
              title_offset = 0;
              scroll_wait = 0;
              substr(old_title, 0, 24, tft_title); // if resetting, place ellipsis for a while
              strcat(tft_title,"..");
            } else {
              title_offset++;
            }
            if (strlen(old_album) >= 53) { 
              substr(old_album, 0, 51, tft_album);
              strcat(tft_album,"..");
            } else {
              strcpy(tft_album, old_album);            
            }
            if (strlen(old_artist) >= 53) { 
              substr(old_artist, 0, 51, tft_artist);
              strcat(tft_artist,"..");
            } else {
              strcpy(tft_artist, old_artist);
            }           
            draw_currentsong = 1;
          }
          scroll_wait++;
        }
  
        // Next song information has changed, fetch the next song information
        int next_artistLen = 0;
        int next_albumLen = 0;
        int next_titleLen = 0;
        int draw_nextsong = 0;
        if (strcmp(old_nextsongid, nextsongid) != 0 && atoi(nextsongid) != 0) {
          strcpy(old_nextsongid, nextsongid); // update old_nextsongid coz we are going to update it
  
          char next_artist[256]; memset(next_artist,0,sizeof(next_artist));
          char next_title[256]; memset(next_title,0,sizeof(next_title));
          char next_album[256]; memset(next_album,0,sizeof(next_album));
  
          // if information already from http, read. Otherwise Socket to MPD.
          if (decoded_line.length() != 0) {
            next_artistLen = getItem(decoded_line, "NextArtist:", next_artist, sizeof(next_artist));
            next_albumLen = getItem(decoded_line, "NextAlbum:", next_album, sizeof(next_album));
            next_titleLen = getItem(decoded_line, "NextTitle:", next_title, sizeof(next_title));
          } else {
            sprintf(smsg,"playlistid ");
            strcat(smsg, nextsongid);
            strcat(smsg, "\n");
            client.print(smsg);
            client.setTimeout(1);
            line = client.readStringUntil('\0');
            next_artistLen = getItem(line, "Artist:", next_artist, sizeof(next_artist));
            next_albumLen = getItem(line, "Album:", next_album, sizeof(next_album));
            next_titleLen = getItem(line, "Title:", next_title, sizeof(next_title));
          }
          if (next_titleLen == 0) strcpy(next_title, "-");
          if (next_artistLen == 0) strcpy(next_artist, "-");
          if (next_albumLen == 0) strcpy(next_album, "-");
          if (next_titleLen >= 30) { 
            substr(next_title, 0, 27, tft_next_title);
            strcat(tft_next_title,"..");
          } else {
            strcpy(tft_next_title, next_title);
          }
          if (next_albumLen >= 30) { 
            substr(next_album, 0, 27, tft_next_album);
            strcat(tft_next_album,"..");
          } else {
            strcpy(tft_next_album, next_album);
          }
          if (next_artistLen >= 30) { 
            substr(next_artist, 0, 27, tft_next_artist);
            strcat(tft_next_artist,"..");
          } else {
            strcpy(tft_next_artist, next_artist);
          }
          // will make nextsongid update in next cycle as it should never be -1
          if (next_titleLen == 0 && next_artistLen == 0 && next_albumLen == 0) {
            strcpy(old_nextsongid, "-1");
          }
          // flag to update nextsong info
          draw_nextsong = 1;
        }
        
        /* 
         *  Graphics update 
         */
        
        // obtain and update the album artwork
        if (update_art < 20) {
          int jpeg_result = 0;
          
          http.setTimeout(2000);
          http.begin(http_client, HTTP_HOST, HTTP_PORT, http_path);
          int httpCode = http.GET();
          if (httpCode <= 0) {
            String err = http.errorToString(httpCode);
            draw_log("mpd", "[H] GET code: " + err);
          } else {
            if (httpCode != 200) { // make sure to service outcome only if HTTP was successful
              draw_log("mpd", "[H] Not OK: " + String(httpCode));
            } else {
              int len = http.getSize();         
              if (len <= 0) { // make sure that content length is > 0
                draw_log("mpd", "[H] Bad image size: " + String(len));
              } else {
                if (len != old_artsize) {
                  // draw the update area only if album art changed. prevents flickering
                  gfx->fillRect(0, 42, 120, 115, WHITE);
                  unsigned long start = millis();
                  uint8_t *buf = (uint8_t *)malloc(len);
                  if (buf) {
                    static WiFiClient *http_stream = http.getStreamPtr();
                    jpeg_result = jpegOpenHttpStreamWithBuffer(http_stream, buf, len, jpegDrawCallback);
                    if (jpeg_result) {
                      jpeg_result = jpegDraw(false /* useBigEndian */, 4 /* x */, 46 /* y */, 120 /* widthLimit */, 115 /* heightLimit */);
                      draw_log("mpd", "[H] Album artwork loaded.");
                      old_artsize = len;
                    } else {
                      draw_log("mpd", "[H] Drawing error");
                    }
                    free(buf);
                  } else {
                    draw_log("mpd", "[H] Buffer error");
                  }
                } else {
                  draw_log("mpd", "[H] No change in album artwork size");
                }
              }
            }
          }
          update_art++;
        }
  
        // current track Update only if there is data
        if (draw_currentsong == 1) draw_current_song(tft_title, tft_album, tft_artist);
    
        // next track. Don't update if there is no data
        if (draw_nextsong == 1) draw_next_song(tft_next_title, tft_next_album, tft_next_artist);

        // draw the time bar and time information
        if (draw_timebar == 1) draw_time_bar(playtime);

        // draw battery level
        draw_battery_level();
  
        // Store old state to determine change later
        strcpy(old_state, state);      
      } else if (strcmp(state,"pause") == 0) {  // state = pause
        pause_counter++;
        draw_log("mpd", "[M] Sleeping in approx. " + String(INACTIVITY_TIMEOUT - pause_counter) + "s");
        if (strcmp(state,old_state) == 0) return;
        draw_default_state("Paused...");
        strcpy(old_nextsongid,"-1");
        strcpy(old_songid,"-1");
        old_artsize = -1;
        strcpy(old_state, state);
      } else if (strcmp(state,"stop") == 0) {  // state = stop
        pause_counter++;
        draw_log("mpd", "[M] Sleeping in approx. " + String(INACTIVITY_TIMEOUT - pause_counter) + "s");
        if (strcmp(state,old_state) == 0) return;
        draw_default_state("Stopped...");
        strcpy(old_nextsongid,"-1");
        strcpy(old_songid,"-1");
        old_artsize = -1;
        strcpy(old_state, state);
      } else {
        pause_counter++;
        draw_log("mpd", "[M] Sleeping in approx. " + String(INACTIVITY_TIMEOUT - pause_counter) + "s");              
        draw_default_state("Unknown State");
        strcpy(old_nextsongid,"-1");
        strcpy(old_songid,"-1");    
        old_artsize = -1;
        delay(1000);
      }
    } else {
      // get spotify state and parse information          
      // Fetch spotify status via HTTP
      String decoded_line = "";
      String endpoint = "/status";
      decoded_line = spotify_status(endpoint);  
      if (decoded_line.length() != 0) {
        char state[40]; memset(state,0,sizeof(state));
        getItem(decoded_line, "state:", state, sizeof(state));
        if (strcmp(state,"play") == 0) {      // spotify is playing
          int draw_timebar = 0;
          int artistLen = 0;
          int albumLen = 0;
          int titleLen = 0;
          int artworkLen = 0;
          int draw_currentsong = 0;
          int draw_nextsong = 0;
          char artist[256]; memset(artist,0,sizeof(artist));
          char title[256]; memset(title,0,sizeof(title));
          char album[256]; memset(album,0,sizeof(album));
          
          // get the time bar
          char playtime[40]; memset(playtime,0,sizeof(playtime));
          int playtimeLen = getItem(decoded_line, "time:", playtime, sizeof(playtime));
          if (playtimeLen != 0) draw_timebar = 1;

          // get playing track
          artistLen = getItem(decoded_line, "Artist:", artist, sizeof(artist));
          albumLen = getItem(decoded_line, "Album:", album, sizeof(album));
          titleLen = getItem(decoded_line, "Title:", title, sizeof(title));

          if (strcmp(old_title,title) != 0) {
            if (titleLen == 0) strcpy(title, "-");
            if (artistLen == 0) strcpy(artist, "-");
            if (albumLen == 0) strcpy(album, "-");
            // format strings so display doesn't wrap
            if (titleLen >= 25) { 
              substr(title, 0, 24, tft_title);
              strcat(tft_title,"..");
            } else {
              strcpy(tft_title, title);
            }
            if (albumLen >= 53) { 
              substr(album, 0, 51, tft_album);
              strcat(tft_album,"..");
            } else {
              strcpy(tft_album, album);            
            }
            if (artistLen >= 53) { 
              substr(artist, 0, 51, tft_artist);
              strcat(tft_artist,"..");
            } else {
              strcpy(tft_artist, artist);
            }
            title_offset = 0;
            scroll_wait = 0;
            // flag to update artwork and titles
            update_art = 0;
            draw_currentsong = 1;
          } else {
            // update the tft title value to scroll. song hasn't changed.
            // waits for scroll_wait seconds before scrolling begins
            if (strlen(old_title) >= 25 && scroll_wait > 7) {           
              int end_str = strlen(old_title);
              if ((title_offset + 24) < end_str) {
                end_str = title_offset + 24;
              } else {
                end_str--;
              }
              substr(old_title, title_offset, end_str, tft_title);
              // make sure that the scorll text does not overflow
              if (strlen(tft_title) > 25) {
                char tmp[27] = "";
                strncpy(tmp,tft_title,25);
                memset(tft_title,0,sizeof(tft_title));
                strncpy(tft_title,tmp,25);
              }
              if (title_offset >= (strlen(old_title) - 24)) {
                title_offset = 0;
                scroll_wait = 0;
                substr(old_title, 0, 24, tft_title); // if resetting, place ellipsis for a while
                strcat(tft_title,"..");
              } else {
                title_offset++;
              }
              if (strlen(old_album) >= 53) { 
                substr(old_album, 0, 51, tft_album);
                strcat(tft_album,"..");
              } else {
                strcpy(tft_album, old_album);            
              }
              if (strlen(old_artist) >= 53) { 
                substr(old_artist, 0, 51, tft_artist);
                strcat(tft_artist,"..");
              } else {
                strcpy(tft_artist, old_artist);
              }           
              draw_currentsong = 1;
            }
            scroll_wait++;           
          }

          if (strcmp(old_title,title) != 0 || strcmp(old_state,state) != 0) {
            draw_nextsong = 1;
            update_art = 0;
          }
          
          // obtain and update the album artwork
          if (update_art < 10) {
            int jpeg_result = 0;
              
            http.setTimeout(2000);
            http.begin(http_client, HTTP_HOST, SPOTIPY_PORT, http_path);
            int httpCode = http.GET();
            if (httpCode > 0) { // code should respond 200 at least
              if (httpCode == 200) {
                int len = http.getSize();         
                if (len <= 0) { // make sure that content length is > 0
                  draw_log("spotify", "[H] Bad image size");
                } else {
                  if (len != old_artsize) {
                    // draw the update area only if album art changed. prevents flickering
                    gfx->fillRect(0, 42, 120, 115, WHITE);
                    unsigned long start = millis();
                    uint8_t *buf = (uint8_t *)malloc(len);
                    if (buf) {
                      static WiFiClient *http_stream = http.getStreamPtr();
                      jpeg_result = jpegOpenHttpStreamWithBuffer(http_stream, buf, len, jpegDrawCallback);
                      if (jpeg_result) {
                        jpeg_result = jpegDraw(false /* useBigEndian */, 4 /* x */, 46 /* y */, 120 /* widthLimit */, 115 /* heightLimit */);
                        draw_log("spotify", "[H] Album artwork loaded.");
                        old_artsize = len;
                      } else {
                       draw_log("spotify", "[H] Drawing error");
                      }
                      free(buf);
                    } else {
                      draw_log("spotify", "[H] Buffer error");
                    }
                  } else {
                    draw_log("spotify", "[H] No change in album artwork size");
                  }
                }
              }
            }
            update_art++;
          }

          /* Graphics Udpate
           *  
           */
          strcpy(old_title,title); // keep the old title for scrolling
          strcpy(old_album,album); 
          strcpy(old_artist,artist); 
          
          // current track Update only if there is data
          if (draw_currentsong == 1) draw_current_song(tft_title, tft_album, tft_artist);

          // spotify doesn't support next track. just draw a fixed state
          if (draw_nextsong == 1) draw_next_song("Spotify does not support", "next track information", "in their APIs");
          
          // draw the time bar and time information
          if (draw_timebar == 1) draw_time_bar(playtime);     

          // draw battery level
          draw_battery_level();          
          draw_log("spotify", "[S] Display updated");
          // Store old state to determine change later
          strcpy(old_state, state);  
        } else if (strcmp(state,"stop") == 0) { // spotify is stopped
          pause_counter++;
          draw_log("spotify", "[S] Sleeping in approx. " + String(INACTIVITY_TIMEOUT - pause_counter) + "s");
          if (strcmp(state,old_state) == 0) return;
          draw_default_state("Spotify Stopped..");
          strcpy(old_state, state);
        } else if (strcmp(state,"auth") == 0) { // spotify needs re-authentication on server side
          pause_counter++;
          draw_log("spotify", "[S] Sleeping in approx. " + String(INACTIVITY_TIMEOUT - pause_counter) + "s");
          if (strcmp(state,old_state) == 0) return;
          draw_default_state("Re-Auth Spotify Server!");
          strcpy(old_state, state);
        } else if (strcmp(state,"unknown") == 0) { // spotify got unknown state, just cycle
          draw_log("spotify", "[S] Unknown state recieved");
          if (strcmp(state,old_state) == 0) return;
          delay(5000); // delay slightly, perhaps throttled.
          strcpy(old_state, state);
        } else {
          pause_counter++;
          draw_log("spotify", "[S] Sleeping in approx. " + String(INACTIVITY_TIMEOUT - pause_counter) + "s");
          if (strcmp(state,old_state) == 0) return;
          draw_default_state("No Spotify State..");
          strcpy(old_state, state);
        }
      } else {
        draw_log("spotify", "[S] No data from Spotify status server.");
      }
    }     
  }
}
