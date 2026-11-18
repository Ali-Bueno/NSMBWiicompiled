#include "accessibility/map/world_map.h"

#include "accessibility/a11y_log.h"
#include "accessibility/game/guest_call.h"
#include "accessibility/game/nsmbw_game.h"
#include "accessibility/guest_read.h"
#include "accessibility/level/level_monitor.h"
#include "accessibility/localization.h"
#include "accessibility/speech.h"
#include "nsmbw_va.h"

namespace a11y::map {
namespace {

// dCourseSelectGuide_c::m_instance (r13 - 23204, set by its constructor func_8000FC30) and the
// fields CourseNumberSet (func_80010B50) writes: mWorldNo +1032, mCourseNo +1036, mCourseType +1040.
constexpr std::uint32_t kGuideInstance = NSMBW_VA(0x80429EDCu);
constexpr std::uint32_t kGuideWorld = 1032;
constexpr std::uint32_t kGuideCourse = 1036;
constexpr std::uint32_t kGuideType = 1040;
constexpr std::uint32_t kMapWorldNo = NSMBW_VA(0x8042A52Du);  // dScWMap_c::m_WorldNo (u8)

// dWmLib::CourseType_e (decomp d_wm_lib.hpp).
enum CourseType : int {
    kNormal, kGhost, kTower, kCastle, kToad, kJunction, kCannon, kStage37, kAirship, kToadStart,
    kPeachCastle, kInvalid
};

// The stage IDs CourseName() knows for the node kinds that have no number of their own.
constexpr int kStageGhost = 20, kStageTower = 21, kStageCastle = 23, kStageToad = 25;
constexpr int kStageCannon = 35, kStageAirship = 37, kStagePeach = 40;

struct Node {
    int world = -1, course = -100, type = -1;
    bool operator==(const Node& o) const { return world == o.world && course == o.course && type == o.type; }
};

Node g_last;
int g_lastWorldSaid = -1;
bool g_wasOnMap = false;

bool ReadNode(Node& out) {
    std::uint32_t guide = 0, world = 0, course = 0, type = 0;
    if (!TryPointer(kGuideInstance, guide) || !TryU32(guide + kGuideWorld, world) ||
        !TryU32(guide + kGuideCourse, course) || !TryU32(guide + kGuideType, type)) {
        return false;
    }
    out.world = static_cast<int>(world);
    out.course = static_cast<int>(course);
    out.type = static_cast<int>(type);
    return true;
}

bool CourseCleared(int world, int course) {
    game::GuestCall call;
    call.Gpr(3, static_cast<std::uint32_t>(world));
    call.Gpr(4, static_cast<std::uint32_t>(course));
    return (call.Invoke<NSMBW_VA(0x800FAF50u)>() & 0xFFu) != 0;  // dWmLib::IsCourseClear
}

// Course numbers of a world's start node and of path points (CourseNumberSet writes -2 and
// below for nodes that are not courses).
constexpr int kStartCourse = 38;

std::string NodeText(const Node& n) {
    if (n.course < 0) return n.type == kJunction ? loc::Get("map_junction") : std::string();
    if (n.course == kStartCourse || n.type == kToadStart) return loc::Get("map_start");
    int stage = n.course;
    switch (n.type) {
        case kGhost: stage = kStageGhost; break;
        case kTower: stage = kStageTower; break;
        case kCastle: stage = kStageCastle; break;
        case kToad: case kToadStart: stage = kStageToad; break;
        case kCannon: stage = kStageCannon; break;
        case kAirship: stage = kStageAirship; break;
        case kPeachCastle: stage = kStagePeach; break;
        default: break;
    }
    std::string text = level::monitor::CourseName(n.world, stage);
    if (n.type != kToad && n.type != kToadStart && n.type != kJunction && CourseCleared(n.world, n.course)) {
        text += ", " + loc::Get("map_node_cleared");
    }
    return text;
}

}  // namespace

void Tick() {
    const bool onMap = game::InScene(game::Scene::WorldMap);
    if (!onMap) {
        g_wasOnMap = false;
        g_last = Node{};
        return;
    }
    if (!g_wasOnMap) {
        g_wasOnMap = true;
        g_lastWorldSaid = -1;
    }
    if (game::GamePaused()) return;  // a map menu is open: the menu reader speaks
    std::uint8_t worldNo = 0;
    if (TryU8(kMapWorldNo, worldNo) && worldNo != g_lastWorldSaid) {
        g_lastWorldSaid = worldNo;
        speech::Say(loc::Format("map_world", {{"n", std::to_string(worldNo + 1)}}), true);
    }
    Node n;
    if (!ReadNode(n) || n == g_last) return;
    g_last = n;
    RT_LOGF(RT_TAG_A11Y, "map: node world %d course %d type %d\n", n.world, n.course, n.type);
    speech::Say(NodeText(n), false);  // Say() skips the empty text of a plain path point
}

std::string Describe() {
    Node n;
    return ReadNode(n) ? NodeText(n) : std::string();
}

}  // namespace a11y::map
