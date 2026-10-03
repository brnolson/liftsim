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

The sun is a directional light, so the shadow map uses an orthographic projection. `LightViewProjection` transforms the 8 corners of the scene bounds into light space and fits the ortho box tightly around them, so no shadow-map texels are wasted. Two measures prevent self-shadowing ("acne"):

- `glPolygonOffset` pushes the stored depth back slightly.
- In the scene pass, the lookup position is offset along the surface normal.

Back-face culling is disabled in this pass so thin panels cast shadows from both sides.

## 2. Scene pass (`scene.vert`, `scene.frag`)

- Normals are transformed by the inverse-transpose of the model matrix, computed on the CPU. The scene uses non-uniform scaling heavily, and a plain model matrix would skew the normals.
- Toon lighting: `N·L` times shadow visibility goes through a three-band ramp (shadow, half-lit, lit) with narrow smooth edges.
- Ambient: a hemisphere light that blends a cool sky color from above with a warm ground bounce from below, weighted by `N.y`.
- PCF shadows: the shadow map is a depth texture with `GL_COMPARE_REF_TO_TEXTURE`, so each lookup returns a hardware-filtered 0–1 comparison. Averaging a 3×3 grid of lookups softens the edges.
- Ray-traced reflections (materials `METAL` and `POLISHED`):
  1. Reflect the view vector about the normal.
  2. Trace that ray against a live list of up to 64 axis-aligned boxes (cars, counterweights, floor slabs, walls, neighbouring towers), using the slab test (`RayBox`). People are deliberately left out: a single box cannot represent a walking pose and appears as a rectangular ghost in mirrors. Per-limb boxes or screen-space reflections would handle them at higher cost.
  3. Shade the nearest hit with its box color, face normal and sun light (one bounce, no shadow ray), or return the sky color on a miss.
  4. Blend with the rasterized color using Schlick's Fresnel approximation: stainless steel has a high, albedo-tinted base reflectance (F0 ≈ 0.55), and polished stone about 4 %, so floors only mirror strongly at grazing angles.

  Rasterization handles primary visibility; rays are only traced for what rasterization cannot answer, which is what a reflective surface sees. `G` toggles it to compare against a sky-only reflection.
- Lighting is computed in linear space. Albedo colors are authored in sRGB and converted with `pow(c, 2.2)`.
- Emissive materials (lamps, lit buttons) output values above 1, so they stay bright after tone mapping.

## 3. Post pass (`fullscreen.vert`, `post.frag`)

- A full-screen triangle generated from `gl_VertexID`. One oversized triangle covers the screen with no vertex buffer and no diagonal seam.
- Sky: where depth = 1, the view ray is reconstructed from the inverse view-projection matrix and shaded with a gradient and a sun disc.
- Ink outlines: each pixel compares linearized depth and normals with its four neighbours. A depth jump marks a silhouette and a normal change marks a crease. Outlines fade with distance to avoid noise.
- Aerial perspective: `1 − e^(−k·distance)` fog toward the horizon color.
- ACES filmic tone mapping (Narkowicz fit), then gamma 2.2, then a light vignette.

## 4. Environment: instancing and procedural surfaces

The surroundings are decoration, so they are drawn cheaply and without asset files.

### GPU instancing (`InstanceBatch`, `Environment`)

The city uses thousands of copies of three unit meshes: 172 towers, 1,100 street trees, 320 lamp posts, 3,500 pieces of road paint, and indoor plants on every floor, about 8,000 instances in total.

- Each batch uploads a per-instance vertex buffer (model matrix + color) and draws every copy with a single `glDrawElementsInstanced` call.
- The model matrix is a `mat4` attribute occupying locations 3–6. `glVertexAttribDivisor(loc, 1)` advances it once per instance instead of once per vertex.
- Every batch is static: it uploads once at start-up.
- The whole environment costs 13 draw calls per pass instead of about 8,000.

### Procedural solid texturing (`ApplySurface` in `scene.frag`)

There are no image files; each surface is a function of world position:

| Surface | Technique |
|---|---|
| Concrete | FBM (fractal value noise) mottling plus hashed aggregate speckles |
| Terrazzo | Cellular pattern: at most one round chip at a random offset per grid cell; floor faces only |
| Carpet | Box projection to 2D, 0.6 m tiles with pile direction alternating per tile |
| Wood | Rings around an axis, `sin(r)`, with the radius distorted by FBM |
| Brushed steel | Noise stretched along one axis (streaks) |
| Asphalt, grass, foliage | Multi-scale noise (large patches plus fine grain) |
| Facades | Box projection to a window grid: one bay per 1.8 m and one floor per 3.6 m, blinds randomized by hashing the cell |

- Value noise: hashed lattice values blended with a smoothstep curve. FBM sums four octaves, each at double the frequency and half the amplitude.
- Box projection: picks the axis-aligned plane the surface faces to get 2D coordinates, so boxes need no UVs.

### Anti-aliasing procedural detail (`Detail()`)

A procedural pattern has no mipmaps, so detail smaller than a pixel would shimmer. `fwidth(worldPos)` measures how far the surface moves across one pixel. When a pattern's period falls below about a pixel, its contribution is faded out, which does the job mipmaps do for image textures.

## 5. Techniques outside the shaders

| Technique | Where |
|---|---|
| Orbit camera with frame-rate-independent smoothing (`1 − e^(−k·dt)`) | `OrbitCamera` |
| Mouse picking: un-project the pixel on the near and far planes, then slab-test cars and floors | `OrbitCamera::ScreenRay`, `RayMath.h`, `Application::HandleClick` |
| Forward-kinematics skeleton: each joint = parent transform × local translate × rotate | `PeopleView::AppendDrawItems` |
| Distance-driven walk cycle: gait phase advances with distance walked, so feet do not slide at any sim speed | `PeopleView::Update` |
| Mapping a unit cylinder onto any segment (ropes, cables): axis-angle rotation from +Y to the segment direction | `SceneBuilder::SegmentTransform` |
| Dollhouse cut-away: a wall is skipped when the camera is on its outer side, `dot(camera − wall, outward) > 0` | `SceneBuilder::Build` |
| Rotating parts driven by physics: sheave angle is integrated from rope speed (`θ += v/r · dt`) | `Car::UpdateTraction`, `SceneBuilder::AddMachine` |
| Fixed-length cable geometry: traveling cable U-loop bottom `y = (y_fixed + y_car + πr − L) / 2` | `SceneBuilder::AddTravelingCable` |
| Fixed-timestep simulation with an accumulator, decoupled from the render rate | `Application` |

## Performance

About 2,000 individual draw items for the building, elevators and people, plus 13 instanced batches holding about 8,000 environment instances. Everything is drawn in both the shadow and scene passes. It runs at about 110–150 fps at 1600×900 on an integrated AMD Radeon GPU. The next optimizations would be:

- instancing the building's repeated parts too (door panels, people, rails);
- merging static geometry into a single vertex buffer;
- a BVH over the ray-trace boxes if the scene grew beyond a few hundred boxes.
