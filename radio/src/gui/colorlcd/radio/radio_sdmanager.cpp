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

#include "radio_sdmanager.h"

#include "edgetx.h"
#include "io/elrs_firmware_update.h"
#include "etx_lv_theme.h"
#include "file_browser.h"
#include "file_preview.h"
#include "fullscreen_dialog.h"
#include "io/bootloader_flash.h"
#include "io/uf2_flash.h"
#include "lib_file.h"
#include "menu.h"
#include "progress.h"
#include "sdcard.h"
#include "standalone_lua.h"
#include "view_text.h"

constexpr int WARN_FILE_LENGTH = 40 * 1024;

#define CELL_CTRL_DIR  LV_TABLE_CELL_CTRL_CUSTOM_1
#define CELL_CTRL_FILE LV_TABLE_CELL_CTRL_CUSTOM_2

RadioSdManagerPage::RadioSdManagerPage(const PageDef& pageDef) :
  PageGroupItem(pageDef)
{
}

template <class T>
class FlashDialog: public FullScreenDialog
{
 public:
  explicit FlashDialog(const T & device):
    FullScreenDialog(WARNING_TYPE_INFO, STR_FLASH_DEVICE),
    device(device)
  {
    progress = new Progress(this, {LCD_W / 2 - PROGRESS_W / 2, LCD_H / 2 + PROGRESS_YO, PROGRESS_W, EdgeTxStyles::UI_ELEMENT_HEIGHT});
  }

  void flash(const char * filename)
  {
    TRACE("flashing '%s'", filename);
    device.flashFirmware(
        filename,
        [=](const char *title, const char *message, int count,
            int total) -> void {
          setMessage(message);
          progress->setValue(total > 0 ? count * 100 / total : 0);
          lv_refr_now(nullptr);
        });
    deleteLater();
  }

 protected:
  T device;
  Progress* progress = nullptr;

  static LAYOUT_VAL_SCALED(PROGRESS_YO, 27)
  static LAYOUT_VAL_SCALED(PROGRESS_W, 200)
};

void RadioSdManagerPage::build(Window * window)
{
  window->padAll(PAD_ZERO);

  coord_t browserWidth = LANDSCAPE ? window->width() * 3 / 5 : window->width();
  coord_t browserHeight = LANDSCAPE ? window->height() : window->height() * 2 / 3;

  browser = new FileBrowser(window, {0, 0, browserWidth, browserHeight}, ROOT_PATH);
  browser->adjustWidth();

  coord_t previewX = (LANDSCAPE ? browserWidth : 0) + PAD_TINY;
  coord_t previewY = (LANDSCAPE ? 0 : browserHeight) + PAD_TINY;
  coord_t previewWidth = (LANDSCAPE ? window->width() - browserWidth : window->width()) - PAD_TINY * 2;
  coord_t previewHeight = (LANDSCAPE ? window->height() : window->height() - browserHeight) - PAD_TINY * 2;

  auto box = new Window(window, {previewX, previewY, previewWidth, previewHeight});

  loading = new StaticText(box, {0, 0, LV_SIZE_CONTENT, LV_SIZE_CONTENT}, STR_LOADING);
  loading->hide();
  lv_obj_center(loading->getLvObj());

  preview = new FilePreview(box, {0, 0, previewWidth, previewHeight});

  browser->setFileAction([=](const char* path, const char* name, const char* fullpath, bool isDir) {
      if (isDir)
        dirAction(path, name, fullpath);
      else
        fileAction(path, name, fullpath);
  });
  browser->setFileSelected([=](const char* path, const char* name, const char* fullpath, bool isDir) {
      preview->setFile(nullptr);
      loadPreview = 0;
      loading->hide();
      if (fullpath && !isDir) {
        auto ext = getFileExtension(fullpath);
        if (ext) {
          if (isExtensionMatching(ext, BITMAPS_EXT)) {
            previewFilename = fullpath;
            loadPreview = 10;
            loading->show();
          }
        }
      }
  });
  browser->refresh();
}

