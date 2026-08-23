// ===================================================
// Copyright (C) 2021 Jarmo Nikkanen
// licensed under LGPL v2
// ===================================================


#include <d3d9.h>
#include <d3dx9.h>
#include "gcCore.h"
#include "D3D9Surface.h"
#include "D3D9Client.h"
#include "Scene.h"
#include "VVessel.h"
#include "VPlanet.h"
#include "Surfmgr2.h"
#include "IProcess.h"

extern D3D9Client *g_client;
extern std::set<Font *> g_fonts;

class gcSwap
{
public:
	gcSwap() : pSwap(NULL), hSurf(NULL), pBack(NULL) { }
	~gcSwap() { Release(); }
	void Release() {
		DELETE_SURFACE(hSurf);
		SAFE_RELEASE(pBack);
		SAFE_RELEASE(pSwap);
	}
	LPDIRECT3DSWAPCHAIN9 pSwap;
	LPDIRECT3DSURFACE9 pBack;
	SURFHANDLE hSurf;
};



DLLCLBK void gcBindCoreMethod(void** ppFnc, const char* name)
{
	*ppFnc = NULL;
#define binder_start
	if (strcmp(name,"RegisterSwap")==0) *ppFnc = &gcCore2::RegisterSwap;
	if (strcmp(name,"FlipSwap")==0) *ppFnc = &gcCore2::FlipSwap;
	if (strcmp(name,"GetRenderTarget")==0) *ppFnc = &gcCore2::GetRenderTarget;
	if (strcmp(name,"GetBackBufferHandle")==0) *ppFnc = &gcCore2::GetBackBufferHandle;
	if (strcmp(name,"CopyResource")==0) *ppFnc = &gcCore2::CopyResource;
	if (strcmp(name,"SuppressReentry")==0) *ppFnc = &gcCore2::SuppressReentry;
	if (strcmp(name,"SuppressExhaust")==0) *ppFnc = &gcCore2::SuppressExhaust;
	if (strcmp(name,"ExemptNewStreams")==0) *ppFnc = &gcCore2::ExemptNewStreams;
	if (strcmp(name,"SetVCShadows")==0) *ppFnc = &gcCore2::SetVCShadows;
	if (strcmp(name,"SetSurfaceWetness")==0) *ppFnc = &gcCore2::SetSurfaceWetness;
	if (strcmp(name,"SetStormLight")==0) *ppFnc = &gcCore2::SetStormLight;
	if (strcmp(name,"SetWetDarkness")==0) *ppFnc = &gcCore2::SetWetDarkness;
	if (strcmp(name,"SetWetGlint")==0) *ppFnc = &gcCore2::SetWetGlint;
	if (strcmp(name,"SetWetReflection")==0) *ppFnc = &gcCore2::SetWetReflection;
	if (strcmp(name,"SetWetGrain")==0) *ppFnc = &gcCore2::SetWetGrain;
	if (strcmp(name,"ReleaseSwap")==0) *ppFnc = &gcCore2::ReleaseSwap;
	if (strcmp(name,"DeleteCustomCamera")==0) *ppFnc = &gcCore2::DeleteCustomCamera;
	if (strcmp(name,"CustomCameraOnOff")==0) *ppFnc = &gcCore2::CustomCameraOnOff;
	if (strcmp(name,"CustomCameraOverlay")==0) *ppFnc = &gcCore2::CustomCameraOverlay;
	if (strcmp(name,"SetupCustomCamera")==0) *ppFnc = &gcCore2::SetupCustomCamera;
	if (strcmp(name,"SketchpadVersion")==0) *ppFnc = &gcCore2::SketchpadVersion;
	if (strcmp(name,"CreatePoly")==0) *ppFnc = &gcCore2::CreatePoly;
	if (strcmp(name,"CreateTriangles")==0) *ppFnc = &gcCore2::CreateTriangles;
	if (strcmp(name,"CreateTrianglesDepth")==0) *ppFnc = &gcCore2::CreateTrianglesDepth;
	if (strcmp(name,"HasDepthBuffer")==0) *ppFnc = &gcCore2::HasDepthBuffer;
	if (strcmp(name,"CreateTrianglesTex")==0) *ppFnc = &gcCore2::CreateTrianglesTex;
	if (strcmp(name,"UpdateTexture2D")==0) *ppFnc = &gcCore2::UpdateTexture2D;
	if (strcmp(name,"GetRenderCam")==0) *ppFnc = &gcCore2::GetRenderCam;
	if (strcmp(name,"GetRenderObjPos")==0) *ppFnc = &gcCore2::GetRenderObjPos;
	if (strcmp(name,"DeletePoly")==0) *ppFnc = &gcCore2::DeletePoly;
	if (strcmp(name,"GetTextLength")==0) *ppFnc = &gcCore2::GetTextLength;
	if (strcmp(name,"GetCharIndexByPosition")==0) *ppFnc = &gcCore2::GetCharIndexByPosition;
	if (strcmp(name,"RegisterRenderProc")==0) *ppFnc = &gcCore2::RegisterRenderProc;
	if (strcmp(name,"CreateSketchpadFont")==0) *ppFnc = &gcCore2::CreateSketchpadFont;
	if (strcmp(name,"GetMeshMaterial")==0) *ppFnc = &gcCore2::GetMeshMaterial;
	if (strcmp(name,"SetMeshMaterial")==0) *ppFnc = &gcCore2::SetMeshMaterial;
	if (strcmp(name,"GetMatrix")==0) *ppFnc = &gcCore2::GetMatrix;
	if (strcmp(name,"SetMatrix")==0) *ppFnc = &gcCore2::SetMatrix;
	if (strcmp(name,"GetDevMesh")==0) *ppFnc = &gcCore2::GetDevMesh;
	if (strcmp(name,"LoadDevMeshGlobal")==0) *ppFnc = &gcCore2::LoadDevMeshGlobal;
	if (strcmp(name,"ReleaseDevMesh")==0) *ppFnc = &gcCore2::ReleaseDevMesh;
	if (strcmp(name,"RenderMesh")==0) *ppFnc = &gcCore2::RenderMesh;
	if (strcmp(name,"PickMesh")==0) *ppFnc = &gcCore2::PickMesh;
	if (strcmp(name,"RenderLines")==0) *ppFnc = &gcCore2::RenderLines;
	if (strcmp(name,"GetSystemSpecs")==0) *ppFnc = &gcCore2::GetSystemSpecs;
	if (strcmp(name,"GetSurfaceSpecs")==0) *ppFnc = &gcCore2::GetSurfaceSpecs;
	if (strcmp(name,"LoadSurface")==0) *ppFnc = &gcCore2::LoadSurface;
	if (strcmp(name,"SaveSurface")==0) *ppFnc = &gcCore2::SaveSurface;
	if (strcmp(name,"GetMipSublevel")==0) *ppFnc = &gcCore2::GetMipSublevel;
	if (strcmp(name,"GenerateMipmaps")==0) *ppFnc = &gcCore2::GenerateMipmaps;
	if (strcmp(name,"CompressSurface")==0) *ppFnc = &gcCore2::CompressSurface;
	if (strcmp(name,"LoadBitmapFromFile")==0) *ppFnc = &gcCore2::LoadBitmapFromFile;
	if (strcmp(name,"GetRenderWindow")==0) *ppFnc = &gcCore2::GetRenderWindow;
	if (strcmp(name,"RegisterGenericProc")==0) *ppFnc = &gcCore2::RegisterGenericProc;
	if (strcmp(name,"StretchRectInScene")==0) *ppFnc = &gcCore2::StretchRectInScene;
	if (strcmp(name,"ClearSurfaceInScene")==0) *ppFnc = &gcCore2::ClearSurfaceInScene;
	if (strcmp(name,"ScanScreen")==0) *ppFnc = &gcCore2::ScanScreen;
	if (strcmp(name,"LockSurface")==0) *ppFnc = &gcCore2::LockSurface;
	if (strcmp(name,"ReleaseLock")==0) *ppFnc = &gcCore2::ReleaseLock;
	if (strcmp(name,"GetPlanetManager")==0) *ppFnc = &gcCore2::GetPlanetManager;
	if (strcmp(name,"SetTileOverlay")==0) *ppFnc = &gcCore2::SetTileOverlay;
	if (strcmp(name,"AddGlobalOverlay")==0) *ppFnc = &gcCore2::AddGlobalOverlay;
	if (strcmp(name,"GetTileData")==0) *ppFnc = &gcCore2::GetTileData;
	if (strcmp(name,"GetTile")==0) *ppFnc = &gcCore2::GetTile;
	if (strcmp(name,"HasTileData")==0) *ppFnc = &gcCore2::HasTileData;
	if (strcmp(name,"SeekTileTexture")==0) *ppFnc = &gcCore2::SeekTileTexture;
	if (strcmp(name,"SeekTileElevation")==0) *ppFnc = &gcCore2::SeekTileElevation;
	if (strcmp(name,"GetElevation")==0) *ppFnc = &gcCore2::GetElevation;
	if (strcmp(name,"CreateIPInterface")==0) *ppFnc = &gcCore2::CreateIPInterface;
	if (strcmp(name,"ReleaseIPInterface")==0) *ppFnc = &gcCore2::ReleaseIPInterface;
#define binder_end
	if (*ppFnc == NULL) oapiWriteLogV("ERROR:gcCoreAPI: Function [%s] failed to bind", name);
}


