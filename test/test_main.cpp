#include <unity.h>  // Unity test harness entry points

#include <stdint.h>  // fixed-width integer helpers for uptime math
#ifndef ARDUINO
#include <cstdio>      // std::remove for snapshot cleanup
#include <filesystem>  // snapshot directory management
#include <fstream>     // std::ifstream to inspect exported images
#include <string>      // parse PGM header tokens

namespace {

std::filesystem::path SnapshotsDir() {
  static std::filesystem::path dir =
      std::filesystem::current_path() / "snapshots";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

std::filesystem::path SnapshotPath(const char* filename) {
  return SnapshotsDir() / filename;
}

void RemoveIfExists(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

}  // namespace
#endif
#include <asap/display/DetectorDisplay.h>  // display driver under test
#include <asap/input/Joystick.h>

using asap::display::DetectorDisplay;
using asap::display::DisplayFrame;
using asap::display::DisplayPins;
using asap::display::FontStyle;
using asap::display::FrameKind;

namespace {
constexpr DisplayPins kDummyPins{0, 0, 0};  // unused pins for the native mock
}  // namespace

void setUp(void) {}
void tearDown(void) {}

// BOOT screen – verifies headline/subtitle/version mapping.
void test_boot_frame_contents(void) {
  DetectorDisplay display(kDummyPins);
  display.drawBootScreen("0.1.0");

  TEST_ASSERT_EQUAL_UINT32(1, display.beginCount());  // begin() auto-invoked
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(FrameKind::Boot),
      static_cast<uint8_t>(display.lastFrameKind()));

  const DisplayFrame& frame = display.lastFrame();
  TEST_ASSERT_EQUAL_UINT8(3, frame.lineCount);
  TEST_ASSERT_EQUAL_STRING("ASAP DETECTOR", frame.lines[0].text);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FontStyle::Title),
                        static_cast<int>(frame.lines[0].font));
  TEST_ASSERT_EQUAL_UINT16(26, frame.lines[0].y);

  TEST_ASSERT_EQUAL_STRING("Titoozelock", frame.lines[1].text);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(FontStyle::Body),
                        static_cast<int>(frame.lines[1].font));
  TEST_ASSERT_EQUAL_UINT16(44, frame.lines[1].y);

  TEST_ASSERT_EQUAL_STRING("FW 0.1.0", frame.lines[2].text);
  TEST_ASSERT_FALSE(frame.spinnerActive);
}

// HEARTBEAT screen – focused on idle telemetry string layout.
void test_heartbeat_frame_updates_spinner(void) {
  DetectorDisplay display(kDummyPins);
  TEST_ASSERT_TRUE(display.begin());

  const uint32_t uptimeMs = 1750U;  // deterministic uptime for spinner math
  display.drawHeartbeatFrame(uptimeMs);

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(FrameKind::Heartbeat),
      static_cast<uint8_t>(display.lastFrameKind()));

  const DisplayFrame& frame = display.lastFrame();
  TEST_ASSERT_EQUAL_UINT8(2, frame.lineCount);
  TEST_ASSERT_FALSE(frame.spinnerActive);
  TEST_ASSERT_EQUAL_UINT8(0, frame.spinnerIndex);
  TEST_ASSERT_EQUAL_STRING("Detector ready", frame.lines[0].text);
  TEST_ASSERT_EQUAL_STRING("Uptime 1s", frame.lines[1].text);
}

// STATUS screen – only the top line should render when the second is empty.
void test_status_frame_handles_empty_second_line(void) {
  DetectorDisplay display(kDummyPins);
  TEST_ASSERT_TRUE(display.begin());

  display.showStatus("RF LINK", "");

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(FrameKind::Status),
      static_cast<uint8_t>(display.lastFrameKind()));
  TEST_ASSERT_EQUAL_UINT32(1, display.beginCount());

  const DisplayFrame& frame = display.lastFrame();
  TEST_ASSERT_FALSE(frame.spinnerActive);
  TEST_ASSERT_EQUAL_UINT8(1, frame.lineCount);
  TEST_ASSERT_EQUAL_STRING("RF LINK", frame.lines[0].text);
  TEST_ASSERT_EQUAL_UINT16(28, frame.lines[0].y);
}