void RadioSdManagerPage::checkEvents()
{
  PageGroupItem::checkEvents();

  if (loadPreview) {
    loadPreview -= 1;
    if (loadPreview == 0) {
      loading->hide();
      auto filename = previewFilename;
      previewFilename = nullptr;
      preview->setFile(filename);
    }
  }
}

void RadioSdManagerPage::dirAction(const char* path, const char* name,
                                    const char* fullpath)
{
  if (strcmp(name, "..") == 0) return;

  auto menu = new Menu();
  menu->addLine(STR_RENAME_FILE, [=]() {
    uint8_t nameLength;
    uint8_t extLength;

    const char *ext = getFileExtension(name, 0, 0, &nameLength, &extLength);

    const uint8_t maxNameLength = SD_SCREEN_FILE_LENGTH - extLength;
    nameLength = min((uint8_t)(nameLength - extLength), maxNameLength);

    std::string fname(name, nameLength);
    std::string extension("");
    if (ext) extension = ext;

    new LabelDialog(fname.c_str(), maxNameLength, STR_RENAME_FILE, [=](std::string label) {
      label += extension;
      f_rename((const TCHAR *)name, (const TCHAR *)label.c_str());
      browser->refresh();
    });
  });
  menu->addLine(STR_DELETE_FILE, [=]() {
    if (f_unlink(fullpath) != FR_OK) {
      new MessageDialog(STR_DELETE_FILE, STR_DEL_DIR_NOT_EMPTY);
    }
    browser->refresh();
  });
}