// ===============================================================================================
// Custom SwapChain Interface
// ===============================================================================================
//
HSWAP gcCore::RegisterSwap(HWND hWnd, HSWAP hData, int AA) 
{ 
	gcSwap * pData = (gcSwap*)hData;

	D3DPRESENT_PARAMETERS pp;
	memset(&pp, 0, sizeof(D3DPRESENT_PARAMETERS));

	pp.BackBufferWidth = 0;
	pp.BackBufferHeight = 0;
	pp.BackBufferFormat = D3DFMT_X8R8G8B8;
	pp.BackBufferCount = 1;
	pp.MultiSampleType = D3DMULTISAMPLE_NONE;
	pp.MultiSampleQuality = 0;
	pp.SwapEffect = D3DSWAPEFFECT_FLIP;
	pp.hDeviceWindow = hWnd;
	pp.Windowed = true;
	pp.EnableAutoDepthStencil = false;
	pp.AutoDepthStencilFormat = D3DFMT_D24S8;
	pp.Flags = 0;
	pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	LPDIRECT3DDEVICE9 pDev = g_client->GetDevice();
	LPDIRECT3DSWAPCHAIN9 pSwap = NULL;

	if (pDev->CreateAdditionalSwapChain(&pp, &pSwap) == S_OK)
	{
		if (!pData) pData = new gcSwap();
		else pData->Release();
		
		LPDIRECT3DSURFACE9 pBack = NULL;
		HR(pSwap->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBack));

		SurfNative *pSrf = new SurfNative(pBack, OAPISURFACE_BACKBUFFER | OAPISURFACE_RENDER3D | OAPISURFACE_RENDERTARGET);
		pSrf->SetName("SwapChainBackBuffer");

		pData->hSurf = SURFHANDLE(pSrf);
		pData->pSwap = pSwap;
		pData->pBack = pBack;

		return HSWAP(pData);
	}
	else {
		LogErr("Failed to create a swapchain. (Feature not supported in true-fullscreen mode)");
		return NULL;
	}
}


// ===============================================================================================
//
void gcCore::FlipSwap(HSWAP hSwap) 
{ 
	HR(((gcSwap*)hSwap)->pSwap->Present(0, 0, 0, 0, 0));
}


// ===============================================================================================
//
SURFHANDLE gcCore::GetRenderTarget(HSWAP hSwap)
{
	return ((gcSwap*)hSwap)->hSurf;
}

// ===============================================================================================
// ORO addon (2026): expose the main backbuffer + a mid-scene-safe surface copy so a module
// can resample the live frame through gcIPInterface (grey-out / blur / tilt).
//
SURFHANDLE gcCore::GetBackBufferHandle()
{
	return g_client->GetBackBufferHandle();
}

