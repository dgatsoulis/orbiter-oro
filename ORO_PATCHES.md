# ORO patches to D3D9Client

This is a fork of [orbitersim/orbiter](https://github.com/orbitersim/orbiter) carrying the
D3D9Client changes that the **ORO** addon (Orbiter Realism Overhaul) depends on.

**It is not a maintained Orbiter distribution.** Nothing here is intended to compete with or
replace upstream Orbiter, and no support is offered for it. If you want Orbiter, get it from
upstream. If you want to know what ORO changed in the graphics client, or you want to rebuild
that client yourself, you are in the right place.

## Why this fork exists

ORO ships a modified `D3D9Client.dll`. Distributing a modified binary of GPL v3 / LGPL v3
software carries an obligation to make the corresponding source available, and this fork is
how that obligation is met. It also means anyone can verify that the DLL in the ORO package
is built from exactly what is published here, rather than taking it on trust.

## Where the changes are

Branch **`oro-patches`**, based on tag **`2024`** (`0cac7a3`) — the commit ORO's client is
built from. Only `OVP/D3D9Client/` is modified; the rest of the Orbiter tree is untouched.

To see everything that differs from stock:

```
git diff 2024..oro-patches
```

## What changed

Forty-one patches, lettered (a)–(al) (plus k2, z2 and z3) to match ORO's own documentation.
The tables below list the first nineteen by category; the ones since 2026-08-24 follow
them, in order, further down.

**New capability for addons** — things the client could not previously do:

| | |
|---|---|
| (b) | `gcCore` backbuffer access: `GetBackBufferHandle()` + a mid-scene-safe `CopyResource()`. This is what makes an addon-side post-process pipeline possible at all. |
| (d) | `SKPBS_ADDITIVE` Sketchpad blend. The API had no additive state. |
| (g) | A depth-clipped Sketchpad path (`CreateTrianglesDepth`, `HasDepthBuffer`) comparing per pixel against the scene depth the client already renders. |
| (i) | `RENDERPROC_PRE_RESOLVE`, a pre-bloom compositing slot. |
| (k) | `GetRenderCam` / `GetRenderObjPos` — the camera and body positions the frame is *actually* rendered with. Module pre-step and post-step both run before Orbiter updates them. |
| (l) | Textured Sketchpad triangles + `UpdateTexture2D`. No public oapi route can put bytes into a texture. |
| (f) | Shadows in the virtual cockpit, plus `SetVCShadows`. |
| (p) | VC shadow depth — lets a shadow take the ambient share with it. Emissive is never scaled. |

**Turning stock behaviour off** — each of these exists because no core API could do it:

| | |
|---|---|
| (c) | `SuppressReentry`. Stock ignores both `VESSEL::SetReentryTexture(NULL)` (a documented API) and the `bReentryFlames` launchpad option. |
| (e) | Reentry particle streams honour (c). The core gives *every* vessel a default reentry stream, and no core API can disable another vessel's streams. |
| (n) | `SuppressExhaust` — stock exhaust billboards and stream emission, over independent flags. |
| (o) | `ExemptNewStreams` — a latch so an addon can *replace* stock exhaust streams rather than only add to them. |
| (s) | **Surface weather** — the largest patch in the set, seven parts: `SetSurfaceWetness` / `SetStormLight` / `SetWetDarkness` / `SetWetGlint` / `SetWetReflection` / `SetWetGrain`, all probe-by-binding, all exactly stock at rest. Wet ground in both ground shaders (terrain *and* base tiles), storm light collapsing the sun at the source, wet hulls in all five vessel paths, a lifecycled drop glint, standing pools pinned to the ground via the LOD-continuous water-microtexture UV, and a planar mirror re-rendering vessels through a ground-reflected camera. Extended 2026-08-23: the cockpit-pass render zeroes the wet uniforms, so a virtual cockpit's interior stays dry while hulls seen through the window keep their full wet look. |

**Stock defects fixed.** These are bugs, not features, and the starred ones are reproducible
with **no addon loaded at all**:

| | |
|---|---|
| (a) | Registering any HUD render proc crashes the client instantly. `MakeRenderProcCall` passes NULL view/proj matrices for the HUD stages and `D3D9Pad::SetViewProj` dereferences them unchecked. |
| (j) ★ | Settings were lost at every launchpad close — the cfg write was not atomic. |
| (m) ★ | Night clouds. From-above night clouds render at `alpha × twilight²`, which is exactly zero past the terminator; cloud tiles bind day-side only; orbital city lights draw 4× overbright. |
| (q) ★ | The reload "Clear storm". On the *focus vessel has no visual yet* path the client clears `ZBUFFER\|STENCIL` with no depth-stencil bound, failing `D3DERR_INVALIDCALL` around thirty times per scenario reload. Clearing `TARGET` alone succeeds — and paints the black loading screen the line was always meant to paint. |
| (f) part 2 ★ | Self-shadowing treats a 0.5-alpha untextured group as fully opaque, so the stock DeltaGlider's canopy casts a solid shadow on its own fuselage in exterior views. |
| (r) ★ | Material emissive is clamped. Both vessel shaders fold `gMtrl.emissive` into the light term and then `saturate()` it (`Light_fx()` in `Common.hlsl` is literally `return saturate(x)`), and that term *multiplies* the texture — so **a surface can never be brighter than its own texture**, no emissive value above 1.0 does anything, and nothing authored this way can ever reach the bloom pass. An addon driving emissive to 3.19 and one driving it to 1.0 render identically, and because all three channels clamp together the tint is erased too. Fixed by re-applying the excess additively, modulated by the albedo, so a texture's dark bands stay dark. Stock content cannot move: the term is exactly zero at or below 1.0. |

**Since 2026-08-24, in order.** Same conventions: a starred entry is a stock defect
reproducible with no addon loaded. Every `gcCore` addition is probe-by-binding (an addon
tests the bound function pointer, never a build number) and exactly stock when unused.

| | |
|---|---|
| (h) | Scene depth into a user IPI shader (`SetIPISceneDepth`); a `RAIN 1` mesh-group token, or a line in `Config\ORO\VesselsRainSurfaces.cfg`, marks a window as glass by writing negated depth in the normal-depth pass; the stock `GENERICPROC_PICK_VESSEL` scene pick gains a NULL guard (a sky click dereferenced `pick.vObj` unchecked ★), `GetDevMeshName`, `ReloadRainSurfaces`, `FlashMeshGroup`. |
| (t) | The menu bar and info bars are drawn LAST: the core paints them in the same call as the instruments, between the two HUD render-proc slots, so any full-frame addon effect smeared them. They are captured during `Render2DOverlay` and replayed after `RENDERPROC_HUD_2ND`, identified by their texture. |
| (u) | `RENDERPROC_WET_MIRROR`, a render-proc slot inside the wet-ground mirror pass, plus a `0x200` "write coverage alpha" Sketchpad bit - both blended pad paths wrote RGB only, which is exactly wrong in an offscreen target whose alpha is a mask. |
| (v) | Reflections behind a fourth Launchpad env-map mode, "Full Scene ORO (exp)": multiple probes per vessel, box-projected sampling, planar vessel mirrors with a curvature warp, `<class>_ecam_oro.cfg` (a file stock never reads, so a stock client's probe cannot be reconfigured by it). The three stock modes are pixel-exact stock. |
| (w) | Planet-shine shadows: stock planet glow has no occlusion term, so a closed payload bay flying bay-to-Earth glows sky-blue inside ★. The focus assembly renders a depth map along the planet direction; the sun self-shadow map, main-scene-only in stock, is bound in probe and mirror passes too. |
| (x) | Particle sun lighting: `Particle.fx` hardcodes `light = 1.0f` with the N.L commented out, so DIFFUSE streams render fully lit at midnight ★. Position-based per-particle sun visibility, a flame term, a dawn/dusk tint; Launchpad "Particle lighting (ORO)" Off / Brightness / Brightness + colour, Off being bit-exact stock. |
| (y) | `GetExhaustStreamSpec`: the core copies a `PARTICLESTREAMSPEC` at construction and exposes no getter, so the client's scene is the only place a vessel author's stream definition can be read back. |
| (z) ★ | The base pass grows up: runway lights, base tiles and runway/pad surfaces obey the depth buffer (they were drawn depth-off, a flat-planet fossil, and painted through mountains); the sun's glare hides behind terrain (the visibility kernel never saw terrain); `mytex_n.dds` night textures for MESH base objects, which the core wires for generic objects only; an Advanced-setup checkbox overlap. |
| (z2) | Base structures join the depth-normal buffer, so glare sprites stop painting through hangars and depth-clipped addon geometry vanishes behind buildings. |
| (z3) | Shadow maps for local SPOT lights: a hangar carves its beam, a vessel in the beam casts onto the ground, a ridge ends it. Terrain tiles register as casters one frame stale, stored in their planet's frame. |
| (aa) | Two analytic exponential fog layers in every shader family (terrain, base tiles, all vessel paths, particles, runway lights), sun-irradiance coloured, pushed in display space; `SetFogLayer` / `SetFogLook` / `SetSnowCover`. |
| (ab) | Terrain into the depth-normal buffer, and a SOFT depth test for stencil ground shadows (a shadow sheet is discarded only where it lies behind the scene by more than a distance-scaled tolerance). The tile registry stores tiles in the planet's frame in doubles - re-anchoring a stale tile by camera translation alone blinked every shadow at KSC at high frame rates, because Orbiter steps the world on only some frames. |
| (ac) | `SetBaseLights`: base night lights on demand, an emission gain, and a halo in fog (an aureole that grows with the optical depth). |
| (ad) | `SetVCNightLight`: the virtual cockpit's fill light - the Launchpad ambient AND the material emissive the stock DeltaGlider carries at 0.8 on every cabin surface - scaled at the source for the cockpit draw only. MFDs, display materials and emission maps are exempt. |
| (ae) | Cascaded shadows, TerrainShadowing mode 3 "Cascaded (ORO)": one camera-fitted sun-shadow atlas (five cascades plus hull boxes) into which every caster draws and from which terrain and every vessel path sample; planet-local texel-snapped anchors; the spot-light maps ride its spare row. Modes 0-2 bit-stock. Also fixes patch (p)'s FAST-path line that broke the vessel shader with self-shadows set to None. 2026-09-13: the taps no longer shadow their own surface at a grazing sun (a 1.5-texel sin-scaled normal offset whose depth margin grows as tan; the slope clamp raised from 4 to 16 and unified; both tunable from spare uniform lanes). |
| (af) ★ | Terrain flattening under CUBIC elevation interpolation: `.flt` files flattened the physics tiles and the float copy but never the raw INT16 array the file-less children hand to the core's spline, so the vessel stood on the flat while the drawn ground kept its hills. |
| (ag) ★ | The animated base objects revived: TRAIN1, TRAIN2 and SOLARPLANT only ever animated in the core's own renderer; every graphics client got static merged exports (the solar plant never exported at all). The client rebuilds them from the base cfg, terrain-following on pylons, on sim time. Plus a step around the core's `SolarPlant` destructor, which frees pointers it never set for any base the session did not activate (heap corruption at exit). |
| (ah) | Local lights: the GPU light struct repacked from six float4 to four; one coherent branch per block of four; `Scene::AddLocalLight` evicted the wrong light once full ★; 12x and 16x rows; up to six spot shadow maps a frame with clustering, sticky selection, a caster-fitted field of view and bilinear PCF; point lights cast too (an aimed pseudo-spot, or a five-face cube); `MESHGROUPS` in `_ecam_oro.cfg`. |
| (ai) | `SetDeferVCHUD`: the VC HUD mesh group is held back and drawn after the addon's world effects (patch (t)'s pattern pointed at the cockpit); `SetWaterMirror`: the planar mirror over open water with the rain off; the wet film and mirror on runways, pads and taxiways; base-local rain glint; no pools on water or slopes; the damp sky film gains the distance falloff it never had ★. |
| (aj) | Planetary rings: `SetRingLook` / `SetRingProfile` per planet - opacity from a real optical depth (derived from the ring texture every ringed planet already ships), lit and backlit faces, the ring's shadow on the planet (absent from stock entirely), objects inside that shadow shaded, the sun glare dimmed through the rings, a near-field draw that stops the sheet cutting on the planet's near plane, grooves and grain as the camera closes. Also ★: the ring mesh was lit by an uninitialised sun (no planet shadow on the rings, rings fully bright from the dark side, since 2024), and the stock shader samples the ring texture through `smoothstep` while the texture is authored linear (ring structure displaced by up to 6,300 km). |
| (ak) ★ | `PostProcess` clamped to its two real values: the read accepted 2 (the disconnected 2016 lens flare) while the Launchpad combo holds two rows, so a visit to that page wrote back -1 and the user lost bloom AND flare. |
| (al) ★ | A repeating error no longer floods the log: an error that recurs every frame wrote a line every frame to the client's log and Orbiter.log, at the default debug level; identical messages are counted and flushed once a real second with their tally. |

## Building

Because the patches are already applied on this branch, there is no patch step. The recipe is
upstream Orbiter's own, restricted to the `D3D9Client` target:

1. Clone this branch:
   `git clone --branch oro-patches https://github.com/dgatsoulis/orbiter-oro.git`
2. Install the **DirectX SDK (June 2010)**. Only `Include` and `Lib` are needed.
3. Two local workarounds are required to configure, neither specific to ORO:
   - comment out `add_dependencies(${OrbiterTgt} orbiter_lua)` in
     `Src/Module/LuaScript/LuaInterpreter/CMakeLists.txt` (it otherwise needs `hhc.exe`
     from HTML Help Workshop);
   - create empty stub directories under
     `Extern/irrKlang/x86/irrKlang-1.6.0/{bin/win32-visualStudio,lib/Win32-visualStudio,include}`
     (the configured download URL is dead; XRSound is configured but never built).
4. From a VS2022 **x86** environment:
   ```
   cmake . --preset windows-x86-release -DORBITER_MAKE_DOC=OFF -DDXSDK_DIR:PATH=<your DXSDK>
   cmake --build out\build\windows-x86-release --target D3D9Client --parallel
   ```
5. Back up `<Orbiter>\Modules\Plugin\D3D9Client.dll`, then copy the built DLL over it.

**Seven shaders are runtime-compiled**, not linked into the DLL: `Sketchpad.fx`,
`NewPlanet.hlsl`, `D3D9Client.fx`, `Vessel.fx`, `PBR.fx`, `Metalness.fx` and `Mesh.fx`
(the last joined with patch (s)'s wet base tiles). They must be copied to
`<Orbiter>\Modules\D3D9Client\` as well, or the DLL and its shaders will disagree.
Editing them needs no rebuild — just restart Orbiter.

Full per-patch detail, the exact apply order, and the landmines hit along the way are in the
ORO repository's `upstream/BUILDING.md`.

## Licensing

Unchanged from upstream, and deliberately so:

- **Orbiter core** — MIT, © 2000–2021 Martin Schweiger. See `LICENSE`.
- **D3D9Client** (`OVP/D3D9Client/`) — dual licensed **GPL v3 and LGPL v3**,
  © 2006–2016 Martin Schweiger, © 2012–2016 Jarmo Nikkanen. The licence is declared in each
  file's own header; those headers are intact and must stay that way.

The patches in this branch are modifications of D3D9Client and therefore carry D3D9Client's
dual GPL v3 / LGPL v3 licence. They are not relicensed and could not be.

D3D9Client is Jarmo Nikkanen's and Martin Schweiger's work. This fork adds to it; it does not
claim it.

## Links

- ORO addon — https://github.com/dgatsoulis/ORO
- Upstream Orbiter — https://github.com/orbitersim/orbiter
