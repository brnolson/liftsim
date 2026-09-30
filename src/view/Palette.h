#pragma once

#include <glm/glm.hpp>

// Shared sRGB colors so the 3D scene and the HUD use the same identities.
namespace palette {

// One identifying color per car (A, B, C, D, ...), used on the car crosshead,
// its machine and its HUD row.
inline glm::vec3 CarAccent(int car) {
    static const glm::vec3 colors[] = {
        {0.20f, 0.55f, 0.95f}, {0.98f, 0.55f, 0.15f}, {0.25f, 0.78f, 0.40f},
        {0.72f, 0.40f, 0.90f}, {0.95f, 0.30f, 0.40f}, {0.20f, 0.80f, 0.80f},
    };
    return colors[car % 6];
}

inline char CarLetter(int car) { return static_cast<char>('A' + car); }

const glm::vec3 kSlab       {0.78f, 0.76f, 0.72f};
const glm::vec3 kCorridor   {0.88f, 0.86f, 0.82f};
const glm::vec3 kLobbyFloor {0.93f, 0.91f, 0.87f};
const glm::vec3 kHoistwayWall{0.60f, 0.60f, 0.63f};
const glm::vec3 kGlass      {0.38f, 0.58f, 0.74f};
const glm::vec3 kSpandrel   {0.86f, 0.84f, 0.79f};
const glm::vec3 kSteelDark  {0.24f, 0.26f, 0.29f};
const glm::vec3 kRail       {0.52f, 0.55f, 0.58f};
const glm::vec3 kStainless  {0.80f, 0.82f, 0.85f};
const glm::vec3 kWoodPanel  {0.64f, 0.44f, 0.27f};
const glm::vec3 kCwtFiller  {0.36f, 0.37f, 0.40f};
const glm::vec3 kMachine    {0.20f, 0.38f, 0.62f};
const glm::vec3 kRope       {0.28f, 0.28f, 0.30f};
const glm::vec3 kGovernor   {0.96f, 0.76f, 0.16f};
const glm::vec3 kBrakeSet   {0.80f, 0.16f, 0.12f};
const glm::vec3 kLampUp     {0.25f, 1.00f, 0.45f};
const glm::vec3 kLampDown   {1.00f, 0.30f, 0.22f};
const glm::vec3 kLampOff    {0.18f, 0.18f, 0.20f};
const glm::vec3 kButtonLit  {1.00f, 0.82f, 0.40f};
const glm::vec3 kCeilingLight{1.00f, 0.95f, 0.85f};
const glm::vec3 kGrass      {0.40f, 0.58f, 0.32f};
const glm::vec3 kRoad       {0.22f, 0.22f, 0.24f};
const glm::vec3 kSidewalk   {0.72f, 0.70f, 0.66f};
const glm::vec3 kFault      {1.00f, 0.15f, 0.10f};

}  // namespace palette