void RadioSdManagerPage::fileAction(const char* path, const char* name,
                                    const char* fullpath)
{
  auto menu = new Menu();
  const char* ext = getFileExtension(name);
  if (ext) {
    if (!strcasecmp(ext, SOUNDS_EXT)) {
      menu->addLine(STR_PLAY_FILE, [=]() {
        audioQueue.stopAll();
        audioQueue.playFile(fullpath, 0, ID_PLAY_FROM_SD_MANAGER);
      });
    }
#if defined(HARDWARE_INTERNAL_MODULE) || defined(HARDWARE_EXTERNAL_MODULE)
    else if (!strcasecmp(ext, ELRS_FIRMWARE_EXT)) {
      menu->addLine(STR_FLASH_EXTERNAL_ELRS, [=]() {
        ElrsFirmwareUpdate(fullpath, EXTERNAL_MODULE);
      });
#endif
    } else if (!strcasecmp(BITMAPS_PATH, path) &&
               isExtensionMatching(ext, BITMAPS_EXT) &&
               strlen(name) <= LEN_BITMAP_NAME) {
      menu->addLine(STR_ASSIGN_BITMAP, [=]() {
        memcpy(g_model.header.bitmap, name, LEN_BITMAP_NAME);
        storageDirty(EE_MODEL);
      });
    } else if (!strcasecmp(ext, TEXT_EXT) || !strcasecmp(ext, LOGS_EXT) ||
               !strcasecmp(ext, SCRIPT_EXT)) {
      menu->addLine(STR_VIEW_TEXT, [=]() {
        FIL file;
        if (FR_OK == f_open(&file, fullpath, FA_OPEN_EXISTING | FA_READ)) {
          const int fileLength = file.obj.objsize;
          f_close(&file);

          if (fileLength > WARN_FILE_LENGTH) {
            char buf[64];
            sprintf(buf, " %s %dkB. %s", STR_FILE_SIZE, fileLength / 1024,
                    STR_FILE_OPEN);
            new ConfirmDialog(STR_WARNING, buf,
                              [=] { new ViewTextWindow(path, name, ICON_RADIO_SD_MANAGER); });
          } else {
            new ViewTextWindow(path, name, ICON_RADIO_SD_MANAGER);
          }
        }
      });
    }
    if (!strcasecmp(ext, FIRMWARE_EXT)) {
//TODO: Find out why UF2FirmwareUpdate is bricking
#if !defined(FIRMWARE_FORMAT_UF2)
      if (isBootloader(fullpath)) {
        menu->addLine(STR_FLASH_BOOTLOADER,
                      [=]() { BootloaderUpdate(fullpath); });
      }
#endif
    }
    else if (isExtensionMatching(ext, SCRIPTS_EXT)) {
      menu->addLine(STR_EXECUTE_FILE, [=]() {
        LuaRuntime::executeStandalone(fullpath);
      });
    }
  }
  menu->addLine(STR_COPY_FILE, [=]() {
    clipboard.type = CLIPBOARD_TYPE_SD_FILE;
    f_getcwd(clipboard.data.sd.directory, CLIPBOARD_PATH_LEN);
    strncpy(clipboard.data.sd.filename, name, CLIPBOARD_PATH_LEN - 1);
  });
  if (clipboard.type == CLIPBOARD_TYPE_SD_FILE) {
    menu->addLine(STR_PASTE, [=]() {
      static char lfn[FF_MAX_LFN + 1];  // TODO optimize that!
      char destFileName[2 * CLIPBOARD_PATH_LEN + 1];
      f_getcwd((TCHAR*)lfn, FF_MAX_LFN);
      // prevent copying to the same directory with the same name
      char* destNamePtr = clipboard.data.sd.filename;
      if (!strcmp(clipboard.data.sd.directory, lfn)) {
        destNamePtr =
            strAppend(destFileName, FILE_COPY_PREFIX, CLIPBOARD_PATH_LEN);
        destNamePtr = strAppend(destNamePtr, clipboard.data.sd.filename,
                                CLIPBOARD_PATH_LEN);
        destNamePtr = destFileName;
      }
      sdCopyFile(clipboard.data.sd.filename, clipboard.data.sd.directory,
                  destNamePtr, lfn);
      clipboard.type = CLIPBOARD_TYPE_NONE;

        browser->refresh();
      });
  }
  menu->addLine(STR_RENAME_FILE, [=]() {
    uint8_t nameLength;
    uint8_t extLength;

    const char *ext = getFileExtension(name, 0, 0, &nameLength, &extLength);

    const uint8_t maxNameLength = SD_SCREEN_FILE_LENGTH - extLength;
    nameLength = min((uint8_t)(nameLength - extLength), maxNameLength);

    std::string fname(name, nameLength);
    std::string extension("");
    if (ext) extension = ext;

    new LabelDialog(fname.c_str(), maxNameLength, STR_RENAME_FILE, [=](std::string label) {
      label += extension;
      f_rename((const TCHAR *)name, (const TCHAR *)label.c_str());
      browser->refresh();
    });
  });
  menu->addLine(STR_DELETE_FILE, [=]() {
    f_unlink(fullpath);
    browser->refresh();
    loadPreview = 0;
    preview->setFile(nullptr);
    loading->hide();
  });
}

#if defined(FIRMWARE_FORMAT_UF2)
void RadioSdManagerPage::FirmwareUpdate(const char* fn)
{
  UF2FirmwareUpdate firmwareUpdate;
  auto dialog =
      new FlashDialog<UF2FirmwareUpdate>(firmwareUpdate);
  dialog->flash(fn);
}
#else
void RadioSdManagerPage::BootloaderUpdate(const char* fn)
{
  BootloaderFirmwareUpdate bootloaderFirmwareUpdate;
  auto dialog =
      new FlashDialog<BootloaderFirmwareUpdate>(bootloaderFirmwareUpdate);
  dialog->flash(fn);
}
#endif

#if defined(BLUETOOTH)
void RadioSdManagerPage::BluetoothFirmwareUpdate(const char* fn)
{
  auto dialog = new FlashDialog<Bluetooth>(bluetooth);
  dialog->flash(fn);
}
#endif

#if defined(HARDWARE_INTERNAL_MODULE) || defined(HARDWARE_EXTERNAL_MODULE)


void RadioSdManagerPage::ElrsFirmwareUpdate(const char* fn,
                                             ModuleIndex module)
{
  ElrsDeviceFirmwareUpdate deviceFirmwareUpdate(module);
  auto dialog =
      new FlashDialog<ElrsDeviceFirmwareUpdate>(deviceFirmwareUpdate);
  dialog->flash(fn);
}
#endif
