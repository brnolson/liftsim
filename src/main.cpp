// LiftSim: real-time 3D simulation of a traction elevator group.
//
// Code map:
//   sim/     simulation core (motion, traction, doors, safety, dispatch) - no graphics
//   view/    turns simulation state into draw items and ray-trace boxes
//   render/  OpenGL pipeline: shadow pass, scene pass, post pass
//   ui/      HUD, component inspector, engineering references
//   app/     window, input and the frame loop
//
// Command line (used for automated screenshots):
//   LiftSim --screenshot out.png [--warmup 120] [--view 0..5] [--inspect 0..7]
//           [--car 0..3] [--traffic 0..3] [--rate N] [--fault]

#include "app/Application.h"
#include <cstdlib>

int main(int argc, char* argv[]) {
    Application app(ParseOptions(argc, argv));
    if (!app.Init()) return EXIT_FAILURE;
    app.Run();
    return EXIT_SUCCESS;
}
