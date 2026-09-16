/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "hal/adc_driver.h"
#include "myeeprom.h"
#include "edgetx.h"
#include "edgetx_helpers.h"
#include "storage.h"
#include "sdcard_common.h"
#include "sdcard_yaml.h"
#include "modelslist.h"
#include "model_config_file.h"
#include "model_config_adapter.h"

const char* readModelYaml(const char* filename, ModelData& model, const char* pathName)
{
  char path[256];
  getModelPath(path, filename, pathName);
  return loadModelConfig(path, model);
}

const char* readModelHeaderYaml(const char* filename, model_config::Header& header, const char* pathName)
{
  char path[256];
  getModelPath(path, filename, pathName);
  return loadModelConfigHeader(path, header);
}

static const char _wrongExtentionError[] = "wrong file extension";

const char* readModel(const char* filename, ModelData& model, const char* pathName)
{
  const char* ext = strrchr(filename, '.');
  if (!ext || strncmp(ext, YAML_EXT, 4) != 0) {
    return _wrongExtentionError;
  }

  return readModelYaml(filename, model, pathName);
}

const char * writeModelYaml(const char* filename)
{
    TRACE("YAML model writer");
    char path[256];
    getModelPath(path, filename);
    return saveModelConfig(path);
}

#if !defined(STORAGE_MODELSLIST)
// EEPROM slot simulation based on file names:
// - /MODELS/model[00-99].yml

void getModelNumberStr(uint8_t idx, char* model_idx)
{
  memcpy(model_idx, MODEL_FILENAME_PREFIX, sizeof(MODEL_FILENAME_PREFIX));
  model_idx[sizeof(MODEL_FILENAME_PREFIX)-1] = '0' + idx / 10;
  model_idx[sizeof(MODEL_FILENAME_PREFIX)]   = '0' + idx % 10;
  model_idx[sizeof(MODEL_FILENAME_PREFIX)+1] = '\0';
}
#endif

const char * writeModel()
{
#if defined(STORAGE_MODELSLIST)
  return writeModelYaml(g_eeGeneral.currModelFilename);
#else
  char fname[MODELIDX_STRLEN + sizeof(YAML_EXT)];
  getModelNumberStr(g_eeGeneral.currModel, fname);
  strcat(fname, YAML_EXT);
  return writeModelYaml(fname);
#endif
}

#if !defined(STORAGE_MODELSLIST)
void loadModelHeader(uint8_t id, ModelHeader* header)
{
  model_config::Header partial{};
  *header = ModelHeader{};

  if (modelExists(id)) {
    char fname[MODELIDX_STRLEN + sizeof(YAML_EXT)];
    getModelNumberStr(id, fname);
    strcat(fname, YAML_EXT);
    if (!readModelHeaderYaml(fname, partial)) *header = partial.header;
  }
}

const char * loadModel(uint8_t idx, bool alarms)
{
  char fname[MODELIDX_STRLEN + sizeof(YAML_EXT)];
  getModelNumberStr(idx, fname);
  strcat(fname, YAML_EXT);
  return loadModel(fname, alarms);
}

bool modelExists(uint8_t idx)
{
  char model_idx[MODELIDX_STRLEN];
  getModelNumberStr(idx, model_idx);
  GET_FILENAME(fname, MODELS_PATH, model_idx, YAML_EXT);

  FILINFO fno;
  return f_stat(fname, &fno) == FR_OK;
}

bool copyModel(uint8_t dst, uint8_t src)
{
  // TODO: overwrite possible?
  char model_idx_src[MODELIDX_STRLEN];
  char model_idx_dst[MODELIDX_STRLEN];
  getModelNumberStr(src, model_idx_src);
  getModelNumberStr(dst, model_idx_dst);

  GET_FILENAME(fname_src, MODELS_PATH, model_idx_src, YAML_EXT);
  GET_FILENAME(fname_dst, MODELS_PATH, model_idx_dst, YAML_EXT);

  if (sdCopyFile(fname_src, fname_dst) == nullptr) {
    // update headers
    memcpy(&modelHeaders[dst], &modelHeaders[src], sizeof(ModelHeader));
    return true;
  }

  return false;
}

