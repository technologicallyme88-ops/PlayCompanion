#include "BluetoothSettingsActivity.h"

#include <GfxRenderer.h>
#include <Logging.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
constexpr uint32_t SCAN_MS = 15000;
constexpr uint32_t LIVE_REFRESH_MS = 500;
}

void BluetoothSettingsActivity::onEnter() {
  UiListActivity::onEnter();
  // Stage 3 may auto-suspend an enabled-but-disconnected BLE host to reclaim
  // heap and reduce radio power. Entering this screen is an explicit request to
  // manage Bluetooth, so resume the host before drawing status or scanning.
  if (SETTINGS.bluetoothEnabled && !BleHid.isRunning()) {
    if (!BleHid.begin("playcompanion")) LOG_ERR("BLEUI", "Failed to resume BLE host");
  }
  view = View::Main;
  status.clear();
  pendingConnectAddr.clear();
  lastLiveRefresh = 0;
  rebuildRows();
}

void BluetoothSettingsActivity::onExit() {
  if (BleHid.isRunning() && BleHid.isScanning()) BleHid.stopScan();
  Activity::onExit();
}

int BluetoothSettingsActivity::listCount() const {
  if (view == View::Main) return 4;
  return BleHid.deviceCount() > 0 ? static_cast<int>(BleHid.deviceCount()) : 1;
}

const char* BluetoothSettingsActivity::headerTitle() const {
  return view == View::Main ? "Bluetooth" : "Scan & Pair";
}

void BluetoothSettingsActivity::drawChrome() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.headerHeight}, headerTitle(),
                 status.empty() ? nullptr : status.c_str());
}

void BluetoothSettingsActivity::setView(const View next) {
  view = next;
  nav.reset();
  rebuildRows();
  requestUpdate();
}

std::string BluetoothSettingsActivity::pairedSummary() const {
  const uint8_t count = BleHid.isRunning() ? BleHid.pairedCount() : 0;
  if (count == 0) return "None";
  if (count == 1) return BleHid.paired(0).name;
  return std::to_string(count) + " paired";
}

void BluetoothSettingsActivity::rebuildRows() {
  rowLabels.clear();
  rowValues.clear();
  rowItems.clear();

  if (view == View::Main) {
    rowLabels = {"Bluetooth", "Scan & Pair", "Disconnect", "Forget Pairings"};
    rowValues.resize(4);
    rowValues[0] = SETTINGS.bluetoothEnabled ? "On" : "Off";
    if (!BleHid.isRunning()) {
      rowValues[1] = "Bluetooth off";
      rowValues[2] = "Not connected";
    } else {
      rowValues[1] = pairedSummary();
      rowValues[2] = BleHid.isConnected() ? BleHid.connectedName() : "Not connected";
    }
    rowValues[3] = pairedSummary();
  } else {
    const uint8_t count = BleHid.deviceCount();
    if (count == 0) {
      rowLabels.push_back(BleHid.isScanning() ? "Scanning..." : "No HID devices found");
      rowValues.emplace_back("");
    } else {
      rowLabels.reserve(count);
      rowValues.reserve(count);
      for (uint8_t i = 0; i < count; ++i) {
        const auto& d = BleHid.device(i);
        rowLabels.emplace_back(d.name[0] ? d.name : d.addr);
        char value[48];
        snprintf(value, sizeof(value), "%s%d dBm", d.hid ? "HID  " : "", d.rssi);
        rowValues.emplace_back(value);
      }
    }
  }

  rowItems.resize(rowLabels.size());
  for (size_t i = 0; i < rowLabels.size(); ++i) {
    rowItems[i].label = rowLabels[i].c_str();
    rowItems[i].value = rowValues[i].empty() ? nullptr : rowValues[i].c_str();
    rowItems[i].actionValue = static_cast<int16_t>(i);
  }
}

void BluetoothSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});

  rebuildRows();
  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);

  if (!status.empty()) {
    // The list carries the controls; status is intentionally represented as the
    // value of the first row on the next refresh to avoid an extra text surface.
    // Serial logs retain full diagnostics.
    LOG_DBG("BLEUI", "%s", status.c_str());
  }
}