#ifndef ARDUINO
// Snapshot smoke test – persists boot/heartbeat/status frames for inspection.
void test_snapshot_export_creates_pgm(void) {
  DetectorDisplay display(kDummyPins);
  TEST_ASSERT_TRUE(display.begin());

  display.drawBootScreen("0.1.0");

  const auto bootPath = SnapshotPath("boot_screen.pgm");
  RemoveIfExists(bootPath);
  TEST_ASSERT_TRUE(display.writeSnapshot(bootPath.string().c_str()));

  std::ifstream in(bootPath, std::ios::binary);
  TEST_ASSERT_TRUE(in.good());

  std::string magic;
  in >> magic;
  TEST_ASSERT_EQUAL_STRING("P5", magic.c_str());

  int width = 0;
  int height = 0;
  in >> width >> height;
  TEST_ASSERT_EQUAL_INT(256, width);
  TEST_ASSERT_EQUAL_INT(64, height);

  int maxVal = 0;
  in >> maxVal;
  TEST_ASSERT_EQUAL_INT(255, maxVal);

  in.get();  // consume the single whitespace between header and data
  in.close();

  TEST_ASSERT_TRUE(!display.pixelBuffer().empty());

  display.drawHeartbeatFrame(1750U);
  const auto heartbeatPath = SnapshotPath("heartbeat_screen.pgm");
  RemoveIfExists(heartbeatPath);
  TEST_ASSERT_TRUE(display.writeSnapshot(heartbeatPath.string().c_str()));

  display.showStatus("RF LINK", "LOCKED");
  const auto statusPath = SnapshotPath("status_rf_link.pgm");
  RemoveIfExists(statusPath);
  TEST_ASSERT_TRUE(display.writeSnapshot(statusPath.string().c_str()));

  // Confirm the boot snapshot is still present for manual inspection.
  TEST_ASSERT_TRUE(std::ifstream(bootPath, std::ios::binary).good());
}
#endif  // ARDUINO

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();
  RUN_TEST(test_boot_frame_contents);
  RUN_TEST(test_heartbeat_frame_updates_spinner);
  RUN_TEST(test_status_frame_handles_empty_second_line);
#ifndef ARDUINO
  RUN_TEST(test_snapshot_export_creates_pgm);
#endif
  // Joystick frame tests
  {
    DetectorDisplay d({0, 0, 0}); d.begin(); d.showJoystick(asap::input::JoyAction::Left);
    TEST_ASSERT_EQUAL_STRING("LEFT", d.lastFrame().lines[0].text);
  }
  {
    DetectorDisplay d({0, 0, 0}); d.begin(); d.showJoystick(asap::input::JoyAction::Right);
    TEST_ASSERT_EQUAL_STRING("RIGHT", d.lastFrame().lines[0].text);
  }
  {
    DetectorDisplay d({0, 0, 0}); d.begin(); d.showJoystick(asap::input::JoyAction::Up);
    TEST_ASSERT_EQUAL_STRING("UP", d.lastFrame().lines[0].text);
  }
  {
    DetectorDisplay d({0, 0, 0}); d.begin(); d.showJoystick(asap::input::JoyAction::Down);
    TEST_ASSERT_EQUAL_STRING("DOWN", d.lastFrame().lines[0].text);
  }
  {
    DetectorDisplay d({0, 0, 0}); d.begin(); d.showJoystick(asap::input::JoyAction::Click);
    TEST_ASSERT_EQUAL_STRING("CLICK", d.lastFrame().lines[0].text);
  }
  {
    DetectorDisplay d({0, 0, 0}); d.begin(); d.showJoystick(asap::input::JoyAction::Neutral);
    TEST_ASSERT_EQUAL_STRING("NEUTRAL", d.lastFrame().lines[0].text);
  }
  return UNITY_END();
}