static void swapModelHeaders(uint8_t id1, uint8_t id2)
{
  char tmp[sizeof(g_model.header)];
  memcpy(tmp, &modelHeaders[id1], sizeof(ModelHeader));
  memcpy(&modelHeaders[id1], &modelHeaders[id2], sizeof(ModelHeader));
  memcpy(&modelHeaders[id2], tmp, sizeof(ModelHeader));
}

void swapModels(uint8_t id1, uint8_t id2)
{
  char model_idx_1[MODELIDX_STRLEN];
  char model_idx_2[MODELIDX_STRLEN];
  getModelNumberStr(id1, model_idx_1);
  getModelNumberStr(id2, model_idx_2);

  GET_FILENAME(fname1, MODELS_PATH, model_idx_1, YAML_EXT);
  GET_FILENAME(fname1_tmp, MODELS_PATH, model_idx_1, ".tmp");
  GET_FILENAME(fname2, MODELS_PATH, model_idx_2, YAML_EXT);

  FILINFO fno;
  if (f_stat(fname2,&fno) != FR_OK) {
    if (f_stat(fname1,&fno) == FR_OK) {
      if (f_rename(fname1, fname2) == FR_OK)
        swapModelHeaders(id1,id2);
    }
    return;
  }

  if (f_stat(fname1,&fno) != FR_OK) {
    f_rename(fname2, fname1);
    return;
  }

  // just in case...
  f_unlink(fname1_tmp);

  if (f_rename(fname1, fname1_tmp) != FR_OK) {
    TRACE("Error renaming 1");
    return;
  }

  if (f_rename(fname2, fname1) != FR_OK) {
    TRACE("Error renaming 2");
    return;
  }

  if (f_rename(fname1_tmp, fname2) != FR_OK) {
    TRACE("Error renaming 1 tmp");
    return;
  }

  swapModelHeaders(id1,id2);
}

int8_t deleteModel(uint8_t idx)
{
  char model_idx[MODELIDX_STRLEN];
  getModelNumberStr(idx, model_idx);
  GET_FILENAME(fname, MODELS_PATH, model_idx, YAML_EXT);

  if (f_unlink(fname) != FR_OK) {
    return -1;
  }

  modelHeaders[idx].name[0] = '\0';
  return 0;
}

const char * backupModel(uint8_t idx)
{
  char * buf = reusableBuffer.modelsel.mainname;

  // check and create folder here
  const char * error = sdCheckAndCreateDirectory(BACKUP_PATH);
  if (error) {
    return error;
  }

  strncpy(buf, modelHeaders[idx].name, sizeof(g_model.header.name));
  buf[sizeof(g_model.header.name)] = '\0';

  int8_t i = sizeof(g_model.header.name)-1;
  uint8_t len = 0;
  while (i > 0) {
    if (!len && buf[i])
      len = i+1;
    if (len) {
      if (!buf[i])
        buf[i] = '_';
    }
    i--;
  }

  if (len == 0) {
    uint8_t num = idx + 1;
    char* s = strAppend(buf, STR_MODEL);
    strAppendUnsigned(s, num, 2);
    len = strlen(buf);
  }

#if defined(RTCLOCK)
  char * tmp = strAppendDate(&buf[len]);
  len = tmp - buf;
#endif

  strcpy(&buf[len], YAML_EXT);

#ifdef SIMU
  TRACE("SD-card backup filename=%s", buf);
#endif

  char model_idx[MODELIDX_STRLEN + sizeof(YAML_EXT)];
  getModelNumberStr(idx, model_idx);
  strcat(model_idx, YAML_EXT);

  return sdCopyFile(model_idx, MODELS_PATH, buf, BACKUP_PATH);
}

const char * restoreModel(uint8_t idx, char *model_name)
{
  char * buf = reusableBuffer.modelsel.mainname;
  strcpy(buf, model_name);
  strcpy(&buf[strlen(buf)], YAML_EXT);

  char model_idx[MODELIDX_STRLEN + sizeof(YAML_EXT)];
  getModelNumberStr(idx, model_idx);
  strcat(model_idx, YAML_EXT);

  const char* error = sdCopyFile(buf, BACKUP_PATH, model_idx, MODELS_PATH);
  if (!error) {
    loadModelHeader(idx, &modelHeaders[idx]);
  }

  return error;
}

#endif
