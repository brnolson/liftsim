#pragma once

// Procedural surface patterns evaluated in scene.frag. Keep the values in
// sync with the SURFACE_* constants there.
enum class Surface : int {
    Plain        = 0,
    Concrete     = 1,   // fractal noise mottling with aggregate speckles
    Terrazzo     = 2,   // polished floor with scattered stone chips
    Carpet       = 3,   // 0.6 m carpet tiles laid in alternating directions
    Wood         = 4,   // growth-ring grain
    BrushedMetal = 5,   // fine vertical streaks
    Asphalt      = 6,   // dark aggregate
    Grass        = 7,   // large patches plus fine blades
    Facade       = 8,   // grid of windows, one floor per 3.6 m
    Foliage      = 9,   // leafy light/dark clusters
};
