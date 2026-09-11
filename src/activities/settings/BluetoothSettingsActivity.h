#pragma once

#include <BleKeyboardHost.h>

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Lightweight r11.4 BLE HID pairing screen built directly on FreeInk's
// BleKeyboardHost. It deliberately avoids a second Bluetooth abstraction layer:
// the SDK owns scanning, bonding, reconnect and key translation; this activity
// only owns UI and the persistent on/off preference.
class BluetoothSettingsActivity final : public UiListActivity {
 public:
  BluetoothSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("BluetoothSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;

 protected:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;
  void onBackButton() override;
  const char* headerTitle() const override;
  void drawChrome() override;

 private:
  enum class View : uint8_t { Main, Devices };
  View view = View::Main;

  std::string status;
  std::string pendingConnectAddr;
  unsigned long lastLiveRefresh = 0;

  std::vector<std::string> rowLabels;
  std::vector<std::string> rowValues;
  std::vector<freeink::ui::ListItem> rowItems;

  void setView(View next);
  void toggleBluetooth();
  void startScan();
  void disconnectCurrent();
  void forgetPairings();
  void connectDevice(int index);
  void rebuildRows();
  std::string pairedSummary() const;
};