// ===============================================================================================
//
bool gcCore::CopyResource(SURFHANDLE tgt, SURFHANDLE src)
{
	if (!tgt || !src) return false;
	LPDIRECT3DSURFACE9 pts = SURFACE(tgt)->GetSurface();
	LPDIRECT3DSURFACE9 pss = SURFACE(src)->GetSurface();
	if (!pts || !pss) return false;
	// Direct device StretchRect - NO Begin/EndScene wrapper (cf. StretchRectInScene), so this
	// is callable from inside a render callback. NULL rects = whole-surface copy (also the form
	// that resolves a multisampled source into a plain texture).
	return g_client->GetDevice()->StretchRect(pss, NULL, pts, NULL, D3DTEXF_LINEAR) == S_OK;
}

// ===============================================================================================
// ORO addon (2026): let a module suppress this client's built-in reentry flames per vessel.
//
// WHY THIS EXISTS. Orbiter documents VESSEL::SetReentryTexture(NULL) as suppressing reentry
// flames, and the inline renderer honours it - Src/Orbiter/Vvessel.cpp gates on
// vessel->reentry.do_render AND the user's CfgVisualPrm.bReentryFlames option. D3D9Client's
// vVessel::RenderReentry checks NEITHER: it gates only on its own globally-loaded
// defreentrytex, so both the documented API and the user's own Launchpad setting are
// silently ignored here. An addon that wants to REPLACE the reentry visuals therefore has
// no way to turn the stock billboards off.
//
// The proper fix is for RenderReentry to honour do_render, but that flag lives in the
// core-internal Vessel.h and is not reachable across the client DLL boundary. This is the
// client-side equivalent: an explicit opt-in list an addon can manage.
//
static std::set<OBJHANDLE> g_gcReentrySuppressed;

void gcCore::SuppressReentry(OBJHANDLE hVessel, bool bSuppress)
{
	if (!hVessel) return;
	if (bSuppress) g_gcReentrySuppressed.insert(hVessel);
	else           g_gcReentrySuppressed.erase(hVessel);
}

// --- ORO patch (f): virtual-cockpit shadow controls -------------------------------
// Read by Scene::RenderMainScene's internal pass (declared extern there, the same
// no-header-churn pattern as gcIsReentrySuppressed above). Defaults reproduce the
// built-in behaviour, so a client nobody calls behaves exactly as if these did not exist.
bool  g_gcVCShadows    = true;
float g_gcVCShadowRad  = 2.2f;   // measured on the stock DeltaGlider, not guessed - the
                                 // sharpest box that still holds its canopy structure

// ORO patch (p): how much of the material AMBIENT the shadow takes with it, 0..1.
// 0 = stock (the shadow scales the sun only, so a shadowed VC surface keeps all its
// ambient and reads as a faint smudge); 1 = the shadow removes the ambient entirely
// and only emissive plus cockpit lights survive. EMISSIVE IS NEVER SCALED - see the
// shader comment. Cockpit pass only (Scene.cpp raises and clears it per frame), so
// exterior shading is untouched at any setting.
float g_gcVCShadowDep  = 0.0f;
float g_gcSurfaceWet   = 0.0f;   // ORO patch (s): 0 = dry (stock), 1 = soaked
float g_gcStormLight   = 0.0f;   // ORO patch (s) part 2: 0 = clear (stock), 1 = full overcast
float g_gcWetDark      = 1.0f;   // ORO patch (s) part 3: albedo darkening gain, 1 = shipped look
float g_gcWetGlint     = 1.0f;   // ORO patch (s) part 5: hull drop-glint gain, 1 = designed
float g_gcWetRefl      = 1.0f;   // ORO patch (s) part 6: puddle reflection gain, 1 = designed
float g_gcWetSwimAmp   = 1.0f;   // ORO patch (s) part 6: ripple-warp amplitude scale, 1 = designed
float g_gcWetSwimRate  = 1.0f;   // ORO patch (s) part 6: ripple cadence scale, 1 = designed
float g_gcWetPoolSize  = 1.0f;   // ORO patch (s) part 7: standing-pool lattice scale, 1 = designed
float g_gcWetPoolReach = 1.0f;   // ORO patch (s) part 7: pool visibility-distance scale, 1 = designed

void gcCore::SetVCShadows(bool bEnable, float radius, float depth)
{
	g_gcVCShadows   = bEnable;
	// Clamp rather than trust: the value reaches D3DXMatrixOrthoOffCenterRH, and a zero
	// or negative box is a degenerate projection.
	g_gcVCShadowRad = (radius < 0.25f) ? 0.25f : (radius > 50.0f ? 50.0f : radius);
	// Clamped too, and for a sharper reason: the shader SUBTRACTS depth x ambient from
	// a sum that also holds emissive, so a value outside 0..1 would either brighten the
	// shadow or drive the term negative.
	g_gcVCShadowDep = (depth < 0.0f) ? 0.0f : (depth > 1.0f ? 1.0f : depth);
}

// ORO patch (s). Clamped rather than trusted: the value scales an albedo and drives a
// Fresnel term, and outside 0..1 it would either brighten the ground or go negative.
void gcCore::SetSurfaceWetness(float wet)
{
	g_gcSurfaceWet = (wet < 0.0f) ? 0.0f : (wet > 1.0f ? 1.0f : wet);
}

// ORO patch (s) part 2. Clamped: the shaders lerp lighting terms by it, and outside 0..1
// it would invert the sun or overdrive the ambient.
void gcCore::SetStormLight(float k)
{
	g_gcStormLight = (k < 0.0f) ? 0.0f : (k > 1.0f ? 1.0f : k);
}

// ORO patch (s) part 3. Defaults to 1 so the shaders behave identically when no addon
// ever calls this.
void gcCore::SetWetDarkness(float k)
{
	g_gcWetDark = (k < 0.0f) ? 0.0f : (k > 2.0f ? 2.0f : k);
}

// ORO patch (s) part 5.
void gcCore::SetWetGlint(float k)
{
	g_gcWetGlint = (k < 0.0f) ? 0.0f : (k > 2.0f ? 2.0f : k);
}

