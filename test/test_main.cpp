#include <unity.h>  // Unity test harness entry points

#include <stdint.h>  // fixed-width integer helpers for uptime math
#ifndef ARDUINO
#include <cstdio>      // std::remove for snapshot cleanup
#include <filesystem>  // snapshot directory management
#include <fstream>     // std::ifstream to inspect exported images
#include <string>      // parse PGM header tokens

namespace
{

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

// Global snapshot numbering shared across tests so filenames are ordered
// according to the execution sequence in this file. This makes it easy to
// visually audit that each joystick action yielded the expected screen.
int& SnapCounter() {
  static int counter = -1;
  return counter;
}

// Note: SaveActionSnapshot is defined after DetectorDisplay is included

}  // namespace
#endif
#include <asap/display/DetectorDisplay.h>  // display driver under test
#include <asap/input/Joystick.h>
#include <asap/ui/UIController.h>
// For pre-filling PlayerPersistent in native tests
#include <asap/player/PlayerData.h>
#include <asap/player/Storage.h>

using asap::display::DetectorDisplay;
using asap::display::DisplayFrame;
using asap::display::DisplayPins;
using asap::display::FontStyle;
using asap::display::FrameKind;

namespace {
constexpr DisplayPins kDummyPins{0, 0, 0};  // unused pins for the native mock
}  // namespace

#ifndef ARDUINO
// Snapshot helper defined after DetectorDisplay is visible.
// Use short action mnemonics: longpress, up, down, left, right, click, neutral.
void SaveActionSnapshot(DetectorDisplay& display, const char* action)
{
  int& c = SnapCounter();
  ++c;
  char fname[64];
  std::snprintf(fname, sizeof(fname), "%03d_%s.pgm", c, action);
  const auto path = SnapshotPath(fname);
  RemoveIfExists(path);
  TEST_ASSERT_TRUE(display.writeSnapshot(path.string().c_str()));
}
#endif

void setUp(void)
{
}
void tearDown(void)
{
}

