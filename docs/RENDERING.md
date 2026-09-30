# Rendering pipeline

`Renderer::Render` draws each frame in three passes. All scene geometry is a flat list of `DrawItem`s (a shared unit mesh plus a model matrix, color and material), built each frame by `SceneBuilder` and `PeopleView`.

```
 DrawItems ──► 1. Shadow pass ──► shadow map (4096², depth)
     │                                   │
     └──────► 2. Scene pass ◄────────────┘
                 toon lighting, PCF shadows, ray-traced reflections
                 ──► G-buffer: HDR color (RGBA16F) + normals + depth
                                   │
              3. Post pass ◄───────┘
                 sky, outlines, fog, ACES tone map, gamma ──► screen
              4. HUD (2D, alpha-blended)
```

## 1. Shadow pass (`shadow.vert`)

The sun is a directional light, so the shadow map uses an **orthographic** projection. `LightViewProjection` transforms the 8 corners of the scene bounds into light space and fits the ortho box tightly around them, so no shadow-map texels are wasted. Two measures prevent self-shadowing ("acne"):

- `glPolygonOffset` pushes the stored depth back slightly.
- In the scene pass, the lookup position is offset along the surface normal.

Back-face culling is disabled in this pass so thin panels cast shadows from both sides.

## 2. Scene pass (`scene.vert`, `scene.frag`)

- **Normals** are transformed by the inverse-transpose of the model matrix, computed on the CPU. The scene uses non-uniform scaling heavily, and a plain model matrix would skew the normals.
- **Toon lighting:** `N·L` times shadow visibility goes through a three-band ramp (shadow, half-lit, lit) with narrow smooth edges.
- **Ambient:** a hemisphere light that blends a cool sky color from above with a warm ground bounce from below, weighted by `N.y`.
- **PCF shadows:** the shadow map is a depth texture with `GL_COMPARE_REF_TO_TEXTURE`, so each lookup returns a hardware-filtered 0–1 comparison. Averaging a 3×3 grid of lookups softens the edges.
- **Hybrid ray-traced reflections** (materials `METAL` and `POLISHED`):
  1. Reflect the view vector about the normal.
  2. Trace that ray against a live list of up to 64 axis-aligned boxes (cars, counterweights, floor slabs, walls, neighbouring towers and the 20 people nearest the camera target), using the **slab test** (`RayBox`).
  3. Shade the nearest hit with its box color, face normal and sun light (one bounce, no shadow ray), or return the sky color on a miss.
  4. Blend with the rasterized color using **Schlick's Fresnel approximation**: stainless steel has a high, albedo-tinted base reflectance (F0 ≈ 0.55), and polished stone about 4 %, so floors only mirror strongly at grazing angles.

  Rasterization handles primary visibility, and rays are spent only where rasterization cannot answer the question (what a reflective surface sees). This is the same division of work as modern hybrid renderers. `G` toggles it to compare against a sky-only reflection.
- Lighting is computed in **linear** space. Albedo colors are authored in sRGB and converted with `pow(c, 2.2)`.
- Emissive materials (lamps, lit buttons) output values above 1, so they stay bright after tone mapping.

## 3. Post pass (`fullscreen.vert`, `post.frag`)

- **Full-screen triangle** generated from `gl_VertexID`. One oversized triangle covers the screen with no vertex buffer and no diagonal seam.
- **Sky:** where depth = 1, the view ray is reconstructed from the inverse view-projection matrix and shaded with a gradient and a sun disc.
- **Ink outlines:** each pixel compares linearized depth and normals with its four neighbours. A depth jump marks a silhouette and a normal change marks a crease. Outlines fade with distance to avoid noise.
- **Aerial perspective:** `1 − e^(−k·distance)` fog toward the horizon color.
- **ACES filmic tone mapping** (Narkowicz fit), then gamma 2.2, then a light vignette.

## 4. Techniques outside the shaders

| Technique | Where |
|---|---|
| Orbit camera with frame-rate-independent smoothing (`1 − e^(−k·dt)`) | `OrbitCamera` |
| Mouse picking: un-project the pixel on the near and far planes, then slab-test cars and floors | `OrbitCamera::ScreenRay`, `RayMath.h`, `main.cpp` |
| Forward-kinematics skeleton: each joint = parent transform × local translate × rotate | `PeopleView::AppendDrawItems` |
| Distance-driven walk cycle: gait phase advances with distance walked, so feet do not slide at any sim speed | `PeopleView::Update` |
| Mapping a unit cylinder onto any segment (ropes, cables): axis-angle rotation from +Y to the segment direction | `SceneBuilder::SegmentTransform` |
| Dollhouse cut-away: a wall is skipped when the camera is on its outer side, `dot(camera − wall, outward) > 0` | `SceneBuilder::Build` |
| Rotating parts driven by physics: sheave angle is integrated from rope speed (`θ += v/r · dt`) | `Car::UpdateTraction`, `SceneBuilder::AddMachine` |
| Fixed-length cable geometry: traveling cable U-loop bottom `y = (y_fixed + y_car + πr − L) / 2` | `SceneBuilder::AddTravelingCable` |
| Fixed-timestep simulation with an accumulator, decoupled from the render rate | `main.cpp` |

## Performance

About 2,000 draw items per frame, each drawn in two passes. It runs at 130–160 fps at 1600×900 on an integrated AMD Radeon GPU. The obvious next optimizations are:

- instanced rendering for repeated parts (door panels, people, rails);
- merging static geometry into a single vertex buffer;
- a BVH over the ray-trace boxes if the scene grew beyond a few hundred boxes.