// ORO patch (s) part 6.
void gcCore::SetWetReflection(float k, float fSwimAmp, float fSwimRate, float fPoolSize, float fPoolReach)
{
	g_gcWetRefl      = (k < 0.0f) ? 0.0f : (k > 2.0f ? 2.0f : k);
	g_gcWetSwimAmp   = (fSwimAmp   < 0.0f) ? 0.0f : (fSwimAmp   > 2.0f ? 2.0f : fSwimAmp);
	g_gcWetSwimRate  = (fSwimRate  < 0.0f) ? 0.0f : (fSwimRate  > 2.0f ? 2.0f : fSwimRate);
	g_gcWetPoolSize  = (fPoolSize  < 0.0f) ? 0.0f : (fPoolSize  > 2.0f ? 2.0f : fPoolSize);
	g_gcWetPoolReach = (fPoolReach < 0.0f) ? 0.0f : (fPoolReach > 2.0f ? 2.0f : fPoolReach);
}

// ORO patch (s) part 7: the pool grain.
float g_gcWetGrainOp   = 1.0f;
float g_gcWetGrainSize = 1.0f;
void gcCore::SetWetGrain(float fOpacity, float fSize)
{
	g_gcWetGrainOp   = (fOpacity < 0.0f) ? 0.0f : (fOpacity > 2.0f ? 2.0f : fOpacity);
	g_gcWetGrainSize = (fSize    < 0.0f) ? 0.0f : (fSize    > 2.0f ? 2.0f : fSize);
}

// Queried by vVessel::RenderReentry (declared extern there - no header churn for one bool).
bool gcIsReentrySuppressed(OBJHANDLE hVessel)
{
	return g_gcReentrySuppressed.find(hVessel) != g_gcReentrySuppressed.end();
}

// --- ORO patch (n): per-vessel ENGINE-EXHAUST suppression ------------------------
// The reentry story again, for the exhaust family: an addon drawing its own exhaust
// visuals (ORO's pressure-dependent plume overlay) has no route to the stock ones.
// The exhaust list is reachable (GetExhaustCount/DelExhaust/AddExhaust) but rewriting
// another vessel's list churns indices that vessel's own code may hold, and particle
// streams are worse - DelExhaustStream needs the PSTREAM_HANDLE only the creating
// vessel ever received. So the render side is gated here instead: billboards in
// vVessel::RenderExhaust, stream EMISSION in ExhaustStream::Update (the patch-(e)
// rule - in-flight particles expire naturally, so lifting suppression releases no
// backlog burst). Same shape as the reentry set above.
// SPLIT 2026-08-09: billboards and particle streams suppress INDEPENDENTLY.
// One flag killed both, which is wrong the moment an addon replaces only one of
// them - ORO's overlay replaces the billboards while its PARTICLES tab may or
// may not be replacing the streams. GCEXH_BILLBOARD / GCEXH_STREAM (gcCore.h).
static std::map<OBJHANDLE, DWORD> g_gcExhaustSuppressed;

void gcCore::SuppressExhaust(OBJHANDLE hVessel, DWORD flags)
{
	if (!hVessel) return;
	if (flags) g_gcExhaustSuppressed[hVessel] = flags;
	else       g_gcExhaustSuppressed.erase(hVessel);
}

static DWORD gcExhaustFlags(OBJHANDLE hVessel)
{
	std::map<OBJHANDLE, DWORD>::const_iterator it = g_gcExhaustSuppressed.find(hVessel);
	return (it == g_gcExhaustSuppressed.end()) ? 0 : it->second;
}

// Queried by vVessel::RenderExhaust (declared extern there): the BILLBOARDS only.
bool gcIsExhaustSuppressed(OBJHANDLE hVessel)
{
	return (gcExhaustFlags(hVessel) & GCEXH_BILLBOARD) != 0;
}

// Queried by ExhaustStream::Update (declared extern there): the PARTICLE STREAMS only.
bool gcIsExhaustStreamSuppressed(OBJHANDLE hVessel)
{
	return (gcExhaustFlags(hVessel) & GCEXH_STREAM) != 0;
}

// --- ORO patch (o): a LATCH marking new streams exempt from (n) ------------------
// (n) is per VESSEL, which is right for hiding what a vessel author shipped and wrong
// the moment an addon adds its OWN exhaust stream to that same vessel - the
// replacement would be suppressed along with the thing it replaces. AddParticleStream
// is not an escape route either: clbkCreateParticleStream is unimplemented in this
// client, so every usable stream is an ExhaustStream and every ExhaustStream is gated.
//
// A LATCH, not a set of stream pointers: the addon raises it, creates its streams,
// lowers it, and each ExhaustStream stamps a plain bool member at construction. The
// pointer-set version of this patch did not work - D3D9ParticleStream has TWO base
// classes, the addon holds a ParticleStream* and the gate runs with an ExhaustStream*,
// so matching them is a bet on base-subobject offsets. A member cannot miss, and the
// addon has nothing to clean up.
static bool g_gcExemptLatch = false;

void gcCore::ExemptNewStreams(bool bExempt)
{
	g_gcExemptLatch = bExempt;
}

// Read by the ExhaustStream constructors (declared extern there).
bool gcExemptLatch()
{
	return g_gcExemptLatch;
}

// ===============================================================================================
//
void gcCore::ReleaseSwap(HSWAP hSwap) 
{ 
	if (hSwap) delete ((gcSwap*)hSwap);
}






// ===============================================================================================
// Custom Camera Interface
// ===============================================================================================
//
CAMERAHANDLE gcCore::SetupCustomCamera(CAMERAHANDLE hCam, OBJHANDLE hVessel, VECTOR3 &pos, VECTOR3 &dir, VECTOR3 &up, double fov, SURFHANDLE hSurf, DWORD flags)
{
	VECTOR3 x = crossp(up, dir);
	MATRIX3 mTake;
	mTake.m11 = x.x;	mTake.m21 = x.y;	mTake.m31 = x.z;
	mTake.m12 = up.x;	mTake.m22 = up.y;	mTake.m32 = up.z;
	mTake.m13 = dir.x;	mTake.m23 = dir.y;	mTake.m33 = dir.z;
	Scene *pScene = g_client->GetScene();
	return pScene ? pScene->SetupCustomCamera(hCam, hVessel, mTake, pos, fov, hSurf, flags) : NULL;
}


// ===============================================================================================
//
void gcCore::CustomCameraOnOff(CAMERAHANDLE hCam, bool bOn)
{
	Scene *pScene = g_client->GetScene();
	if (pScene) {
		pScene->CustomCameraOnOff(hCam, bOn);
	}
}