void BluetoothSettingsActivity::toggleBluetooth() {
  if (BleHid.isRunning()) {
    BleHid.end();
    SETTINGS.bluetoothEnabled = 0;
    SETTINGS.saveToFile();
    status = "Bluetooth disabled";
  } else if (BleHid.begin("playcompanion")) {
    SETTINGS.bluetoothEnabled = 1;
    SETTINGS.saveToFile();
    status = "Bluetooth enabled";
  } else {
    SETTINGS.bluetoothEnabled = 0;
    SETTINGS.saveToFile();
    status = "Bluetooth failed to start";
  }
  rebuildRows();
  requestUpdate();
}

void BluetoothSettingsActivity::startScan() {
  if (!BleHid.isRunning()) {
    status = "Enable Bluetooth first";
    requestUpdate();
    return;
  }
  BleHid.startScan(SCAN_MS);
  lastLiveRefresh = millis();
  setView(View::Devices);
}

void BluetoothSettingsActivity::disconnectCurrent() {
  if (BleHid.isRunning() && BleHid.isConnected()) {
    BleHid.disconnect();
    status = "Disconnected";
  } else {
    status = "No device connected";
  }
  rebuildRows();
  requestUpdate();
}

void BluetoothSettingsActivity::forgetPairings() {
  if (!BleHid.isRunning()) {
    status = "Enable Bluetooth to forget bonds";
    requestUpdate();
    return;
  }
  if (BleHid.isConnected()) BleHid.disconnect();
  uint8_t removed = 0;
  while (BleHid.pairedCount() > 0) {
    const freeink::PairedHidDevice p = BleHid.paired(0);
    BleHid.forget(p.addr);
    ++removed;
  }
  status = removed ? "Pairings cleared" : "No pairings saved";
  rebuildRows();
  requestUpdate();
}

void BluetoothSettingsActivity::connectDevice(const int index) {
  if (BleHid.deviceCount() == 0 || index < 0 || index >= BleHid.deviceCount()) return;
  const auto& d = BleHid.device(static_cast<uint8_t>(index));
  if (!d.connectable) {
    status = "Device is not connectable";
    requestUpdate();
    return;
  }
  pendingConnectAddr = d.addr;
  status = std::string("Connecting to ") + d.name;
  BleHid.stopScan();
  if (!BleHid.connect(d.addr)) {
    status = "Could not start connection";
    pendingConnectAddr.clear();
  }
  requestUpdate();
}

void BluetoothSettingsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (view == View::Devices) {
    connectDevice(index);
    return;
  }
  switch (index) {
    case 0:
      toggleBluetooth();
      break;
    case 1:
      startScan();
      break;
    case 2:
      disconnectCurrent();
      break;
    case 3:
      forgetPairings();
      break;
    default:
      break;
  }
}

bool BluetoothSettingsActivity::handleCustomInput() {
  if (!BleHid.isRunning()) return false;

  uint32_t passkey = 0;
  if (BleHid.takePairingPasskey(passkey)) {
    char buf[40];
    snprintf(buf, sizeof(buf), "Pairing code: %06lu", static_cast<unsigned long>(passkey));
    status = buf;
    LOG_INF("BLEUI", "%s", buf);
    requestUpdate();
  }

  char failure[64] = {};
  if (BleHid.takeConnectFailure(failure, sizeof(failure))) {
    status = failure[0] ? failure : "Connection failed";
    pendingConnectAddr.clear();
    requestUpdate();
  }

  if (!pendingConnectAddr.empty() && BleHid.isConnected()) {
    status = std::string("Connected: ") + BleHid.connectedName();
    pendingConnectAddr.clear();
    BleHid.releaseScanResults();
    setView(View::Main);
    return false;
  }

  const unsigned long now = millis();
  if (view == View::Devices && now - lastLiveRefresh >= LIVE_REFRESH_MS) {
    lastLiveRefresh = now;
    rebuildRows();
    requestUpdate();
  }
  return false;
}

void BluetoothSettingsActivity::onBackButton() {
  if (view == View::Devices) {
    if (BleHid.isRunning() && BleHid.isScanning()) BleHid.stopScan();
    BleHid.releaseScanResults();
    setView(View::Main);
    return;
  }
  finish();
}
