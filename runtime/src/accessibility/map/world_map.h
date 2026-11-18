#ifndef NSMBW_ACCESSIBILITY_MAP_WORLD_MAP_H
#define NSMBW_ACCESSIBILITY_MAP_WORLD_MAP_H

#include <string>

// World map: says the world on arrival and every node the player stops on (level number or
// kind, and whether it is cleared), from the course-select guide the HUD itself reads.
namespace a11y::map {

void Tick();
std::string Describe();  // the current node again (F3 on the map)

}  // namespace a11y::map

#endif  // NSMBW_ACCESSIBILITY_MAP_WORLD_MAP_H