// ===============================================================================================
//
void gcCore::CustomCameraOverlay(CAMERAHANDLE hCam, __gcRenderProc clbk, void *pUser)
{
	CAMERA(hCam)->pRenderProc = clbk;
	CAMERA(hCam)->pUser = pUser;
}


// ===============================================================================================
//
int gcCore::DeleteCustomCamera(CAMERAHANDLE hCam)
{
	Scene *pScene = g_client->GetScene();
	return pScene ? pScene->DeleteCustomCamera(hCam) : 0;
}







// ===============================================================================================
// SketchPad Interface
// ===============================================================================================


// ===============================================================================================
//
int gcCore::SketchpadVersion(Sketchpad* pSkp)
{
	return ((D3D9Pad*)pSkp)->GetVersion();
}


// ===============================================================================================
//
oapi::Font* gcCore::CreateSketchpadFont(int height, char* face, int width, int weight, FontStyle Style, float spacing)
{
	return g_client->clbkCreateFontEx(height, face, width, weight, Style, spacing);
}


// ===============================================================================================
//
HPOLY gcCore::CreatePoly(HPOLY hPoly, const FVECTOR2 *pt, int npt, DWORD flags)
{
	LPDIRECT3DDEVICE9 pDev = g_client->GetDevice();
	if (!hPoly) return new D3D9PolyLine(pDev, pt, npt, (flags&PF_CONNECT) != 0);
	((D3D9PolyLine *)hPoly)->Update(pt, npt, (flags&PF_CONNECT) != 0);
	return hPoly;
}


// ===============================================================================================
//
HPOLY gcCore::CreateTriangles(HPOLY hPoly, const gcCore::clrVtx *pt, int npt, DWORD flags)
{
	LPDIRECT3DDEVICE9 pDev = g_client->GetDevice();
	if (!hPoly) return new D3D9Triangle(pDev, pt, npt, flags);
	((D3D9Triangle *)hPoly)->Update(pt, npt);
	return hPoly;
}


// ORO patch (g): CreateTriangles with a per-vertex camera-space depth (pDepth) for the
// depth-clip draw path. Identical lifecycle to CreateTriangles - create once, update in
// place - just threading the depth array through to the vertex buffer's spare .l channel.
HPOLY gcCore::CreateTrianglesDepth(HPOLY hPoly, const gcCore::clrVtx *pt, const float *pDepth, int npt, DWORD flags)
{
	LPDIRECT3DDEVICE9 pDev = g_client->GetDevice();
	if (!hPoly) return new D3D9Triangle(pDev, pt, npt, flags, pDepth);
	((D3D9Triangle *)hPoly)->Update(pt, npt, pDepth);
	return hPoly;
}


// ORO patch (g): does the scene depth buffer exist this session (SunGlare on)?
bool gcCore::HasDepthBuffer()
{
	Scene *pScene = g_client->GetScene();
	return pScene && (pScene->GetDepthTexture() != NULL);
}


// ORO patch (l): CreateTriangles with TEXTURED vertices (texture x Gouraud colour per
// fragment) + the optional per-vertex depth of patch (g). Same create-once/update-in-place
// lifecycle as the other two. The texture is resolved to its D3D9 object here and stored
// on the poly; DrawPoly binds it through the pad's own texture state machinery.
HPOLY gcCore::CreateTrianglesTex(HPOLY hPoly, const gcCore::texVtx *pt, const float *pDepth, int npt, DWORD flags, SURFHANDLE hTex)
{
	LPDIRECT3DDEVICE9 pDev = g_client->GetDevice();
	LPDIRECT3DTEXTURE9 pTex = (hTex && SURFACE(hTex)->IsTexture()) ? SURFACE(hTex)->GetTexture() : NULL;
	if (!hPoly) {
		D3D9Triangle *pT = new D3D9Triangle(pDev, NULL, npt, flags);
		pT->SetTex(pTex);
		if (pt) pT->UpdateTex(pt, npt, pDepth);
		return pT;
	}
	D3D9Triangle *pT = (D3D9Triangle *)hPoly;
	pT->SetTex(pTex);
	pT->UpdateTex(pt, npt, pDepth);
	return hPoly;
}


// ORO patch (l): CPU bytes -> texture surface, via a SYSTEMMEM staging texture and
// UpdateTexture (the canonical D3D9 upload; the destination is the plain DEFAULT-pool
// texture oapiCreateSurfaceEx(OAPISURFACE_TEXTURE) creates). Main thread only.
bool gcCore::UpdateTexture2D(SURFHANDLE hSurf, const void* pBits, int w, int h)
{
	if (!hSurf || !pBits || w <= 0 || h <= 0) return false;
	if (!SURFACE(hSurf)->IsTexture()) return false;
	LPDIRECT3DTEXTURE9 pTex = SURFACE(hSurf)->GetTexture();
	if (!pTex) return false;

	D3DSURFACE_DESC desc;
	if (pTex->GetLevelDesc(0, &desc) != S_OK) return false;
	if ((int)desc.Width != w || (int)desc.Height != h) return false;
	if (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8) return false;
	if (desc.Usage & D3DUSAGE_RENDERTARGET) return false;   // UpdateTexture cannot target a RT

	LPDIRECT3DDEVICE9 pDev = g_client->GetDevice();
	LPDIRECT3DTEXTURE9 pStage = NULL;
	// Staging matches the destination's full mip chain so UpdateTexture's level pairing
	// holds; only level 0 is filled (create surfaces with OAPISURFACE_NOMIPMAPS).
	if (pDev->CreateTexture(w, h, pTex->GetLevelCount(), 0, desc.Format, D3DPOOL_SYSTEMMEM, &pStage, NULL) != S_OK) return false;

	bool ok = false;
	D3DLOCKED_RECT lr;
	if (pStage->LockRect(0, &lr, NULL, 0) == S_OK) {
		for (int y = 0; y < h; y++)
			memcpy((BYTE*)lr.pBits + (size_t)y * lr.Pitch, (const BYTE*)pBits + (size_t)y * w * 4, (size_t)w * 4);
		pStage->UnlockRect(0);
		ok = (pDev->UpdateTexture(pStage, pTex) == S_OK);
	}
	pStage->Release();
	return ok;
}

