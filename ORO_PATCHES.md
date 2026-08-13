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

Seventeen patches, lettered (a)–(g) and (i)–(q) to match ORO's own documentation.

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

**Stock defects fixed.** These are bugs, not features, and the starred ones are reproducible
with **no addon loaded at all**:

| | |
|---|---|
| (a) | Registering any HUD render proc crashes the client instantly. `MakeRenderProcCall` passes NULL view/proj matrices for the HUD stages and `D3D9Pad::SetViewProj` dereferences them unchecked. |
| (j) ★ | Settings were lost at every launchpad close — the cfg write was not atomic. |
| (m) ★ | Night clouds. From-above night clouds render at `alpha × twilight²`, which is exactly zero past the terminator; cloud tiles bind day-side only; orbital city lights draw 4× overbright. |
| (q) ★ | The reload "Clear storm". On the *focus vessel has no visual yet* path the client clears `ZBUFFER\|STENCIL` with no depth-stencil bound, failing `D3DERR_INVALIDCALL` around thirty times per scenario reload. Clearing `TARGET` alone succeeds — and paints the black loading screen the line was always meant to paint. |
| (f) part 2 ★ | Self-shadowing treats a 0.5-alpha untextured group as fully opaque, so the stock DeltaGlider's canopy casts a solid shadow on its own fuselage in exterior views. |

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

**Six shaders are runtime-compiled**, not linked into the DLL: `Sketchpad.fx`,
`NewPlanet.hlsl`, `D3D9Client.fx`, `Vessel.fx`, `PBR.fx` and `Metalness.fx`. They must be
copied to `<Orbiter>\Modules\D3D9Client\` as well, or the DLL and its shaders will disagree.
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
