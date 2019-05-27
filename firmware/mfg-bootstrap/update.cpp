#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

//#define UP_DEBUG

#define UPDATE_MAGIC      (0xACE1)
#define UPDATE_HEADER     (0x1)
#define UPDATE_IMAGE      (0x2)

MD5Builder update_md5;

#define PRODUCT           "Wi-Fi Stepper"
#define MODEL             "wsx100"
#define BRANCH            "stable"

#define LEN_PRODUCT       (36)
#define LEN_INFO          (36)
#define LEN_MESSAGE       (64)

#define ispacked __attribute__((packed))

typedef struct ispacked {
  uint16_t magic;
  char product[LEN_PRODUCT];
  char model[LEN_INFO];
  char swbranch[LEN_INFO];
  uint16_t version;
  char builddate[11];
} update_header_t;

typedef struct ispacked {
  char md5[33];
  uint32_t size;
} update_image_t;

typedef struct ispacked {
  char filename[36];
  char md5[33];
  uint32_t size;
} update_data_t;

typedef struct ispacked {
  char md5[33];
} update_md5_t;

inline void up_print(const char * str) { Serial.println(str); }
inline void up_print(const char * str1, const char * str2) { Serial.printf("%s%s\n", str1, str2); }
inline void up_print(const char * str1, const String & str2) { up_print(str1, str2.c_str()); }
inline void up_print(const char * str1, size_t i) { Serial.printf("%s %u\n", str1, i); }

typedef struct {
  bool iserror;
  char message[LEN_MESSAGE];
  uint8_t ontype;
  uint8_t preamble[123];
  size_t preamblelen, length;
  char imagemd5[33];
  unsigned int files;
} update_sketch;
update_sketch update;

#ifdef UP_DEBUG
#define up_debug(...) up_print(__VA_ARGS__)
#else
#define up_debug(...)
#endif

size_t update_handlechunk(uint8_t * data, size_t length) {
  up_debug("Handle chunk of size: ", length);
  ESP.wdtFeed();
  
  update_sketch * u = &update;
  size_t index = 0;

  // Handle preamble
  if (u->ontype == 0) u->ontype = data[index++];
  up_debug("On type: ", u->ontype);

  size_t preamble_size = 0;
  switch (u->ontype) {
    case UPDATE_HEADER: preamble_size = sizeof(update_header_t);    break;
    case UPDATE_IMAGE:  preamble_size = sizeof(update_image_t);     break;
    default: {
      // Unknown preamble, break out
      up_print("Unknown preamble in image file.");
      strlcpy(u->message, "Unknown preamble in image file.", LEN_MESSAGE);
      u->iserror = true;
      return 0;
    }
  }
  if (u->preamblelen < preamble_size) {
    size_t numbytes = min(preamble_size - u->preamblelen, length - index);
    memcpy(&u->preamble[u->preamblelen], &data[index], numbytes);
    u->preamblelen += numbytes;
    index += numbytes;
  }

  if (u->preamblelen == preamble_size) {
    // We have full preamble, start writing chunk
    switch (u->ontype) {
      case UPDATE_HEADER: {
        up_debug("Handle header chunk");
        
        update_header_t * header = (update_header_t *)u->preamble;
        if (header->magic != UPDATE_MAGIC || strcmp(header->model, MODEL) != 0) {
          // Bad header
          up_print("Bad image file header.");
          strlcpy(u->message, "Bad image file header.", LEN_MESSAGE);
          u->iserror = true;
          return 0;
        }
        up_print("Beginning image update");
        u->ontype = 0;
        u->preamblelen = u->length = 0;
        break;
      }
      case UPDATE_IMAGE: {
        up_debug("Handle image chunk");
        
        if (index == length) break;
        update_image_t * image = (update_image_t *)u->preamble;
        
        if (u->length == 0) {
          // Begin image update
          WiFiUDP::stopAll();
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          Update.setMD5(image->md5);
          if (!Update.begin(maxSketchSpace)) {
            Update.printError(Serial);
            u->iserror = true;
            return 0;
          }
        }
        if (u->length < image->size) {
          size_t numbytes = min(image->size - u->length, length - index);
          if (Update.write(&data[index], numbytes) != numbytes) {
            Update.printError(Serial);
            u->iserror = true;
            return 0;
          }
          u->length += numbytes;
          index += numbytes;
        }
        if (u->length == image->size) {
          // End image update
          if (!Update.end(true)) {
            Update.printError(Serial);
            u->iserror = true;
            return 0;
          }

          up_print("Updated image");
          u->ontype = 0;
          u->preamblelen = u->length = 0;
          u->files += 1;
        }
        break;
      }
    }
  }

  return index;
}

bool update_firmware(const char * host, int port) {
  memset(&update, 0, sizeof(update_sketch));

  WiFiClient client;
  if (!client.connect(host, port) || !client.connected()) {
    Serial.println("[Update] ERROR: Failed to connect!");
    return false;
  }

  uint8_t data[512];
  while (client.available() || client.connected()) {
    ESP.wdtFeed();
    yield();
    
    size_t lendata = client.read(data, sizeof(data));
    if (lendata == 0) continue;

    size_t index = 0;
    while (!update.iserror && index < lendata) {
      index += update_handlechunk(&data[index], lendata - index);
    }
  }

  if (update.iserror) {
    up_print("[Update] ERROR: ", update.message);
    return false;
  }

  return !update.iserror;
}

