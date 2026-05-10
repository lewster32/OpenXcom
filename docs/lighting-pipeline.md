# Lighting Pipeline (coloured-lighting-oxce)

This document maps the modules that participate in the OXCE realistic-lighting
pipeline, the contracts that cross between them, and the invariants every
contributor must preserve. It exists because the pipeline is genuinely
cross-cutting (eight files hold one contract) and because some of those
contracts have no compile-time enforcement -- breaking them produces silent
visual regressions that surface only on certain maps or under certain mod
configurations.

If you are about to change anything that reads tile light, writes tile light,
mutates a `Palette`, or passes data into `TileEngine::addLight`, read the
"Invariants" section first.

## Modules and what they own

```
                       +-------------------+
                       |  Mod / RuleItem / |  source-spec parsing
                       |  MCDPatch         |  (light: { color, radius,
                       |                   |   intensity, offset,
                       |                   |   fullBright, litChance })
                       +---------+---------+
                                 |
            { LightParams + tilePos / itemId for litChance hash }
                                 v
              +------------------+------------------+
              |  TileEngine::addLight  (one entry)  |  falloff + LOS trace
              |  + calculateTerrainBackground/Items |  + winner-takes-all per
              |  + calculateUnitLighting            |  emitter
              +------------------+------------------+
                                 |
              { scalar 0..15 }   |   { RGB additive, 4 corners per tile }
                                 v
                       +-------------------+
                       |  Tile             |  per-tile light buffers
                       |  addLight()       |  scalar = max-wins per layer
                       |  addLightRGB()    |  RGB    = additive per corner
                       +---------+---------+
                                 |
                       { scalar -> SCALAR_GAMMA at draw time }
                                 v
                       +-------------------+
                       |  Map::drawTerrain |  reads tile light, gates
                       |  Map::drawUnit    |  fullBright sites, picks
                       |                   |  blit variant
                       +---------+---------+
                                 |
              { paletted source + RGB tint quad + gridIdx packed 12-bit }
                                 v
                       +-------------------+
                       |  Surface          |  blitRawTint / TintWall
                       |  blitRaw*         |  per-pixel paletted blend
                       |                   |  (uses Palette::getTintLUT)
                       +---------+---------+
                                 |
                       { gridIdx -> palette index }
                                 v
                       +-------------------+
                       |  Palette          |  256x4096 LUT keyed on mix.
                       |  getTintLUT(mix)  |  Builds linear-light multiply
                       |  setColor()       |  + OKLab nearest-search.
                       |  ensureOKLabCache |  Cache invalidates only via
                       |                   |  setColor / setColors /
                       |                   |  copyColor.
                       +-------------------+
```

Edge labels are the contracts. Each one is a place where two modules agree on
a representation; if either side drifts, the result is silent.


## Invariants

These are the rules with no compile-time guard. Memorise or grep before you
change a file in the pipeline.

