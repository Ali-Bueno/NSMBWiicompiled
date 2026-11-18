#ifndef NSMBW_ACCESSIBILITY_LEVEL_ACTORS_H
#define NSMBW_ACCESSIBILITY_LEVEL_ACTORS_H

#include <cstdint>
#include <string>
#include <vector>

#include "accessibility/game/nsmbw_game.h"

namespace a11y::level {

// Enemy: can be stomped. Danger: hurts from above too (spikes, fire, piranha plants). Boss.
enum class ActorCategory { Unknown, Enemy, Danger, Boss, Item, StarCoin, Coin, Block, Goal, Door, Checkpoint, Prop };

inline bool IsHostile(ActorCategory c) {
    return c == ActorCategory::Enemy || c == ActorCategory::Danger || c == ActorCategory::Boss;
}

struct NearbyActor {
    game::ActorInfo info;
    ActorCategory category = ActorCategory::Unknown;
    std::string phraseKey;  // spoken name, "" for an unnamed enemy
    float dx = 0.0f, dy = 0.0f;  // tiles from the player's centre, +x right, +y up
    float distance = 0.0f;
};

// Classification from the profile table (actor_names.inc); unlisted profiles are Unknown.
ActorCategory Classify(const game::ActorInfo& info, std::string* phraseKey);

// Every interesting actor within the given tile box around the player, nearest first.
std::vector<NearbyActor> Scan(const game::PlayerState& player, float rangeX, float rangeY);

// The goal pole's position in the player's area, if one exists.
bool FindGoal(const game::PlayerState& player, Vec3& goal);

// Plays the object cues (enemies, items, goal beacon); call once per frame in a level.
void TickCues(const game::PlayerState& player, float frameSec);
void StopCues();

// "3 to the right, 2 above": where something is, in whole tiles from the player.
std::string Where(int dx, int dy);

// Spoken summary of what is near, nearest first (F4).
std::string DescribeNearby(const game::PlayerState& player);

}  // namespace a11y::level

#endif  // NSMBW_ACCESSIBILITY_LEVEL_ACTORS_H