// ===============================================================================================
// ORO patch (k): the render camera, for CPU projection of world-anchored geometry from
// inside a render callback. See gcCore.h for why the pre/post-step camera cannot serve.
bool gcCore::GetRenderCam(VECTOR3* pos, MATRIX3* rot, double* tanAp)
{
	if (!pos || !rot || !tanAp) return false;
	Scene *pScene = g_client->GetScene();
	if (!pScene) return false;
	pScene->GetRenderCam(pos, rot, tanAp);
	return true;
}

// ===============================================================================================
// ORO patch (k2): the render-epoch position of a body - the anchor companion to
// GetRenderCam (see gcCore.h). vObject::gpos is refreshed from oapiGetGlobalPos at
// render start, in the same breath as the camera, so it IS the frame's number.
bool gcCore::GetRenderObjPos(OBJHANDLE hObj, VECTOR3* pos)
{
	if (!hObj || !pos) return false;
	Scene *pScene = g_client->GetScene();
	if (!pScene) return false;
	vObject *vo = pScene->GetVisObject(hObj);
	if (!vo) return false;
	*pos = vo->GlobalPos();
	return true;
}


// ===============================================================================================
//
void gcCore::DeletePoly(HPOLY hPoly)
{
	if (hPoly) {
		((D3D9PolyBase *)hPoly)->Release();
		delete ((D3D9PolyBase *)hPoly);
	}
}


// ===============================================================================================
//
DWORD gcCore::GetTextLength(oapi::Font *hFont, const char *pText, int len)
{
	return DWORD((static_cast<D3D9PadFont *>(hFont))->GetTextLength(pText, len));
}


// ===============================================================================================
//
DWORD gcCore::GetCharIndexByPosition(oapi::Font *hFont, const char *pText, int pos, int len)
{
	return DWORD((static_cast<D3D9PadFont *>(hFont))->GetIndexByPosition(pText, pos, len));
}


// ===============================================================================================
//
bool gcCore::RegisterRenderProc(__gcRenderProc proc, DWORD flags, void *pParam)
{
	return g_client->RegisterRenderProc(proc, flags, pParam);
}




// ===============================================================================================
// Mesh interface functions
// ===============================================================================================
//
int gcCore::GetMatrix(MatrixId matrix_id, OBJHANDLE hVessel, DWORD mesh, DWORD group, FMATRIX4 *pMat)
{
	if (oapiGetObjectType(hVessel) != OBJTP_VESSEL) return -10;
	Scene *pScn = g_client->GetScene();
	vVessel *pVes = (vVessel *)pScn->GetVisObject(hVessel);
	if (pVes) return pVes->GetMatrixTransform(matrix_id, mesh, group, pMat);
	return -11;
}


// ===============================================================================================
//
int gcCore::SetMatrix(MatrixId matrix_id, OBJHANDLE hVessel, DWORD mesh, DWORD group, const FMATRIX4 *pMat)
{
	if (oapiGetObjectType(hVessel) != OBJTP_VESSEL) return -10;
	Scene *pScn = g_client->GetScene();
	vVessel *pVes = (vVessel *)pScn->GetVisObject(hVessel);
	if (pVes) return pVes->SetMatrixTransform(matrix_id, mesh, group, pMat);
	return -11;
}


// ===============================================================================================
//
int gcCore::GetMeshMaterial(DEVMESHHANDLE hMesh, DWORD idx, MatProp prop, FVECTOR4* value)
{
	return g_client->clbkMeshMaterialEx(hMesh, idx, prop, value);
}


// ===============================================================================================
//
int gcCore::SetMeshMaterial(DEVMESHHANDLE hMesh, DWORD idx, MatProp prop, const FVECTOR4* value)
{
	return g_client->clbkSetMeshMaterialEx(hMesh, idx, prop, value);
}

// ===============================================================================================
//
DEVMESHHANDLE gcCore::GetDevMesh(MESHHANDLE hMesh)
{
	return g_client->GetDevMesh(hMesh);
}


// ===============================================================================================
//
DEVMESHHANDLE gcCore::LoadDevMeshGlobal(const char* file_name, bool bUseCache)
{
	MESHHANDLE hMesh = oapiLoadMeshGlobal(file_name);
	return g_client->GetDevMesh(hMesh);
}


// ===============================================================================================
//
void gcCore::ReleaseDevMesh(DEVMESHHANDLE hMesh)
{
	delete (D3D9Mesh*)(hMesh);
}


// ===============================================================================================
//
void gcCore::RenderMesh(DEVMESHHANDLE hMesh, const oapi::FMATRIX4* pWorld)
{
	Scene* pScene = g_client->GetScene();
	pScene->RenderMesh(hMesh, pWorld);
}


// ===============================================================================================
//
bool gcCore::PickMesh(PickMeshStruct* pm, DEVMESHHANDLE hMesh, const FMATRIX4* pWorld, short x, short y)
{
	Scene* pScene = g_client->GetScene();
	D3D9Pick pk = pScene->PickMesh(hMesh, (const LPD3DXMATRIX)pWorld, x, y);
	if (pk.group >= 0) {
		if (pk.dist < pm->dist) {
			pm->pos = _FV(pk.pos);
			pm->normal = _FV(pk.normal);
			pm->grp_inst = pk.group;
			pm->dist = pk.dist;
			return true;
		}
	}
	return false;
}







// ===============================================================================================
// Custom Render Interface
// ===============================================================================================
//
// ===============================================================================================
//
SURFHANDLE gcCore::LoadSurface(const char* fname, DWORD flags)
{
	return g_client->clbkLoadSurface(fname, flags);
}

// ===============================================================================================
//
bool gcCore::SaveSurface(const char* file, SURFHANDLE hSrf)
{
	return NatSaveSurface(file, SURFACE(hSrf)->GetResource());
}

// ===============================================================================================
//
SURFHANDLE gcCore::GetMipSublevel(SURFHANDLE hSrf, int level)
{
	return NatGetMipSublevel(hSrf, level);
}

// ===============================================================================================
//
bool gcCore::GenerateMipmaps(SURFHANDLE hSurface)
{
	return NatGenerateMipmaps(hSurface);
}

