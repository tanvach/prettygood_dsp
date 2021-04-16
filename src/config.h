/* Prettygood3D Over Ear (OE-1) DSP board
 * Copyright (c) 2021, Pithawat Vachiramon, prettygood3d@gmail.com
 *
 * Development of this code was funded by Prettygood3D.com. Please support
 * Prettygood3D's efforts to develop open source software by purchasing the
 * DSP board and other Prettygood3D products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <FlashStorage.h>
#include <ArduinoJson.h>

#define START_ADDRESS 0
#define JSON_DOC_SIZE 2048
#define MSGPACK_SIZE 4048
const int WRITTEN_SIGNATURE = 0x1324CAAA;

typedef struct {
  int signature;
  char msgpack[MSGPACK_SIZE];
} MSGPACK;

FlashStorage(msgpack_flash_store, MSGPACK);

DynamicJsonDocument config_doc(JSON_DOC_SIZE);

void LoadConfig(boolean reload_defaults = false) {

    MSGPACK msgpack;
    msgpack = msgpack_flash_store.read();

    // If there's a valid signature already, get MsgPack from flash
    if ((msgpack.signature == WRITTEN_SIGNATURE) & (!reload_defaults)) {
        deserializeMsgPack(config_doc, msgpack.msgpack);
        return;
    }

    DynamicJsonDocument new_config_doc(JSON_DOC_SIZE);

    // Default values
    new_config_doc["volume"] = 0.7;

    // EQ Settings
    new_config_doc["filter_type"]  = 1; // 1 = Parametric EQ
    new_config_doc["filter_count"] = 7;
    new_config_doc["filter_fc"][0] = 59;
    new_config_doc["filter_db"][0] = 6.6;
    new_config_doc["filter_q"][0]  = 0.17;
    new_config_doc["filter_fc"][1] = 243;
    new_config_doc["filter_db"][1] = -6.4;
    new_config_doc["filter_q"][1]  = 1.71;
    new_config_doc["filter_fc"][2] = 811;
    new_config_doc["filter_db"][2] = 2.2;
    new_config_doc["filter_q"][2]  = 1.90;
    new_config_doc["filter_fc"][3] = 1189;
    new_config_doc["filter_db"][3] = 3.3;
    new_config_doc["filter_q"][3]  = 2.45;
    new_config_doc["filter_fc"][4] = 1645;
    new_config_doc["filter_db"][4] = -7.2;
    new_config_doc["filter_q"][4]  = 0.64;
    new_config_doc["filter_fc"][5] = 6243;
    new_config_doc["filter_db"][5] = -1.4;
    new_config_doc["filter_q"][5]  = 0.30;
    new_config_doc["filter_fc"][6] = 19864;
    new_config_doc["filter_db"][6] = -7.8;
    new_config_doc["filter_q"][6]  = 0.38;

    // Enhance bass (Psycho-acoustic bass)
    new_config_doc["enhance_bass"] = false;
    new_config_doc["enhance_bass_lr_vol"] = 1.0;
    new_config_doc["enhance_bass_bass_vol"] = 0.3;
    new_config_doc["enhance_bass_high_pass"] = 0;
    new_config_doc["enhance_bass_cutoff"] = 4;

    config_doc = new_config_doc;
}

void SaveConfig() {
    // Serialize and write to flash
    MSGPACK msgpack;
    serializeMsgPack(config_doc, msgpack.msgpack);
    msgpack.signature = WRITTEN_SIGNATURE;
    msgpack_flash_store.write(msgpack);
}