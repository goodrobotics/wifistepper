#include <vector>

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>

#include "wifistepper.h"

//#define UP_DEBUG

extern ESP8266WebServer server;
extern StaticJsonBuffer<2560> jsonbuf;

extern volatile bool flag_reboot;


#define UPDATE_MAGIC      (0xACE1)
#define UPDATE_TMPFNAME   ("/_update.tmp")
#define UPDATE_HEADER     (0x1)
#define UPDATE_IMAGE      (0x2)
#define UPDATE_DATA       (0x3)
#define UPDATE_MD5        (0x4)

MD5Builder update_md5;
std::vector<String> update_files;

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
inline void up_print(const char * str1, const char * str2) { Serial.print(str1); Serial.println(str2); }
inline void up_print(const char * str1, const String & str2) { up_print(str1, str2.c_str()); }
inline void up_print(const char * str1, size_t i) { Serial.print(str1); Serial.println(i); }

#ifdef UP_DEBUG
#define up_debug(...) up_print(__VA_ARGS__)
#else
#define up_debug(...)
#endif

size_t update_handlechunk(uint8_t * data, size_t length) {
  up_debug("Handle chunk of size: ", length);
  ESP.wdtFeed();
  
  update_sketch * u = &sketch.update;
  size_t index = 0;

  // Handle preamble
  if (u->ontype == 0) u->ontype = data[index++];
  up_debug("On type: ", u->ontype);

  size_t preamble_size = 0;
  switch (u->ontype) {
    case UPDATE_HEADER: preamble_size = sizeof(update_header_t);    break;
    case UPDATE_IMAGE:  preamble_size = sizeof(update_image_t);     break;
    case UPDATE_DATA:   preamble_size = sizeof(update_data_t);      break;
    case UPDATE_MD5:    preamble_size = sizeof(update_md5_t);       break;
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
    if (u->ontype == UPDATE_MD5) {
      // Zero out data bytes for md5 preamble (so it isn't used in calculation)
      memset(&data[index], 0, numbytes);
    }
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
  
      case UPDATE_DATA: {
        up_debug("Handle data chunk");
        
        if (index == length) break;
        update_data_t * datafile = (update_data_t *)u->preamble;
  
        if (u->length == 0) {
          // Start data file write
          SPIFFS.remove(UPDATE_TMPFNAME);
        }
        if (u->length < datafile->size) {
          size_t numbytes = min(datafile->size - u->length, length - index);
          File fp = SPIFFS.open(UPDATE_TMPFNAME, "a");
          {
            if (!fp) { up_print("Could not open data file."); strlcpy(u->message, "Could not open data file.", LEN_MESSAGE); u->iserror = true; return 0; }
            fp.write(&data[index], numbytes);
            fp.close();
          }
          u->length += numbytes;
          index += numbytes;
        }
        if (u->length == datafile->size) {
          // End data file write
          MD5Builder md5;
          md5.begin();
  
          File fp = SPIFFS.open(UPDATE_TMPFNAME, "r");
          {
            if (!fp) { up_print("Could not open data file for MD5 verification."); strlcpy(u->message, "Could not open data file for MD5 verification.", LEN_MESSAGE); u->iserror = true; return 0; }
            md5.addStream(fp, fp.size());
            fp.close();
          }
  
          md5.calculate();
          if (strcmp(datafile->md5, md5.toString().c_str()) != 0) {
            // Bad md5
            up_print("Bad data file MD5 verification."); 
            strlcpy(u->message, "Bad data file MD5 verification.", LEN_MESSAGE);
            u->iserror = true;
            return 0;
          }

          SPIFFS.remove(datafile->filename);
          if (!SPIFFS.rename(UPDATE_TMPFNAME, datafile->filename)) {
            up_print("Could not rename data file."); 
            strlcpy(u->message, "Could not rename data file.", LEN_MESSAGE);
            u->iserror = true;
            return 0;
          }
          update_files.push_back(String(datafile->filename));
          up_print("Updated file: ", datafile->filename);
          u->ontype = 0;
          u->preamblelen = u->length = 0;
          u->files += 1;
        }
        break;
      }
      case UPDATE_MD5: {
        up_debug("Handle md5 chunk");
        
        update_md5_t * md5 = (update_md5_t *)u->preamble;
        up_print("Checking against md5: ", md5->md5);
        strcpy(u->imagemd5, md5->md5);
        u->ontype = 0;
        u->preamblelen = u->length = 0;
        break;
      }
    }
  }

  if (index > 0) update_md5.add(data, index);
  return index;
}

void update_init() {
  server.on("/update/image", HTTP_POST, [](){
    up_debug("After upload");
    
    add_headers()
    check_auth()
    JsonObject& root = jsonbuf.createObject();
    root["status"] = !sketch.update.iserror? "ok" : "error";
    root["message"] = sketch.update.message;
    root["updated_files"] = sketch.update.files;
    if (sketch.update.iserror) root["possible_corruption"] = sketch.update.files > 0? "YES. Hard reset and use http://192.168.1.4/update/recovery if needed." : "no";
    JsonVariant v = root;
    server.send(200, "application/json", v.as<String>());
    jsonbuf.clear();
    flag_reboot = true;
    //if (sketch.update.preamble) free(sketch.update.preamble);
  }, []() {
    if (config.service.auth.enabled && !server.authenticate(config.service.auth.username, config.service.auth.password)) {
      return;
    }
    
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      up_debug("Upload file start");
      
      memset(&sketch.update, 0, sizeof(update_sketch));
      /*sketch.update.preamble = (uint8_t *)malloc(sizeof(update_header_t));
      if (!sketch.update.preamble) {
        up_print("Not enough memory.");
        strlcpy(sketch.update.message, "Not enough memory.", LEN_MESSAGE);
        sketch.update.iserror = true;
      }
      memset(&sketch.update.preamble, 0, sizeof(update_header_t));*/
      update_md5.begin();
      update_files.clear();
      update_files.push_back(String(FNAME_WIFICFG));
      update_files.push_back(String(FNAME_SERVICECFG));
      update_files.push_back(String(FNAME_DAISYCFG));
      update_files.push_back(String(FNAME_MOTORCFG));
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      up_debug("Upload file write");
      
      size_t index = 0;
      while (!sketch.update.iserror && index < upload.currentSize) {
        index += update_handlechunk(&upload.buf[index], upload.currentSize - index);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      up_debug("Upload file end");
      
      // Check md5
      if (!sketch.update.iserror) {
        update_md5.calculate();
        if (strcmp(sketch.update.imagemd5, update_md5.toString().c_str()) != 0) {
          // Bad md5
          strlcpy(sketch.update.message, "Bad image file MD5 verification.", LEN_MESSAGE);
          sketch.update.iserror = true;
        } else {
          // Delete orphaned files
          Dir root = SPIFFS.openDir("/");
          while (root.next()) {
            if (std::find(update_files.begin(), update_files.end(), root.fileName()) == update_files.end()) {
              up_print("Deleting orphaned file: ", root.fileName());
              SPIFFS.remove(root.fileName());
            }
          }
          up_print("Update complete.");
          strlcpy(sketch.update.message, "Upload complete (success).", LEN_MESSAGE);
        }
      }
    }
    yield();
  });
  server.on("/update/recovery", HTTP_GET, [](){
    check_auth()
    server.send(200, "text/html", "<html><body><h1>Recovery Image Upload</h1><h3>Select image update file</h3><form method='post' action='/update/image' enctype='multipart/form-data'><input type='file' name='image'><input type='submit' value='Upload'></form></body></html>");
  });
}