// ===============================================================================================
//
SURFHANDLE gcCore::CompressSurface(SURFHANDLE hSurface, DWORD flags)
{
	return NatCompressSurface(hSurface, flags);
}


// ===============================================================================================
//
void gcCore::RenderLines(const FVECTOR3* pVtx, const WORD* pIdx, int nVtx, int nIdx, const FMATRIX4* pWorld, DWORD color)
{
	D3D9Effect::RenderLines((const D3DXVECTOR3*)pVtx, pIdx, nVtx, nIdx, (const D3DXMATRIX*)pWorld, color);
}


// ===============================================================================================
//
bool gcCore::StretchRectInScene(SURFHANDLE tgt, SURFHANDLE src, LPRECT tr, LPRECT sr)
{
	if (S_OK == g_client->BeginScene())
	{
		LPDIRECT3DSURFACE9 pss = SURFACE(src)->GetSurface();
		LPDIRECT3DSURFACE9 pts = SURFACE(tgt)->GetSurface();
		HRESULT hr = g_client->GetDevice()->StretchRect(pss, sr, pts, tr, D3DTEXF_LINEAR);
		g_client->EndScene();
		return hr == S_OK;
	}
	return false;
}

// ===============================================================================================
//
bool gcCore::ClearSurfaceInScene(SURFHANDLE tgt, DWORD color, LPRECT tr)
{
	if (S_OK == g_client->BeginScene())
	{
		LPDIRECT3DSURFACE9 pts = SURFACE(tgt)->GetSurface();
		HRESULT hr = g_client->GetDevice()->ColorFill(pts, tr, (D3DCOLOR)color);
		g_client->EndScene();
		return hr == S_OK;
	}
	return false;
}





// ===============================================================================================
// Some Helper Functions
// ===============================================================================================
//
// ===============================================================================================
//
gcCore::PickGround gcCore::ScanScreen(int scr_x, int scr_y)
{
	PickGround pg; memset(&pg, 0, sizeof(PickGround));

	Scene* pScene = g_client->GetScene();
	TILEPICK tp = pScene->PickSurface(scr_x, scr_y);
	SurfTile* pTile = static_cast<SurfTile*>(tp.pTile);

	if (pTile) {

		pTile->GetIndex(&pg.iLng, &pg.iLat);

		pg.Bounds.left = pTile->bnd.minlng;
		pg.Bounds.right = pTile->bnd.maxlng;
		pg.Bounds.top = pTile->bnd.maxlat;
		pg.Bounds.bottom = pTile->bnd.minlat;

		pg.lat = tp.lat;
		pg.lng = tp.lng;

		pg.emax = float(pTile->GetMaxElev());
		pg.emin = float(pTile->GetMinElev());

		pg.msg = 0;
		pg.dist = tp.d;
		pg.elev = tp.elev;
		pg.level = pTile->Level();
		pg.hTile = HTILE(pTile);
		pg.normal = _FV(tp._n);
		pg.pos = _FV(tp._p);
	}
	return pg;
}


// ===============================================================================================
//
void gcCore::GetSystemSpecs(SystemSpecs* sp, int size)
{
	if (size == sizeof(SystemSpecs)) {
		sp->DisplayMode = g_client->GetFramework()->GetDisplayMode();
		sp->MaxTexSize = g_client->GetHardwareCaps()->MaxTextureWidth;
		sp->MaxTexRep = g_client->GetHardwareCaps()->MaxTextureRepeat;
		sp->gcAPIVer = BuildDate();
	}
}

// ===============================================================================================
//
bool gcCore::GetSurfaceSpecs(SURFHANDLE hSrf, SurfaceSpecs* sp, int size)
{
	return SURFACE(hSrf)->GetSpecs(sp, size);
}


// ===============================================================================================
//
bool gcCore::RegisterGenericProc(__gcGenericProc proc, DWORD id, void* pParam)
{
	return g_client->RegisterGenericProc(proc, id, pParam);
}


// ===============================================================================================
//
HBITMAP	gcCore::LoadBitmapFromFile(const char* fname)
{
	return g_client->gcReadImageFromFile(fname);
}


// ===============================================================================================
//
HWND gcCore::GetRenderWindow()
{
	return g_client->GetRenderWindow();
}


// ===============================================================================================
// gcCore2 Interface --- Tile access interface functions
// ===============================================================================================
//

HPLANETMGR gcCore2::GetPlanetManager(OBJHANDLE hPlanet)
{
	Scene *pScene = g_client->GetScene();
	vPlanet *vPl = (vPlanet *)pScene->GetVisObject(hPlanet);
	return HPLANETMGR(vPl);
}


// ===============================================================================================
//
HTILE gcCore2::GetTile(HPLANETMGR vPl, double lng, double lat, int maxlevel)
{
	vPlanet *vP = static_cast<vPlanet *>(vPl);
	return HTILE(vP->FindTile(lng, lat, maxlevel));
}


// ===============================================================================================
//
gcCore::PickGround gcCore2::GetTileData(HPLANETMGR vPl, double lng, double lat, int maxlevel)
{
	PickGround pg; memset(&pg, 0, sizeof(PickGround));
	if (!vPl) return pg;

	vPlanet *vP = static_cast<vPlanet *>(vPl);
	SurfTile *pTile = static_cast<SurfTile *>(vP->FindTile(lng, lat, maxlevel));

	if (!pTile) {
		oapiWriteLogV("gcCore::FindTile() Failed");
		return pg;
	}

	pTile->GetIndex(&pg.iLng, &pg.iLat);
	pTile->GetElevation(lng, lat, &pg.elev, &pg.normal, NULL, true, false);

	VECTOR3 pos = vP->GetUnitSurfacePos(lng, lat) * (vP->GetSize() + pg.elev);
	MATRIX3 mRot; oapiGetRotationMatrix(vP->Object(), &mRot);

	pos = mul(mRot, pos) + vP->PosFromCamera();

	pg.Bounds.left = pTile->bnd.minlng;
	pg.Bounds.right = pTile->bnd.maxlng;
	pg.Bounds.top = pTile->bnd.maxlat;
	pg.Bounds.bottom = pTile->bnd.minlat;

	pg.lat = lat;
	pg.lng = lng;

	pg.emax = float(pTile->GetMaxElev());
	pg.emin = float(pTile->GetMinElev());

	pg.msg = 0;
	pg.dist = length(pos);
	pg.level = pTile->Level();
	pg.hTile = HTILE(pTile);
	pg.pos = FVECTOR3(pos);

	return pg;
}