// BOOT screen – verifies headline/subtitle/version mapping.
void test_boot_frame_contents(void)
{
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
void test_heartbeat_frame_updates_spinner(void)
{
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
void test_status_frame_handles_empty_second_line(void)
{
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
// Snapshot smoke test — persists boot/heartbeat/status frames for inspection.
void test_snapshot_export_creates_pgm(void)
{
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
  TEST_ASSERT_EQUAL_INT(15, maxVal);

  in.get();  // consume the single whitespace between header and data
  in.close();

  // Native path does not expose raw buffer; rely on snapshot presence instead.

  display.drawHeartbeatFrame(1750U);
  const auto heartbeatPath = SnapshotPath("heartbeat_screen.pgm");
  RemoveIfExists(heartbeatPath);
  TEST_ASSERT_TRUE(display.writeSnapshot(heartbeatPath.string().c_str()));

  display.showStatus("RF LINK", "LOCKED");
  const auto statusPath = SnapshotPath("status_rf_link.pgm");
  RemoveIfExists(statusPath);
  TEST_ASSERT_TRUE(display.writeSnapshot(statusPath.string().c_str()));

  // Joystick snapshots for LEFT/RIGHT/UP/DOWN/CLICK/NEUTRAL
  display.showJoystick(asap::input::JoyAction::Left);
  const auto jsLeft = SnapshotPath("joystick_left.pgm");
  RemoveIfExists(jsLeft);
  TEST_ASSERT_TRUE(display.writeSnapshot(jsLeft.string().c_str()));

  display.showJoystick(asap::input::JoyAction::Right);
  const auto jsRight = SnapshotPath("joystick_right.pgm");
  RemoveIfExists(jsRight);
  TEST_ASSERT_TRUE(display.writeSnapshot(jsRight.string().c_str()));

  display.showJoystick(asap::input::JoyAction::Up);
  const auto jsUp = SnapshotPath("joystick_up.pgm");
  RemoveIfExists(jsUp);
  TEST_ASSERT_TRUE(display.writeSnapshot(jsUp.string().c_str()));

  display.showJoystick(asap::input::JoyAction::Down);
  const auto jsDown = SnapshotPath("joystick_down.pgm");
  RemoveIfExists(jsDown);
  TEST_ASSERT_TRUE(display.writeSnapshot(jsDown.string().c_str()));

  // Click is an edge in hardware; for snapshot we render the word
  display.showJoystick(asap::input::JoyAction::Click);
  const auto jsClick = SnapshotPath("joystick_click.pgm");
  RemoveIfExists(jsClick);
  TEST_ASSERT_TRUE(display.writeSnapshot(jsClick.string().c_str()));

  display.showJoystick(asap::input::JoyAction::Neutral);
  const auto jsNeutral = SnapshotPath("joystick_neutral.pgm");
  RemoveIfExists(jsNeutral);
  TEST_ASSERT_TRUE(display.writeSnapshot(jsNeutral.string().c_str()));

  // Confirm the boot snapshot is still present for manual inspection.
  TEST_ASSERT_TRUE(std::ifstream(bootPath, std::ios::binary).good());
}
#endif  // ARDUINO

#ifndef ARDUINO
// Anomaly HUD snapshot with per-indicator stages -, I, II, III for review
void test_anomaly_hud_stage_snapshots(void)
{
  using asap::ui::UIController;
  using asap::input::JoyAction;

  DetectorDisplay display(kDummyPins);
  TEST_ASSERT_TRUE(display.begin());
  UIController ui(display);

  // Configure distinct stages and exposure percentages
  ui.setAnomalyExposure(10, 35, 65, 5);   // percents within current turn
  ui.setAnomalyStage(0, 1, 2, 3);         // -, I, II, III

  // Render initial anomaly page
  ui.onTick(0, {/*centerDown=*/false, JoyAction::Neutral});
  SaveActionSnapshot(display, "anomaly_hud_stages_init");
}
#endif  // ARDUINO

#ifndef ARDUINO
// End-to-end UI navigation snapshots with numbered action-based filenames.
void test_ui_menu_navigation_snapshots(void)
{
  using asap::ui::UIController;
  using asap::input::JoyAction;

  DetectorDisplay display(kDummyPins);
  TEST_ASSERT_TRUE(display.begin());
  UIController ui(display);

  auto Save = [&](const char* action) { SaveActionSnapshot(display, action); };

  // Boot to anomaly page with bar at 0%, then 50%, then 100%.
  // Configure anomaly HUD to show all four stages on first render:
  // rad: '-', therm: 'I', chem: 'II', psy: 'III'
  ui.setAnomalyExposure(25, 50, 75, 100);
  ui.setAnomalyStage(0, 1, 2, 3);
  ui.setAnomalyStrength(0);
  ui.onTick(0, {/*centerDown=*/false, JoyAction::Neutral});
  Save("neutral");

  ui.setAnomalyStrength(50);
  ui.onTick(50, {/*centerDown=*/false, JoyAction::Neutral});
  Save("neutral");

  ui.setAnomalyStrength(100);
  ui.onTick(100, {/*centerDown=*/false, JoyAction::Neutral});
  Save("neutral");

  // Enter menu via long-press (1000ms center hold).
  ui.onTick(2000, {/*centerDown=*/true, JoyAction::Neutral});
  Save("longpress");
  ui.onTick(3000, {/*centerDown=*/true, JoyAction::Neutral});
  Save("longpress");

  // Navigate to Tracking, enter, confirm to switch main mode.
  ui.onTick(3100, {/*centerDown=*/false, JoyAction::Down});
  Save("down");

  ui.onTick(3200, {/*centerDown=*/false, JoyAction::Click}); // enter tracking page
  Save("click");

  ui.onTick(3300, {/*centerDown=*/false, JoyAction::Click}); // confirm & exit
  Save("click");

  // Re-enter menu and navigate to CONFIG; stop inside config page.
  ui.onTick(5000, {/*centerDown=*/true, JoyAction::Neutral});
  Save("longpress");
  ui.onTick(6000, {/*centerDown=*/true, JoyAction::Neutral});
  Save("longpress");

  ui.onTick(6100, {/*centerDown=*/false, JoyAction::Down}); // select TRACKING
  Save("down");

  ui.onTick(6200, {/*centerDown=*/false, JoyAction::Down}); // select PLAYER DATA
  Save("down");
  ui.onTick(6250, {/*centerDown=*/false, JoyAction::Down}); // select CONFIG
  Save("down");

  ui.onTick(6300, {/*centerDown=*/false, JoyAction::Click}); // enter CONFIG
  Save("click");
  // Assert we are in CONFIG list and selection starts at first item
  {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(FrameKind::Menu), static_cast<uint8_t>(display.lastFrameKind()));
    const DisplayFrame& f = display.lastFrame();
    TEST_ASSERT_EQUAL_UINT8(3, f.lineCount);
    TEST_ASSERT_EQUAL_STRING("  VERSION", f.lines[0].text); // prev wraps to last
    TEST_ASSERT_EQUAL_STRING("> INVERT X JOYSTICK", f.lines[1].text);
    TEST_ASSERT_EQUAL_STRING("  INVERT Y JOYSTICK", f.lines[2].text);
  }

  // (Rotation toggle test removed)

  // --- Player Data page snapshots ---
  // Prefill persistent with non-trivial values
  {
    asap::player::PlayerPersistent p{};
    asap::player::initDefaults(p);
    p.version = asap::player::kPersistentVersion;
    p.logic.fire_resistance = 3;
    p.logic.psy_resistance = 2;
    p.logic.radiation_resistance = 5;
    p.logic.chemical_resistance = 1;
    p.logic.armor = 7;
    p.logic.faction = 42;
    p.logic.money_units = 123; // 123*100 rubles
    // Small description text (with wrapping)
    const char* desc = "First line of text\r\nSecond line here\r\nAlongerlinethatshouldwrapacrossmultipledisplaylineswhenrenderedidon'thaveanymoreideasthisisstrange.Alongerlinethatshouldwrapacrossmultipledisplaylineswhenrenderedidon'thaveanymoreideasthisisstrange.";
    // Clear then copy
    for (size_t i=0;i<asap::player::kDescriptionSize;++i) p.description[i]='\0';
    size_t k=0; while(desc[k] && k<asap::player::kDescriptionSize){ p.description[k]=desc[k]; ++k; }
    asap::player::savePersistent(p);
  }
  // Open menu again
  ui.onTick(8000, {/*centerDown=*/true, JoyAction::Neutral});
  Save("longpress");
  ui.onTick(9000, {/*centerDown=*/true, JoyAction::Neutral});
  Save("longpress");
  // Move selection to PLAYER DATA (two downs from ANOMALY)
  ui.onTick(9100, {/*centerDown=*/false, JoyAction::Down});
  Save("down");
  ui.onTick(9200, {/*centerDown=*/false, JoyAction::Down});
  Save("down");
  // Enter PLAYER DATA
  ui.onTick(9300, {/*centerDown=*/false, JoyAction::Click});
  Save("click");
  // Scroll down through the list; snapshot each step
  ui.onTick(9400, {/*centerDown=*/false, JoyAction::Down});
  Save("down");
  ui.onTick(9500, {/*centerDown=*/false, JoyAction::Down});
  Save("down");
  ui.onTick(9600, {/*centerDown=*/false, JoyAction::Down});
  Save("down");
  ui.onTick(9700, {/*centerDown=*/false, JoyAction::Down});
  Save("down");
}
#endif  // ARDUINO

// Removed: detailed config submenu walkthrough; simplified flow is covered in the main navigation test.
#ifndef ARDUINO

int main(int, char**)
{
  // Clean previous snapshot files and reset counter so numbering is stable per run
  {
    int& c = SnapCounter();
    c = -1;
    const auto dir = SnapshotsDir();
    std::error_code ec;
    for (auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
      const auto& p = entry.path();
      if (p.extension() == ".pgm")
      {
        std::filesystem::remove(p, ec);
      }
    }
  }

  UNITY_BEGIN();
  RUN_TEST(test_anomaly_hud_stage_snapshots);
  RUN_TEST(test_ui_menu_navigation_snapshots);
  // Player data framing + CRC round-trip (native-only)
  void test_uart_frame_playerpersistent_roundtrip(void);
  RUN_TEST(test_uart_frame_playerpersistent_roundtrip);
  return UNITY_END();
}

#else

int main()
{
  // Native-only snapshot tests are not executed on embedded targets.
  return 0;
}

#endif