1. **Palette mutations MUST go through `setColor` / `setColors` / `copyColor`.**
   Direct writes through `getColors()[i].r = ...` skip the OKLab cache
   invalidation and the next `getTintLUT` call returns colours snapped against
   stale LAB triples. The PAL_BATTLESCAPE gradient overwrite at
   [Mod.cpp:5599](../src/Mod/Mod.cpp#L5599) is the canonical example -- it had
   to be rerouted through `setColor` to fix mis-tinted shadows.

2. **`oxceBattleRealisticLighting` is the master gate.** Every read site that
   conditionally takes the realistic-lighting path -- mod overrides, RGB
   writes, falloff math, lightOffset application -- must AND the option in.
   The `_gates` config block is **UI-only** (it surfaces toggles in the
   battlescape options screen); the master option is what the engine reads.
   Touched files are TileEngine.cpp, MapData.cpp, RuleItem.cpp.

3. **`SCALAR_GAMMA` is applied at *draw time* in `Map::drawUnit`, never at
   write time in `addLight`.** The corpse-fullBright bug came from gamma being
   on the write side; do not put it back. The constant lives in
   [LightingHash.h](../src/Battlescape/LightingHash.h) so write-side and
   read-side reference one source.

4. **`light.offset` is a *delta* from the natural source centre, never a
   replacement.** The pre-2026-05-05 implementation replaced the centre, which
   silently dropped the terrain-level / waist-height z bias and started LOS
   traces in neighbour tiles. See the doc comment at
   [TileEngine.cpp:1772](../src/Battlescape/TileEngine.cpp#L1772) for the
   resolution caveats (sub-`divide` voxels round to zero).

5. **`fullBright` reads must AND with `isLitInstance`.** `litChance` makes
   emission stochastic per (tilePos, part) for tiles or per BattleItem id for
   items. Both `addLight` emission and the five `fullBright` consumer sites in
   `Map.cpp` must gate on the same hash, or you get sources that emit but do
   not appear lit (or vice versa). Helpers live in
   [LightingHash.h](../src/Battlescape/LightingHash.h).

6. **Wall sprites occupy half the sprite-x range.** North walls sit in
   `[srcW/2, srcW]`, west walls in `[0, srcW/2]`. Anything painting across
   walls (gradients, masks, tint quads) must respect this or it bleeds into
   the floor sprite. `Surface::blitRawTintWall` is the only existing blit
   that gets this right; see bug-181 in `.wolf/buglog.json`.

7. **`addLight` writes RGB additively, scalar with max-wins.** Multiple
   sources at the same tile sum their colour contributions but only the
   strongest decides scalar brightness. Do not unify the two -- they serve
   different draw passes.

8. **OKLab math in the engine and `lighting-sandbox/` MUST stay in lockstep.**
   The sandbox is the prototype playground; if a sandbox-driven change ships
   to C++ but the JS isn't updated, future prototypes start from a stale
   baseline. The single source of truth for the math is
   [src/Engine/OkLab.h](../src/Engine/OkLab.h); the JS port carries a
   cross-reference comment to that file.


## Where to look first, by change type

| You are changing... | Read first |
|---|---|
| The light source spec (new YAML field) | RuleItem.cpp `loadLightBlock`, MCDPatch `light:` parser, MapData accessors |
| Falloff / LOS / per-source intensity | TileEngine.cpp `addLight` and the three calculate*Lighting drivers |
| Tile-side light storage | Tile.h scalar+RGB members, Tile::addLight / addLightRGB |
| Draw-time gamma or fullBright behaviour | Map::drawUnit + the five `fullBright` sites in Map.cpp |
| Palette colour math (multiply space, snap metric) | Palette.cpp `getTintLUT` + Engine/OkLab.h |
| Palette mutation timing (when colours change) | Palette.cpp `setColor` / `setColors` / `copyColor` -- check OKLab invalidation |
| Wall gradient blits | Surface::blitRawTintWall + the two O_OBJECT call sites in Map::drawTerrain |
| Anything in the sandbox | lighting-sandbox/lighting.js -- mirror any algorithm change in src/Engine/OkLab.h or vice versa |


## Files involved (anatomy aid)

- `src/Battlescape/TileEngine.{h,cpp}` -- addLight, computeFalloff, three
  calculate* drivers
- `src/Battlescape/Map.cpp` -- rendering, draw-time gamma, fullBright sites
- `src/Battlescape/LightingHash.h` -- shared math + SCALAR_GAMMA
- `src/Savegame/Tile.{h,cpp}` -- per-tile scalar+RGB buffers
- `src/Mod/MapData.{h,cpp}` -- tile-level mod-supplied tracks +
  isLitInstance
- `src/Mod/RuleItem.{h,cpp}` -- item-level fields + loadLightBlock +
  isLitInstance
- `src/Mod/MCDPatch.{h,cpp}` -- nested `light:` block parser
- `src/Mod/Mod.cpp` -- PAL_BATTLESCAPE gradient overwrite (must use setColor)
- `src/Engine/Palette.{h,cpp}` -- OKLab cache, getTintLUT, setColor invalidation
- `src/Engine/OkLab.h` -- single-source perceptual colour math (engine side)
- `src/Engine/Surface.{h,cpp}` -- paletted blit primitives, blitRawTintWall
- `lighting-sandbox/lighting.js` -- JS port of OKLab + the snap pipeline,
  used for prototyping before engine changes