// ===============================================================================================
//
bool gcCore2::SeekTileElevation(HPLANETMGR hMgr, int iLng, int iLat, int level, int flags, ElevInfo *pInfo)
{
	ELEVFILEHEADER hdr;
	if (!hMgr) return false;
	if (((vPlanet*)(hMgr))->SurfMgr2()) {
		float* pData = ((vPlanet*)(hMgr))->SurfMgr2()->BrowseElevationData(level, iLat, iLng, flags, &hdr);
		if (!pData) return false;
		pInfo->MaxElev = hdr.emax;
		pInfo->MinElev = hdr.emin;
		pInfo->MeanElev = hdr.emean;
		pInfo->Resolution = hdr.scale;
		pInfo->Offset = hdr.offset;
		pInfo->pElevData = pData;
		return true;
	}
	return false;
}


// ===============================================================================================
//
SURFHANDLE gcCore2::SeekTileTexture(HPLANETMGR hMgr, int iLng, int iLat, int level, int flags, void *reserved)
{
	if (!hMgr) return NULL;
	if (((vPlanet *)(hMgr))->SurfMgr2()) {
		return ((vPlanet *)(hMgr))->SurfMgr2()->SeekTileTexture(iLng, iLat, level, flags);
	}
	return NULL;
}


// ===============================================================================================
//
bool gcCore2::HasTileData(HPLANETMGR hMgr, int iLng, int iLat, int level, int flags)
{
	if (!hMgr) return false;
	if (((vPlanet *)(hMgr))->SurfMgr2()) {
		return ((vPlanet *)(hMgr))->SurfMgr2()->HasTileData(iLng, iLat, level, flags);
	}
	return false;
}


// ===============================================================================================
//
int gcCore2::GetElevation(HTILE hTile, double lng, double lat, double *out_elev)
{
	SurfTile *pTile = static_cast<SurfTile *>(hTile);
	return pTile->GetElevation(lng, lat, out_elev, NULL, NULL, true, true);
}


// ===============================================================================================
//
SURFHANDLE gcCore2::SetTileOverlay(HTILE hTile, const SURFHANDLE hOverlay)
{
	//SurfTile *pTile = static_cast<SurfTile *>(hTile);
	//LPDIRECT3DTEXTURE9 pTex = static_cast<LPDIRECT3DTEXTURE9>(hOverlay);
	//return HSURFNATIVE(pTile->SetOverlay(pTex, true));
	return NULL;
}


// ===============================================================================================
//
HOVERLAY gcCore2::AddGlobalOverlay(HPLANETMGR hMgr, VECTOR4 mmll, OlayType type, const SURFHANDLE hOverlay, HOVERLAY hOld, const FVECTOR4* pBlend)
{
	if (!hMgr) return NULL;
	vPlanet *vP = static_cast<vPlanet *>(hMgr);
	vPlanet::sOverlay* oLay = static_cast<vPlanet::sOverlay*>(hOld);
	if (hOverlay) {
		LPDIRECT3DTEXTURE9 pTex = SURFACE(hOverlay)->GetTexture();
		return vP->AddOverlaySurface(mmll, type, pTex, oLay, pBlend);
	}
	return vP->AddOverlaySurface(mmll, type, NULL, oLay, pBlend);
}

// ===============================================================================================
//
bool gcCore::LockSurface(SURFHANDLE hSrf, Lock* pOut, bool bWait)
{
	D3DLOCKED_RECT lock;
	LPDIRECT3DRESOURCE9 pResource = SURFACE(hSrf)->GetResource();
	DWORD flags = 0;
	if (!bWait) flags |= D3DLOCK_DONOTWAIT;

	if (pResource->GetType() == D3DRTYPE_SURFACE) {
		LPDIRECT3DSURFACE9 pSurf = static_cast<LPDIRECT3DSURFACE9>(hSrf);
		if (HROK(pSurf->LockRect(&lock, NULL, flags))) {
			pOut->pData = lock.pBits;
			pOut->Pitch = lock.Pitch;
			return true;
		}
		return false;
	}

	if (pResource->GetType() == D3DRTYPE_TEXTURE) {
		LPDIRECT3DTEXTURE9 pTex = static_cast<LPDIRECT3DTEXTURE9>(hSrf);
		if (HROK(pTex->LockRect(0, &lock, NULL, flags))) {
			pOut->pData = lock.pBits;
			pOut->Pitch = lock.Pitch;
			return true;
		}
		return false;
	}

	return false;
}


// ===============================================================================================
//
void gcCore::ReleaseLock(SURFHANDLE hSrf)
{
	LPDIRECT3DRESOURCE9 pResource = SURFACE(hSrf)->GetResource();
	if (pResource->GetType() == D3DRTYPE_SURFACE) {
		LPDIRECT3DSURFACE9 pSurf = static_cast<LPDIRECT3DSURFACE9>(hSrf);
		HR(pSurf->UnlockRect());
	}
	if (pResource->GetType() == D3DRTYPE_TEXTURE) {
		LPDIRECT3DTEXTURE9 pTex = static_cast<LPDIRECT3DTEXTURE9>(hSrf);
		HR(pTex->UnlockRect(0));
	}
}

// ===============================================================================================
//
gcIPInterface* gcCore2::CreateIPInterface(const char* file, const char* PSEntry, const char* VSEntry, const char* ppf)
{
	ImageProcessing* pIPI = new ImageProcessing(g_client->GetDevice(), file, PSEntry, ppf);

	if (pIPI->IsOK() == false) {
		oapiWriteLogV("gcCore::CreateIPInterface() Failed !  File = [%s]", file);
		return NULL;
	}
	return new gcIPInterface(pIPI);
}

// ===============================================================================================
//
void gcCore2::ReleaseIPInterface(gcIPInterface* pIPI)
{
	if (!pIPI) return;
	if (pIPI->pIPI) delete pIPI->pIPI;
	delete pIPI;
}

