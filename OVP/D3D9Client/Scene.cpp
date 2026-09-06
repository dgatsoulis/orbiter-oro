// ==============================================================
// Scene.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2007 - 2016 Martin Schweiger
//				 2012 - 2016 Jarmo Nikkanen
// ==============================================================

#include "Scene.h"
#include "VPlanet.h"
#include "VVessel.h"
#include "VBase.h"
#include "Particle.h"
#include "CSphereMgr.h"
#include "D3D9Util.h"
#include "D3D9Config.h"
#include "D3D9Surface.h"
#include "D3D9TextMgr.h"
#include "D3D9Catalog.h"
#include "AABBUtil.h"
#include "OapiExtension.h"
#include "DebugControls.h"
#include "IProcess.h"
#include "VectorHelpers.h"
#include <sstream>
#include <vector>

#define IKernelSize 150

using namespace oapi;

// ORO patch (g): the live scene depth texture, defined in D3D9Pad.cpp and read by
// D3D9Pad::Flush's depth-clip path. Set here at buffer creation, cleared at teardown.
extern LPDIRECT3DTEXTURE9 g_gcSceneDepth;

static D3DXMATRIX ident;

const double LABEL_DISTLIMIT = 0.6;

struct PList { // auxiliary structure for object distance sorting
	vObject *vo;
	double dist;
};

const int MAXPLANET = 512; // hard limit; should be fixed
static PList plist[MAXPLANET];

ID3DXEffect * Scene::FX = 0;
D3DXHANDLE Scene::eLine = 0;
D3DXHANDLE Scene::eStar = 0;
D3DXHANDLE Scene::eWVP = 0;
D3DXHANDLE Scene::eColor = 0;
D3DXHANDLE Scene::eTex0 = 0;


D3DXVECTOR4 IKernel[IKernelSize];

bool sort_vessels(const vVessel *a, const vVessel *b)
{
	return a->CameraTgtDist() < b->CameraTgtDist();
}

float Rand()
{
	return float(rand()) / 32768.0f;
}

// ===========================================================================================
//
Scene::Scene(D3D9Client *_gc, DWORD w, DWORD h)
{
	_TRACE;

	gc = _gc;
	vobjEnv = NULL;
	vobjIrd = NULL;
	m_celSphere = NULL;
	Lights = NULL;
	hSun = NULL;
	pAxisFont  = NULL;
	pLabelFont = NULL;
	pDebugFont = NULL;
	pBlur = NULL;
	pOffscreenTarget = NULL;
	pLocalCompute = NULL;
	pRenderGlares = NULL;
	ptWetRefl = NULL;
	psWetRefl = NULL;
	psWetReflDS = NULL;
	ptRflPln[0] = ptRflPln[1] = NULL;	// ORO patch (v) part 2
	ptPShn = NULL; psPShn = NULL;		// ORO patch (w)
	ptSunCpy = NULL; psSunCpy = NULL; sunCpyLive = false;
	memset(&sunCpy, 0, sizeof(sunCpy));
	memset(&pshn, 0, sizeof(pshn));
	psRflPln[0] = psRflPln[1] = NULL;
	pRflPlnVes = NULL;
	nRflPlnLive = 0;
	rflPlnEq[0] = rflPlnEq[1] = D3DXVECTOR4(0, 0, 0, 0);
	rflPlnDist[0] = rflPlnDist[1] = 5.0f;
	bWetReflLive = false;
	bMirrorCam = false;			// ORO patch (u): only true inside the wet-mirror render proc
	mirrorCamPos = _V(0, 0, 0);
	mirrorCamRot = identity();
	pCreateGlare = NULL;
	viewH = h;
	viewW = w;
	nLights = 0;
	dwTurn = 0;
	dwFrameId = 0;
	surfLabelsActive = false;

	pSunTex = NULL;
	pLightGlare = NULL;
	pSunGlare = NULL;
	pSunGlareAtm = NULL;
	pEnvDS = NULL;
	pIrradDS = NULL;
	pIrradiance = NULL;
	pIrradTemp = NULL;
	pIrradTemp2 = NULL;
	pIrradTemp3 = NULL;
	pDepthNormalDS = NULL;
	pVisDepth = NULL;
	pLocalResults = NULL;
	pLocalResultsSL = NULL;

	fDisplayScale = float(viewH) / 1080.0f;

	for (auto& a : DepthSampleKernel) a = FVECTOR2(0, 0);

	memset(&psShmDS, 0, sizeof(psShmDS));
	memset(&ptShmRT, 0, sizeof(ptShmRT));
	memset(&psShmRT, 0, sizeof(psShmRT));

	// ORO patch (z3): the local-light shadow map starts dark and unowned
	ptLclShm = NULL;
	psLclShm = NULL;
	psLclShmDS = NULL;
	pTileShd = NULL;
	pTileDepth = NULL;	// ORO patch (ab)
	ptCasc = NULL; psCasc = NULL; psCascDS = NULL; cascSize = 0; cascLive = false; cascLclLive = false;   // ORO patch (ae)
	memset(casc, 0, sizeof(casc)); memset(cascSplit, 0, sizeof(cascSplit)); cascAtl = D3DXVECTOR4(0, 0, 0, 0);
	for (int i = 0; i < 9; i++) { cascAnch[i] = _V(0, 0, 0); cascAnchTexel[i] = 0.0f; cascAnchOK[i] = false; }
	cascAnchPlanet = NULL; cascDbgAnchD = 0.0f;
	cascVes[0] = cascVes[1] = cascVes[2] = NULL; cascU = cascV = cascL = D3DXVECTOR3(0, 0, 0);
	memset(&lsmap, 0, sizeof(lsmap));
	lsmap.idx = -1;
	memset(&LightOwners, 0, sizeof(LightOwners));



	pDevice = _gc->GetDevice();

	memset(&Camera, 0, sizeof(Camera));

	D3DXMatrixIdentity(&ident);

	SetCameraAperture(float(RAD*50.0), float(viewH)/float(viewW));
	SetCameraFrustumLimits(2.5f, 5e6f); // initial limits

	m_celSphere = new D3D9CelestialSphere(gc, this);
	Lights = new D3D9Light[MAX_SCENE_LIGHTS];

	bLocalLight = *(bool*)gc->GetConfigParam(CFGPRM_LOCALLIGHT);
	
	memset(&sunLight, 0, sizeof(D3D9Sun));
	memset(&smap, 0, sizeof(smap));

	CLEARARRAY(pBlrTemp);
	CLEARARRAY(pTextures);
	CLEARARRAY(ptgBuffer);
	CLEARARRAY(psgBuffer);

	vobjFirst = vobjLast = NULL;
	nstream = 0;
	iVCheck = 0;

	InitGDIResources();

	while (true) {
		float dx = 0;
		float dy = 0;
		for (int i = 0; i < IKernelSize; i++) {
			double r = oapiRand();
			double a = oapiRand() * PI2;
			IKernel[i].x = float(cos(a) * r);
			IKernel[i].y = float(sin(a) * r);
			dx += IKernel[i].x;
			dy += IKernel[i].y;
		}
		if ((abs(dx) < 1.0) && (abs(dy) < 1.0)) break;
	}

	for (int i = 0; i < IKernelSize; i++) {
		float d = IKernel[i].x*IKernel[i].x + IKernel[i].y*IKernel[i].y;
		IKernel[i].z = sqrt(1.0f - saturate(d));
		IKernel[i].w = sqrt(IKernel[i].z);
	}



	// ------------------------------------------------------------------------------
	// Read Sun glare sampling kernel file

	ifstream fs("Modules/D3D9Client/GKernel.txt");
	if (fs.good()) {
		string line; vector<FVECTOR2> data;
		while (getline(fs, line)) {
			std::istringstream iss(line);
			char c;	float a, b;	iss >> a >> c >> b;
			data.push_back(FVECTOR2(a, b));
		}
		if (data.size() != ARRAYSIZE(DepthSampleKernel)) LogErr("Modules/D3D9Client/GKernel.txt Size missmatch. Expecting 57 entries");
		else for (int i = 0; i < ARRAYSIZE(DepthSampleKernel); i++) DepthSampleKernel[i] = data[i];
		data.clear();
	} else LogErr("Failed to read: Modules/D3D9Client/GKernel.txt");
	fs.close();

	CreateSunGlare();

	// ------------------------------------------------------------------------------
	// ORO patch (s) part 6: the WET-GROUND PLANAR REFLECTION target. Half resolution -
	// the image lands in rippling puddles through a mask, so full res would buy nothing.
	// Created unconditionally (~2 MB); the PASS that fills it is gated on wetness, so a
	// dry world never pays a frame cost.
	HR(pDevice->CreateTexture(viewW / 2, viewH / 2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &ptWetRefl, NULL));
	if (ptWetRefl) HR(ptWetRefl->GetSurfaceLevel(0, &psWetRefl));
	HR(pDevice->CreateDepthStencilSurface(viewW / 2, viewH / 2, D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, true, &psWetReflDS, NULL));
	// ORO patch (v) part 2: the VESSEL planar-mirror targets - same size, same format,
	// same reasoning; the pass that fills them is gated on a vessel DECLARING planes,
	// so nobody pays a frame cost without asking for one. They share the wet mirror's
	// depth-stencil: the passes run strictly one after another and each clears it.
	for (int i = 0; i < 2; i++) {
		HR(pDevice->CreateTexture(viewW / 2, viewH / 2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &ptRflPln[i], NULL));
		if (ptRflPln[i]) HR(ptRflPln[i]->GetSurfaceLevel(0, &psRflPln[i]));
	}


	// ------------------------------------------------------------------------------
	// Initialize a shaders for local lights visibility checks and rendering 

	if (Config->bGlares || Config->bLocalGlares)
	{
		pRenderGlares = new ShaderClass(pDevice, "Modules/D3D9Client/Glare.hlsl", "GlareVS", "GlarePS", "RenderGlares", "");
		pLocalCompute = new ShaderClass(pDevice, "Modules/D3D9Client/Glare.hlsl", "VisibilityVS", "VisibilityPS", "LocalVisCheck", "");
		D3DXCreateTexture(pDevice, 32, 1, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R16F, D3DPOOL_DEFAULT, &pLocalResults);
		HR(pLocalResults->GetSurfaceLevel(0, &pLocalResultsSL));
	}


	// Render screen depth and screen space normals
	//

	if (Config->bGlares || Config->bLocalGlares) {
		pVisDepth = new ImageProcessing(pDevice, "Modules/D3D9Client/LightBlur.hlsl", "PSDepth", NULL);
		pVisDepth->CompileShader("PSNormal");
	}

	// Initialize envmapping and shadow maps -----------------------------------------------------------------------------------------------
	//
	DWORD EnvMapSize = Config->EnvMapSize;
	DWORD ShmMapSize = Config->ShadowMapSize;

	if (Config->EnvMapMode) {
		HR(pDevice->CreateDepthStencilSurface(EnvMapSize, EnvMapSize, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, true, &pEnvDS, NULL));
	}

	if (Config->bIrradiance) {
		HR(pDevice->CreateDepthStencilSurface(128, 128, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, true, &pIrradDS, NULL));
	}


	if (Config->ShadowMapMode) {
		UINT size = ShmMapSize;
		for (int i = 0; i < SHM_LOD_COUNT; i++) {
			HR(pDevice->CreateDepthStencilSurface(size, size, D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, true, &psShmDS[i], NULL));
			HR(pDevice->CreateTexture(size, size, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &ptShmRT[i], NULL));
			HR(ptShmRT[i]->GetSurfaceLevel(0, &psShmRT[i]));
			size >>= 1;
		}

		smap.pShadowMap = ptShmRT[0];
	}

	// ORO patch (z3): the local-light shadow map's own target. Created only when the
	// feature is on AND local light sources are enabled at all - a map nobody would
	// ever render into is VRAM for nothing. Same R32F + D24X8 recipe as the sun's;
	// capped at 2048 because a spot light's footprint never earns a 4096 map.
	if (Config->LocalLightShadows && bLocalLight) {
		UINT lsize = min(ShmMapSize, (DWORD)2048);
		HR(pDevice->CreateDepthStencilSurface(lsize, lsize, D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, true, &psLclShmDS, NULL));
		HR(pDevice->CreateTexture(lsize, lsize, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &ptLclShm, NULL));
		if (ptLclShm) {
			HR(ptLclShm->GetSurfaceLevel(0, &psLclShm));
			lsmap.pShadowMap = ptLclShm;
			lsmap.size = (int)lsize;
			// ORO patch (z3) round 2c: the depth-only tile shader for terrain casters
			pTileShd = new ShaderClass(pDevice, "Modules/D3D9Client/NewPlanet.hlsl", "TileShdVS", "TileShdPS", "OroLclTileShd", NULL);
		}
	}

	// ORO patch (ae): THE CASCADE ATLAS - four sun maps in one R32F texture (2x2 slots of
	// ShadowCascadeSize) plus its depth-stencil. Terrain shadowing mode 3 only - and
	// INDEPENDENT of the vessel map mode since round 9 (it never used the per-vessel
	// maps; "self-shadows None + Cascaded" is a legitimate cheaper setting, the focus
	// hull still shadows itself through its own box). The tile caster shader is shared
	// with the local map.
	if (Config->TerrainShadowing == 3) {
		cascSize = max(512, min(4096, Config->ShadowCascadeSize));
		// the atlas is 3 x 2 cascades (round 7) - halve the cascade until the device can
		// hold a texture three of them wide and two tall (DX9-class hardware caps at
		// 4096, DX10-class at 8192; 4096 cascades need the 16384 of DX11-class cards)
		D3DCAPS9 caps; ZeroMemory(&caps, sizeof(caps)); pDevice->GetDeviceCaps(&caps);
		while (cascSize > 512 && ((UINT)cascSize * 3 > caps.MaxTextureWidth || (UINT)cascSize * 2 > caps.MaxTextureHeight)) cascSize /= 2;
		UINT aw = (UINT)cascSize * 3, ah = (UINT)cascSize * 2;
		HR(pDevice->CreateDepthStencilSurface(aw, ah, D3DFMT_D24X8, D3DMULTISAMPLE_NONE, 0, true, &psCascDS, NULL));
		HR(pDevice->CreateTexture(aw, ah, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &ptCasc, NULL));
		if (ptCasc) HR(ptCasc->GetSurfaceLevel(0, &psCasc));
		if (!pTileShd) pTileShd = new ShaderClass(pDevice, "Modules/D3D9Client/NewPlanet.hlsl", "TileShdVS", "TileShdPS", "OroLclTileShd", NULL);
		oapiWriteLogV("D3D9: ORO cascaded shadows - atlas %u x %u (nine slots: three of %d, six of %d; %.0f MB with its depth), far %.0f m",
			aw, ah, cascSize, cascSize / 2, (double)aw * ah * 8.0 / 1048576.0, Config->ShadowCascadeFar);
	}

// Create auxiliary color buffer for on screen GDI
	//
	if (Config->GDIOverlay) {
		HR(D3DXCreateTexture(pDevice, viewW, viewH, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &ptgBuffer[GBUF_GDI]));
		pGDIOverlay = new ImageProcessing(pDevice, "Modules/D3D9Client/GDIOverlay.hlsl", "PSMain");
	}
	else pGDIOverlay = NULL;


	// Create an auxiliary screen space normal and depth buffer (i.e. Shader readable depth buffer)
	//
	if (Config->bGlares || Config->bLocalGlares) {
		HR(pDevice->CreateDepthStencilSurface(viewW, viewH, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, true, &pDepthNormalDS, NULL));
		HR(D3DXCreateTexture(pDevice, viewW, viewH, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &ptgBuffer[GBUF_DEPTH]));
	}
	// ORO patch (g): publish the depth texture for the Sketchpad's depth-clip path
	// (D3D9Pad::Flush reads this global; it is NULL when the buffer was not created, so the
	// clip silently no-ops and depth-tagged polys draw as plain additive geometry).
	g_gcSceneDepth = ptgBuffer[GBUF_DEPTH];
	// ORO patch (ab): TERRAIN JOINS GBUF_DEPTH. Until now the buffer held vessels, the
	// cockpit and (z2) base structures; terrain never wrote it, so every consumer that
	// asks it a visibility question was blind to hills - the sun and local glares
	// painted through mountains, patch (g)'s clip drew addon geometry through terrain,
	// and the stencil ground shadows could not be depth-tested at all. The (z3) tile
	// registry already re-draws the camera's rendered tiles through a two-instruction
	// shader for the local-light map; this points the same list at GBUF_DEPTH with the
	// camera's matrices (TileDepthVS/PS, NewPlanet.hlsl). One frame stale, like the map.
	if (ptgBuffer[GBUF_DEPTH])
		pTileDepth = new ShaderClass(pDevice, "Modules/D3D9Client/NewPlanet.hlsl", "TileDepthVS", "TileDepthPS", "OroTileDepth", NULL);


	// Initialize post processing effects --------------------------------------------------------------------------------------------------
	//
	pLightBlur = NULL;

	if (Config->PostProcess) {

		int BufSize = 1;
		int BufFmt = 0;

		// Get the actual back buffer description
		D3DSURFACE_DESC desc;
		gc->GetBackBuffer()->GetDesc(&desc);

		char flags[32] = { 0 };
		if (Config->ShaderDebug) strcpy_s(flags, 32, "DISASM");

		// Load postprocessing effects
		if (Config->PostProcess == PP_DEFAULT)
			pLightBlur = new ImageProcessing(pDevice, "Modules/D3D9Client/LightBlur.hlsl", "PSMain", flags);

		if (pLightBlur) {
			BufSize = pLightBlur->FindDefine("BufferDivider");
			BufFmt = pLightBlur->FindDefine("BufferFormat");
		}

		D3DFORMAT BackBuffer = desc.Format;
		if (BufFmt == 1) BackBuffer = D3DFMT_A16B16G16R16F;
		if (BufFmt == 2) BackBuffer = D3DFMT_A2R10G10B10;

		// Create auxiliary color buffer for color operations
		HR(D3DXCreateTexture(pDevice, viewW, viewH, 1, D3DUSAGE_RENDERTARGET, BackBuffer, D3DPOOL_DEFAULT, &ptgBuffer[GBUF_COLOR]));

		// Load some textures
		char buff[MAX_PATH];
		if (gc->TexturePath("D3D9Noise.dds", buff)) HR(D3DXCreateTextureFromFileA(pDevice, buff, &pTextures[TEX_NOISE]));
		if (gc->TexturePath("D3D9CLUT.dds", buff)) HR(D3DXCreateTextureFromFileA(pDevice, buff, &pTextures[TEX_CLUT]));

		if (pLightBlur) {
			HR(D3DXCreateTexture(pDevice, viewW / BufSize, viewH / BufSize, 1, D3DUSAGE_RENDERTARGET, BackBuffer, D3DPOOL_DEFAULT, &ptgBuffer[GBUF_BLUR]));
			HR(D3DXCreateTexture(pDevice, viewW / BufSize, viewH / BufSize, 1, D3DUSAGE_RENDERTARGET, BackBuffer, D3DPOOL_DEFAULT, &ptgBuffer[GBUF_TEMP]));
		}

		if (pLightBlur) {
			// Construct an offscreen backbuffer with custom pixel format
			if (pDevice->CreateRenderTarget(viewW, viewH, BackBuffer, desc.MultiSampleType, desc.MultiSampleQuality, false, &pOffscreenTarget, NULL) != S_OK) {
				LogErr("Creation of Offscreen render target failed");
				SAFE_DELETE(pLightBlur);
			}
		}
	}

	for (int i = 0; i < ARRAYSIZE(ptgBuffer);i++)  if (ptgBuffer[i]) ptgBuffer[i]->GetSurfaceLevel(0, &psgBuffer[i]);


	if (Config->GDIOverlay) {
		HDC hDC;
		// Clear the GDI Overlay with transparency
		if (psgBuffer[GBUF_GDI]->GetDC(&hDC) == S_OK) {
			DWORD color = 0xF08040; // BGR "Color Key" value for transparency
			HBRUSH hBrush = CreateSolidBrush((COLORREF)color);
			RECT r = _RECT( 0, 0, viewW, viewH );
			FillRect(hDC, &r, hBrush);
			DeleteObject(hBrush);
			psgBuffer[GBUF_GDI]->ReleaseDC(hDC);
		}
	}

	LogAlw("================ Scene Created ===============");
}

// ===========================================================================================
//
Scene::~Scene ()
{
	_TRACE;

	pDevice->SetRenderTarget(0, NULL);
	pDevice->SetRenderTarget(1, NULL);
	pDevice->SetRenderTarget(2, NULL);
	pDevice->SetRenderTarget(3, NULL);

	g_gcSceneDepth = NULL;   // ORO patch (g): before the depth texture is released below
	for (int i = 0; i < ARRAYSIZE(psgBuffer); i++) SAFE_RELEASE(psgBuffer[i]);
	for (int i = 0; i < ARRAYSIZE(ptgBuffer); i++) SAFE_RELEASE(ptgBuffer[i]);
	for (int i = 0; i < ARRAYSIZE(pTextures); i++) SAFE_RELEASE(pTextures[i]);

	SAFE_DELETE(pGDIOverlay);
	SAFE_DELETE(pBlur);
	SAFE_DELETE(pVisDepth);
	SAFE_DELETE(pLightBlur);
	SAFE_DELETE(pIrradiance);
	SAFE_DELETE(m_celSphere);
	SAFE_DELETE(pLocalCompute);
	SAFE_DELETE(pRenderGlares);
	SAFE_DELETE(pCreateGlare);

	SAFE_RELEASE(pOffscreenTarget);
	SAFE_RELEASE(pEnvDS);
	SAFE_RELEASE(pIrradDS);
	SAFE_RELEASE(pIrradTemp);
	SAFE_RELEASE(pIrradTemp2);
	SAFE_RELEASE(pIrradTemp3);
	SAFE_RELEASE(pDepthNormalDS);
	SAFE_RELEASE(psWetRefl);
	SAFE_RELEASE(ptWetRefl);
	SAFE_RELEASE(psWetReflDS);
	for (int i = 0; i < 2; i++) { SAFE_RELEASE(psRflPln[i]); SAFE_RELEASE(ptRflPln[i]); }	// ORO patch (v) part 2
	SAFE_RELEASE(pLocalResults);
	SAFE_RELEASE(pLocalResultsSL);
	SAFE_RELEASE(pSunTex);
	SAFE_RELEASE(pLightGlare);
	SAFE_RELEASE(pSunGlare);
	SAFE_RELEASE(pSunGlareAtm);

	for (int i = 0; i < ARRAYSIZE(psShmDS); i++) SAFE_RELEASE(psShmDS[i]);
	SAFE_RELEASE(psPShn);	// ORO patch (w)
	SAFE_RELEASE(ptPShn);
	SAFE_RELEASE(psSunCpy);
	SAFE_RELEASE(ptSunCpy);
	sunCpyLive = false;
	for (int i = 0; i < ARRAYSIZE(ptShmRT); i++) SAFE_RELEASE(ptShmRT[i]);
	for (int i = 0; i < ARRAYSIZE(psShmRT); i++) SAFE_RELEASE(psShmRT[i]);
	for (int i = 0; i < ARRAYSIZE(pBlrTemp); i++) SAFE_RELEASE(pBlrTemp[i]);
	// ORO patch (z3): the local-light shadow map's target
	SAFE_RELEASE(psLclShm);
	SAFE_RELEASE(psLclShmDS);
	SAFE_RELEASE(ptLclShm);
	// ORO patch (ae): the cascade atlas
	SAFE_RELEASE(psCasc);
	SAFE_RELEASE(psCascDS);
	SAFE_RELEASE(ptCasc);
	// ORO patch (z3) round 2c: the tile-caster shader and any still-held tile buffers
	SAFE_DELETE(pTileShd);
	SAFE_DELETE(pTileDepth);	// ORO patch (ab)
	for (auto& t : LclTiles) { t.pVB->Release(); t.pIB->Release(); }
	LclTiles.clear();

	if (Lights) {
		delete []Lights;
		Lights = NULL;
	}

	// Particle Streams
	if (nstream) {
		for (DWORD j=0;j<nstream;j++) delete pstream[j];
		delete []pstream;
		pstream = NULL;
	}

	DeleteAllCustomCameras();
	DeleteAllVisuals();
	ExitGDIResources();

	FreePooledSketchpads();
}


// ===========================================================================================
//
void Scene::CreateSunGlare()
{
	// ------------------------------------------------------------------------------
	// Create sun texture and glares

	if (pCreateGlare) SAFE_DELETE(pCreateGlare);

	pCreateGlare = new ImageProcessing(pDevice, "Modules/D3D9Client/Glare.hlsl", "CreateSunGlarePS");
	pCreateGlare->CompileShader("CreateLocalGlarePS");
	pCreateGlare->CompileShader("CreateSunGlareAtmPS");
	pCreateGlare->CompileShader("CreateSunTexPS");
	

	if (!pSunTex) {
		UINT ts = (viewH >> 4) & 0xFFFC; // "ts" will be 64 for a Full HD display;  
		HR(D3DXCreateTexture(pDevice, ts * 5, ts * 5, 0, D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pSunTex));
		HR(D3DXCreateTexture(pDevice, ts * 4, ts * 4, 0, D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP, D3DFMT_R16F, D3DPOOL_DEFAULT, &pLightGlare));
		HR(D3DXCreateTexture(pDevice, ts * 12, ts * 12, 0, D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP, D3DFMT_R16F, D3DPOOL_DEFAULT, &pSunGlare));
		HR(D3DXCreateTexture(pDevice, ts * 12, ts * 12, 0, D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP, D3DFMT_R16F, D3DPOOL_DEFAULT, &pSunGlareAtm));
	}

	LPDIRECT3DSURFACE9 pTgt = NULL;

	pCreateGlare->Activate("CreateSunGlarePS");
	pSunGlare->GetSurfaceLevel(0, &pTgt);
	pCreateGlare->SetOutputNative(0, pTgt);
	if (!pCreateGlare->Execute(false)) LogErr("pCreateGlare Execute Failed (CreateSunGlarePS)");
	SAFE_RELEASE(pTgt);

	pCreateGlare->Activate("CreateSunGlareAtmPS");
	pSunGlareAtm->GetSurfaceLevel(0, &pTgt);
	pCreateGlare->SetOutputNative(0, pTgt);
	if (!pCreateGlare->Execute(false)) LogErr("pCreateGlare Execute Failed (CreateSunGlareAtmPS)");
	SAFE_RELEASE(pTgt);

	pCreateGlare->Activate("CreateLocalGlarePS");
	pLightGlare->GetSurfaceLevel(0, &pTgt);
	pCreateGlare->SetOutputNative(0, pTgt);
	if (!pCreateGlare->Execute(false)) LogErr("pCreateGlare Execute Failed (CreateLocalGlarePS)");
	SAFE_RELEASE(pTgt);

	pCreateGlare->Activate("CreateSunTexPS");
	pSunTex->GetSurfaceLevel(0, &pTgt);
	pCreateGlare->SetOutputNative(0, pTgt);
	if (!pCreateGlare->Execute(false)) LogErr("pCreateGlare Execute Failed (CreateSunTexPS)");
	SAFE_RELEASE(pTgt);
}


// ===========================================================================================
//
void Scene::Initialise()
{
	_TRACE;

	hSun = oapiGetGbodyByIndex(0); // generalise later

	DWORD ambient = *(DWORD*)gc->GetConfigParam(CFGPRM_AMBIENTLEVEL);

	// Setup sunlight -------------------------------
	//
	sunLight.Color = 1.0f;
	sunLight.Ambient = float(ambient)*0.0039f;
	sunLight.Transmission = 1.0f;
	sunLight.Incatter = 0.0f;

	// Update Sunlight direction -------------------------------------
	//
	VECTOR3 rpos, cpos;
	oapiGetGlobalPos(hSun, &rpos);
	oapiCameraGlobalPos(&cpos); rpos-=cpos;
	sunLight.Dir = -unit(rpos);

	// Do not "pre-create" visuals here. Will cause changed call order for vessel callbacks
}


// ===========================================================================================
// Pooled Sketchpad API
// ===========================================================================================

static D3D9Pad *_pad = NULL;

#define SKETCHPAD_LABELS        0  ///< Sketchpad for planetarium mode labels and markers
#define SKETCHPAD_2D_OVERLAY    1  ///< Sketchpad for HUD Overlay render to backbuffer directly
#define SKETCHPAD_DEBUG_TEXT    2  ///< Sketchpad to draw Debug String on a bottom of the screen
#define SKETCHPAD_PLANETARIUM   3  ///< Sketchpad to draw user defined planetarium

// ===========================================================================================

void Scene::OnOptionChanged(int cat, int item)
{
	if (cat == OPTCAT_CELSPHERE)
		m_celSphere->OnOptionChanged(cat, item);
}

// ===========================================================================================
// Get pooled Sketchpad instance
//
D3D9Pad *Scene::GetPooledSketchpad (int id) // one of SKETCHPAD_xxx
{
	assert(id <= SKETCHPAD_PLANETARIUM);

	if (!_pad) _pad = new D3D9Pad("POOLED_SKETCHPAD");

	// Automatically binds a Sketchpad to a top render target
	_pad->BeginDrawing();
	_pad->LoadDefaults();

	switch (id)
	{
	case SKETCHPAD_LABELS:
		_pad->SetFont(pLabelFont);
		_pad->SetTextAlign(Sketchpad::CENTER, Sketchpad::BOTTOM);
		break;

	case SKETCHPAD_2D_OVERLAY:
		break;

	case SKETCHPAD_DEBUG_TEXT:
		_pad->SetFont(pDebugFont);
		_pad->SetTextColor(0xFFFFFF);
		_pad->SetTextAlign(Sketchpad::LEFT, Sketchpad::BOTTOM);
		_pad->QuickPen(0xFF000000);
		_pad->QuickBrush(0xB0000000);
		break;

	case SKETCHPAD_PLANETARIUM:
		break;
	}

	return _pad;
}

// ===========================================================================================
// Release pooled Sketchpad instances
void Scene::FreePooledSketchpads()
{
	SAFE_DELETE(_pad);
}

// ===========================================================================================
//
double Scene::GetObjectAppRad(OBJHANDLE hObj) const
{
	VECTOR3 pos,cam;
	oapiGetGlobalPos (hObj, &pos);
	oapiCameraGlobalPos(&cam); // must use oapiCam.. here. called before camera setup
	double rad = oapiGetSize (hObj);
	double dst = dist (pos, cam);
	return (rad*double(viewH))/(dst*tan(oapiCameraAperture()));
}

// ===========================================================================================
//
double Scene::GetObjectAppRad2(OBJHANDLE hObj) const
{
	VECTOR3 pos;
	oapiGetGlobalPos (hObj, &pos);
	VECTOR3 cam = GetCameraGPos();
	double rad = oapiGetSize (hObj);
	double dst = dist (pos, cam);
	return (rad*double(viewH))/(dst*tan(oapiCameraAperture()));
}

// ===========================================================================================
//
void Scene::CheckVisual(OBJHANDLE hObj)
{
	_TRACE;

	if (hObj==NULL) return;

	VOBJREC *pv = FindVisual(hObj);
	if (!pv) pv = AddVisualRec(hObj);

	pv->apprad = float(GetObjectAppRad(hObj));

	if (pv->type == OBJTP_STAR) {
		pv->vobj->Activate(true);
		return;
	}

	if (pv->vobj->IsActive()) {
		if (pv->apprad < 1.0) pv->vobj->Activate(false);
	} else {
		if (pv->apprad > 2.0) pv->vobj->Activate(true);
	}
	// the range check has a small hysteresis to avoid continuous
	// creation/deletion for objects at the edge of visibility
}

// ===========================================================================================
//
const D3D9Light *Scene::GetLight(int index) const
{
	if ((DWORD)index<MAX_SCENE_LIGHTS || index>=0) return &Lights[index];
	return NULL;
}

// ===========================================================================================
//
Scene::VOBJREC *Scene::FindVisual(OBJHANDLE hObj) const
{
	if (hObj==NULL) return NULL;
	VOBJREC *pv;
	for (pv=vobjFirst; pv; pv=pv->next) if (pv->vobj->Object()==hObj) return pv;
	return NULL;
}

// ===========================================================================================
//
class vObject *Scene::GetVisObject(OBJHANDLE hObj) const
{
	Scene::VOBJREC *v = FindVisual(hObj);
	if (v) return v->vobj;
	return NULL;
}

// ===========================================================================================
//
std::set<vVessel *> Scene::GetVessels(double max_dst, bool bAct)
{
	std::set<vVessel *> List;
	VOBJREC *pv;
	for (pv = vobjFirst; pv; pv = pv->next) {
		if (pv->type != OBJTP_VESSEL) continue;
		if (bAct && pv->vobj->IsActive() == false) continue;
		if (pv->vobj->CamDist() < max_dst) List.insert((vVessel *)pv->vobj);
	}
	return List;
}

// ===========================================================================================
//
void Scene::DelVisualRec (VOBJREC *pv)
{
	_TRACE;
	// unlink the entry
	if (pv->prev) pv->prev->next = pv->next;
	else          vobjFirst = pv->next;

	if (pv->next) pv->next->prev = pv->prev;
	else          vobjLast = pv->prev;

	DebugControls::RemoveVisual(pv->vobj);

	vobjEnv = NULL;
	vobjIrd = NULL;

	// delete the visual, its children and the entry itself
	gc->UnregisterVisObject(pv->vobj->GetObject());

	delete pv->vobj;
	delete pv;
}

// ===========================================================================================
//
void Scene::DeleteAllVisuals()
{
	_TRACE;
	VOBJREC *pv = vobjFirst;
	while (pv) {
		VOBJREC *pvn = pv->next;

		DebugControls::RemoveVisual(pv->vobj);

		gc->UnregisterVisObject(pv->vobj->GetObject());
		
		LogAlw("Deleting Visual %s", _PTR(pv->vobj));
		delete pv->vobj;
		delete pv;
		pv = pvn;
	}
	vobjFirst = vobjLast = NULL;
	vobjEnv = NULL;
	vobjIrd = NULL;
}

// ===========================================================================================
//
Scene::VOBJREC *Scene::AddVisualRec(OBJHANDLE hObj)
{
	_TRACE;

	char buf[256];

	// create the visual and entry
	VOBJREC *pv = new VOBJREC;

	memset(pv, 0, sizeof(VOBJREC));

	pv->vobj = vObject::Create(hObj, this);
	pv->type = oapiGetObjectType(hObj);

	oapiGetObjectName(hObj, buf, 255);

	VESSEL *hVes=NULL;
	if (pv->type==OBJTP_VESSEL) hVes = oapiGetVesselInterface(hObj);

	// link entry to end of list
	pv->prev = vobjLast;
	pv->next = NULL;
	if (vobjLast) vobjLast->next = pv;
	else          vobjFirst = pv;
	vobjLast = pv;

	LogAlw("RegisteringVisual (%s) hVessel=%s, hObj=%s, Vis=%s, Rec=%s, Type=%d", buf, _PTR(hVes), _PTR(hObj), _PTR(pv->vobj), _PTR(pv), pv->type);

	gc->RegisterVisObject(hObj, (VISHANDLE)pv->vobj);
	
	// Initialize Meshes
	pv->vobj->PreInitObject();

	return pv;
}

// ===========================================================================================
//
DWORD Scene::GetActiveParticleEffectCount()
{
	// render exhaust particle system
	DWORD count = 0;
	for (DWORD n = 0; n < nstream; n++) if (pstream[n]->IsActive()) count++;
	return count;
}

// ===========================================================================================
//
VECTOR3 Scene::SkyColour ()
{
	VECTOR3 col = {0,0,0};
	OBJHANDLE hProxy = oapiCameraProxyGbody();
	if (hProxy && oapiPlanetHasAtmosphere (hProxy)) {
		const ATMCONST *atmp = oapiGetPlanetAtmConstants (hProxy);
		VECTOR3 rc, rp, pc;
		rc = GetCameraGPos();
		oapiGetGlobalPos (hProxy, &rp);
		pc = rc-rp;
		double cdist = length (pc);
		if (cdist < atmp->radlimit) {
			ATMPARAM prm;
			oapiGetPlanetAtmParams (hProxy, cdist, &prm);
			normalise (rp);
			double coss = dotp (pc, rp) / -cdist;
			double intens = min (1.0,(1.0839*coss+0.4581)) * sqrt (prm.rho/atmp->rho0);
			// => intensity=0 at sun zenith distance 115?
			//    intensity=1 at sun zenith distance 60?
			if (intens > 0.0)
				col += _V(atmp->color0.x*intens, atmp->color0.y*intens, atmp->color0.z*intens);
		}
		for (int i=0;i<3;i++) if (col.data[i] > 1.0) col.data[i] = 1.0;
	}
	return col;
}

// ===========================================================================================
//
void Scene::Update ()
{
	_TRACE;

	// update particle streams - should be skipped when paused
	if (!oapiGetPause()) {
		for (DWORD i=0;i<nstream;) {
			if (pstream[i]->Expired()) DelParticleStream(i);
			else pstream[i++]->Update();
		}
	}

	static bool bFirstUpdate = true;

	// check object visibility (one object per frame in the interest
	// of scalability)
	DWORD nobj = oapiGetObjectCount();

	if (bFirstUpdate) {
		bFirstUpdate = false;
		for (DWORD i=0;i<nobj;i++) {
			OBJHANDLE hObj = oapiGetObjectByIndex(i);
			CheckVisual(hObj);
		}
	}
	else {

		if (iVCheck >= nobj) iVCheck = 0;

		// This function will browse through vessels and planets. (not bases)
		// Base visuals don't exist in the visual record.
		OBJHANDLE hObj = oapiGetObjectByIndex(iVCheck++);
		CheckVisual(hObj);
	}


	// If Camera target has changed, setup mesh debugger
	//
	OBJHANDLE hTgt = oapiCameraTarget();

	if (hTgt!=Camera.hTarget && hTgt!=NULL) {

		Camera.hTarget = hTgt;

		if (DebugControls::IsActive()) {
			if (oapiGetObjectType(hTgt) == OBJTP_SURFBASE) {
				OBJHANDLE hPlanet = oapiGetBasePlanet(hTgt);
				vPlanet *vp = static_cast<vPlanet *>(GetVisObject(hPlanet));
				if (vp) {
					vBase *vb = vp->GetBaseByHandle(hTgt);
					if (vb) {
						DebugControls::SetVisual(vb);
					}
				}
				return; // why?
			}
		}

		vObject *vo = GetVisObject(hTgt);

		if (vo) {

			if (DebugControls::IsActive()) {
				DebugControls::SetVisual(vo);
			}

			// Why is this here ?
			//
			// kuddel: OrbiterSound 4.0 did not play the sounds of the 'focused'
			//         Vessel when focus changed during playback. Therfore the
			//         D3D9Client does a oapiSetFocusObject call when playback
			//         is running. Is a OrbiterSound error, but we can work-around
			//         this, so we do! To reproduce, just disable the following
			//         code and run the 'Welcome.scn'.
			//         See also: http://www.orbiter-forum.com/showthread.php?p=392689&postcount=18
			//         and following...

			// OrbiterSound 4.0 'playback helper'
			if (OapiExtension::RunsOrbiter2010() &&
			    OapiExtension::RunsOrbiterSound40() &&
				oapiIsVessel(hTgt) && // oapiGetObjectType(vo->Object()) == OBJTP_VESSEL &&
				dynamic_cast<vVessel*>(vo)->Playback()
				)
			{
				// Orbiter doesn't do this when (only) camera focus changes
				// during playback, therfore we do it ;)
				oapiSetFocusObject(hTgt);
			}
		}
	}
}

// ===========================================================================================
//
double Scene::GetTargetElevation() const
{
	VESSEL *hVes = oapiGetVesselInterface(Camera.hTarget);
	if (hVes) return hVes->GetSurfaceElevation();
	return 0.0;
}


// ===========================================================================================
//
double Scene::GetFocusGroundAltitude() const
{
	VESSEL *hVes = oapiGetFocusInterface();
	if (hVes) return hVes->GetAltitude() - hVes->GetSurfaceElevation();
	return 0.0;
}



// ===========================================================================================
//
double Scene::GetTargetGroundAltitude() const
{
	VESSEL *hVes = oapiGetVesselInterface(Camera.hTarget);
	if (hVes) return hVes->GetAltitude() - hVes->GetSurfaceElevation();
	return 0.0;
}



// ============================================================================================
// Up, North, Forward in Ecliptic frame
//
void Scene::GetLVLH(vVessel *vV, D3DXVECTOR3 *up, D3DXVECTOR3 *nr, D3DXVECTOR3 *fw)
{
	if (!vV || !up || !nr || !fw) return;

	MATRIX3 grot; VECTOR3 rpos;
	VESSEL *hV = vV->GetInterface(); assert(hV);
	OBJHANDLE hRef = hV->GetGravityRef();
	oapiGetRotationMatrix(hRef, &grot);
	hV->GetRelativePos(hRef, rpos);
	VECTOR3 axis = mul(grot, _V(0, 1, 0));
	normalise(rpos);
	*up = D3DXVEC(rpos);
	*fw = D3DXVEC(unit(crossp(axis, rpos)));
	D3DXVec3Cross(nr, up, fw);
	D3DXVec3Normalize(nr, nr);
}



// ===========================================================================================
// Compute a distance to a near/far plane
// ===========================================================================================

float Scene::ComputeNearClipPlane()
{
	float zsurf = 1000.0f;
	VOBJREC *pv = NULL;

	OBJHANDLE hObj = Camera.hObj_proxy;
	OBJHANDLE hTgt = Camera.hTarget;
	VESSEL *hVes = oapiGetVesselInterface(hTgt);

	if (hObj && hVes) {
		VECTOR3 pos;
		oapiGetGlobalPos(hObj,&pos);
		double g = atan(Camera.apsq);
		double t = dotp(unit(Camera.pos-pos), unit(Camera.dir));
		if (t<-1.0) t=1.0; if (t>1.0) t=1.0f;
		double a = PI - acos(t);
		double R = oapiGetSize(hObj) + hVes->GetSurfaceElevation();
		double r = length(Camera.pos-pos);
		double h = r - R;
		if (h<10e3) {
			double d = a - g; if (d<0) d=0;
			zsurf = float(h*cos(g)/cos(d));
			if (zsurf>1000.0f || zsurf<0.0f) zsurf=1000.0f;
		}
	}

	float zmin = 1.0f;
	if (GetCameraAltitude()>10e3) zmin = 0.1f;

	int count = 0;
	int actbase = 0;
	vPlanet *pl = NULL;

	float farpoint = 0.0f;
	float nearpoint = 10e3f;
	float neardist = 10e3f;

	for (pv = vobjFirst; pv; pv = pv->next) {

		float nr = 10e3f;
		float fr = 0.0f;
		float dn = 10e3f;

		bool bCockpit = false;

		if (pv->type==OBJTP_VESSEL) {
			if (pv->vobj==vFocus) {
				bCockpit = oapiCameraInternal();
			}
		}

		if (pv->apprad>0.01 && pv->vobj->IsActive()) {

			vObject *obj = pv->vobj;

			if (pv->type==OBJTP_PLANET) {
				if (obj->Object()==hObj) pl = (vPlanet*)obj;
				obj->GetMinMaxDistance(&nr, &fr, &dn);
				if (dn<neardist) neardist = dn;
				if (nr<nearpoint) nearpoint = nr;
				if (fr>farpoint) farpoint = fr;
				continue;
			}

			if (pv->type==OBJTP_VESSEL) {

				if (obj->IsVisible()) {

					if (bCockpit) if (vFocus->HasExtPass()==false) continue; // Ignore MinMax

					obj->GetMinMaxDistance(&nr, &fr, &dn);

					if (dn<neardist) neardist = dn;
					if (nr<nearpoint) nearpoint = nr;
					if (fr>farpoint) farpoint = fr;
					count++;
				}
			}
		}
	}

	if (pl) {

		float nr = 10e3;
		float fr = 0.0f;
		float dn = 10e3;

		DWORD bc = pl->GetBaseCount();
		for (DWORD i=0;i<bc;i++) {
			vBase *vb = pl->GetBaseByIndex(i);
			if (vb) {
				if (vb->IsActive() && vb->IsVisible()) {
					vb->GetMinMaxDistance(&nr, &fr, &dn);
					if (dn<neardist) neardist = dn;
					if (nr<nearpoint) nearpoint = nr;
					if (fr>farpoint) farpoint = fr;
					actbase++;
				}
			}
		}
	}

	DWORD prteff = GetActiveParticleEffectCount();

	if (farpoint==0.0) farpoint = 20e4;

	float znear = D9NearPlane(pDevice, nearpoint, farpoint, neardist, GetProjectionMatrix(), (prteff!=0));

	if (oapiCameraInternal()) {
		if (Config->NearClipPlane==0) zmin = 1.0f;
		else						  zmin = 0.1f;
	}

	znear = min(znear, zsurf);
	znear = max(znear, zmin);

	return znear;
}





// ===========================================================================================
// Prepare scene for rendering
//
// - Update camera for rendering of the main scene
// - Update all visuals
// - Distance sort planets
// - Setup sky color
// - Setup local light sources
// ===========================================================================================

bool Scene::UpdateCamVis()
{

	// Update camera parameters --------------------------------------
	// and call vObject::Update() for all visuals
	//
	bool bRet = UpdateCameraFromOrbiter(RENDERPASS_MAINSCENE);

	if (Camera.hObj_proxy) D3D9Effect::UpdateEffectCamera(Camera.hObj_proxy);

	// Update Sunlight direction -------------------------------------
	//
	VECTOR3 rpos;
	oapiGetGlobalPos(hSun, &rpos);
	rpos -= Camera.pos;
	sunLight.Dir = -unit(rpos);

	// Get focus visual -----------------------------------------------
	//
	OBJHANDLE hFocus = oapiGetFocusObject();
	vFocus = NULL;
	for (VOBJREC *pv=vobjFirst; pv; pv=pv->next) {
		if (pv->type==OBJTP_VESSEL) if (pv->vobj->Object()==hFocus) {
			vFocus = (vVessel *)pv->vobj;
			break;
		}
	}

	// Compute SkyColor -----------------------------------------------
	//
	sky_color = SkyColour();
	bglvl = (sky_color.x + sky_color.y + sky_color.z) / 3.0;
	bg_rgba = D3DCOLOR_RGBA ((int)(sky_color.x*255), (int)(sky_color.y*255), (int)(sky_color.z*255), 255);


	// Process Local Light Sources -------------------------------------
	//
	if (bLocalLight) {

		ClearLocalLights();

		VOBJREC *pv = NULL;
		for (pv = vobjFirst; pv; pv = pv->next) {
			if (!pv->vobj->IsActive()) continue;
			OBJHANDLE hObj = pv->vobj->Object();
			if (oapiGetObjectType (hObj) == OBJTP_VESSEL) {
				VESSEL *vessel = oapiGetVesselInterface (hObj);
				DWORD nemitter = vessel->LightEmitterCount();
				for (DWORD j = 0; j < nemitter; j++) {
					const LightEmitter *em = vessel->GetLightEmitter(j);
					if ((em->GetVisibility() == LightEmitter::VIS_EXTERNAL) || (em->GetVisibility() == LightEmitter::VIS_ALWAYS))
						AddLocalLight(em, pv->vobj);
				}
			}
		}
	}


	// ----------------------------------------------------------------
	// render solar system celestial objects (planets and moons)
	// we render without z-buffer, so need to distance-sort the objects
	// ----------------------------------------------------------------

	VOBJREC *pv = NULL;
	nplanets = 0;

	for (pv = vobjFirst; pv && nplanets < MAXPLANET; pv = pv->next) {
		if (pv->apprad < 0.01 && pv->type != OBJTP_STAR) continue;
		if (pv->type == OBJTP_PLANET || pv->type == OBJTP_STAR) {
			plist[nplanets].vo = pv->vobj;
			plist[nplanets].dist = pv->vobj->CamDist();
			nplanets++;
		}
	}

	int distcomp(const void *arg1, const void *arg2);

	qsort((void*)plist, nplanets, sizeof(PList), distcomp);

	return bRet && (vFocus != nullptr);
}

// ===========================================================================================
//
void Scene::ClearLocalLights()
{
	nLights  = 0;
	lmaxdst2 = 0.0f;

	// Clear active local lisghts list -------------------------------
	for (int i = 0; i < MAX_SCENE_LIGHTS; i++) Lights[i].Reset();

	// ORO patch (z3): no lights, no owners
	memset(&LightOwners, 0, sizeof(LightOwners));
}

// ===========================================================================================
//
void Scene::AddLocalLight(const LightEmitter *le, const vObject *vo)
{
	if (Lights==NULL) return;
	if (le->IsActive()==false || le->GetIntensity()==0.0) return;

	assert(vo != NULL);

	D3D9Light lght(le, vo);

	// -----------------------------------------------------------------------------
	// Replace or Add
	//
	if (nLights == MAX_SCENE_LIGHTS) {
		if (lght.Dst2 > lmaxdst2) return;
		DWORD imax = 0;
		for (DWORD i = 0; i < MAX_SCENE_LIGHTS; i++) if (Lights[i].Dst2 > lmaxdst2) imax = i;
		Lights[imax] = lght;
		LightOwners[imax] = vo;		// ORO patch (z3)
		lmaxdst2 = lght.Dst2;
	}
	else {
		Lights[nLights] = lght;
		LightOwners[nLights] = vo;	// ORO patch (z3)
		if (lght.Dst2>lmaxdst2) lmaxdst2 = lght.Dst2;
		nLights++;
	}
}

// ===========================================================================================
//
void Scene::ComputeLocalLightsVisibility()
{

	if (!ptgBuffer[GBUF_DEPTH] || !pLocalCompute) {
		Config->bGlares = false;
		Config->bLocalGlares = false;
		return;
	}

	VECTOR3 gsun;
	oapiGetGlobalPos(oapiGetObjectByIndex(0), &gsun);

	// Put the Sun on a top of the list
	LLCBuf[0].index = 0.0f;
	LLCBuf[0].pos = FVECTOR3(unit(gsun - Camera.pos)) * 10e4;
	LLCBuf[0].cone = 1.0f;

	int nGlares = 1;

	for (int i = 0; i < nLights; i++)
	{
		if (Lights[i].cone > 0.0f) {
			LLCBuf[nGlares].index = float(nGlares);
			LLCBuf[nGlares].pos = Lights[i].Position;
			LLCBuf[nGlares].cone = Lights[i].cone;
			Lights[i].GPUId = nGlares;
			nGlares++;
		}
	}

	struct {
		D3DXMATRIX mVP;
		D3DXMATRIX mSVP;
		FVECTOR4 vSrc;
		FVECTOR3 vDir;
	} ComputeData;

	D3DSURFACE_DESC desc;
	pLocalResultsSL->GetDesc(&desc);

	D3DXMatrixOrthoOffCenterLH(&ComputeData.mVP, 0.0f, (float)desc.Width, (float)desc.Height, 0.0f, 0.0f, 1.0f);

	psgBuffer[GBUF_DEPTH]->GetDesc(&desc);

	ComputeData.vSrc = FVECTOR4((float)desc.Width, (float)desc.Height, 1.0f / (float)desc.Width, 1.0f / (float)desc.Height);
	ComputeData.vDir = Camera.z;
	ComputeData.mSVP = Camera.mProjView;

	// Must setup render target before calling Setup()
	gc->PushRenderTarget(pLocalResultsSL, NULL, RENDERPASS_UNKNOWN);

	pLocalCompute->ClearTextures();
	pLocalCompute->SetPSConstants("cbPS", &ComputeData, sizeof(ComputeData));
	pLocalCompute->SetPSConstants("cbKernel", DepthSampleKernel, sizeof(DepthSampleKernel));
	pLocalCompute->SetVSConstants("cbPS", &ComputeData, sizeof(ComputeData));
	pLocalCompute->SetTexture("tDepth", ptgBuffer[GBUF_DEPTH], IPF_CLAMP | IPF_POINT);
	pLocalCompute->Setup(pLocalLightsDecl, false, 0);
	pLocalCompute->UpdateTextures();

	// Compute local lights visibility
	HR(pDevice->DrawPrimitiveUP(D3DPT_POINTLIST, nGlares, &LLCBuf, sizeof(LocalLightsCompute)));

	pLocalCompute->DetachTextures();
	gc->PopRenderTargets();
}


// ===========================================================================================
//
void Scene::RecallDefaultState()
{
	HR(pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID));
	HR(pDevice->SetRenderState(D3DRS_STENCILENABLE, false));
	HR(pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF));
	HR(pDevice->SetRenderState(D3DRS_ZENABLE, true));
	HR(pDevice->SetRenderState(D3DRS_ZWRITEENABLE, true));
	HR(pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, false));
	HR(pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, false));
	HR(pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
	HR(pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
	HR(pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
	HR(pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW));
}


// ===========================================================================================
//
static void OroGatherAssembly(Scene* scn, vVessel* vRoot, std::list<vVessel*>& out);	// ORO patch (w), defined below

// ============================================================================
// ORO patch (aa): THE AIR - FOG and SNOW COVER, the client half.
// ----------------------------------------------------------------------------
// The addon hands the client two fog LAYERS (gcCore::SetFogLayer - ORO uses 0 for a
// ground fog anchored to the terrain under the vessel and 1 for the storm's mist up to
// its deck) plus two taste numbers (SetFogLook). Everything else is decided HERE, once
// per frame, from the client's own atmosphere model at the camera: the fog's colour from
// the extinction-reddened sun the client already computes for the glare sprite and the
// daylight at the camera, the forward-scatter lobe toward the sun, how much sunlight the
// fog scatters back as ambient, and the camera's own sun transmittance (the glare sprite,
// the stencil ground shadows, the CPU-drawn sprites).
// The constants go to the shared D3D9Effect - every vessel path, base tiles, particles,
// runway lights, one push that binds in the probe cubes and both mirror passes by
// construction because it lands before the first pass of the frame - and, BY NAME, to
// the three planet-family shader objects (terrain per tile, the cloud layer, the sky
// dome) through OroFogPushPS. Four push sites: the patch (s) lesson, that a value pushed
// to D3D9Effect alone reaches the base tiles and never the ground a vessel parks on.
// THE ONE COLOUR RULE: the colour is pushed in DISPLAY space and every shader lerps its
// FINAL output toward it (the terrain after its HDR() curve), so a hull, the apron it
// sits on and the sky behind them converge on the same grey. Anything else leaves a
// hull-shaped patch of a slightly different fog.
// Exactly inert at density 0: no addon, no change.
// ============================================================================
float g_oroFogSunCam = 1.0f;         // sun transmittance at the CAMERA through both layers
// ORO patch (ab): the stencil ground shadows' SOFT depth-test tolerance - metres, and
// metres per metre of distance. A bump under the flat sheet is metres; a hill between
// the eye and the sheet is tens to hundreds. See ShadowTechPS in Mesh.fx.
float g_oroShadowTolBase = 1.0f;    // overwritten from Config->ShadowDepthTol every frame
float g_oroShadowTolK    = 0.001f;  // ...and ShadowDepthTolK
float g_oroFogPrm[6][4];             // gFogPrm: cam, L0, L1, M0, M1, misc (K in .x)
float g_oroFogClr[3][4];             // gFogClr: colour, sun lobe, ambient lift (in .w)
float g_oroSnowPrm[4];               // gSnow: cover, line altitude, 1/line width, 0
static bool s_oroFogLive = false;    // any layer carrying density this frame

static inline float OroClamp01(double x) { return x < 0.0 ? 0.0f : (x > 1.0 ? 1.0f : (float)x); }

static void OroFogFrame(Scene* scn)
{
	extern double g_gcFogBase[2];
	extern float  g_gcFogTop[2], g_gcFogScale[2], g_gcFogDens[2];
	extern float  g_gcFogBright, g_gcFogSunGlow;
	extern float  g_gcSnowCover, g_gcSnowLine, g_gcSnowLineW;
	extern float  g_gcStormLight;

	memset(g_oroFogPrm, 0, sizeof(g_oroFogPrm));
	memset(g_oroFogClr, 0, sizeof(g_oroFogClr));
	g_oroFogPrm[5][0] = 1.0f;        // K: fog on (the cockpit bracket lowers it)
	g_oroFogPrm[5][1] = 1.0f;        // the camera's sun transmittance, for the FAST vessel path
	g_oroFogSunCam = 1.0f;
	s_oroFogLive = false;

	g_oroSnowPrm[0] = g_gcSnowCover;
	g_oroSnowPrm[1] = g_gcSnowLine;
	g_oroSnowPrm[2] = 1.0f / (g_gcSnowLineW < 1.0f ? 1.0f : g_gcSnowLineW);
	g_oroSnowPrm[3] = 0.0f;

	vPlanet* vp = scn->GetCameraProxyVisual();
	const bool any = (g_gcFogDens[0] > 0.0f && g_gcFogTop[0] > 0.0f)
	              || (g_gcFogDens[1] > 0.0f && g_gcFogTop[1] > 0.0f);
	if (vp && any) {
		const VECTOR3 crel = vp->CameraPos();          // camera relative to the proxy body
		const double  camR = length(crel);
		if (camR > 1.0) {
			const VECTOR3 up = crel / camR;
			VECTOR3 sunp; oapiGetGlobalPos(oapiGetGbodyByIndex(0), &sunp);
			const VECTOR3 toSun = unit(sunp - scn->GetCameraGPos());
			const double sinE = dotp(up, toSun);
			// Daylight at the camera: a twilight ramp over ~ -4.6..+4.6 deg of sun elevation.
			const float dayF = OroClamp01((sinE + 0.08) / 0.16);
			// The sun as the client sees it at the camera: extinction-reddened, 0 below the
			// horizon - the same call the glare sprite makes.
			const FVECTOR4 sc = vp->SunLightColor(crel, 2.0);
			const float st = g_gcStormLight;
			const float bright = g_gcFogBright, glow = g_gcFogSunGlow;

			g_oroFogPrm[0][0] = (float)up.x; g_oroFogPrm[0][1] = (float)up.y;
			g_oroFogPrm[0][2] = (float)up.z; g_oroFogPrm[0][3] = (float)camR;

			double tauCam = 0.0;
			for (int i = 0; i < 2; i++) {
				if (g_gcFogDens[i] <= 0.0f || g_gcFogTop[i] <= 0.0f) continue;
				const float hc   = (float)(camR - g_gcFogBase[i]);
				const float top  = g_gcFogTop[i];
				const float iH   = 1.0f / (g_gcFogScale[i] < 1.0f ? 1.0f : g_gcFogScale[i]);
				const float dn   = g_gcFogDens[i];
				const float eTop = expf(-top * iH);
				const float ksun = dn / (iH * (float)(sinE > 0.06 ? sinE : 0.06));
				float* L = g_oroFogPrm[1 + i]; L[0] = hc;   L[1] = top;  L[2] = iH;   L[3] = dn;
				float* M = g_oroFogPrm[3 + i]; M[0] = eTop; M[1] = ksun; M[2] = 0.0f; M[3] = 0.0f;
				// the camera's own column to the top - glare sprite, stencil shadows, sprites
				const float hcc = hc < 0.0f ? 0.0f : (hc > top ? top : hc);
				const float dcol = expf(-hcc * iH) - eTop;
				tauCam += ksun * (dcol > 0.0f ? dcol : 0.0f);
				s_oroFogLive = true;
			}
			g_oroFogSunCam = expf(-(float)tauCam);

			// THE COLOUR. Fog is a cloud at ground level: lit by the sky (a grey that follows
			// the daylight and drops under the storm deck), tinted by the sun seen through the
			// fog above (warm at dawn, gone at night), plus a forward-scatter lobe toward the
			// sun that the shaders shape per pixel, and a little of the Launchpad ambient so
			// night fog is dark grey rather than black.
			// THE BRIGHTNESS FOLLOWS THE SUN, NOT THE CLOCK (his dawn report: a fog invoked
			// at dawn BRIGHTENED the scene). The first build lit the fog by a geometric
			// twilight ramp, which is ~70% at a sun two degrees up while the ground under it
			// is lit by a sun that has lost most of its light to the long path. sunLum is the
			// sun's transmitted irradiance at the camera (the same extinction that lights
			// the terrain): 1 at noon, a few tenths at dawn, 0 below the horizon. The SKY
			// share rides the twilight ramp scaled by it, the SUN share is the sun's own
			// colour - so a dawn fog is dim and warm, a noon fog bright and neutral, and a
			// night fog the Launchpad ambient's grey. The Brightness slider scales the lot.
			const float amb    = scn->GetSun()->Ambient.x;
			const float sunLum = sc.r > sc.g ? (sc.r > sc.b ? sc.r : sc.b) : (sc.g > sc.b ? sc.g : sc.b);
			const float skyF   = dayF * (0.40f + 0.60f * sunLum);
			const float sky    = (0.50f * skyF * (1.0f - 0.32f * st) + 1.4f * amb) * bright;
			const float sunW   = (1.0f - st) * bright;
			g_oroFogClr[0][0] = sky * 0.940f + sc.r * sunW * 0.35f;
			g_oroFogClr[0][1] = sky * 0.955f + sc.g * sunW * 0.35f;
			g_oroFogClr[0][2] = sky * 0.985f + sc.b * sunW * 0.35f;
			g_oroFogClr[0][3] = 1.0f;
			g_oroFogClr[1][0] = sc.r * sunW * 0.55f * glow;
			g_oroFogClr[1][1] = sc.g * sunW * 0.55f * glow;
			g_oroFogClr[1][2] = sc.b * sunW * 0.55f * glow;
			g_oroFogClr[1][3] = 1.0f;
			// The scattered sun raising the ambient INSIDE a sunlit fog - a gain on each
			// shader family's own ambient term, so terrain and hulls lift by the same ratio.
			g_oroFogClr[2][3] = 1.6f * sunLum * (1.0f - st);
		}
	}
	if (!s_oroFogLive) g_oroFogSunCam = 1.0f;
	g_oroFogPrm[5][1] = g_oroFogSunCam;

	if (D3D9Effect::eFogPrm) D3D9Effect::FX->SetVectorArray(D3D9Effect::eFogPrm, (const D3DXVECTOR4*)g_oroFogPrm, 6);
	if (D3D9Effect::eFogClr) D3D9Effect::FX->SetVectorArray(D3D9Effect::eFogClr, (const D3DXVECTOR4*)g_oroFogClr, 3);
	if (D3D9Effect::eSnow)   D3D9Effect::FX->SetVector(D3D9Effect::eSnow, (const D3DXVECTOR4*)g_oroSnowPrm);
}

// The cockpit interior is clear air (patch (s)'s "the interior is dry", for the fog):
// K goes to 0 for the cockpit draw and back after. What is seen THROUGH the glass keeps
// its fog - it is drawn in the main pass with K at 1.
void OroFogInterior(bool bInterior)
{
	g_oroFogPrm[5][0] = bInterior ? 0.0f : 1.0f;
	// The sun that lights the cabin HAS crossed the fog above it: only the air between
	// the eye and the panel is clear. First flight had this at 1 inside, and the panel
	// shadows stayed hard in a fog that had erased the world outside (his report).
	g_oroFogPrm[5][1] = g_oroFogSunCam;
	if (D3D9Effect::eFogPrm) D3D9Effect::FX->SetVectorArray(D3D9Effect::eFogPrm, (const D3DXVECTOR4*)g_oroFogPrm, 6);
}

// The planet family (terrain per tile, the cloud layer, the sky dome) has its own
// constant tables: the same values, by name, one call per site. A shader variant that
// does not reference a constant skips it silently (SetPSConstants returns on a NULL
// handle), which is what lets CloudPS and HorizonPS ignore gSnow.
void OroFogPushPS(ShaderClass* pShader)
{
	pShader->SetPSConstants("gFogPrm", g_oroFogPrm, sizeof(g_oroFogPrm));
	pShader->SetPSConstants("gFogClr", g_oroFogClr, sizeof(g_oroFogClr));
	pShader->SetPSConstants("gSnow",   g_oroSnowPrm, sizeof(g_oroSnowPrm));
}

float OroFogTransmittance(const D3DXVECTOR3& posW);   // defined below

// CPU mirror of the shaders' SUN attenuation at a camera-relative position: the column
// of fog above it to each layer's top, over the sine of the sun's elevation (folded into
// M.y on the CPU, exactly as the shaders read it). Keep in step with OroFogSunTau.
float OroFogSunAttenuation(const D3DXVECTOR3& posW)
{
	if (!s_oroFogLive) return 1.0f;
	const float* c = g_oroFogPrm[0];
	const float px = c[0] * c[3] + posW.x, py = c[1] * c[3] + posW.y, pz = c[2] * c[3] + posW.z;
	const float dr = sqrtf(px * px + py * py + pz * pz) - c[3];
	float tau = 0.0f;
	for (int i = 0; i < 2; i++) {
		const float* L = g_oroFogPrm[1 + i];
		const float* M = g_oroFogPrm[3 + i];
		if (L[3] <= 0.0f) continue;
		float hp = L[0] + dr;
		hp = hp < 0.0f ? 0.0f : (hp > L[1] ? L[1] : hp);
		const float e = expf(-hp * L[2]) - M[0];
		tau += M[1] * (e > 0.0f ? e : 0.0f);
	}
	return expf(-tau * g_oroFogPrm[5][0]);
}

// The fade for a STENCIL ground shadow at posW (camera-relative): these are drawn after
// the scene as dark overlays with their own alpha, so neither the storm's sun collapse
// nor the fog reaches them on its own. A shadow is the ABSENCE of direct sun, so it
// weakens with the sun (the storm, the fog column above it) and is seen through the air
// between it and the eye (the transmittance) - the same three things the surface under
// it already got from its shader. One law, evaluated at the shadow's own position; the
// first version used the camera's column for every vessel and nothing for the bases,
// which is how a fog that erased the buildings left their shadows floating in it.
// ORO patch (ab) INSTRUMENT (2026-09-06, flight one of the shadow rewrite - his KSC
// blink). Counts what the stencil sheets and the terrain depth pass did each frame;
// ShadowDebug > 0 logs them once a second (Orbiter.log - client diagnostics that must
// land go through oapiWriteLogV) and colours the sheets in Mesh.fx. Temporary scaffold.
// ORO patch (ae): raised around the cascade pass - ShaderClass::Setup leaves the slot
// viewport alone while it is up (see D3D9Util.cpp).
bool  g_oroKeepViewport = false;
int   g_oroDbgTileDrawn = 0, g_oroDbgTileSkip = 0;
int   g_oroDbgVesDrawn = 0, g_oroDbgVesSkip = 0, g_oroDbgStrDrawn = 0, g_oroDbgStrSkip = 0;
float g_oroDbgDepth = -1.0f, g_oroDbgFade = -1.0f, g_oroDbgDv = -1.0f;

// ORO patch (ab) round 4: a registered terrain tile is stored in its PLANET's frame and
// rebuilt from the planet's current rotation + position at every draw. See LCLTILECASTER
// in Scene.h. D3DX matrices are row-vector (v * M), so rows _11.._33 are the images of
// the tile's basis vectors in the camera-relative global frame; Orbiter's MATRIX3 is
// local -> global under mul(). The oapi values are what vObject::Update copied for this
// frame (no sim step runs between Update and Render), so registration and draw agree.
void Scene::OroTileToPlanet(const D3DXMATRIX& mW, const VECTOR3& cam, LCLTILECASTER& t)
{
	MATRIX3 R; VECTOR3 P;
	oapiGetRotationMatrix(t.hPlanet, &R);
	oapiGetGlobalPos(t.hPlanet, &P);
	const VECTOR3 o = _V(mW._41, mW._42, mW._43) + cam - P;      // tile origin, global, relative to the planet centre
	t.lpos    = tmul(R, o);
	t.lrow[0] = tmul(R, _V(mW._11, mW._12, mW._13));
	t.lrow[1] = tmul(R, _V(mW._21, mW._22, mW._23));
	t.lrow[2] = tmul(R, _V(mW._31, mW._32, mW._33));
}
void Scene::OroTileFromPlanet(const LCLTILECASTER& t, const VECTOR3& cam, D3DXMATRIX& mW)
{
	MATRIX3 R; VECTOR3 P;
	oapiGetRotationMatrix(t.hPlanet, &R);
	oapiGetGlobalPos(t.hPlanet, &P);
	mW = t.mW;
	const VECTOR3 o  = mul(R, t.lpos) + P - cam;
	const VECTOR3 r0 = mul(R, t.lrow[0]), r1 = mul(R, t.lrow[1]), r2 = mul(R, t.lrow[2]);
	mW._11 = (float)r0.x; mW._12 = (float)r0.y; mW._13 = (float)r0.z;
	mW._21 = (float)r1.x; mW._22 = (float)r1.y; mW._23 = (float)r1.z;
	mW._31 = (float)r2.x; mW._32 = (float)r2.y; mW._33 = (float)r2.z;
	mW._41 = (float)o.x;  mW._42 = (float)o.y;  mW._43 = (float)o.z;
}

float OroGroundShadowFade(const D3DXVECTOR3& posW)
{
	extern float g_gcStormLight;
	return (1.0f - g_gcStormLight) * OroFogSunAttenuation(posW) * OroFogTransmittance(posW);
}

// CPU mirror of the shaders' transmittance, for what the client draws from the CPU with
// one alpha (beacons and nav lights, the stock exhaust billboards). posW is
// camera-relative, exactly what the shaders see. Keep in step with OroFogTau in
// D3D9Client.fx / NewPlanet.hlsl.
float OroFogTransmittance(const D3DXVECTOR3& posW)
{
	if (!s_oroFogLive) return 1.0f;
	const float d = sqrtf(posW.x * posW.x + posW.y * posW.y + posW.z * posW.z);
	const float* c = g_oroFogPrm[0];
	const float px = c[0] * c[3] + posW.x, py = c[1] * c[3] + posW.y, pz = c[2] * c[3] + posW.z;
	const float dr = sqrtf(px * px + py * py + pz * pz) - c[3];
	float tau = 0.0f;
	for (int i = 0; i < 2; i++) {
		const float* L = g_oroFogPrm[1 + i];
		const float dens = L[3] * g_oroFogPrm[5][0];
		if (dens <= 0.0f) continue;
		const float hc = L[0], top = L[1], iH = L[2];
		const float hp = hc + dr;
		const float dh = hp - hc;
		const float idh = (fabsf(dh) > 1e-3f) ? 1.0f / dh : 0.0f;
		float t0 = 0.0f, t1 = 1.0f;
		if (hc > top) { if (hp >= top) continue; t0 = (top - hc) * idh; }
		else if (hp > top) t1 = (top - hc) * idh;
		if (hc < 0.0f) { if (hp <= 0.0f) continue; float tb = -hc * idh; if (tb > t0) t0 = tb; }
		else if (hp < 0.0f) { float tb = -hc * idh; if (tb < t1) t1 = tb; }
		if (t1 <= t0) continue;
		const float h0 = hc + dh * t0, h1 = hc + dh * t1;
		const float e0 = expf(-h0 * iH), e1 = expf(-h1 * iH);
		const float k = (h1 - h0) * iH;
		const float mean = (fabsf(k) > 1e-3f) ? (e0 - e1) / k : 0.5f * (e0 + e1);
		tau += dens * d * (t1 - t0) * mean;
	}
	return expf(-tau);
}

void Scene::RenderMainScene()
{
	_TRACE;

	dwFrameId++; // Advance to a next frame

	double scene_time = D3D9GetTime();
	D3D9SetTime(D3D9Stats.Timer.CamVis, scene_time);

	if (!UpdateCamVis()) {
		if (SUCCEEDED(gc->BeginScene())) {
			// ORO patch (q) - STOCK BUG. This early-out runs before the scene has a
			// depth-stencil bound (it is the "focus vessel has no visual yet" path, i.e.
			// every scenario reload), so asking to clear ZBUFFER|STENCIL here returns
			// D3DERR_INVALIDCALL, HR() logs it, and the clear does not happen at all -
			// which is why the intended black loading screen is never painted. Clearing
			// the TARGET alone is what this line was actually for, it succeeds, and it
			// removes ~30 D3D9ERROR lines from every reload in every user's log.
			// Reproducible with no addon loaded.
			HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0L));
			gc->EndScene();
		}
		return; // Scene not yet properly inilialized, return
	}

	// ORO patch (s): ground wetness, pushed ONCE PER FRAME into the shared effect so every
	// shader in it can read gSurfWet - base tiles for the ground, and the vessel shaders
	// for a wet hull. Pushing it only in RenderBaseTile (where it started) left the value
	// stale or unset for everything else in the frame. 0 is stock, so this line changes
	// nothing until an addon asks for it.
	{
		extern float g_gcSurfaceWet;
		extern float g_gcStormLight;
		extern float g_gcWetDark;
		if (D3D9Effect::eSurfWet) D3D9Effect::FX->SetFloat(D3D9Effect::eSurfWet, g_gcSurfaceWet);
		if (D3D9Effect::eStorm)   D3D9Effect::FX->SetFloat(D3D9Effect::eStorm,   g_gcStormLight);
		if (D3D9Effect::eWetDark) D3D9Effect::FX->SetFloat(D3D9Effect::eWetDark, g_gcWetDark);
		// REAL time for the drop-sparkle animation (a blink stays a blink at 100x warp) -
		// but ACCUMULATED only while the sim runs, because raw system time kept the hull
		// glint dancing through a pause while the rain streaks (ORO's clock, which stops
		// with clbkPreStep) hung frozen mid-air: two clocks, one scene, visibly absurd.
		// His report. Wrapped hourly so float precision never decays.
		{
			static double wetT = 0.0, wetLast = -1.0;
			const double now = oapiGetSysTime();
			if (wetLast >= 0.0 && !oapiGetPause()) wetT += now - wetLast;
			wetLast = now;
			if (D3D9Effect::eWetTime) D3D9Effect::FX->SetFloat(D3D9Effect::eWetTime, (float)fmod(wetT, 3600.0));
		}
		{ extern float g_gcWetGlint; if (D3D9Effect::eWetGlint) D3D9Effect::FX->SetFloat(D3D9Effect::eWetGlint, g_gcWetGlint); }
		// ORO patch (aa): the fog + snow constants for the frame - see OroFogFrame above.
		// Lands here, before the first pass of the frame, so probes and mirrors see it.
		OroFogFrame(this);
		// ORO patch (ab): no soft shadow test until this frame's depth pass has run
		if (D3D9Effect::eSceneDepthPrm) D3D9Effect::FX->SetVector(D3D9Effect::eSceneDepthPrm, ptr(D3DXVECTOR4(0, 0, 0, 0)));
		if (D3D9Effect::eCascAtlas) D3D9Effect::FX->SetVector(D3D9Effect::eCascAtlas, ptr(D3DXVECTOR4(0, 0, 0, 0)));   // ORO patch (ae): no cascades until this frame's pass ran
		cascLive = false;
	}


	// Update Vessel Animations
	//
	for (VOBJREC *pv = vobjFirst; pv; pv = pv->next) {
		if (pv->type == OBJTP_VESSEL) {
			vVessel *vv = (vVessel *)pv->vobj;
			vv->UpdateAnimations();
		}
	}


	if (vFocus == NULL) return;

	LPDIRECT3DSURFACE9 pBackBuffer;

	if (pOffscreenTarget) pBackBuffer = pOffscreenTarget;
	else				  pBackBuffer = gc->GetBackBuffer();


	// Begin a Scene ------------------------------------------------------------------------------------
	//
	if (FAILED (gc->BeginScene())) return;



	// -------------------------------------------------------------------------------------------------------
	// Render Custom Camera and Environment Views
	// -------------------------------------------------------------------------------------------------------
	bool bIrrad = Config->EnvMapMode && Config->bIrradiance;

	if (Config->CustomCamMode == 0 && dwTurn == RENDERTURN_CUSTOMCAM) dwTurn++;
	if (Config->EnvMapMode == 0 && dwTurn == RENDERTURN_ENVCAM) dwTurn++;
	if (!bIrrad && dwTurn == RENDERTURN_IRRADIANCE) dwTurn++;

	if (dwTurn>RENDERTURN_LAST) dwTurn = 0;

	int RenderCount = max(1, Config->EnvMapFaces);


	// --------------------------------------------------------------------------------------------------------
	// Render Custom Camera view for a focus vessel
	// --------------------------------------------------------------------------------------------------------

	if (dwTurn == RENDERTURN_CUSTOMCAM)
	{
		if (Config->CustomCamMode && (CustomCams.size() > 0))
		{
			if (camCurrent == CustomCams.cend()) camCurrent = CustomCams.cbegin();

			OBJHANDLE hVessel = vFocus->GetObjectA();
			
			vObject *vO = GetVisObject((*camCurrent)->hVessel);
			double maxd = min(500e3, GetCameraAltitude() + 15e3);

			if (vO->CamDist() < maxd && (*camCurrent)->bActive)
			{
				RenderCustomCameraView((*camCurrent));

				if ((*camCurrent)->pRenderProc) {
					D3D9Pad *pSkp = (D3D9Pad * )gc->clbkGetSketchpad((*camCurrent)->hSurface);
					pSkp->LoadDefaults();
					(*camCurrent)->pRenderProc(pSkp, (*camCurrent)->pUser);
					gc->clbkReleaseSketchpad(pSkp);
				}
			}
			camCurrent++;
		}
	}


	// -------------------------------------------------------------------------------------------------------
	// Render Environmental Map For the Vessels
	// -------------------------------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------------------------------
	// ORO patch (w): PLANET-SHINE SHADOWS. Stock adds Earth glow to every planet-facing
	// surface with NO occlusion term of any kind, so a closed payload bay glows sky-blue
	// (reported on the stock client; the sun term gets ComputeShadow, the glow term gets
	// nothing). In "Full Scene ORO (exp)" only: render the focus vessel's attachment
	// assembly into a depth map along the PLANET direction - RenderShadowMap reused
	// wholesale - then copy the result out and RESTORE the sun's struct; the sun's own
	// pass (much later) repaints the borrowed LOD target. The glow sites in PBR.fx /
	// Metalness.fx attenuate by this map for assembly members. The pass sits HERE -
	// before the env-cube turn and both mirror passes - so every reflection of the
	// assembly consumes THIS frame's map: a mirror must not create light, and a
	// mirrored bay lit by unshadowed glow inside a dark real bay did exactly that.
	//
	pshnSet.clear();
	if (Config->EnvMapMode >= 3 && Config->ShadowMapMode >= 1 && psShmRT[0] && vFocus && vFocus->IsActive() && Camera.hObj_proxy)
	{
		std::list<vVessel*> assy;
		OroGatherAssembly(this, vFocus, assy);
		if (!assy.empty())
		{
			// merged bounding sphere of the assembly
			D3DXVECTOR3 c = assy.front()->GetBoundingSpherePosDX();
			float r = assy.front()->GetBoundingSphereRadius();
			for (auto* vv : assy) {
				D3DXVECTOR3 d = vv->GetBoundingSpherePosDX() - c;
				float dist = D3DXVec3Length(&d);
				float rr = vv->GetBoundingSphereRadius();
				if (dist + rr > r) {
					float nr = (r + dist + rr) * 0.5f;
					if (dist > 1e-4f) c += d * ((nr - r) / dist);
					r = nr;
				}
			}
			// light direction: the way planet shine travels, planet centre -> assembly
			VECTOR3 gp; oapiGetGlobalPos(Camera.hObj_proxy, &gp);
			VECTOR3 gcam = GetCameraGPos();
			D3DXVECTOR3 pp(float(gp.x - gcam.x), float(gp.y - gcam.y), float(gp.z - gcam.z));
			D3DXVECTOR3 ld = c - pp;
			D3DXVec3Normalize(&ld, &ld);

			SmapRenderList.clear();
			for (auto* vv : assy) SmapRenderList.push_back(vv);

			SHADOWMAPPARAM save = smap;
			int lod = RenderShadowMap(c, ld, r, false, true);
			if (lod >= 0 && EnsurePShnTarget())
			{
				HR(pDevice->StretchRect(psShmRT[lod], NULL, psPShn, NULL, D3DTEXF_POINT));
				pshn = smap;
				pshn.pShadowMap = ptPShn;
				for (auto* vv : assy) pshnSet.insert(vv);
				// logs on every CHANGE of the assembly size - the first frame runs before
				// all visuals exist (they are created over successive frames), so a one-shot
				// would report the transient and never the steady state
				static int logPShnN = -1;
				if ((int)pshnSet.size() != logPShnN) {
					logPShnN = (int)pshnSet.size();
					oapiWriteLogV("D3D9: planet-shine shadows LIVE (%d vessel(s) in assembly)", logPShnN);
				}
			}
			smap = save;
		}
	}


	if (dwTurn == RENDERTURN_ENVCAM) {

		if (Config->EnvMapMode) {
			DWORD flags = 0;
			if (Config->EnvMapMode == 1) flags |= 0x01;
			if (Config->EnvMapMode >= 2) flags |= (0x03 | 0x20);	// ORO patch (v): mode 3 = full scene + exp

			if (vobjEnv == NULL) vobjEnv = vobjFirst;

			while (vobjEnv) {
				if (vobjEnv->type == OBJTP_VESSEL && vobjEnv->apprad>8.0f) {
					if (vobjEnv->vobj) {
						vVessel *vVes = (vVessel *)vobjEnv->vobj;
						if (vVes->RenderENVMap(pDevice, RenderCount, flags) == false) break; // Not yet done with this vessel
					}
				}
				vobjEnv = vobjEnv->next; // Move to the next one
			}
		}
	}



	// -------------------------------------------------------------------------------------------------------
	// Render Irradiance Map For Vessels
	// -------------------------------------------------------------------------------------------------------

	if (dwTurn == RENDERTURN_IRRADIANCE) {

		if (Config->EnvMapMode && Config->bIrradiance) {
			DWORD flags = 0;
			if (Config->EnvMapMode == 1) flags |= 0x01;
			if (Config->EnvMapMode >= 2) flags |= (0x03 | 0x20);	// ORO patch (v): mode 3 = full scene + exp

			if (vobjIrd == NULL) vobjIrd = vobjFirst;

			while (vobjIrd) {
				if (vobjIrd->type == OBJTP_VESSEL && vobjIrd->apprad>8.0f) {
					if (vobjIrd->vobj) {
						vVessel *vVes = (vVessel *)vobjIrd->vobj;
						if (vVes->ProbeIrradiance(pDevice, RenderCount, flags) == false) break; // Not yet done with this vessel
					}
				}
				vobjIrd = vobjIrd->next; // Move to the next one
			}
		}
	}


	// ---------------------------------------------------------------------------------------------
	// Init. camera setup and create a render list
	// ---------------------------------------------------------------------------------------------

	VOBJREC* pv = NULL;
	LPDIRECT3DTEXTURE9 pShdMap = NULL;

	UpdateCameraFromOrbiter(RENDERPASS_MAINSCENE);
	UpdateCamVis();

	RenderList.clear();

	for (pv = vobjFirst; pv; pv = pv->next) {
		if (!pv->vobj->IsActive()) continue;
		if (!pv->vobj->IsVisible()) continue;
		if (pv->type == OBJTP_VESSEL) {
			vVessel* vV = (vVessel*)pv->vobj;
			RenderList.push_back(vV);
			vV->bStencilShadow = true;
		}
	}

	float znear_for_vessels = ComputeNearClipPlane();




	// ---------------------------------------------------------------------------------------------
	// Start Rendering of Normal and Depth Buffer for SSAO and (point in scene) visibility checks
	// ---------------------------------------------------------------------------------------------

	if (psgBuffer[GBUF_DEPTH] && pDepthNormalDS)
	{
		SetCameraFrustumLimits(0.1f, 1e6f);
		BeginPass(RENDERPASS_NORMAL_DEPTH);

		gc->PushRenderTarget(psgBuffer[GBUF_DEPTH], pDepthNormalDS, RENDERPASS_NORMAL_DEPTH);

		RecallDefaultState();

		// Clear buffers
		HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0L));

		// Render vessels
		for (auto* vVes : RenderList) vVes->Render(pDevice, false);

		// Render Cockpit
		if (oapiCameraInternal() && vFocus) vFocus->Render(pDevice, true);

		// ORO patch (z2): base structures join the buffer, so the glare visibility
		// kernels (sun AND local lights) and the patch-(g) Sketchpad clip stop being
		// blind to buildings. See vBase::RenderStructureDepth.
		if (Camera.vProxy) Camera.vProxy->RenderBaseDepth(GetProjectionViewMatrix());

		// ORO patch (ab): TERRAIN, through the (z3) tile registry - the tiles the
		// PREVIOUS frame's terrain render registered (terrain does not move; one
		// frame stale costs a one-frame edge at a LOD pop). Z-tested against the
		// vessels and structures already in the pass. Cull NONE: a heightfield has no
		// meaningful backface for a depth buffer.
		if (pTileDepth && LclTiles.size()) {
			pTileDepth->ClearTextures();
			pTileDepth->Setup(pPatchVertexDecl, true, 0);
			pTileDepth->SetVSConstants("mTileDepthVP", GetProjectionViewMatrix(), sizeof(D3DXMATRIX));
			pTileDepth->SetPSConstants("vTileDepthCamX", (void*)&Camera.x, sizeof(D3DXVECTOR3));
			pTileDepth->SetPSConstants("vTileDepthCamY", (void*)&Camera.y, sizeof(D3DXVECTOR3));
			HANDLE hW = pTileDepth->GetVSHandle("mTileShdW");
			pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			for (auto& t : LclTiles) {
				if (dwFrameId - t.stamp > 1) { g_oroDbgTileSkip++; continue; }	// only what the last frame drew
				g_oroDbgTileDrawn++;
				// ORO patch (ab) round 4: rebuilt from the planet's CURRENT frame - see
				// LCLTILECASTER (rounds 3+4: the KSC blink, then the time-warp blink).
				D3DXMATRIX mW;
				OroTileFromPlanet(t, Camera.pos, mW);
				pTileDepth->SetVSConstants(hW, (void*)&mW, sizeof(D3DXMATRIX));
				pDevice->SetStreamSource(0, t.pVB, 0, sizeof(VERTEX_2TEX));
				pDevice->SetIndices(t.pIB);
				pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, t.nv, 0, t.nf);
			}
			pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		}

		gc->PopRenderTargets();
		PopPass();

		// ORO patch (ab): the finished buffer goes to the stencil ground-shadow shader
		// for its SOFT depth test (Mesh.fx ShadowTechPS): a sheet is discarded only
		// where it lies behind the scene by more than base + k * distance metres, so
		// bumps under it pass (the flat plane is metres off the real ground) while a
		// hill between it and the eye hides it. The two numbers are the tolerance in
		// metres and per metre of distance. Pushed HERE, after the pass, so the probe
		// cubes (rendered before it) never test against a buffer from another camera;
		// zeroed again at the top of the next frame.
		{
			extern float g_oroShadowTolBase, g_oroShadowTolK;
			// the two tolerances are D3D9Client.cfg keys (ShadowDepthTol / ShadowDepthTolK)
			// so they can be found in the sim without a rebuild - the origin-tilt pattern
			g_oroShadowTolBase = (float)Config->ShadowDepthTol;
			g_oroShadowTolK    = (float)Config->ShadowDepthTolK;
			if (D3D9Effect::eSceneDepth)    D3D9Effect::FX->SetTexture(D3D9Effect::eSceneDepth, ptgBuffer[GBUF_DEPTH]);
			if (D3D9Effect::eSceneDepthPrm) D3D9Effect::FX->SetVector(D3D9Effect::eSceneDepthPrm,
				ptr(D3DXVECTOR4(1.0f / (float)viewW, 1.0f / (float)viewH, g_oroShadowTolBase, g_oroShadowTolK)));
		}
		// ORO patch (ab) INSTRUMENT: the mode to the shader, and once a second what the
		// machinery did - THIS frame's depth pass, LAST frame's stencil sheets (they are
		// drawn later in the frame; the counters reset here).
		{
			extern float g_oroShadowTolBase, g_oroShadowTolK;
			extern float g_gcStormLight;
			if (Config->ShadowDebug > 0) {
				if (D3D9Effect::eOroDbg) D3D9Effect::FX->SetFloat(D3D9Effect::eOroDbg, (float)Config->ShadowDebug);
				static DWORD   s_last = 0;
				static VECTOR3 s_cam  = _V(0, 0, 0);
				const DWORD now = GetTickCount();
				const VECTOR3 cd = Camera.pos - s_cam; s_cam = Camera.pos;
				if (now - s_last > 1000) {
					s_last = now;
					if (casc[1].live) {
						char tx[256]; int n = 0;
						for (int k = 0; k < 9; k++) n += sprintf_s(tx + n, sizeof(tx) - n, " %.3f", casc[k].live ? casc[k].texel : 0.0f);
						oapiWriteLogV("ORO shadow dbg: cascades (last frame): slot1 anchor-camera %.2f m; texels (slots 0-8):%s m; hull boxes %d",
							cascDbgAnchD, tx, (int)(cascVes[0] != NULL) + (int)(cascVes[1] != NULL) + (int)(cascVes[2] != NULL));
					}
					oapiWriteLogV("ORO shadow dbg: frame %u  registry %u  depth-pass tiles %d (stamp-skipped %d)  cam moved %.3f m  tol %.2f + %.4f/m  storm %.2f fogSunCam %.3f  |  stencil (last frame): vessels drawn %d skipped %d (depth %.3f fade %.3f dv %.3f)  structures drawn %d skipped %d",
						dwFrameId, (unsigned)LclTiles.size(), g_oroDbgTileDrawn, g_oroDbgTileSkip, length(cd),
						g_oroShadowTolBase, g_oroShadowTolK, g_gcStormLight, g_oroFogSunCam,
						g_oroDbgVesDrawn, g_oroDbgVesSkip, g_oroDbgDepth, g_oroDbgFade, g_oroDbgDv,
						g_oroDbgStrDrawn, g_oroDbgStrSkip);
				}
			}
			else if (D3D9Effect::eOroDbg) D3D9Effect::FX->SetFloat(D3D9Effect::eOroDbg, 0.0f);
			g_oroDbgTileDrawn = g_oroDbgTileSkip = 0;
			g_oroDbgVesDrawn = g_oroDbgVesSkip = g_oroDbgStrDrawn = g_oroDbgStrSkip = 0;
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Compute visibility of the Sun and Local light sources. After field depth render ! ! !
	// ---------------------------------------------------------------------------------------------

	ComputeLocalLightsVisibility();


	// -------------------------------------------------------------------------------------------------------
	// -------------------------------------------------------------------------------------------------------
	// ORO patch (s) part 6: WET-GROUND PLANAR REFLECTIONS. Render the VESSELS - only the
	// vessels: no terrain, no sky, no bases - through a camera mirrored about the local
	// ground plane, into the half-res target. BaseTilePS then samples it at each pixel's
	// own screen position, masked by the puddle lattice and the Fresnel term, so the
	// image appears as rippling patches in the standing water rather than as a mirror.
	//
	// The mirrored view-projection is THE standard planar trick: pre-multiplying the
	// reflection matrix means a real point P renders exactly where the main camera sees
	// its virtual image - so the ground shader lookup is just its own VPOS. A mirror
	// flips the winding, hence the CULLMODE swap. The pass runs under CUSTOMCAM (the
	// pass id that already means "the scene, rendered again elsewhere") and costs
	// nothing when the world is dry.
	// -------------------------------------------------------------------------------------------------------
	bWetReflLive = false;
	{
		extern float g_gcSurfaceWet;
		if (g_gcSurfaceWet > 0.01f && psWetRefl && psWetReflDS && Camera.hObj_proxy)
		{
			VECTOR3 pC; oapiGetGlobalPos(Camera.hObj_proxy, &pC);
			VECTOR3 rel = Camera.pos - pC;
			double  cr  = length(rel);
			double  lng = 0.0, lat = 0.0, rad = 0.0;
			oapiGlobalToEqu(Camera.hObj_proxy, Camera.pos, &lng, &lat, &rad);
			double  elv  = oapiSurfaceElevation(Camera.hObj_proxy, lng, lat);
			double  hAGL = cr - (oapiGetSize(Camera.hObj_proxy) + elv);
			// ⚠️ THE PLANE ANCHORS TO THE GROUND UNDER THE FOCUS VESSEL, NOT UNDER THE
			// CAMERA (round 4: "the reflection changes altitude as I rotate the view").
			// The elevation sampled under the CAMERA changes as it orbits across
			// undulating terrain, so a camera-anchored plane made the image rise and
			// sink while the vessel never moved - paused or not, since it is pure
			// camera position. The vessel's own ground level is constant under an
			// orbiting camera, which pins the image; one plane cannot match every
			// terrain height at once, so it is matched where the eye looks: the ship.
			// Falls back to the camera anchor when there is no focus vessel nearby or
			// the camera sits below the vessel's ground plane (a mirror plane above
			// the camera is nonsense for this use).
			double planeAGL = hAGL;
			OBJHANDLE hFoc = oapiGetFocusObject();
			if (hFoc) {
				VECTOR3 fpos; oapiGetGlobalPos(hFoc, &fpos);
				if (length(fpos - Camera.pos) < 2000.0) {
					double lngF = 0.0, latF = 0.0, radF = 0.0;
					oapiGlobalToEqu(Camera.hObj_proxy, fpos, &lngF, &latF, &radF);
					double elvF = oapiSurfaceElevation(Camera.hObj_proxy, lngF, latF);
					double pa   = cr - (oapiGetSize(Camera.hObj_proxy) + elvF);
					if (pa > 0.3 && pa < 400.0) planeAGL = pa;
				}
			}
			if (cr > 1.0 && hAGL > 1.0 && hAGL < 250.0)
			{
				VECTOR3 up = rel / cr;
				// plane through the ground point under the FOCUS VESSEL (see above),
				// in the client's camera-relative world space (camera = origin)
				D3DXPLANE plane((float)up.x, (float)up.y, (float)up.z, (float)planeAGL);
				D3DXMATRIX mRefl, mVP1;
				D3DXMATRIX mSave = Camera.mProjView;
				D3DXMatrixReflect(&mRefl, &plane);
				D3DXMatrixMultiply(&mVP1, &mRefl, &mSave);
				// ⚠️ A SECOND MIRROR, IN CLIP SPACE, AND IT IS THE WHOLE TRICK (round 2).
				// One reflection makes every triangle wind backwards, and a cull-mode
				// override cannot fix that here: D3D9Mesh::Render RE-SETS the cull mode
				// PER GROUP (Mesh.cpp:1924/1927), so any state set before the loop dies
				// at the first group - the mirrored vessels rendered inside-out and
				// culled to nothing, which is why round 1 drew an empty target.
				// Flipping clip-space X is a second mirror: two mirrors = even = the
				// meshes' own culling is correct untouched. The image lands horizontally
				// flipped in the RT, and the sampler flips it back (BaseTilePS).
				D3DXMATRIX mFlipX; D3DXMatrixIdentity(&mFlipX); mFlipX._11 = -1.0f;
				D3DXMatrixMultiply(&Camera.mProjView, &mVP1, &mFlipX);
				// ⚠️ AND PUSH IT INTO THE EFFECT (round 3, the actual bug). The meshes do
				// NOT fetch the view-projection per draw - gVP is set ONCE per camera
				// update (SetCameraAperture) and every mesh trusts the stored value. So
				// overriding Camera.mProjView alone mirrored NOTHING: the pass rendered a
				// plain right-side-up image with the normal camera, and the sampler then
				// displayed it at x-flipped positions - upright ghost vessels orbiting
				// the wrong way as the camera turned, which is exactly what he reported.
				D3D9Effect::SetViewProjMatrix(&Camera.mProjView);

				// the RT must not be bound as a sampler while it is the target
				if (D3D9Effect::eWetReflTex) D3D9Effect::FX->SetTexture(D3D9Effect::eWetReflTex, NULL);

				BeginPass(RENDERPASS_CUSTOMCAM);
				gc->PushRenderTarget(psWetRefl, psWetReflDS, RENDERPASS_CUSTOMCAM);
				RecallDefaultState();
				HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0L));
				for (auto* vVes : RenderList)
					if (vVes->CamDist() < 1500.0) vVes->Render(pDevice, false);
				// ⚠️ BEACONS TOO, IN A SECOND PASS (2026-08-24, a beta tester's ask:
				// "if the beacons could be added to the water reflection?"). Nav lights
				// and strobes were never being lost or clipped here - they are simply
				// not part of vVessel::Render. The main scene draws them in its own
				// later loop, and this pass only ever called the mesh half.
				//
				// IT NEEDS NO CAMERA PLUMBING, which is the whole reason it is one
				// loop and not a rework: RenderSpot builds its billboard from the
				// object's CAMERA-RELATIVE POSITION and draws through the same gVP this
				// pass already overrides. The blob therefore faces the REAL camera
				// rather than the mirrored one and is tilted by the angle between the
				// two view rays - about 11 deg for a camera 3 m up and a vessel 30 m
				// away, i.e. a round blob at 98% of its width. A soft spot tolerates
				// that completely, which is what lets a MATRIX-ONLY mirror carry
				// billboards at all; a custom camera has to swap the whole Camera
				// struct because it cannot assume the object is near the plane.
				//
				// A SECOND LOOP, not folded into the first, matching the main scene:
				// a beacon has to composite over EVERY hull in the reflection, not
				// just the one it belongs to. Strobe phase is fmod(simt, period), so
				// both passes agree within the frame and a reflection can never flash
				// out of step with the light casting it.
				// ⚠️ EXHAUST AND PARTICLES TOO (2026-08-25, the same tester's follow-up:
				// the reflection showed the hull but not what was coming out of it).
				// Same story as the beacons and the same reason it is this cheap:
				// vVessel::RenderExhaust orients its billboard from cdir = the camera's
				// position in VESSEL frame, and D3D9ParticleStream::RenderDiffuse builds
				// every sprite from (p->pos - camera_gpos) - both CAMERA-RELATIVE
				// POSITIONS, both drawn through the gVP this pass already overrides. So a
				// matrix-only mirror carries them, with the same negligible tilt the
				// beacons have.
				// ORDER MATCHES THE MAIN SCENE - exhausts, beacons, then streams - because
				// these are additive layers and the order they accumulate in is the look.
				// ⚠️ RenderExhaust's FIRST LINE is gcIsExhaustSuppressed(), which is patch
				// (n). Anyone running ORO's own plume has stock exhaust suppressed, so this
				// draws nothing for them and costs nothing - it is here for the users who
				// keep the stock billboards. ORO's own plume cannot arrive by this route at
				// all: it is screen-space Sketchpad geometry drawn after the scene, so it
				// would need a render-proc slot INSIDE this pass plus a second geometry
				// build against the mirrored camera.
				for (auto* vVes : RenderList)
					if (vVes->IsActive() && vVes->CamDist() < 1500.0)
						vVes->RenderExhaust();
				for (auto* vVes : RenderList)
					if (vVes->IsActive() && vVes->CamDist() < 1500.0)
						vVes->RenderBeacons(pDevice);
				// The streams are scene-owned and carry no cheap distance handle, so they
				// go in whole, exactly as the main scene and RenderSecondaryScene do. This
				// only ever runs while the ground is wet and the camera is under 250 m AGL.
				for (DWORD ns = 0; ns < nstream; ns++) pstream[ns]->Render(pDevice);
				// ⚠️ AND ORO'S OWN PLUME, WHICH CANNOT ARRIVE BY ANY OF THE ROUTES ABOVE
				// (2026-08-25, patch (u)). Everything else in this pass is scene geometry
				// the CLIENT draws, so overriding gVP carried it for free. ORO's jet is
				// SCREEN-SPACE Sketchpad triangles projected on the CPU and drawn after
				// the scene, in the pre-resolve slot - by which time this pass is long
				// finished. So it gets a slot of its own, inside the push.
				//
				// TWO THINGS THE SLOT HANDS IT, and both are SUBSTITUTIONS rather than
				// new API - which is why the addon side is a parameterisation and not a
				// second renderer:
				//  - THE PAD IS BOUND TO THE HALF-RES RT, not the backbuffer, because
				//    BeginDrawing() takes gc->GetTopRenderTarget(). Its ortho matrix and
				//    its GetRenderSurfaceSize() therefore both describe the reflection
				//    texture, so an addon reads its viewport out of the Sketchpad it was
				//    handed and never has to know this target is half resolution. Change
				//    that resolution and every consumer follows with no addon edit.
				//  - GetRenderCam() REPORTS THE MIRRORED CAMERA for the duration, so an
				//    addon that already projects against the render camera (patch k)
				//    needs no second camera path: one build function, run twice a frame
				//    against two cameras.
				//
				// THE MIRRORED CAMERA IS A REAL CAMERA, and that is what makes the second
				// point work at all. Reflecting the eye point and the three basis vectors
				// through the plane gives a view in which a real point P lands exactly
				// where the main camera sees P's virtual image - the same identity the
				// matrix trick above relies on. Aperture is untouched: a mirror does not
				// change the field of view.
				//
				// ⚠️ IT IS A PURE MIRROR, SO ITS BASIS IS LEFT-HANDED (det = -1), AND THE
				// CONSUMER MUST RECONCILE THAT WITH THE RT ITSELF. This pass draws the
				// meshes through an extra clip-space X flip (mFlipX above) purely to keep
				// their winding legal, so the RT holds a horizontally mirrored image that
				// the ground shaders undo when they sample it. CPU-projected geometry gets
				// no mFlipX, so whatever draws here has to mirror its own screen X to land
				// in the same convention. Deliberately NOT folded into the reported basis:
				// the camera then describes the pass's real geometry, and the one place
				// that has to know about the RT's flip is the code putting pixels in it.
				//
				// ⚠️ SCENE DEPTH IS THE MAIN CAMERA'S. ptgBuffer[GBUF_DEPTH] was filled in
				// RENDERPASS_NORMAL_DEPTH from the real view, so patch (g)'s per-pixel
				// clip is meaningless in here and would cut the reflection against the
				// wrong geometry. The doc comment on RENDERPROC_WET_MIRROR says so; there
				// is nothing this side can do to enforce it.
				{
					VECTOR3 mp = Camera.pos - up * (2.0 * planeAGL);
					MATRIX3 mr = Camera.grot;
					for (int j = 0; j < 3; j++) {
						VECTOR3 v = _V(mr.data[j], mr.data[3 + j], mr.data[6 + j]);   // column j
						v -= up * (2.0 * dotp(up, v));
						mr.data[j] = v.x; mr.data[3 + j] = v.y; mr.data[6 + j] = v.z;
					}
					mirrorCamPos = mp;
					mirrorCamRot = mr;
					bMirrorCam = true;
					D3D9Pad *pSkpM = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);
					if (pSkpM) {
						gc->MakeRenderProcCall(pSkpM, RENDERPROC_WET_MIRROR, NULL, NULL);
						pSkpM->EndDrawing();
					}
					bMirrorCam = false;   // lowered before anything else can ask
				}
				gc->PopRenderTargets();
				PopPass();

				Camera.mProjView = mSave;
				D3D9Effect::SetViewProjMatrix(&Camera.mProjView);   // hand the frame back
				bWetReflLive = true;
			}

			// one-shot breadcrumbs: the difference between "the pass never ran" and
			// "it ran and the look is wrong" costs a fly-and-report round otherwise
			static int logLive = 0, logSkip = 0;
			if (bWetReflLive && logLive == 0) {
				oapiWriteLogV("D3D9: ORO wet-mirror pass LIVE (hAGL %.1f m)", hAGL);
				logLive = 1;
			}
			if (!bWetReflLive && logSkip == 0) {
				oapiWriteLogV("D3D9: ORO wet-mirror pass skipped (hAGL %.1f m, need 1..250)", hAGL);
				logSkip = 1;
			}
		}

		// feed the shaders either way: .w tells BaseTilePS whether the mirror is live
		extern float g_gcWetRefl;
		extern float g_gcWetSwimAmp;
		extern float g_gcWetSwimRate;
		extern float g_gcWetPoolSize;
		extern float g_gcWetPoolReach;
		extern float g_gcWetGrainOp;
		extern float g_gcWetGrainSize;
		if (D3D9Effect::eWetReflPrm) {
			D3DXVECTOR4 wrp(1.0f / (float)viewW, 1.0f / (float)viewH,
			                g_gcWetRefl, bWetReflLive ? 1.0f : 0.0f);
			D3D9Effect::FX->SetVector(D3D9Effect::eWetReflPrm, &wrp);
		}
		if (D3D9Effect::eWetSwimPrm) {
			D3DXVECTOR4 wsp(g_gcWetSwimAmp, g_gcWetSwimRate, g_gcWetPoolSize, g_gcWetPoolReach);
			D3D9Effect::FX->SetVector(D3D9Effect::eWetSwimPrm, &wsp);
		}
		// ⚠️ .z CARRIES THE REFLECTION BLUR, WHICH IS NOT A GRAIN PARAMETER (2026-08-24).
		// It rides here purely because gWetReflPrm's four channels are all taken (1/W,
		// 1/H, gain, live) and this vector already had two spare - a transport, not a
		// grouping. Anyone hunting the blur will look in gWetReflPrm first; this note is
		// the signpost. See SetWetReflection's fBlur.
		extern float g_gcWetBlur;
		if (D3D9Effect::eWetGrainPrm) {
			D3DXVECTOR4 wgp(g_gcWetGrainOp, g_gcWetGrainSize, g_gcWetBlur, 0.0f);
			D3D9Effect::FX->SetVector(D3D9Effect::eWetGrainPrm, &wgp);
		}
		if (bWetReflLive && D3D9Effect::eWetReflTex)
			D3D9Effect::FX->SetTexture(D3D9Effect::eWetReflTex, ptWetRefl);

		// one-shot: the handles and what the effect ACTUALLY stored after SetVector -
		// splits "handle never bound" from "value set but not reaching the shader"
		static int logRb = 0;
		if (logRb == 0 && bWetReflLive) {
			D3DXVECTOR4 rb(0, 0, 0, 0);
			if (D3D9Effect::eWetReflPrm) D3D9Effect::FX->GetVector(D3D9Effect::eWetReflPrm, &rb);
			oapiWriteLogV("D3D9: ORO wet-mirror handles Prm=%p Tex=%p readback %.5f %.5f %.2f %.1f",
			              (void*)D3D9Effect::eWetReflPrm, (void*)D3D9Effect::eWetReflTex,
			              rb.x, rb.y, rb.z, rb.w);
			logRb = 1;
		}
	}

	// -------------------------------------------------------------------------------------------------------
	// ORO patch (v) part 2: VESSEL PLANAR MIRRORS - the EXACT reflection for the flat
	// near-mirror surfaces that expose probe parallax (the shuttle's radiators and
	// door inner faces). For each plane the FOCUS vessel declares (_ecam BEGIN_PLANE),
	// render the vessel list - SELF INCLUDED, which is the whole point - through a
	// camera mirrored about the plane, into a half-res target; PBR groups assigned to
	// the plane sample it at their own screen position, the wet-ground mirror's exact
	// contract. Distant environment stays with the probes (probe parallax error
	// vanishes at infinity), so the two techniques cover each other's blind spots.
	// The CLIP PLANE removes geometry BEHIND the mirror - the vessel's own far half -
	// which a ground-plane mirror never had to worry about; with shaders active D3D9
	// clip planes live in CLIP SPACE (inverse-transpose of the VP).
	// -------------------------------------------------------------------------------------------------------
	OroHangTrace("mainscene reached planar gate");
	nRflPlnLive = 0; pRflPlnVes = NULL;
	if (Config->EnvMapMode >= 3 && vFocus && vFocus->IsActive() && vFocus->CamDist() < 1500.0 && psWetReflDS)
	{
		const DWORD npl = min(vFocus->RflPlaneCount(), (DWORD)2);
		for (DWORD ip = 0; ip < npl; ip++)
		{
			if (!psRflPln[ip]) continue;
			OroHangTrace("planar plane %u begin", ip);
			D3DXVECTOR3 pw, nw; float rd = 5.0f;
			if (!vFocus->GetRflPlane(ip, &pw, &nw, &rd)) continue;
			// the camera (this space's ORIGIN) must sit on the reflective side, with
			// half a metre of margin so a grazing eye cannot z-fight the plane
			const float d0 = -D3DXVec3Dot(&nw, &pw);
			if (d0 < 0.5f) continue;

			D3DXPLANE plane(nw.x, nw.y, nw.z, d0);
			// the same equation feeds the shader's curvature warp (gPlnEq)
			rflPlnEq[ip] = D3DXVECTOR4(nw.x, nw.y, nw.z, d0);
			rflPlnDist[ip] = rd;
			D3DXMATRIX mRefl, mVP1, mFlipX;
			D3DXMATRIX mSave = Camera.mProjView;
			D3DXMatrixReflect(&mRefl, &plane);
			D3DXMatrixMultiply(&mVP1, &mRefl, &mSave);
			// the wet mirror's double-flip: a second mirror in clip space keeps every
			// group's own culling legal; the sampler undoes the X flip (PBR.fx)
			D3DXMatrixIdentity(&mFlipX); mFlipX._11 = -1.0f;
			D3DXMatrixMultiply(&Camera.mProjView, &mVP1, &mFlipX);
			D3D9Effect::SetViewProjMatrix(&Camera.mProjView);

			D3DXMATRIX mI; D3DXMatrixInverse(&mI, NULL, &Camera.mProjView);
			D3DXMatrixTranspose(&mI, &mI);
			D3DXPLANE cp; D3DXPlaneTransform(&cp, &plane, &mI);

			// the RT must not be bound as a sampler while it is the target
			if (D3D9Effect::ePlnMap) D3D9Effect::FX->SetTexture(D3D9Effect::ePlnMap, NULL);

			BeginPass(RENDERPASS_CUSTOMCAM);
			gc->PushRenderTarget(psRflPln[ip], psWetReflDS, RENDERPASS_CUSTOMCAM);
			RecallDefaultState();
			HR(pDevice->SetClipPlane(0, (const float*)&cp));
			HR(pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 1));
			HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0L));
			for (auto* vVes : RenderList)
				if (vVes->CamDist() < 1500.0) vVes->Render(pDevice, false);
			HR(pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0));
			gc->PopRenderTargets();
			PopPass();

			Camera.mProjView = mSave;
			D3D9Effect::SetViewProjMatrix(&Camera.mProjView);

			OroHangTrace("planar plane %u done", ip);
			nRflPlnLive |= (1 << ip);
		}
		if (nRflPlnLive) pRflPlnVes = vFocus;

		static int logPln = 0;
		if (nRflPlnLive && logPln == 0) {
			oapiWriteLogV("D3D9: planar mirror pass LIVE (%d of %d plane(s), mask 0x%X)",
				(nRflPlnLive & 1) + ((nRflPlnLive >> 1) & 1), (int)npl, nRflPlnLive);
			logPln = 1;
		}
	}

	// Start Main Scene Rendering
	// -------------------------------------------------------------------------------------------------------

	RenderFlags = 0xFFFFFFFF; // Not used for main scene, set to 0xFFFFFFFF 

	// Push main render target and depth surfaces
	//
	gc->PushRenderTarget(pBackBuffer, gc->GetDepthStencil(), RENDERPASS_MAINSCENE);	// Main Scene


	if (DebugControls::IsActive()) {
		HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0L));
		DWORD flags = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDEBUGFLAGS);
		if (flags&DBG_FLAGS_WIREFRAME) pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		else						   pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	}
	else {
		// Clear the viewport
		HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0L));
	}



	// Do we use z-clear render mode or not ?
	bool bClearZBuffer = false;
	if ( (GetTargetGroundAltitude() > 2e3) && (oapiCameraInternal() == false)) bClearZBuffer = true;
	if (IsProxyMesh()) bClearZBuffer = false;


	if (DebugControls::IsActive()) {
		DWORD camMode = *(DWORD*)gc->GetConfigParam(CFGPRM_GETCAMERAMODE);
		if (camMode!=0) znear_for_vessels = 0.1f;
	}

	// -------------------------------------------------------------------------------------------------------
	// render celestial sphere background
	// -------------------------------------------------------------------------------------------------------

	pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);

	bool bEnableAtmosphere = false;

	vPlanet *vPl = GetCameraProxyVisual();

	// -------------------------------------------------------------------------------------------------------
	// Render the celestial sphere (background image, stars, planetarium features)
	// -------------------------------------------------------------------------------------------------------

	// Set generic clip plane distances for celestial sphere
	SetCameraFrustumLimits(0.1, 10);

	m_celSphere->Render(pDevice, sky_color);

	// Set Initial Near clip plane distance
	if (bClearZBuffer) SetCameraFrustumLimits(1e3, 3e8f);
	else			   SetCameraFrustumLimits(znear_for_vessels, 3e8f);

	pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

	// ---------------------------------------------------------------------------------------------
	// Create a caster list for shadow mapping
	// ---------------------------------------------------------------------------------------------

	Casters.clear();

	for (pv = vobjFirst; pv; pv = pv->next) {
		if (!pv->vobj->IsActive()) continue;
		if (pv->type == OBJTP_VESSEL) Casters.push_back((vVessel *)pv->vobj);
	}

	Casters.sort(sort_vessels);



	// ---------------------------------------------------------------------------------------------
	// Render shadow map for vFocus early for surface base and planet rendering
	// ---------------------------------------------------------------------------------------------

	int shadow_lod = -1;
	float bouble_rad = 10.0f;		// Terrain shadow mapping coverage

	if (Config->ShadowMapMode >= 1 && Config->TerrainShadowing == 2) {

		SmapRenderList.clear();
		SmapRenderList.push_back(vFocus);

		D3DXVECTOR3 ld = sunLight.Dir;
		D3DXVECTOR3 pos = vFocus->GetBoundingSpherePosDX();
		float rad = vFocus->GetBoundingSphereRadius();
		float frad = rad;

		vFocus->bStencilShadow = false;


		// What else should be included besides vFocus ?

		for (auto v : Casters)
		{
			if (v == vFocus) continue;
			if (v->HasShadow() == false) continue;

			D3DXVECTOR3 bs_pos = v->GetBoundingSpherePosDX();
			float bs_rad = v->GetBoundingSphereRadius();

			if (bs_rad > 80.0) continue;

			D3DXVECTOR3 bc = bs_pos - pos;
			float z = D3DXVec3Dot(&ld, &bc);
			if (fabs(z) > 1e3) continue;
			D3DXVECTOR3 fbc = bc - ld * z;
			float dst = D3DXVec3Length(&fbc);
			if (dst > 1e3) continue;

			float nrd = (rad + dst + bs_rad) * 0.5f;

			bool bInclude = false;

			if (dst < (bs_rad + frad)) bInclude = true;
			if (nrd < bouble_rad) bInclude = true;

			if (bInclude) {

				v->bStencilShadow = false;
				SmapRenderList.push_back(v);

				if (nrd < rad) continue;

				if (nrd < bs_rad) {
					pos = bs_pos;
					rad = bs_rad;
				}
				else {
					if (dst > 0.001f) pos += fbc * ((nrd - rad) / dst);
					rad = nrd;
				}
			}
		}

		shadow_lod = RenderShadowMap(pos, ld, rad, false, true);

		pShdMap = smap.pShadowMap;
	}


	// ---------------------------------------------------------------------------------------------
	// ORO patch (z3): render the LOCAL-LIGHT shadow map - after the sun's map (so the
	// smap save/restore below cannot disturb it) and before the planets render (the
	// terrain is the consumer). Self-gates on config, targets and light availability.
	// ---------------------------------------------------------------------------------------------

	RenderLocalLightShadowMap();
	RenderCascadeShadows();   // ORO patch (ae): the sun's cascade atlas - before the planets, which consume it


	// ---------------------------------------------------------------------------------------------
	// Render Planets
	// ---------------------------------------------------------------------------------------------

	DWORD plnmode = *(DWORD*)gc->GetConfigParam(CFGPRM_PLANETARIUMFLAG);
	DWORD mkrmode = *(DWORD*)gc->GetConfigParam(CFGPRM_SURFMARKERFLAG);

	for (DWORD i=0;i<nplanets;i++) {

		// double nplane, fplane;
		// plist[i].vo->RenderZRange (&nplane, &fplane);
		// cam->SetFrustumLimits (nplane, fplane);
		// since we are not using z-buffers here, we can adjust the projection
		// matrix at will to make sure the object is within the viewing frustum

		OBJHANDLE hObj = plist[i].vo->Object();
		bool isActive = plist[i].vo->IsActive();

		if (isActive) plist[i].vo->Render(pDevice);
		else		  plist[i].vo->RenderDot(pDevice);


		D3D9Pad *pSketch = GetPooledSketchpad(SKETCHPAD_LABELS);

		if (pSketch) {

			if (isActive) plist[i].vo->RenderVectors(pDevice, pSketch);

			if (mkrmode & MKR_ENABLE) {

				if (mkrmode & MKR_CMARK) {
					VECTOR3 pp;
					char name[256];
					oapiGetObjectName(hObj, name, 256);
					oapiGetGlobalPos(hObj, &pp);

					m_celSphere->EnsureMarkerDrawingContext((oapi::Sketchpad**)&pSketch, 0, m_celSphere->MarkerColor(0), m_celSphere->MarkerPen(0));
					RenderObjectMarker(pSketch, pp, std::string(name), std::string(), 0, viewH / 80);
				}

				if (isActive && (mkrmode & MKR_SURFMARK) && (oapiGetObjectType(hObj) == OBJTP_PLANET))
				{
					int label_format = *(int*)oapiGetObjectParam(hObj, OBJPRM_PLANET_LABELENGINE);
					if (label_format < 2 && (mkrmode & MKR_LMARK)) // user-defined planetary surface labels
					{
						double rad = oapiGetSize(hObj);
						double apprad = rad / (plist[i].dist * tan(GetCameraAperture()));
						const GraphicsClient::LABELLIST *list;
						DWORD n, nlist;
						MATRIX3 prot;
						VECTOR3 ppos, cpos;

						nlist = gc->GetSurfaceMarkers(hObj, &list);

						oapiGetRotationMatrix(hObj, &prot);
						oapiGetGlobalPos(hObj, &ppos);
						VECTOR3 cp = GetCameraGPos();
						cpos = tmul(prot, cp - ppos); // camera in local planet coords

						for (n = 0; n < nlist; n++) {

							if (list[n].active && apprad*list[n].distfac > LABEL_DISTLIMIT) {

								int size = (int)(viewH / 80.0*list[n].size + 0.5);
								int col = list[n].colour;

								m_celSphere->EnsureMarkerDrawingContext((oapi::Sketchpad**)&pSketch, 0, m_celSphere->MarkerColor(col), m_celSphere->MarkerPen(col));
								const std::vector<oapi::GraphicsClient::LABELSPEC>& ls = list[n].marker;
								VECTOR3 sp;
								for (int j = 0; j < ls.size(); j++) {
									if (dotp(ls[j].pos, cpos - ls[j].pos) >= 0.0) { // surface point visible?
										sp = mul(prot, ls[j].pos) + ppos;
										RenderObjectMarker(pSketch, sp, ls[j].label[0], ls[j].label[1], list[n].shape, size);
									}
								}
							}
						}
					}

					if (mkrmode & MKR_BMARK) {

						DWORD n = oapiGetBaseCount(hObj);
						MATRIX3 prot;
						oapiGetRotationMatrix(hObj, &prot);
						int size = (int)(viewH / 80.0);

						m_celSphere->EnsureMarkerDrawingContext((oapi::Sketchpad**)&pSketch, 0, m_celSphere->MarkerColor(0), m_celSphere->MarkerPen(0));

						for (DWORD i = 0; i < n; i++) {

							OBJHANDLE hBase = oapiGetBaseByIndex(hObj, i);

							VECTOR3 ppos, cpos, bpos;

							oapiGetGlobalPos(hObj, &ppos);
							oapiGetGlobalPos(hBase, &bpos);
							VECTOR3 cp = GetCameraGPos();
							cpos = tmul(prot, cp - ppos); // camera in local planet coords
							bpos = tmul(prot, bpos - ppos);

							double apprad = 8000e3 / (length(cpos - bpos) * tan(GetCameraAperture()));

							if (dotp(bpos, cpos - bpos) >= 0.0 && apprad > LABEL_DISTLIMIT) { // surface point visible?
								char name[64]; oapiGetObjectName(hBase, name, 63);
								VECTOR3 sp = mul(prot, bpos) + ppos;
								RenderObjectMarker(pSketch, sp, std::string(name), std::string(), 0, size);
							}
						}
					}
				}
			}

			pSketch->EndDrawing();	// SKETCHPAD_LABELS
		}
	}


	// -------------------------------------------------------------------------------------------------------
	// render a user defined planetarium art
	// -------------------------------------------------------------------------------------------------------

	if (plnmode & PLN_ENABLE) {
		D3D9Pad *pSketch = GetPooledSketchpad(SKETCHPAD_PLANETARIUM);
		gc->MakeRenderProcCall(pSketch, RENDERPROC_PLANETARIUM, GetViewMatrix(), GetProjectionMatrix());
		pSketch->EndDrawing(); // SKETCHPAD_PLANETARIUM
	}


	// -------------------------------------------------------------------------------------------------------
	// render a user defined exterior art
	// -------------------------------------------------------------------------------------------------------

	if (oapiCameraInternal() == false) {
		D3D9Pad *pSketch = GetPooledSketchpad(SKETCHPAD_PLANETARIUM);
		gc->MakeRenderProcCall(pSketch, RENDERPROC_EXTERIOR, GetViewMatrix(), GetProjectionMatrix());
		pSketch->EndDrawing(); // SKETCHPAD_PLANETARIUM
	}

	/*for (DWORD i = 0; i < nplanets; ++i)
	{
		OBJHANDLE hObj = plist[i].vo->Object();
		if (oapiGetObjectType(hObj) != OBJTP_PLANET) continue;
		D3D9Pad* pSketch = GetPooledSketchpad(SKETCHPAD_PLANETARIUM);
		pSketch->LoadDefaults();
		pSketch->SetViewMode(Sketchpad::USER);
		pSketch->SetViewProj(GetViewMatrix(), GetProjectionMatrix());
		static_cast<vPlanet*>(plist[i].vo)->TestComputations(pSketch);
		pSketch->EndDrawing(); // SKETCHPAD_PLANETARIUM
	}*/

	// -------------------------------------------------------------------------------------------------------
	// render new-style surface markers
	// -------------------------------------------------------------------------------------------------------

	if ((mkrmode & MKR_ENABLE) && (mkrmode & MKR_LMARK))
	{
		D3D9Pad* pSketch = GetPooledSketchpad(SKETCHPAD_LABELS);
		m_celSphere->EnsureMarkerDrawingContext((oapi::Sketchpad**)&pSketch, 0, 0, m_celSphere->MarkerPen(6));

		int fontidx = -1;
		for (DWORD i = 0; i < nplanets; ++i)
		{
			OBJHANDLE hObj = plist[i].vo->Object();
			if (oapiGetObjectType(hObj) != OBJTP_PLANET) { continue; }
			if (!surfLabelsActive) {
				static_cast<vPlanet*>( plist[i].vo )->ActivateLabels(true);
			}

			int label_format = *(int*)oapiGetObjectParam(hObj, OBJPRM_PLANET_LABELENGINE);

			if (label_format == 2)
			{
				static_cast<vPlanet*>(plist[i].vo)->RenderLabels(pDevice, pSketch, label_font, &fontidx);
			}
		}

		pSketch->EndDrawing();	// SKETCHPAD_LABELS

		surfLabelsActive = true;
	}
	else {
		surfLabelsActive = false;
	}


	// -------------------------------------------------------------------------------------------------------
	// render the vessel objects
	// -------------------------------------------------------------------------------------------------------

	// Set near clip plane for vessel exterior rendering
	if (bClearZBuffer) {
		pDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0L); // clear z-buffer
		SetCameraFrustumLimits(znear_for_vessels, 1e8f);
	}


	D3D9Effect::UpdateEffectCamera(Camera.hObj_proxy);


	auto RenderMarkers = RenderList;

	// Render the vessels inside the shadows
	//
	if (Config->ShadowMapMode >= 1) {

		D3DXVECTOR3 ld = sunLight.Dir;
		D3DXVECTOR3 pos = vFocus->GetBoundingSpherePosDX();
		float rad = vFocus->GetBoundingSphereRadius();

		int shadow_lod = RenderShadowMap(pos, ld, rad);

		if (shadow_lod >= 0) {

			pShdMap = ptShmRT[shadow_lod];

			// ORO patch (w) part 2: copy the FOCUS sun map for next frame's secondary
			// passes (probe cubes, mirrors) - see Scene.h. Exp mode only.
			if (Config->EnvMapMode >= 3 && EnsureSunCpyTarget()) {
				HR(pDevice->StretchRect(psShmRT[shadow_lod], NULL, psSunCpy, NULL, D3DTEXF_POINT));
				sunCpy = smap;
				sunCpy.pShadowMap = ptSunCpy;
				sunCpyLive = true;
			}

			auto it = RenderList.begin();

			while (it != RenderList.end()) {
				if ((*it)->IsInsideShadows()) {
					(*it)->Render(pDevice);
					it = RenderList.erase(it);
				}
				else ++it;
			}
		}
	}



	if ((Config->ShadowMapMode >= 2) && (DebugControls::IsActive()==false)) {
		// Don't render more shadows if debug controls are open

		std::list<vVessel *> Intersect;

		// Select the objects to shadow map
		//
		if (Config->ShadowMapMode >= 3) {
			for (auto it = RenderList.begin(); it != RenderList.end(); ++it) {
				if ((*it)->CamDist() < 1e3) Intersect.push_back((*it));
			}
		}
		else {
			for (auto it = RenderList.begin(); it != RenderList.end(); ++it) {
				if ((*it)->IntersectShadowTarget()) Intersect.push_back((*it));
			}
		}


		while (!Intersect.empty()) {

			D3DXVECTOR3 ld = sunLight.Dir;
			D3DXVECTOR3 pos = Intersect.front()->GetBoundingSpherePosDX();
			float rad = Intersect.front()->GetBoundingSphereRadius();

			Intersect.pop_front();

			int lod = RenderShadowMap(pos, ld, rad);

			if (lod >= 0) {

				// Render objects in shadow
				auto it = RenderList.begin();

				while (it != RenderList.end()) {
					if ((*it)->IsInsideShadows()) {
						(*it)->Render(pDevice);
						Intersect.remove((*it));
						it = RenderList.erase(it);
					}
					else ++it;
				}
			}
		}
	}


	// Render the remaining vessels those are not yet renderred
	//
	smap.pShadowMap = NULL;

	while (RenderList.empty()==false) {
		RenderList.front()->Render(pDevice);
		RenderList.pop_front();
	}

	D3D9Pad* pSketch = GetPooledSketchpad(SKETCHPAD_LABELS);
	if (pSketch) {
		m_celSphere->EnsureMarkerDrawingContext((oapi::Sketchpad**)&pSketch, 0, m_celSphere->MarkerColor(0), m_celSphere->MarkerPen(0));
		for (auto x : RenderMarkers) RenderVesselMarker(x, pSketch);
		pSketch->EndDrawing();	// SKETCHPAD_LABELS
	}



	// -------------------------------------------------------------------------------------------------------
	// render custom user objects
	// -------------------------------------------------------------------------------------------------------

	if (oapiCameraInternal() == false) {
		if (gc->IsGenericProcEnabled(GENERICPROC_RENDER_EXTERIOR)) {
			gc->MakeGenericProcCall(GENERICPROC_RENDER_EXTERIOR, 0, NULL);
		}
	}


	// -------------------------------------------------------------------------------------------------------
	// render the vessel sub-systems
	// -------------------------------------------------------------------------------------------------------

	// render exhausts
	//
	for (pv=vobjFirst; pv; pv=pv->next) {
		if (!pv->vobj->IsActive() || !pv->vobj->IsVisible() || pv->vobj->GetMeshCount() < 1) continue;
		OBJHANDLE hObj = pv->vobj->Object();
		if (oapiGetObjectType(hObj) == OBJTP_VESSEL) {
			((vVessel*)pv->vobj)->RenderExhaust();
		}
	}

	// render beacons
	//
	for (pv=vobjFirst; pv; pv=pv->next) {
		if (!pv->vobj->IsActive()) continue;
		pv->vobj->RenderBeacons(pDevice);
	}

	// render grapple points
    //
    for (pv=vobjFirst; pv; pv=pv->next) {
        if (!pv->vobj->IsActive()) continue;
        pv->vobj->RenderGrapplePoints(pDevice);
    }

	// render exhaust particle system
	//
	for (DWORD n = 0; n < nstream; n++) pstream[n]->Render(pDevice);


	// -------------------------------------------------------------------------------------------------------
	// Render vessel axis vectors
	// -------------------------------------------------------------------------------------------------------

	DWORD bfvmode = *(DWORD*)gc->GetConfigParam(CFGPRM_FORCEVECTORFLAG);
	DWORD favmode = *(DWORD*)gc->GetConfigParam(CFGPRM_FRAMEAXISFLAG);

	if (bfvmode & BFV_ENABLE || favmode & FAV_ENABLE)
	{

		pDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER,  0, 1.0f, 0L); // clear z-buffer

		pSketch = GetPooledSketchpad(SKETCHPAD_LABELS);
		pSketch->SetFont(pAxisFont);
		pSketch->SetTextAlign(Sketchpad::LEFT, Sketchpad::TOP);

		for (pv=vobjFirst; pv; pv=pv->next) {
			if (!pv->vobj->IsActive()) continue;
			if (!pv->vobj->IsVisible()) continue;
			if (oapiCameraInternal() && vFocus==pv->vobj) continue;

			pv->vobj->RenderVectors(pDevice, pSketch);
		}

		pSketch->EndDrawing();	// SKETCHPAD_LABELS
	}





	// -------------------------------------------------------------------------------------------------------
	// render the internal parts of the focus object in a separate render pass
	// -------------------------------------------------------------------------------------------------------

	if (oapiCameraInternal() && vFocus) {

		// --- ORO patch (f): SHADOWS IN THE VIRTUAL COCKPIT --------------------
		// The VC never received shadows, and nothing was actually missing to make it
		// work. The focus vessel's shadow map is rendered above; then, before the
		// "remaining vessels" loop, smap.pShadowMap is set to NULL - and THIS pass,
		// the internal one, runs after that. vVessel::Render binds the map only while
		// shd->pShadowMap is non-NULL, so gShadowsEnabled was false for every VC draw
		// and Common.hlsl's ComputeShadow() early-returned "fully lit" for the whole
		// cabin. An ordering consequence, not an absent feature.
		//
		// Re-rendering here rather than stashing the earlier map is deliberate: smap
		// carries the light's matrices as well as the texture, and with
		// ShadowMapMode >= 2 the loop above may have left those describing some OTHER
		// vessel while reusing the same per-LOD render target. Pairing the focus
		// vessel's texture with a stranger's matrices would project garbage. One extra
		// shadow pass - only while the camera is inside - refills the whole struct
		// consistently. (If the cost ever shows, the optimisation is to stash the
		// entire smap struct after the first call and prove the LOD was not reused.)
		//
		// The caster set needs no work whatsoever: RENDERPASS_SHADOWMAP forces
		// bCockpit = bVC = false in vVessel::Render, so what lands in the map is the
		// ship's EXTERIOR hull. Sunlight reaching the cabin is therefore cut by the
		// vessel's own skin and arrives through the window apertures - no window
		// identification, no per-addon heuristic, just geometry the client already
		// rasterises every frame. Transparent panes are handled too: the shadow pass
		// has an OIT variant (SHADER_SHADOWMAP_OIT).
		//
		// FIT THE MAP TO THE CABIN, NOT THE HULL. The exterior pass sizes its ortho box
		// to the vessel's bounding sphere - about 20 m across on a DeltaGlider, which at
		// ShadowMapSize 2048 is ~1 cm per texel. That is invisible on a fuselage seen from
		// fifty metres and glaring on a panel forty centimetres from your eye: it is what
		// makes VC shadow edges stair-step. The camera sits at the ORIGIN of this space
		// (vObject::mWorld carries the camera-relative translation), so centring there and
		// shrinking the box to a few metres puts the texels where the eye actually is -
		// ~4 mm each - while still comfortably containing the canopy frames and nose
		// structure that do the casting. Shrinking the box also shrinks gSHD[0], hence the
		// slope-scaled bias in ComputeShadow(), so edges tighten twice over. Geometry
		// further out stops casting into the cabin, which costs nothing: at 1 cm texels it
		// was not resolving anything meaningful anyway.
		// The box half-width and an on/off are exposed through gcCore::SetVCShadows (see
		// gcCore.cpp) so an addon can A/B the pass and tune the radius per vessel without
		// a rebuild. Both default to the values below, so a client nobody calls behaves
		// exactly as if the entry point did not exist.
		extern bool  g_gcVCShadows;
		extern float g_gcVCShadowRad;
		if (Config->ShadowMapMode >= 1 && g_gcVCShadows) {
			D3DXVECTOR3 ld  = sunLight.Dir;
			float       rad = min(vFocus->GetBoundingSphereRadius(), g_gcVCShadowRad);
			// Deliberately NOT the exact origin: RenderShadowMap divides by |pos| to pick
			// the LOD, and log2f(size / (inf * 1.5f)) would hand int(round(-inf)) straight
			// to the psShmRT[] index. A centimetre off-centre on a metres-wide box is free.
			D3DXVECTOR3 pos(0.0f, 0.0f, 0.01f);
			RenderShadowMap(pos, ld, rad);
		}

		// switch cockpit lights on, external-only lights off
		//
		if (bLocalLight) {
			ClearLocalLights();
			VESSEL *vessel = oapiGetFocusInterface();
			DWORD nemitter = vessel->LightEmitterCount();
			for (DWORD j = 0; j < nemitter; j++) {
				const LightEmitter *em = vessel->GetLightEmitter(j);
				if ((em->GetVisibility() == LightEmitter::VIS_COCKPIT) || (em->GetVisibility() == LightEmitter::VIS_ALWAYS))
					AddLocalLight(em, vFocus);
			}
		}

		pDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER,  0, 1.0f, 0L); // clear z-buffer
		double znear = Config->VCNearPlane;
		if (znear<0.01) znear=0.01;
		if (znear>1.0)  znear=1.0;
		OBJHANDLE hFocus = oapiGetFocusObject();
		SetCameraFrustumLimits(znear, oapiGetSize(hFocus)*2.0);
		// ORO patch (p): the shadow may take the material AMBIENT with it, but ONLY
		// in here. Raised for the cockpit draw and cleared immediately after, so every
		// exterior pass keeps stock shading no matter what the addon asked for.
		extern float g_gcVCShadowDep;
		// ORO patch (aa) round 3: THE AMBIENT BITE FOLLOWS THE SUN. Shadow depth 1 takes the
		// whole ambient share with the shadow - the hard orbital contrast he flies with - and
		// the fog and the storm only ever dimmed the SUN term, so a shadowed patch of panel
		// stayed as dark as ever while the lit patch kept its ambient: crisp at any fog
		// density (his rain + fog screenshots). The bite stands in for "the sun is the
		// dominant light", so it scales with the sun that actually reaches the cabin: the
		// storm collapse times the fog's sun transmittance. Clear air = his setting exactly.
		extern float g_gcStormLight;
		if (D3D9Effect::eVCShdDepth) D3D9Effect::FX->SetFloat(D3D9Effect::eVCShdDepth, g_gcVCShadowDep * (1.0f - g_gcStormLight) * g_oroFogSunCam);
		// ORO patch (s) extension (2026-08-23): the cockpit INTERIOR is dry - no
		// drop glint sparkling on the instrument panel, no wet sheen or darkening
		// on the cabin walls. Both ride gSurfWet (the glint also has its own gain),
		// so both uniforms are zeroed for the cockpit draw only and restored after:
		// vessels seen THROUGH the window keep their full wet look. Same bracket,
		// same reasoning as patch (p) above.
		extern float g_gcWetGlint;
		extern float g_gcSurfaceWet;
		if (D3D9Effect::eWetGlint) D3D9Effect::FX->SetFloat(D3D9Effect::eWetGlint, 0.0f);
		if (D3D9Effect::eSurfWet)  D3D9Effect::FX->SetFloat(D3D9Effect::eSurfWet, 0.0f);
		// ORO patch (ad): THE CABIN AT NIGHT. The addon's scale on the light that is not
		// there - the Launchpad ambient fill (vVessel::Render, the VC's own sun copy) and
		// the material emissive fill (Mesh.cpp, at the material push) - raised for the
		// cockpit draw only. Exterior passes, probes and mirrors never see it.
		extern float g_gcVCNight;
		extern float g_oroVCNightNow;
		// ORO patch (ae): the cabin keeps its own eye-fitted map - the cascades stay outside
		if (D3D9Effect::eCascAtlas) D3D9Effect::FX->SetVector(D3D9Effect::eCascAtlas, ptr(D3DXVECTOR4(0, 0, 0, 0)));
		g_oroVCNightNow = g_gcVCNight;
		OroFogInterior(true);     // ORO patch (aa): the cabin is clear air
		vFocus->Render(pDevice, true);
		OroFogInterior(false);
		g_oroVCNightNow = 1.0f;
		if (D3D9Effect::eCascAtlas && cascLive) D3D9Effect::FX->SetVector(D3D9Effect::eCascAtlas, &cascAtl);   // ORO patch (ae)
		if (D3D9Effect::eVCShdDepth) D3D9Effect::FX->SetFloat(D3D9Effect::eVCShdDepth, 0.0f);
		if (D3D9Effect::eWetGlint) D3D9Effect::FX->SetFloat(D3D9Effect::eWetGlint, g_gcWetGlint);
		if (D3D9Effect::eSurfWet)  D3D9Effect::FX->SetFloat(D3D9Effect::eSurfWet, g_gcSurfaceWet);
	}

	pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);


	// End Of Main Scene Rendering ---------------------------------------------
	//

	// -------------------------------------------------------------------------------------------------------
	// ORO patch (i): pre-resolve render proc. The complete scene (terrain, vessels,
	// transparency, VC) is down; the light-blur resolve and the HUD have not run yet, and
	// the top render target is still the main scene target - the fp16 offscreen buffer
	// when PostProcess is enabled. Addon art drawn here is therefore composited in HDR
	// space and participates in the threshold bloom + tonemap below, and always lands
	// under the HUD/2D overlay. Invoked with NULL matrices: the pad stays in its ortho
	// pixel-space defaults, the same contract as the HUD stages (see patch (a) note in
	// MakeRenderProcCall).
	// -------------------------------------------------------------------------------------------------------

	{
		D3D9Pad *pSketch = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);
		if (pSketch) {
			gc->MakeRenderProcCall(pSketch, RENDERPROC_PRE_RESOLVE, NULL, NULL);
			pSketch->EndDrawing(); // SKETCHPAD_2D_OVERLAY
		}
	}




	// -------------------------------------------------------------------------------------------------------
	// Copy Offscreen render target to backbuffer
	// -------------------------------------------------------------------------------------------------------


	if (pOffscreenTarget && pLightBlur) {

		int iGensPerFrame = pLightBlur->FindDefine("PassCount");

		D3DSURFACE_DESC colr;
		D3DSURFACE_DESC blur;

		psgBuffer[GBUF_BLUR]->GetDesc(&blur);
		psgBuffer[GBUF_COLOR]->GetDesc(&colr);

		D3DXVECTOR2 scr = D3DXVECTOR2(1.0f / float(colr.Width), 1.0f / float(colr.Height));
		D3DXVECTOR2 sbf = D3DXVECTOR2(1.0f / float(blur.Width), 1.0f / float(blur.Height));


		if (pLightBlur->IsOK())
		{
			float fInt = float(Config->GFXIntensity);
			float fDst = float(Config->GFXDistance);
			float fThr = float(Config->GFXThreshold);
			float fGam = float(Config->GFXGamma);

			// Grap a copy of a backbuffer
			pDevice->StretchRect(pOffscreenTarget, NULL, psgBuffer[GBUF_COLOR], NULL, D3DTEXF_POINT);

			pLightBlur->SetFloat("vSB", &sbf, sizeof(D3DXVECTOR2));
			pLightBlur->SetBool("bBlendIn", false);
			pLightBlur->SetBool("bBlur", false);

			pLightBlur->SetFloat("fIntensity", &fInt, sizeof(float));
			pLightBlur->SetFloat("fDistance", &fDst, sizeof(float));
			pLightBlur->SetFloat("fThreshold", &fThr, sizeof(float));
			pLightBlur->SetFloat("fGamma", &fGam, sizeof(float));	

			// -----------------------------------------------------
			pLightBlur->SetBool("bSample", true);
			pLightBlur->SetTextureNative("tBack", ptgBuffer[GBUF_COLOR], IPF_POINT | IPF_CLAMP);
			pLightBlur->SetOutputNative(0, psgBuffer[GBUF_BLUR]);

			if (!pLightBlur->Execute(true)) LogErr("pLightBlur Execute Failed");

			// -----------------------------------------------------
			pLightBlur->SetBool("bSample", false);
			pLightBlur->SetBool("bBlur", true);

			for (int i = 0; i < iGensPerFrame; i++) {

				pLightBlur->SetBool("bDir", false);
				pLightBlur->SetTextureNative("tBlur", ptgBuffer[GBUF_BLUR], IPF_POINT | IPF_CLAMP);
				pLightBlur->SetOutputNative(0, psgBuffer[GBUF_TEMP]);

				if (!pLightBlur->Execute(true)) LogErr("pLightBlur Execute Failed");

				pLightBlur->SetBool("bDir", true);
				pLightBlur->SetTextureNative("tBlur", ptgBuffer[GBUF_TEMP], IPF_POINT | IPF_CLAMP);
				pLightBlur->SetOutputNative(0, psgBuffer[GBUF_BLUR]);

				if (!pLightBlur->Execute(true)) LogErr("pLightBlur Execute Failed");
			}

			pLightBlur->SetBool("bBlendIn", true);
			pLightBlur->SetBool("bBlur", false);
			pLightBlur->SetTextureNative("tBack", ptgBuffer[GBUF_COLOR], IPF_LINEAR | IPF_CLAMP);
			pLightBlur->SetTextureNative("tBlur", ptgBuffer[GBUF_BLUR], IPF_LINEAR | IPF_CLAMP);
			pLightBlur->SetOutputNative(0, gc->GetBackBuffer());

			if (!pLightBlur->Execute(true)) LogErr("pLightBlur Execute Failed");
		}
		else {
			LogErr("pLightBlur is not o.k.");
		}
	}


	// -------------------------------------------------------------------------------------------------------
	// Render glares for the Sun and local lights
	// -------------------------------------------------------------------------------------------------------

	gc->PushRenderTarget(gc->GetBackBuffer(), gc->GetDepthStencil(), RENDERPASS_MAINSCENE);	
	RenderGlares();
	gc->PopRenderTargets();


	// -------------------------------------------------------------------------------------------------------
	// Render GDI Overlay to backbuffer directly
	// -------------------------------------------------------------------------------------------------------

	if (pGDIOverlay)
	{
		if (pGDIOverlay->IsOK())
		{
			gc->bGDIClear = true; // Must clear background before continuing drawing into overlay
			D3DXCOLOR clr(0x4080F0); // RGB ColorKey
			pGDIOverlay->SetTextureNative("tSrc", ptgBuffer[GBUF_GDI], IPF_POINT | IPF_CLAMP);
			pGDIOverlay->SetFloat("vColorKey", &clr, sizeof(clr));
			pGDIOverlay->SetOutputNative(0, gc->GetBackBuffer());
			if (!pGDIOverlay->Execute(true)) LogErr("pGDIOverlay Execute Failed");
		}
		else
		{
			LogErr("pGDIOverlay is not OK.");
		}
	}

	/*if (Camera.vNear) {
		D3D9DebugLog("vNear = %s", Camera.vNear->GetName());
		D3D9DebugLog("vProxy = %s", Camera.vProxy->GetName());
		D3D9DebugLog("vGravRef = %s", Camera.vGravRef->GetName());
	}*/

	// -------------------------------------------------------------------------------------------------------
	// Render HUD Overlay to backbuffer directly
	// -------------------------------------------------------------------------------------------------------


	gc->PushRenderTarget(gc->GetBackBuffer(), gc->GetDepthStencil(), RENDERPASS_MAINOVERLAY);	// Overlay

	pSketch = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);

	if (pSketch) {
		gc->MakeRenderProcCall(pSketch, RENDERPROC_HUD_1ST, NULL, NULL);
		pSketch->EndDrawing(); // SKETCHPAD_2D_OVERLAY
	}
	gc->ChromeDeferBegin();		// ORO patch (t): hold back the menu/info bar draws...
	gc->Render2DOverlay();
	pSketch = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);
	if (pSketch) {
		gc->MakeRenderProcCall(pSketch, RENDERPROC_HUD_2ND, NULL, NULL);
		pSketch->EndDrawing(); // SKETCHPAD_2D_OVERLAY
	}
	gc->ChromeDeferFlush();		// ...and put them back ON TOP of the addon overlay, so a
								// full-frame effect cannot smear Orbiter's own UI.
								// UNCONDITIONAL, outside the pSketch guard on purpose: a
								// frame with no pooled sketchpad must still get its bars.


	// Enable Freeze mode after the main scene is complete
	//
	if (bFreezeEnable) bFreeze = true;

	
	// -------------------------------------------------------------------------------------------------------
	// EnvMap Debugger  TODO: Should be allowed to visualize other maps as well, not just index 0
	// -------------------------------------------------------------------------------------------------------

	if (DebugControls::IsActive()) {
		
		int sel = DebugControls::GetSelectedEnvMap();

		switch (sel) {
		case 1:		case 2:		case 3:		case 4:
		case 5:
			VisualizeCubeMap(vFocus->GetEnvMap(ENVMAP_MAIN), sel - 1);
			break;
		case 6:
			VisualizeCubeMap(vFocus->GetIrradEnv(), 0);
			break;
		case 7:
			VisualizeCubeMap(pIrradTemp, 0);
			break;
		case 8:
			if (pShdMap) {
				pSketch = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);
				pSketch->CopyRectNative(pShdMap, NULL, 0, 0);
				pSketch->EndDrawing();
			}
			break;
		case 9:
			if (vFocus->GetIrradianceMap()) {
				pSketch = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);
				pSketch->CopyRectNative(vFocus->GetIrradianceMap(), NULL, 0, 0);
				pSketch->EndDrawing();
			}
			break;
		case 10:
			if (ptgBuffer[GBUF_BLUR]) {
				pSketch = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);
				pSketch->CopyRectNative(ptgBuffer[GBUF_BLUR], NULL, 0, 0);
				pSketch->EndDrawing();
			}
			break;
		case 11:
			if (ptgBuffer[GBUF_DEPTH]) {
				if (pVisDepth) {
					if (pVisDepth->IsOK()) {
						pVisDepth->Activate("PSDepth");
						pVisDepth->SetTextureNative("tBack", ptgBuffer[GBUF_DEPTH], IPF_POINT | IPF_CLAMP);
						pVisDepth->SetOutputNative(0, gc->GetBackBuffer());
						pVisDepth->Execute(true);
					}
				}
			}
			break;
		case 12:
			if (ptgBuffer[GBUF_DEPTH]) {
				if (pVisDepth) {
					if (pVisDepth->IsOK()) {
						pVisDepth->Activate("PSNormal");
						pVisDepth->SetTextureNative("tBack", ptgBuffer[GBUF_DEPTH], IPF_POINT | IPF_CLAMP);
						pVisDepth->SetOutputNative(0, gc->GetBackBuffer());
						pVisDepth->Execute(true);
					}
				}
			}
			break;
		case 13:
			if (pLocalResults) {
				pSketch = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);
				pSketch->SetBlendState(Sketchpad::BlendState::FILTER_POINT);
				pSketch->StretchRectNative(pLocalResults, NULL, ptr(_RECT(0, 0, viewW, 10)));
				pSketch->SetBlendState(Sketchpad::BlendState::FILTER_LINEAR);
				pSketch->EndDrawing();
			}
			break;
		case 14:
			if (Camera.vNear) {
				auto ptE = Camera.vNear->GetEclipse();
				if (ptE) {
					pSketch = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);
					pSketch->SetBlendState(Sketchpad::BlendState::FILTER_POINT);
					pSketch->StretchRectNative(ptE, NULL, ptr(_RECT(0, 0, viewW, 10)));
					pSketch->SetBlendState(Sketchpad::BlendState::FILTER_LINEAR);
					pSketch->EndDrawing();
				}
			}
			break;
		default:
			break;
		}
	}


	if (AtmoControls::Visualize())
	{
		vPlanet* vP = GetCameraProxyVisual();
		pSketch = GetPooledSketchpad(SKETCHPAD_2D_OVERLAY);
		pSketch->SetBlendState(Sketchpad::COPY);
		int x = 0, y = ViewH();

		LPDIRECT3DTEXTURE9 pTab = vP->GetScatterTable(RAY_LAND);
		D3DSURFACE_DESC desc;
		if (pTab) {
			pTab->GetLevelDesc(0, &desc);
			pSketch->StretchRectNative(pTab, NULL, ptr(_R(0, y - desc.Height, desc.Width, y)));
			y -= (desc.Height + 5);
		}
		pTab = vP->GetScatterTable(MIE_LAND);
		if (pTab) {
			pTab->GetLevelDesc(0, &desc);
			pSketch->StretchRectNative(pTab, NULL, ptr(_R(0, y - desc.Height, desc.Width, y)));
			y -= (desc.Height + 5);
		}
		pTab = vP->GetScatterTable(ATN_LAND);
		if (pTab) {
			pTab->GetLevelDesc(0, &desc);
			pSketch->StretchRectNative(pTab, NULL, ptr(_R(0, y - desc.Height, desc.Width, y)));
			y -= (desc.Height + 5);
		}
		for (int i=0;i<9;i++)
		{
			if (i == RAY_LAND || i == MIE_LAND || i == ATN_LAND) continue;
			pTab = vP->GetScatterTable(i);
			if (!pTab) continue;
			pTab->GetLevelDesc(0, &desc);
			pSketch->CopyRectNative(pTab, NULL, x, y - desc.Height);
			x += desc.Width + 5;
		}
		pSketch->SetBlendState(Sketchpad::ALPHABLEND);
		pSketch->EndDrawing();
	}


	// -------------------------------------------------------------------------------------------------------
	// Draw Debug String on a bottom of the screen
	// -------------------------------------------------------------------------------------------------------

	const char* dbgString = oapiDebugString();
	int len = lstrlen(dbgString);

	if (len>0 || !D3D9DebugQueue.empty()) {

		pSketch = GetPooledSketchpad(SKETCHPAD_DEBUG_TEXT);

		DWORD height = Config->DebugFontSize;

		// Display Orbiter's debug string
		if (len > 0) {
			DWORD width = pSketch->GetTextWidth(dbgString, len);
			pSketch->Rectangle(-1, viewH - height - 1, width + 4, viewH);
			pSketch->Text(2, viewH - 2, dbgString, len);
		}

		DWORD pos = viewH;

		// Display additional debug string queue
		//
		while (!D3D9DebugQueue.empty()) {
			pos -= (height * 3) / 2;
			std::string str = D3D9DebugQueue.front();
			len = lstrlen(str.c_str());
			DWORD width = pSketch->GetTextWidth(str.c_str(), len);
			pSketch->Rectangle(-1, pos - height - 1, width + 4, pos);
			pSketch->Text(2, pos - 2, str.c_str(), len);
			D3D9DebugQueue.pop();
		}

		pSketch->EndDrawing(); // SKETCHPAD_DEBUG_TEXT
	}


	gc->PopRenderTargets();	// Overlay
	gc->PopRenderTargets();	// Main Scene

	gc->HackFriendlyHack();
	gc->EndScene();

	dwTurn++;
}



// ===========================================================================================
//
void Scene::RenderVesselMarker(vVessel *vV, D3D9Pad *pSketch)
{
	DWORD mkrmode = *(DWORD*)gc->GetConfigParam(CFGPRM_SURFMARKERFLAG);
	if ((mkrmode & (MKR_ENABLE | MKR_VMARK)) == (MKR_ENABLE | MKR_VMARK)) {
		RenderObjectMarker(pSketch, vV->GlobalPos(), std::string(vV->GetName()), std::string(), 0, viewH / 80);
	}
}


// ===========================================================================================
// Lens flare code (SolarLiner)
//
Scene::SUNVISPARAMS Scene::GetSunScreenVisualState()
{
	SUNVISPARAMS result = SUNVISPARAMS();

	VECTOR3 cam = GetCameraGPos();
	VECTOR3 sunGPos;
	oapiGetGlobalPos(oapiGetGbodyByIndex(0), &sunGPos);
	sunGPos -= cam;

	DWORD w, h;
	oapiGetViewportSize(&w, &h);

	const LPD3DXMATRIX pVP = GetProjectionViewMatrix();
	D3DXVECTOR4 pos;
	D3DXVECTOR4 sun = D3DXVECTOR4(float(sunGPos.x), float(sunGPos.y), float(sunGPos.z), 1.0f);
	D3DXVec4Transform(&pos, &sun, pVP);
	result.brightness = saturate(pos.z);

	D3DXVECTOR2 scrPos = D3DXVECTOR2(pos.x, pos.y);
	scrPos /= pos.w;
	scrPos *= 0.5f;
	scrPos.x *= w / h;

	result.position = scrPos;
	result.position.x *= 1.8f;

	short xpos = short((scrPos.x + 0.5f) * w);
	short ypos = short((1.0f - (scrPos.y + 0.5f)) * h);

	D3D9Pick pick = PickScene(xpos, ypos);

	if (pick.pMesh != NULL)
	{
		DWORD matIndex = pick.pMesh->GetMeshGroupMaterialIdx(pick.group);
		D3D9MatExt material;
		pick.pMesh->GetMaterial(&material, matIndex);
		D3DXCOLOR surfCol(material.Diffuse.x, material.Diffuse.y, material.Diffuse.z, material.Diffuse.w);

		result.visible = (surfCol.a != 1.0f);
		if (result.visible)
		{
			D3DXCOLOR color = D3DXCOLOR(surfCol.r*surfCol.a, surfCol.g*surfCol.a, surfCol.b*surfCol.a, 1.0f);
			color += GetSunDiffColor() * (1 - surfCol.a);

			result.color = color;
		}
		return result;
	}
	result.visible = true;
	result.color = GetSunDiffColor();

	return result;
}


// ===========================================================================================
// Lens flare code (SolarLiner)
//
D3DXCOLOR Scene::GetSunDiffColor()
{
	vPlanet *vP = Camera.vProxy;

	D3DXVECTOR3 _one(1, 1, 1);
	VECTOR3 GS, GP, GO;
	oapiCameraGlobalPos(&GO);

	OBJHANDLE hS = oapiGetGbodyByIndex(0);	// the central star
	OBJHANDLE hP = vP->Object();			// the planet object
	oapiGetGlobalPos(hS, &GS);				// sun position
	oapiGetGlobalPos(hP, &GP);				// planet position

	VECTOR3 S = GS - GO;						// sun's position from object
	VECTOR3 P = GO - GP;

	double s = length(S);

	float pwr = 1.0f;

	if (hP == hS) return GetSun()->Color;

	double r = length(P);
	double pres = 1000.0;
	double size = oapiGetSize(hP) + vP->GetMinElevation();
	double grav = oapiGetMass(hP) * 6.67259e-11 / (size*size);

	float aalt = 1.0f;
	//float amb0 = 0.0f;
	float disp = 0.0f;
	float amb = 0.0f;
	float aq = 0.342f;
	float ae = 0.242f;
	float al = 0.0f;
	float k = float(sqrt(r*r - size*size));		// Horizon distance
	float alt = float(r - size);
	float rs = float(oapiGetSize(hS) / s);
	float ac = float(-dotp(S, P) / (r*s));					// sun elevation

															// Avoid some fault conditions
	if (alt<0) alt = 0, k = 1e3, size = r;

	if (ac>1.0f) ac = 1.0f; if (ac<-1.0f) ac = -1.0f;

	ac = acos(ac) - asin(float(size / r));

	if (ac>1.39f)  ac = 1.39f;
	if (ac<-1.39f) ac = -1.39f;

	float h = tan(ac);

	const ATMCONST *atm = (oapiGetObjectType(hP) == OBJTP_PLANET ? oapiGetPlanetAtmConstants(hP) : NULL);

	if (atm) {
		aalt = float(atm->p0 * log(atm->p0 / pres) / (atm->rho0*grav));
		//amb0 = float(min(0.7, log(atm->rho0 + 1.0f)*0.4));
		disp = float(max(0.02, min(0.9, log(atm->rho0 + 1.0))));
	}

	if (alt>10e3f) al = aalt / k;
	else           al = 0.173f;

	D3DXVECTOR3 lcol(1, 1, 1);
	//D3DXVECTOR3 r0 = _one - D3DXVECTOR3(0.65f, 0.75f, 1.0f) * disp;
	D3DXVECTOR3 r0 = _one - D3DXVECTOR3(1.15f, 1.65f, 2.35f) * disp;

	if (atm) {
		float x = sqrt(saturate(h / al));
		float y = sqrt(saturate((h + rs) / (2.0f*rs)));
		lcol = (r0 + (_one - r0) * x) * y;
	}
	else {
		lcol = r0 * saturate((h + rs) / (2.0f*rs));
	}

	return D3DXCOLOR(lcol.x, lcol.y, lcol.z, 1);
}



// ===========================================================================================
//
// ===========================================================================================
// ORO patch (w): the focus vessel's ATTACHMENT ASSEMBLY - climb to the root of the
// attachment tree, then collect every visual below it. Berthed payloads ride their
// carrier's planet-shine shadow map this way, and focusing the payload instead of
// the carrier changes nothing about the lighting (as it must not).
//
static void OroGatherAssembly(Scene* scn, vVessel* vRoot, std::list<vVessel*>& out)
{
	VESSEL* v = vRoot->GetInterface();
	if (!v) return;
	for (int guard = 0; guard < 8; guard++) {
		VESSEL* parent = NULL;
		DWORD n = v->AttachmentCount(true);
		for (DWORD i = 0; i < n; i++) {
			OBJHANDLE hP = v->GetAttachmentStatus(v->GetAttachmentHandle(true, i));
			if (hP) { parent = oapiGetVesselInterface(hP); break; }
		}
		if (!parent) break;
		v = parent;
	}
	std::list<VESSEL*> open; open.push_back(v);
	std::set<VESSEL*> seen; seen.insert(v);
	while (!open.empty()) {
		VESSEL* cur = open.front(); open.pop_front();
		vObject* vo = scn->GetVisObject(cur->GetHandle());
		if (vo && vo->IsActive()) out.push_back((vVessel*)vo);
		DWORD n = cur->AttachmentCount(false);
		for (DWORD i = 0; i < n && seen.size() < 16; i++) {
			OBJHANDLE hC = cur->GetAttachmentStatus(cur->GetAttachmentHandle(false, i));
			if (!hC) continue;
			VESSEL* c = oapiGetVesselInterface(hC);
			if (c && !seen.count(c)) { seen.insert(c); open.push_back(c); }
		}
	}
}

// ===========================================================================================
// ORO patch (w): the planet-shine map's own target (full ShadowMapSize, R32F like
// the sun LODs). Lazy - never allocated under the three stock reflection modes.
//
bool Scene::EnsurePShnTarget()
{
	if (ptPShn) return true;
	int size = Config->ShadowMapSize;
	if (pDevice->CreateTexture(size, size, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &ptPShn, NULL) != S_OK) { ptPShn = NULL; return false; }
	if (ptPShn->GetSurfaceLevel(0, &psPShn) != S_OK) { SAFE_RELEASE(ptPShn); psPShn = NULL; return false; }
	return true;
}

// ===========================================================================================
// ORO patch (w) part 2: the sun-map copy target - same recipe.
//
bool Scene::EnsureSunCpyTarget()
{
	if (ptSunCpy) return true;
	int size = Config->ShadowMapSize;
	if (pDevice->CreateTexture(size, size, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &ptSunCpy, NULL) != S_OK) { ptSunCpy = NULL; return false; }
	if (ptSunCpy->GetSurfaceLevel(0, &psSunCpy) != S_OK) { SAFE_RELEASE(ptSunCpy); psSunCpy = NULL; return false; }
	return true;
}

// ===========================================================================================
//
int Scene::RenderShadowMap(D3DXVECTOR3 &pos, D3DXVECTOR3 &ld, float rad, bool bInternal, bool bListExists)
{
	rad *= 1.02f;

	smap.pos = pos;
	smap.ld = ld;
	smap.rad = rad;

	float mnd =  1e16f;
	float mxd = -1e16f;
	float rsmax = 0.0f;
	float tanap = float(GetTanAp());
	float viewh = float(ViewH());

	if (!bListExists) {

		// If the list doesn't exists then create it...
		SmapRenderList.clear();

		// browse through vessels to find shadowers --------------------------
		//
		for (VOBJREC *pv = vobjFirst; pv; pv = pv->next) {
			if (pv->type != OBJTP_VESSEL) continue;
			vVessel *vV = (vVessel *)pv->vobj;
			if (!vV->IsActive()) continue;
			if (vV->IntersectShadowVolume()) {
				SmapRenderList.push_back(vV);
				vV->GetMinMaxLightDist(&mnd, &mxd);
			}
		}

		// Compute shadow lod
		rsmax = viewh * rad / (tanap * D3DXVec3Length(&pos));
	}


	if (SmapRenderList.size() == 0) return -1;	// The list is empty, Nothing to render


	if (bListExists) {

		for (auto vV : SmapRenderList)
		{
			// Get shadow min-max distances
			vV->GetMinMaxLightDist(&mnd, &mxd);

			// Compute shadow lod
			D3DXVECTOR3 bspos = vV->GetBoundingSpherePosDX();
			float rs = viewh * rad / (tanap * D3DXVec3Length(&bspos));
			if (rs > rsmax) rsmax = rs;
		}
	}

	smap.depth = (mxd - mnd) + 10.0f;

	D3DXMatrixOrthoOffCenterRH(&smap.mProj, -rad, rad, rad, -rad, 50.0f, 50.0f + smap.depth);

	smap.dist = mnd - 55.0f;

	D3DXVECTOR3 lp = pos + ld * smap.dist;

	D3DXMatrixLookAtRH(&smap.mView, &lp, &pos, ptr(D3DXVECTOR3(0, 1, 0)));
	D3DXMatrixMultiply(&smap.mViewProj, &smap.mView, &smap.mProj);

	float lod = log2f(float(Config->ShadowMapSize) / (rsmax*1.5f));

	smap.lod = min(int(round(lod)), SHM_LOD_COUNT - 1);
	smap.lod = max(smap.lod, 0);
	smap.size = Config->ShadowMapSize >> smap.lod;

	gc->PushRenderTarget(psShmRT[smap.lod], psShmDS[smap.lod], RENDERPASS_SHADOWMAP);

	// Clear the viewport
	HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0L));


	// render the vessel objects --------------------------------
	//
	BeginPass(RENDERPASS_SHADOWMAP);

	while(SmapRenderList.size()>0) {
		SmapRenderList.front()->Render(pDevice, bInternal);
		SmapRenderList.pop_front();
	}

	PopPass();

	gc->PopRenderTargets();

	smap.pShadowMap = ptShmRT[smap.lod];

	return smap.lod;
}


// ===========================================================================================
// ORO patch (z3): one perspective shadow map from the frame's strongest shadow-casting
// SPOT light. Stock local lights have NO occlusion term anywhere - a spotlight beam
// passes through a hangar and a vessel standing in the beam casts nothing - which is
// the (w) finding (planet glow had no shadow) told for the light family. v1 receiver
// is the TERRAIN (NewPlanet.hlsl), which covers both flagship complaints: the beam
// carved by a building onto the ground, and a vessel casting onto the apron. POINT
// lights stay stock (a cube map is a different cost class).
//
// ORO patch (ae): THE CASCADED SUN SHADOW ATLAS - see Scene.h. One pass per frame,
// before the planets render (the terrain consumes it this frame; the mesh family
// reads the same uniforms all frame long). The registry's tiles are last frame's,
// rebuilt in the planet's frame, so "one frame stale" is exact (rounds 3-4 of (ab)).
//
// The slot table, in half-cascade units (cascSize / 2 pixels) on the 6 x 4 grid of the
// 3 x 2 atlas (round 7): x, y, size.
static const struct { int x, y, size; } ORO_CASC_SLOT[9] = {
	{ 0, 2, 1 },   // 0: the focus vessel's box
	{ 0, 0, 2 },   // 1: 0 .. 50 m
	{ 2, 0, 2 },   // 2: 50 .. 450 m
	{ 4, 0, 2 },   // 3: 450 .. 2000 m (full-size since round 7 - the base at telephoto range)
	{ 1, 2, 1 },   // 4: 2000 .. 8000 m
	{ 2, 2, 1 },   // 5: 8000 m .. far
	{ 3, 2, 1 },   // 6: the nearest other vessel's box
	{ 4, 2, 1 },   // 7: the second nearest
	{ 5, 2, 1 },   // 8: the third        (row 3, six cells, is spare - the light atlas, later)
};

bool Scene::FitCascade(int i, const D3DXVECTOR3& c0, float r, float margin, float reach, bool snap)
{
	D3DXVECTOR3 c = c0;
	const D3DXVECTOR3 ld = cascL;                            // the direction the light travels
	const D3DXVECTOR3 up = (fabs(ld.y) < 0.9f) ? D3DXVECTOR3(0, 1, 0) : D3DXVECTOR3(1, 0, 0);
	const int   q = cascSize / 2;
	const int   slotSize = ORO_CASC_SLOT[i].size * q;
	const float W = 3.0f * (float)cascSize, Hh = 2.0f * (float)cascSize;

	// a snapped slot's radius is QUANTISED (eighth-octave steps): a frustum slice's
	// bounding sphere follows the field of view, and a zoom would otherwise rescale the
	// lattice continuously - every edge re-rasterised every frame of the zoom
	if (snap) r = exp2f(ceilf(log2f(max(r, 1.0f)) * 8.0f) / 8.0f);
	const float texel = 2.0f * r / (float)slotSize;

	// TEXEL SNAP, in the planet's frame, about an anchor NEAR the camera (Scene.h, round
	// 5): the light-space x/y of the box centre are quantised to whole texels measured
	// from this slot's planet-local anchor, and the anchor itself walks to the camera
	// every frame in whole lattice steps, so the lattice keeps its phase (no pop) and
	// the rotation of the light in the planet's frame turns it about a point at the
	// camera. Doubles throughout - the planet-local coordinates are ~1e6 m.
	if (snap && Camera.hObj_proxy) {
		const VECTOR3 ug = _V(cascU.x, cascU.y, cascU.z), vg = _V(cascV.x, cascV.y, cascV.z), lg = _V(ld.x, ld.y, ld.z);
		MATRIX3 R; oapiGetRotationMatrix(Camera.hObj_proxy, &R);
		VECTOR3 P; oapiGetGlobalPos(Camera.hObj_proxy, &P);
		const VECTOR3 camL = tmul(R, Camera.pos - P);
		if (cascAnchPlanet != Camera.hObj_proxy) { cascAnchPlanet = Camera.hObj_proxy; for (int k = 0; k < 9; k++) cascAnchOK[k] = false; }
		if (!cascAnchOK[i] || cascAnchTexel[i] != texel) {
			cascAnch[i] = camL; cascAnchTexel[i] = texel; cascAnchOK[i] = true;
		}
		else {
			const VECTOR3 uL = tmul(R, ug), vL = tmul(R, vg), lL = tmul(R, lg);
			const VECTOR3 dL = camL - cascAnch[i];
			const double  nu = floor(dotp(dL, uL) / texel + 0.5), nv = floor(dotp(dL, vL) / texel + 0.5);
			cascAnch[i] = cascAnch[i] + uL * (nu * texel) + vL * (nv * texel) + lL * dotp(dL, lL);
		}
		const VECTOR3 g = _V(c.x, c.y, c.z) + Camera.pos - (P + mul(R, cascAnch[i]));
		const double su = dotp(g, ug);
		const double sv = dotp(g, vg);
		const double du = floor(su / texel) * texel - su;
		const double dv = floor(sv / texel) * texel - sv;
		c += cascU * (float)du + cascV * (float)dv;
		if (i == 1) cascDbgAnchD = (float)length(camL - cascAnch[i]);
	}

	const D3DXVECTOR3 eye = c - ld * (r + margin);
	D3DXMATRIX mView, mProj;
	D3DXMatrixLookAtRH(&mView, &eye, &c, &up);
	// THE DEPTH WINDOW (round 10, his KSC sunset: both hulls' ground shadows cut along a
	// straight line perpendicular to the sun, appearing as it sank). The eye sits
	// (r + margin) toward the light from the centre, so the window runs from
	// (margin + r - 1) BEFORE the centre to (reach + r) PAST it. A camera cascade wants
	// the margin (tall casters outside its slice, toward the sun) and no reach (its
	// receivers are inside the slice). A hull box wants the OPPOSITE: nothing casts
	// into it but its own hull, and its receivers - the ground shadow - lie PAST the
	// hull along the light, h / sin(elevation) beyond it. Round 5 put the hull boxes'
	// kilometre on the margin side and left them r + 1 m of reach: a DG's shadow was
	// cut 14 m along the light past the hull, which a sun under 15 degrees reaches.
	const float zn = 1.0f, zf = margin + 2.0f * r + reach;
	D3DXMatrixOrthoOffCenterRH(&mProj, -r, r, r, -r, zn, zf);
	casc[i].mView = mView;
	D3DXMatrixMultiply(&casc[i].mVP, &mView, &mProj);
	casc[i].c = c;
	casc[i].r = r;
	casc[i].zf = zf;
	casc[i].texel = texel;
	casc[i].range = zf - zn;
	casc[i].px = ORO_CASC_SLOT[i].x * q;
	casc[i].py = ORO_CASC_SLOT[i].y * q;
	casc[i].size = slotSize;
	casc[i].uvx = (float)casc[i].px / W;
	casc[i].uvy = (float)casc[i].py / Hh;
	casc[i].scale = (float)slotSize / W;
	// the receiver's compact form (round 7): the box in the shared light basis. The view
	// x/y of a point are dot(p - c, U/V) (U, V lie across the light), its depth along
	// the light dot(p - eye, L), and the ortho maps them to sp = (x/r, y/r) * 0.5 + 0.5
	// (the y flip in the projection and the one in the receiver cancel) and to
	// z = (depth - zn) / (zf - zn). Same numbers the casters rasterise through mVP.
	casc[i].A = D3DXVECTOR4(D3DXVec3Dot(&c, &cascU), D3DXVec3Dot(&c, &cascV), D3DXVec3Dot(&eye, &ld) + zn, 1.0f / (zf - zn));
	casc[i].B = D3DXVECTOR4(casc[i].uvx, casc[i].uvy, casc[i].scale, texel);
	casc[i].live = true;
	return true;
}

void Scene::RenderCascadeCasters(int i)
{
	const CASCADE& Cc = casc[i];

	// a hull box - the focus vessel's (0) or one of the three nearest others' (6-8):
	// that hull alone, in its own vessel-anchored box (Scene.h, rounds 5 and 7)
	if (i == 0 || i >= 6) {
		vVessel* v = (i == 0) ? vFocus : cascVes[i - 6];
		if (v && v->IsActive()) v->Render(pDevice, false);
		return;
	}

	// vessels: every active one WITHOUT a box of its own whose sphere meets this box
	for (VOBJREC* pv = vobjFirst; pv; pv = pv->next) {
		if (pv->type != OBJTP_VESSEL) continue;
		vVessel* vV = (vVessel*)pv->vobj;
		if (!vV->IsActive() || vV == vFocus || vV == cascVes[0] || vV == cascVes[1] || vV == cascVes[2]) continue;
		D3DXVECTOR3 bs = vV->GetBoundingSpherePosDX();
		const float bsr = vV->GetBoundingSphereRadius();
		D3DXVECTOR3 l; D3DXVec3TransformCoord(&l, &bs, &Cc.mView);
		if (fabs(l.x) > Cc.r + bsr || fabs(l.y) > Cc.r + bsr) continue;
		if (-l.z + bsr < 0.0f || -l.z - bsr > Cc.zf) continue;          // RH view: forward is -z
		vV->Render(pDevice, false);
	}

	// base structures (the above-shadow set): opt 0 = the shadow-map technique
	if (Camera.vProxy) Camera.vProxy->RenderBaseDepth((LPD3DXMATRIX)&Cc.mVP, 0);

	// terrain tiles: the registry, each rebuilt in the planet's frame, culled to the box
	if (pTileShd) {
		pTileShd->ClearTextures();
		pTileShd->Setup(pPatchVertexDecl, true, 0);
		pTileShd->SetVSConstants("mTileShdVP", (void*)&Cc.mVP, sizeof(D3DXMATRIX));
		HANDLE hW = pTileShd->GetVSHandle("mTileShdW");
		pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		for (auto& t : LclTiles) {
			if (dwFrameId - t.stamp > 1) continue;
			D3DXMATRIX mW; OroTileFromPlanet(t, Camera.pos, mW);
			D3DXVECTOR3 bs(t.bs.x + mW._41 - t.mW._41, t.bs.y + mW._42 - t.mW._42, t.bs.z + mW._43 - t.mW._43);
			D3DXVECTOR3 l; D3DXVec3TransformCoord(&l, &bs, &Cc.mView);
			if (fabs(l.x) > Cc.r + t.bsRad || fabs(l.y) > Cc.r + t.bsRad) continue;
			if (-l.z + t.bsRad < 0.0f || -l.z - t.bsRad > Cc.zf) continue;
			pTileShd->SetVSConstants(hW, (void*)&mW, sizeof(D3DXMATRIX));
			pDevice->SetStreamSource(0, t.pVB, 0, sizeof(VERTEX_2TEX));
			pDevice->SetIndices(t.pIB);
			pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, t.nv, 0, t.nf);
		}
		pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	}
}

void Scene::GetCascadeConstants(D3DXVECTOR4* basis, D3DXVECTOR4* A, D3DXVECTOR4* tx, D3DXVECTOR4* split, D3DXVECTOR4* atl, bool live) const
{
	basis[0] = D3DXVECTOR4(cascU.x, cascU.y, cascU.z, (float)Config->ShadowDebug);   // .w: the terrain's copy of the debug mode
	basis[1] = D3DXVECTOR4(cascV.x, cascV.y, cascV.z, (float)Config->ShadowCascadeSoft);   // .w: the soft-far-shadows switch (the terrain's tent)
	basis[2] = D3DXVECTOR4(cascL.x, cascL.y, cascL.z, 0.0f);
	// the texels packed four to a register (slot k = tx[k / 4][k % 4]); the slots' uv
	// rectangles are fixed by the atlas layout and computed in the shaders - a pushed
	// rect per slot was the register that put the vessel family over ps_3_0's 224
	float t[12] = { 0 };
	for (int i = 0; i < 9; i++) {
		A[i] = D3DXVECTOR4(0, 0, 0, 0);
		if (live && casc[i].live) { A[i] = casc[i].A; t[i] = casc[i].texel; }
	}
	for (int k = 0; k < 3; k++) tx[k] = D3DXVECTOR4(t[4 * k], t[4 * k + 1], t[4 * k + 2], t[4 * k + 3]);
	*split = live ? D3DXVECTOR4(cascSplit[0], cascSplit[1], cascSplit[2], cascSplit[3]) : D3DXVECTOR4(0, 0, 0, 0);
	*atl   = live ? cascAtl : D3DXVECTOR4(0, 0, 0, 0);
}

void Scene::RenderCascadeShadows()
{
	cascLive = false;
	cascLclLive = false;
	for (int i = 0; i < 9; i++) casc[i].live = false;
	cascVes[0] = cascVes[1] = cascVes[2] = NULL;
	if (!ptCasc || !psCasc || !psCascDS || !pTileShd) return;
	if (Config->TerrainShadowing != 3) return;
	if (!Camera.vProxy) return;

	// the light basis every slot shares (round 7): U, V across the light, L along it -
	// the axes any of the slots' LookAt would produce
	{
		const D3DXVECTOR3 ld = sunLight.Dir;
		const D3DXVECTOR3 up = (fabs(ld.y) < 0.9f) ? D3DXVECTOR3(0, 1, 0) : D3DXVECTOR3(1, 0, 0);
		D3DXMATRIX mB; D3DXVECTOR3 e0 = -ld * 10.0f, a0(0, 0, 0);
		D3DXMatrixLookAtRH(&mB, &e0, &a0, &up);
		cascU = D3DXVECTOR3(mB._11, mB._21, mB._31);
		cascV = D3DXVECTOR3(mB._12, mB._22, mB._32);
		cascL = ld;
	}

	// the splits: two fine near slices, three coarser far ones out to the far limit
	const float farD = (float)max(1000.0, min(60000.0, Config->ShadowCascadeFar));
	const float fs[5] = { min(50.0f, farD), min(450.0f, farD), min(2000.0f, farD), min(8000.0f, farD), farD };
	for (int k = 0; k < 4; k++) cascSplit[k] = fs[k];
	cascAtl = D3DXVECTOR4(1.0f / (3.0f * (float)cascSize), 1.0f / (2.0f * (float)cascSize), 1.0f, farD);   // 1/W, 1/H, on, far

	bool any = false;
	// slot 0: the focus vessel's own box (stock's fit), ALWAYS (round 1 gated it to 600 m
	// and the vessel lost its shadow there), VESSEL-ANCHORED (round 4 snapped it, wrongly:
	// the round-3 flicker was the planet-centre lattice of the OTHER slots, Scene.h round
	// 5 - a box that rides the hull rasterises a parked hull identically every frame),
	// and holding the focus vessel ALONE (RenderCascadeCasters). Its depth window reaches
	// 1000 m PAST the hull along the light (round 10 - round 5 had put that kilometre on
	// the sun's side) so a low sun's long shadow stays inside it down to ~0.2 deg.
	if (vFocus && vFocus->IsActive()) {
		D3DXVECTOR3 c = vFocus->GetBoundingSpherePosDX();
		const float r = vFocus->GetBoundingSphereRadius() * 1.02f;
		if (D3DXVec3Length(&c) < farD) any |= FitCascade(0, c, r, 2.0f, 1000.0f, false);
	}
	// slots 6-8: the three nearest OTHER vessels, each in a box of its own (round 7) -
	// the same treatment the focus vessel gets, so a parked hull beside it is not the
	// pixelated one in the picture. Nearest by the camera's distance to the hull's
	// sphere, within 6 km (beyond that a hull is pixels); station-sized hulls stay in
	// the cascades (a 600 m box at 1024 texels would be coarser than slot 2).
	{
		float best[3] = { 1e30f, 1e30f, 1e30f };
		const float reach = min(farD, 6000.0f);
		for (VOBJREC* pv = vobjFirst; pv; pv = pv->next) {
			if (pv->type != OBJTP_VESSEL) continue;
			vVessel* vV = (vVessel*)pv->vobj;
			if (!vV->IsActive() || vV == vFocus) continue;
			D3DXVECTOR3 bs = vV->GetBoundingSpherePosDX();
			const float bsr = vV->GetBoundingSphereRadius();
			if (bsr <= 0.0f || bsr > 300.0f) continue;
			const float sc = D3DXVec3Length(&bs) - bsr;
			if (sc > reach) continue;
			int at = -1;
			for (int k = 0; k < 3; k++) if (sc < best[k]) { at = k; break; }
			if (at < 0) continue;
			for (int k = 2; k > at; k--) { best[k] = best[k - 1]; cascVes[k] = cascVes[k - 1]; }
			best[at] = sc; cascVes[at] = vV;
		}
		for (int k = 0; k < 3; k++) {
			if (!cascVes[k]) break;
			D3DXVECTOR3 c = cascVes[k]->GetBoundingSpherePosDX();
			const float r = cascVes[k]->GetBoundingSphereRadius() * 1.02f;
			any |= FitCascade(6 + k, c, r, 2.0f, 1000.0f, false);
		}
	}
	// slots 1-5: camera frustum slices as bounding spheres, snapped in the planet's frame
	{
		const float tanAp = (float)GetTanAp();
		const float asp   = GetCameraAspect();
		const float k     = tanAp * sqrtf(1.0f + asp * asp);
		float n = 0.5f;
		for (int i = 0; i < 5; i++) {
			const float f = fs[i];
			if (f <= n + 1.0f) { n = f; continue; }              // an empty slice past the far limit
			const float zc = 0.5f * (n + f);
			const float dz = f - zc;
			const float r  = sqrtf(dz * dz + f * f * k * k);
			const D3DXVECTOR3 c = Camera.z * zc;
			any |= FitCascade(1 + i, c, r, max(500.0f, r), 1.0f, true);
			n = f;
		}
	}
	if (!any) return;

	// the casters, one slot at a time, into one target
	SHADOWMAPPARAM save = smap;
	gc->PushRenderTarget(psCasc, psCascDS, RENDERPASS_SHADOWMAP);
	// clear the three rows the slots occupy - the spare fourth is never read except the
	// one cell the local map is copied INTO (a quarter of the target, every frame)
	{
		const int q = cascSize / 2;
		D3DRECT used = { 0, 0, 6 * q, 3 * q };
		HR(pDevice->Clear(1, &used, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0L));
	}
	BeginPass(RENDERPASS_SHADOWMAP);
	g_oroKeepViewport = true;                          // the casters' Setup() must not reset the slot viewport
	for (int i = 0; i < 9; i++) {
		if (!casc[i].live) continue;
		D3DVIEWPORT9 vp;
		vp.X = (DWORD)casc[i].px;
		vp.Y = (DWORD)casc[i].py;
		vp.Width = vp.Height = (DWORD)casc[i].size;
		vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
		pDevice->SetViewport(&vp);
		smap.mViewProj = casc[i].mVP;
		RenderCascadeCasters(i);
	}
	g_oroKeepViewport = false;
	PopPass();
	gc->PopRenderTargets();
	smap = save;
	cascLive = true;

	// ROUND 8 - THE LIGHT JOINS THE ATLAS. The (z3) local-light map was rendered just
	// before this pass (RenderLocalLightShadowMap, the call above ours); copy it into
	// the spare row's first cell (half-size - 2048 -> 1024, point-sampled: a depth map
	// must not be averaged) so the TERRAIN reads it from the same texture as the sun's
	// cascades. That is the whole daylight fix: the terrain receiver used to BORROW the
	// tShadowMap slot for the local map and yield its sun shadow (the 16-sampler
	// ceiling), so the borrow was night-only and a beam painted through a wall by day
	// (his Moon flight). In the atlas nothing is borrowed and nothing is gated. Vessels
	// keep the full-size map through their own sampler.
	if (lsmap.idx >= 0 && psLclShm) {
		const int q = cascSize / 2;
		RECT dst = { 0, 3 * q, q, 4 * q };
		if (SUCCEEDED(pDevice->StretchRect(psLclShm, NULL, psCasc, &dst, D3DTEXF_POINT))) cascLclLive = true;
	}

	// INSTRUMENT (ShadowDebug >= 3): dump the atlas once, a few hundred frames in, so the
	// slot contents can be looked at directly instead of inferred from shadows.
	if (Config->ShadowDebug >= 3) {
		static bool s_dumped = false;
		if (!s_dumped && dwFrameId > 300) {
			s_dumped = true;
			HRESULT hr = D3DXSaveTextureToFileA("ORO_cascade_atlas.dds", D3DXIFF_DDS, ptCasc, NULL);
			char tx[256]; int n = 0;
			for (int k = 0; k < 9; k++) n += sprintf_s(tx + n, sizeof(tx) - n, " %.3f", casc[k].live ? casc[k].texel : 0.0f);
			oapiWriteLogV("ORO shadow dbg: cascade atlas dumped to ORO_cascade_atlas.dds (hr 0x%08X); texels (slots 0-8):%s m", (unsigned)hr, tx);
		}
	}

	// the mesh family's uniforms, for the whole frame (the terrain takes its own copy
	// per planet render, Surfmgr2): the shared basis and two float4 per slot
	D3DXVECTOR4 basis[3], A[9], tx[3], split, atl;
	GetCascadeConstants(basis, A, tx, &split, &atl, true);
	// the mesh family reads SEVEN slots - the focus box, cascades 1-3, the three hull
	// boxes - in that order (it never reaches cascades 4-5), and derives L from U x V:
	// every register counts there (X4507 under his effect set, three times over)
	static const int fxSlot[7] = { 0, 1, 2, 3, 6, 7, 8 };
	D3DXVECTOR4 Afx[7], txfx[2];
	float tfx[8] = { 0 };
	for (int k = 0; k < 7; k++) { Afx[k] = A[fxSlot[k]]; tfx[k] = casc[fxSlot[k]].live ? casc[fxSlot[k]].texel : 0.0f; }
	txfx[0] = D3DXVECTOR4(tfx[0], tfx[1], tfx[2], tfx[3]);
	txfx[1] = D3DXVECTOR4(tfx[4], tfx[5], tfx[6], tfx[7]);
	if (D3D9Effect::eCascBasis) D3D9Effect::FX->SetVectorArray(D3D9Effect::eCascBasis, basis, 2);
	if (D3D9Effect::eCascA)     D3D9Effect::FX->SetVectorArray(D3D9Effect::eCascA, Afx, 7);
	if (D3D9Effect::eCascTx)    D3D9Effect::FX->SetVectorArray(D3D9Effect::eCascTx, txfx, 2);
	if (D3D9Effect::eCascSplit) D3D9Effect::FX->SetVector(D3D9Effect::eCascSplit, &split);
	if (D3D9Effect::eCascAtlas) D3D9Effect::FX->SetVector(D3D9Effect::eCascAtlas, &atl);
	if (D3D9Effect::eCascMap)   D3D9Effect::FX->SetTexture(D3D9Effect::eCascMap, ptCasc);
}

void Scene::RenderLocalLightShadowMap()
{
	RenderLocalLightShadowMap2(LclTiles);

	// ORO patch (z3) round 2c: age out tiles not sighted for ~2 s. The purge runs
	// on EVERY path (lightless frames included), so the list can neither leak nor
	// grow without bound; the TTL is what keeps a camera rotation from churning
	// the caster set (see the LCLTILECASTER note in Scene.h).
	for (auto it = LclTiles.begin(); it != LclTiles.end(); ) {
		if (dwFrameId - it->stamp > 120) {
			it->pVB->Release();
			it->pIB->Release();
			it = LclTiles.erase(it);
		}
		else ++it;
	}
}


// ===========================================================================================
//
void Scene::RenderLocalLightShadowMap2(const std::vector<LCLTILECASTER>& tiles)
{
	lsmap.idx = -1;
	lsmap.terrainOK = false;

	if (!ptLclShm || !psLclShm || !psLclShmDS) return;
	if (!bLocalLight || !Lights || nLights == 0) return;
	if (Config->LocalLightShadows == 0) return;

	// ⚠️ THE NIGHT GATE IS THE TERRAIN'S ALONE SINCE 2026-09-05 (his Brighton Beach
	// day test: the DG's land light lit a vessel behind a hangar in full lunar
	// daylight - a VESSEL-receiver case the old whole-system gate was silencing).
	// The daylight conflict was only ever the TERRAIN receiver's: it borrows the
	// sun map's sampler slot (the 16-sampler ceiling), so a beam-lit tile yields
	// its SUN shadow - invisible at night, but while the sun still paints it ate
	// the vessel's own shadow in tile-shaped bites (flown, 2026-09-03). The VESSEL
	// shaders bind both maps with headroom and never had the conflict. So: the map
	// BUILDS whenever a qualifying spot exists (no light on = no map = no cost),
	// vessels receive day and night, casters and tile registration key off idx as
	// ever - and ONLY the terrain borrow keeps a sun-elevation gate (terrainOK,
	// consumed in Surfmgr2's per-tile slot match).
	// KNOWN AND ACCEPTED: the ground pool paints through a wall while the gate is
	// closed - the sampler ceiling has not moved; the honest fix is a sun+local
	// map atlas (shelved), and the ceiling is a Vulkan-requirements line.
	lsmap.terrainOK = true;
	if (Camera.vProxy) {
		D3DXVECTOR3 up = -Camera.vProxy->GetBoundingSpherePosDX();
		D3DXVec3Normalize(&up, &up);
		D3DXVECTOR3 sd = sunLight.Dir;
		if (D3DXVec3Dot(&up, &sd) < -0.045f) lsmap.terrainOK = false;
	}

	// -----------------------------------------------------------------------------
	// Pick the strongest shadow-casting SPOT. Score favours bright, long-range
	// lights near the camera; the 2 km gate keeps the map from being spent on a
	// base light too far away to resolve a texel.
	//
	int best = -1;
	float bestScore = 0.0f;

	for (DWORD i = 0; i < nLights; i++)
	{
		if (Lights[i].Type != 1) continue;							// spots only
		float rng = Lights[i].Param[D3D9LRange];
		if (rng < 5.0f) continue;									// a glow, not a beam
		float d2 = D3DXVec3Dot(&Lights[i].Position, &Lights[i].Position);
		if (d2 > 4.0e6f) continue;
		float lum = max(max(Lights[i].Diffuse.r, Lights[i].Diffuse.g), Lights[i].Diffuse.b);
		if (lum <= 0.0f) continue;
		float score = lum * rng * rng / (100.0f + d2);
		if (score > bestScore) { bestScore = score; best = (int)i; }
	}

	if (best < 0) return;

	const D3D9Light& L = Lights[best];

	// -----------------------------------------------------------------------------
	// Light view-projection: perspective from the emitter along its axis. FOV = the
	// spot's PENUMBRA cone (Param[D3D9LPhi] = cos(P/2)) plus a hair of margin, far
	// plane = the light's range. The near plane is pushed out a little so skin
	// centimetres from an emitter authored just inside a fixture cannot fill the map.
	//
	float cosPhi = L.Param[D3D9LPhi];
	if (cosPhi > 0.9999f) return;			// degenerate cone

	float fov = 2.0f * acosf(min(1.0f, max(-1.0f, cosPhi))) * 1.05f;
	fov = min(fov, 2.9f);					// keep the projection sane near 180 deg

	D3DXVECTOR3 P = L.Position;
	D3DXVECTOR3 D = L.Direction;
	D3DXVec3Normalize(&D, &D);

	D3DXVECTOR3 up = (fabs(D.y) < 0.9f) ? D3DXVECTOR3(0, 1, 0) : D3DXVECTOR3(1, 0, 0);
	D3DXVECTOR3 tgt = P + D;

	float zfar = L.Param[D3D9LRange];
	float znear = max(0.75f, zfar * 0.004f);

	D3DXMATRIX mV, mP;
	D3DXMatrixLookAtRH(&mV, &P, &tgt, &up);
	D3DXMatrixPerspectiveFovRH(&mP, fov, 1.0f, znear, zfar);
	D3DXMatrixMultiply(&lsmap.mViewProj, &mV, &mP);

	lsmap.pos = P;
	lsmap.range = zfar;
	// The receivers' normal-offset scale: how many metres one map texel spans per
	// metre of distance from the light. A near-horizontal beam grazing flat ground
	// makes every depth test marginal at once (the whole lit pool blinked while
	// the DG pitched on its gear - flown, not theorised); offsetting the receiver
	// point along its surface normal by ~2 texels clears it geometrically.
	lsmap.texel = 2.0f * tanf(fov * 0.5f) / (float)lsmap.size;
	// ... and the metres-to-depth-units factor for the receivers' texel-footprint
	// bias: at grazing incidence one texel's ground footprint spans metres of ray
	// depth, and the compare must clear exactly that span or flat ground strobes
	// against itself (his runway test) - while a REAL caster's separation dwarfs it.
	lsmap.kdepth = znear * zfar / (zfar - znear);

	// -----------------------------------------------------------------------------
	// Render the casters. Vessels go through vVessel::Render under
	// RENDERPASS_SHADOWMAP so animation matrices apply - that path reads
	// scn->GetSMapData()->mViewProj (VVessel.cpp), so the SUN's smap matrices are
	// swapped out and restored around the pass (the patch-(s) push/restore law).
	// The emitter's OWN vessel is EXCLUDED: emitter positions are routinely
	// authored inside the hull (the DG dock light sits in the nose), and an honest
	// self-shadow from in there blacks the whole beam out. Own-hull shadows are a
	// later question, not a v1 regression - stock casts nothing at all.
	//
	SHADOWMAPPARAM save = smap;
	smap.mViewProj = lsmap.mViewProj;

	gc->PushRenderTarget(psLclShm, psLclShmDS, RENDERPASS_SHADOWMAP);
	HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0L));

	BeginPass(RENDERPASS_SHADOWMAP);

	const vObject* pOwner = LightOwners[best];

	for (VOBJREC* pv = vobjFirst; pv; pv = pv->next) {
		if (pv->type != OBJTP_VESSEL) continue;
		vVessel* vV = (vVessel*)pv->vobj;
		if (!vV->IsActive()) continue;
		if (pv->vobj == pOwner) continue;
		D3DXVECTOR3 bs = vV->GetBoundingSpherePosDX();
		float bsr = vV->GetBoundingSphereRadius();
		D3DXVECTOR3 rel = bs - P;
		if (D3DXVec3Length(&rel) > zfar + bsr) continue;
		vV->Render(pDevice, false);
	}

	// Base structures (the above-shadow set) join as casters - the flagship case:
	// the beam carved by a hangar. opt 0 routes the same meshes into the
	// SHADER_SHADOWMAP technique instead of patch (z2)'s NORMAL_DEPTH.
	if (Camera.vProxy) Camera.vProxy->RenderBaseDepth(&lsmap.mViewProj, 0);

	// ORO patch (z3) round 2c: TERRAIN joins the casters - the beam dies at a
	// ridge instead of painting the valley behind it. The tiles are the ones the
	// PREVIOUS frame's terrain render registered within the light's range (one
	// frame stale, and terrain does not move). Cull NONE: a heightfield has no
	// meaningful backface for a depth map, and explicit is robust against
	// whatever the mesh pass left behind.
	if (pTileShd && tiles.size()) {
		pTileShd->ClearTextures();
		pTileShd->Setup(pPatchVertexDecl, true, 0);
		pTileShd->SetVSConstants("mTileShdVP", &lsmap.mViewProj, sizeof(D3DXMATRIX));
		pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		for (auto& t : tiles) {
			// A stale entry's mW is camera-relative to where the camera WAS at its
			// registration - re-anchor by the camera's translation since (the world
			// orientation never changes, only the origin rides the camera).
			// ORO patch (ab) round 3: RELATIVE TO THE PLANET - see the depth pass and
			// LCLTILECASTER. The global-frame version put every stale tile ~150 m off on
			// the frames the sim stepped (the beam's shimmer under manoeuvres, most likely).
			D3DXMATRIX mW;
			OroTileFromPlanet(t, Camera.pos, mW);   // ORO patch (ab) round 4: from the planet's current frame
			// ORO patch (ab): registration is no longer range-gated (the depth pass
			// wants every nearby tile), so the light's range filter lives here now.
			{
				const D3DXVECTOR3 sh(mW._41 - t.mW._41, mW._42 - t.mW._42, mW._43 - t.mW._43);   // ORO patch (ab) round 4: how far the rebuild moved the origin
				D3DXVECTOR3 b(t.bs.x + sh.x, t.bs.y + sh.y, t.bs.z + sh.z);
				D3DXVECTOR3 r = b - lsmap.pos;
				if (D3DXVec3Length(&r) > lsmap.range + t.bsRad) continue;
			}
			pTileShd->SetVSConstants("mTileShdW", (void*)&mW, sizeof(D3DXMATRIX));
			pDevice->SetStreamSource(0, t.pVB, 0, sizeof(VERTEX_2TEX));
			pDevice->SetIndices(t.pIB);
			pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, t.nv, 0, t.nf);
		}
		pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	}

	PopPass();
	gc->PopRenderTargets();

	smap = save;
	lsmap.idx = best;
}


// ===========================================================================================
// ORO patch (z3) round 2c: a terrain tile volunteers as a local-light shadow caster.
// Called from the Surfmgr2 tile render (main scene only); consumed by the NEXT frame's
// RenderLocalLightShadowMap. AddRef keeps the buffers alive across the frame boundary
// even if the tile manager evicts the tile - released in the wrapper above.
//
void Scene::RegisterLclShadowTile(LPDIRECT3DVERTEXBUFFER9 pVB, LPDIRECT3DINDEXBUFFER9 pIB,
                                  DWORD nv, DWORD nf, const D3DXMATRIX* pW,
                                  const D3DXVECTOR3* pBs, float bsRad, OBJHANDLE hPlanet) const
{
	// ORO patch (ab): the registry serves the depth pass too, so it lives whenever
	// EITHER consumer exists; each consumer applies its own range filter at draw time.
	if (!pTileShd && !pTileDepth) return;
	if (!pVB || !pIB) return;
	// ORO patch (ab) round 2 (2026-09-06, his blinking-shadow screenshots at KSC): MAIN
	// SCENE ONLY. The reflection probes (ENVCAM) and custom cameras render the terrain
	// too - from another position, at a coarser LOD - and their tiles were landing in
	// the depth pass one frame stale, metres NEARER than the fine surface wherever the
	// two LODs disagreed, so the shadow sheet failed the tolerance on whichever frame a
	// probe face had rendered: whole vessel and building shadows blinking frame to
	// frame. Both consumers want the camera's own tile set anyway (the (z3) casters
	// were only ever meant to be what the camera sees).
	if (GetRenderPass() != RENDERPASS_MAINSCENE) return;
	if (!hPlanet) return;   // ORO patch (ab) round 4: the tile is stored in its planet's frame

	// Already registered? Refresh in place - the stamp keeps it alive, the fresh
	// mW/cpos re-anchor it, and the buffers keep their single AddRef.
	for (auto& t : LclTiles) {
		if (t.pVB == pVB) {
			t.mW = *pW;
			t.stamp = dwFrameId;
			t.hPlanet = hPlanet;
			OroTileToPlanet(*pW, Camera.pos, t);
			t.bs = *pBs;
			t.bsRad = bsRad;
			return;
		}
	}

	// THE CAP WAS 512 AND IT WAS THE FLICKER (his Brighton Beach test, 2026-09-05): a
	// lunar horizon with no atmosphere renders many hundreds of tiles a frame, the TTL
	// keeps each for ~2 s after last sighting, and once the list was full every NEW tile
	// was refused - so the hill's tiles came and went as the camera moved, and a missing
	// tile reads as sky, which the soft test treats as nothing in front. An entry is ~100
	// bytes; the buffers it holds are alive anyway while rendered.
	if (LclTiles.size() >= 4096) return;

	pVB->AddRef();
	pIB->AddRef();

	LCLTILECASTER t;
	t.pVB = pVB;
	t.pIB = pIB;
	t.nv = nv;
	t.nf = nf;
	t.mW = *pW;
	t.stamp = dwFrameId;
	t.hPlanet = hPlanet;
	OroTileToPlanet(*pW, Camera.pos, t);
	t.bs = *pBs;
	t.bsRad = bsRad;
	LclTiles.push_back(t);
}




// ===========================================================================================
//
void Scene::RenderSecondaryScene(std::set<vVessel*> &RndList, std::set<vVessel*> &LightsList, DWORD flags)
{
	_TRACE;
	RenderFlags = flags;

	// Process Local Light Sources -------------------------------------
	// And toggle external lights on
	//
	if (bLocalLight) {

		ClearLocalLights();

		for (auto vVes : RndList) {
			if (!vVes->IsActive()) continue;
			VESSEL *vessel = vVes->GetInterface();
			DWORD nemitter = vessel->LightEmitterCount();
			for (DWORD j = 0; j < nemitter; j++) {
				const LightEmitter *em = vessel->GetLightEmitter(j);
				if ((em->GetVisibility() == LightEmitter::VIS_EXTERNAL) || (em->GetVisibility() == LightEmitter::VIS_ALWAYS)) AddLocalLight(em, vVes);
			}		
		}

		for (auto vVes : LightsList) {
			if (!vVes->IsActive()) continue;
			if (RndList.count(vVes)) continue; // Already included skip it
			VESSEL *vessel = vVes->GetInterface();
			DWORD nemitter = vessel->LightEmitterCount();
			for (DWORD j = 0; j < nemitter; j++) {
				const LightEmitter *em = vessel->GetLightEmitter(j);
				if ((em->GetVisibility() == LightEmitter::VIS_EXTERNAL) || (em->GetVisibility() == LightEmitter::VIS_ALWAYS)) AddLocalLight(em, vVes);
			}
		}
	}

	D3D9Effect::UpdateEffectCamera(GetCameraProxyBody());

	// Clear the viewport
	HR(pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0L));

	
	// render planets -------------------------------------------
	//
	if (flags & 0x01) {
		for (DWORD i = 0; i<nplanets; i++) {
			bool isActive = plist[i].vo->IsActive();
			if (isActive) plist[i].vo->Render(pDevice);
			else		  plist[i].vo->RenderDot(pDevice);
		}
	}

	// render the vessel objects --------------------------------
	//
	if (flags & 0x02) {
		for (auto vVes : RndList) {
			if (!vVes->IsActive()) continue;
			if (!vVes->IsVisible()) continue;
			vVes->Render(pDevice);
		}
	}

	// render exhausts -------------------------------------------
	//
	if (flags & 0x04) {
		for (auto vVes : RndList) {
			if (!vVes->IsActive()) continue;
			if (!vVes->IsVisible()) continue;
			vVes->RenderExhaust();
		}
	}

	// render beacons -------------------------------------------
	//
	if (flags & 0x08) {
		for (auto vVes : RndList) {
			if (!vVes->IsActive()) continue;
			vVes->RenderBeacons(pDevice);
		}
	}

	// render exhaust particle system ----------------------------
	if (flags & 0x10) {
		for (DWORD n = 0; n < nstream; n++) pstream[n]->Render(pDevice);
	}

	// Flags 0x20 = BaseStructures
}


// ===========================================================================================
//
bool Scene::RenderBlurredMap(LPDIRECT3DDEVICE9 pDev, LPDIRECT3DCUBETEXTURE9 pSrc)
{
	bool bQuality = true;

	if (!pSrc) return false;

	if (!pBlur) {
		pBlur = new ImageProcessing(pDev, "Modules/D3D9Client/EnvMapBlur.hlsl", "PSBlur");
	}

	if (!pBlur->IsOK()) {
		LogErr("pBlur is not OK");
		return false;
	}

	if (!pEnvDS) {
		LogErr("EnvDepthStencil doesn't exists");
		return false;
	}

	D3DSURFACE_DESC desc;
	pEnvDS->GetDesc(&desc);
	DWORD width = min((UINT)512, desc.Width);


	if (!pBlrTemp[0]) {
		if (D3DXCreateCubeTexture(pDev, width >> 0, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pBlrTemp[0]) != S_OK) return false;
		if (D3DXCreateCubeTexture(pDev, width >> 1, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pBlrTemp[1]) != S_OK) return false;
		if (D3DXCreateCubeTexture(pDev, width >> 2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pBlrTemp[2]) != S_OK) return false;
		if (D3DXCreateCubeTexture(pDev, width >> 3, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pBlrTemp[3]) != S_OK) return false;
		if (D3DXCreateCubeTexture(pDev, width >> 4, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pBlrTemp[4]) != S_OK) return false;
	}


	D3DXVECTOR3 dir, up, cp;
	LPDIRECT3DSURFACE9 pSrf = NULL;
	LPDIRECT3DSURFACE9 pTmp = NULL;

	// Create clurred mip sub-levels
	//
	for (DWORD i = 0; i < 6; i++) {
		pSrc->GetCubeMapSurface(D3DCUBEMAP_FACES(i), 0, &pSrf);
		pBlrTemp[0]->GetCubeMapSurface(D3DCUBEMAP_FACES(i), 0, &pTmp);
		pDevice->StretchRect(pSrf, NULL, pTmp, NULL, D3DTEXF_POINT);
		SAFE_RELEASE(pSrf);
		SAFE_RELEASE(pTmp);
	}


	// Create clurred mip sub-levels
	//
	for (int mip = 1; mip < 5; mip++) {

		pBlur->SetFloat("fD", (4.0f / float(256 >> (mip - 1))));
		pBlur->SetBool("bDir", false);
		pBlur->SetTextureNative("tCube", pBlrTemp[mip-1], IPF_LINEAR);

		for (DWORD i = 0; i < 6; i++) {

			EnvMapDirection(i, &dir, &up);
			D3DXVec3Cross(&cp, &up, &dir);
			D3DXVec3Normalize(&cp, &cp);

			pSrc->GetCubeMapSurface(D3DCUBEMAP_FACES(i), mip, &pSrf);

			pBlur->SetOutputNative(0, pSrf);
			pBlur->SetFloat("vDir", &dir, sizeof(D3DXVECTOR3));
			pBlur->SetFloat("vUp", &up, sizeof(D3DXVECTOR3));
			pBlur->SetFloat("vCp", &cp, sizeof(D3DXVECTOR3));

			if (!pBlur->Execute(true)) {
				LogErr("pBlur Execute Failed");
				return false;
			}

			pBlrTemp[mip-1]->GetCubeMapSurface(D3DCUBEMAP_FACES(i), 0, &pTmp);
			pDevice->StretchRect(pSrf, NULL, pTmp, NULL, D3DTEXF_POINT);
			SAFE_RELEASE(pSrf);
			SAFE_RELEASE(pTmp);
		}

		pBlur->SetBool("bDir", true);

		for (DWORD i = 0; i < 6; i++) {

			EnvMapDirection(i, &dir, &up);
			D3DXVec3Cross(&cp, &up, &dir);
			D3DXVec3Normalize(&cp, &cp);

			pSrc->GetCubeMapSurface(D3DCUBEMAP_FACES(i), mip, &pSrf);

			pBlur->SetOutputNative(0, pSrf);
			pBlur->SetFloat("vDir", &dir, sizeof(D3DXVECTOR3));
			pBlur->SetFloat("vUp", &up, sizeof(D3DXVECTOR3));
			pBlur->SetFloat("vCp", &cp, sizeof(D3DXVECTOR3));

			if (!pBlur->Execute(true)) {
				LogErr("pBlur Execute Failed");
				return false;
			}

			pBlrTemp[mip]->GetCubeMapSurface(D3DCUBEMAP_FACES(i), 0, &pTmp);
			pDevice->StretchRect(pSrf, NULL, pTmp, NULL, D3DTEXF_POINT);
			SAFE_RELEASE(pSrf);
			SAFE_RELEASE(pTmp);
		}
	}

	return true;
}

// ===========================================================================================
//
bool Scene::IntegrateIrradiance(vVessel *vV, LPDIRECT3DCUBETEXTURE9 pSrc, LPDIRECT3DTEXTURE9 pOut)
{
	if (!pSrc) return false;

	if (!pIrradiance) {
		pIrradiance = new ImageProcessing(pDevice, "Modules/D3D9Client/IrradianceInteg.hlsl", "PSPreInteg");
		pIrradiance->CompileShader("PSInteg");
		pIrradiance->CompileShader("PSPostBlur");
	}

	if (!pIrradiance->IsOK()) {
		LogErr("pIrradiance is not OK");
		return false;
	}

	if (!pIrradDS) {
		LogErr("pIrradDS doesn't exists");
		return false;
	}

	LPDIRECT3DSURFACE9 pOuts = NULL;
	HR(pOut->GetSurfaceLevel(0, &pOuts));

	D3DSURFACE_DESC desc, desc_out;
	pIrradDS->GetDesc(&desc);
	pOuts->GetDesc(&desc_out);
	
	if (!pIrradTemp) {
		if (D3DXCreateCubeTexture(pDevice, 16, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &pIrradTemp) != S_OK) {
			LogErr("Failed to create irradiance temp");
			return false;
		}
		if (D3DXCreateTexture(pDevice, 128, 128, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &pIrradTemp2) != S_OK) {
			LogErr("Failed to create irradiance temp");
			return false;
		}
		if (D3DXCreateTexture(pDevice, desc_out.Width, desc_out.Height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &pIrradTemp3) != S_OK) {
			LogErr("Failed to create irradiance temp");
			return false;
		}
	}

	D3DXVECTOR3 nr, up, cp;
	LPDIRECT3DSURFACE9 pSrf = NULL;
	LPDIRECT3DSURFACE9 pTgt = NULL;
	LPDIRECT3DSURFACE9 pTmp2 = NULL;
	LPDIRECT3DSURFACE9 pTmp3 = NULL;
	
	HR(pIrradTemp2->GetSurfaceLevel(0, &pTmp2));
	HR(pIrradTemp3->GetSurfaceLevel(0, &pTmp3));


	// ---------------------------------------------------------------------
	// Pre-Integrate Irradiance Cube
	//
	pIrradiance->Activate("PSPreInteg");
	pIrradiance->SetFloat("fD", ptr(D3DXVECTOR2(1.0f / float(desc.Width), 1.0f / float(desc.Height))), sizeof(D3DXVECTOR2));

	for (DWORD i = 0; i < 6; i++)
	{
		pSrc->GetCubeMapSurface(D3DCUBEMAP_FACES(i), 0, &pSrf);
		pIrradTemp->GetCubeMapSurface(D3DCUBEMAP_FACES(i), 0, &pTgt);
		
		pDevice->StretchRect(pSrf, NULL, pTmp2, NULL, D3DTEXF_POINT);

		pIrradiance->SetOutputNative(0, pTgt);
		pIrradiance->SetTextureNative("tSrc", pIrradTemp2, IPF_POINT | IPF_CLAMP);

		if (!pIrradiance->Execute(true)) {
			LogErr("pIrradiance Execute Failed");
			return false;
		}

		SAFE_RELEASE(pTgt);
		SAFE_RELEASE(pSrf);
	}

	


	// ---------------------------------------------------------------------
	// Main Integration
	//
	GetLVLH(vV, &up, &nr, &cp);

	float Glow = float(Config->PlanetGlow);

	pIrradiance->Activate("PSInteg");
	pIrradiance->SetOutputNative(0, pTmp3);
	pIrradiance->SetTextureNative("tCube", pIrradTemp, IPF_LINEAR);
	pIrradiance->SetFloat("Kernel", IKernel, sizeof(IKernel));
	pIrradiance->SetFloat("vNr", &nr, sizeof(D3DXVECTOR3));
	pIrradiance->SetFloat("vUp", &up, sizeof(D3DXVECTOR3));
	pIrradiance->SetFloat("vCp", &cp, sizeof(D3DXVECTOR3));
	pIrradiance->SetFloat("fIntensity", &Glow, sizeof(float));
	pIrradiance->SetBool("bUp", false);
	pIrradiance->SetTemplate(0.5f, 1.0f, 0.0f, 0.0f);

	if (!pIrradiance->Execute(true)) {
		LogErr("pIrradiance Execute Failed");
		return false;
	}

	pIrradiance->SetBool("bUp", true);
	pIrradiance->SetTemplate(0.5f, 1.0f, 0.5f, 0.0f);

	if (!pIrradiance->Execute(true)) {
		LogErr("pIrradiance Execute Failed");
		return false;
	}

	// ---------------------------------------------------------------------
	// Post Blur
	//
	pIrradiance->Activate("PSPostBlur");
	pIrradiance->SetFloat("fD", ptr(D3DXVECTOR2(1.0f / float(desc_out.Width), 1.0f / float(desc_out.Height))), sizeof(D3DXVECTOR2));
	pIrradiance->SetOutputNative(0, pOuts);
	pIrradiance->SetTextureNative("tSrc", pIrradTemp3, IPF_POINT | IPF_WRAP);

	if (!pIrradiance->Execute(true)) {
		LogErr("pIrradiance Execute Failed");
		return false;
	}

	SAFE_RELEASE(pTmp2);
	SAFE_RELEASE(pTmp3);
	SAFE_RELEASE(pOuts);

	return true;
}



// ===========================================================================================
//
void Scene::ClearOmitFlags()
{
	VOBJREC *pv = NULL;
	for (pv=vobjFirst; pv; pv=pv->next) pv->vobj->bOmit = false;
}


// ===========================================================================================
//
void Scene::VisualizeCubeMap(LPDIRECT3DCUBETEXTURE9 pCube, int mip)
{
	if (!pCube) return;

	LPDIRECT3DSURFACE9 pSrf = NULL;
	LPDIRECT3DSURFACE9 pBack = gc->GetBackBuffer();

	D3DSURFACE_DESC bdesc;

	if (!pBack) return;

	HR(pBack->GetDesc(&bdesc));

	DWORD x, y, h = bdesc.Height / 3;

	for (DWORD i=0;i<6;i++) {

		HR(pCube->GetCubeMapSurface(D3DCUBEMAP_FACES(i), mip, &pSrf));

		switch (i) {
			case 0:	x = 2*h; y=h; break;
			case 1:	x = 0;   y=h; break;
			case 2:	x = 1*h; y=0; break;
			case 3:	x = 1*h; y=2*h; break;
			case 4:	x = 1*h; y=h; break;
			case 5:	x = 3*h; y=h; break;
		}

		RECT dr;
		dr.left = x;
		dr.top = y;
		dr.bottom = y+h;
		dr.right = x+h;

		HR(pDevice->StretchRect(pSrf, NULL, pBack, &dr, D3DTEXF_POINT));

		SAFE_RELEASE(pSrf);
	}
}



// ===========================================================================================
//
void Scene::RenderVesselShadows (OBJHANDLE hPlanet, float depth) const
{
	// If this planet is not a proxy body skip the rest
	if (hPlanet != oapiCameraProxyGbody()) return;

	// ORO patch (s) part 2: a sharp projected shadow under a storm deck is the single
	// loudest "this is actually a sunny day" tell. The shader-side sun collapse fades the
	// shadow-MAP shadows automatically (they only modulate the sun term), but these
	// stencil-projected ground shadows are drawn as dark geometry with their own alpha,
	// so they need fading explicitly. 0 storm = stock exactly.
	// ⚠️ `depth` IS AN INVERSE ALPHA - RenderGroundShadow draws at (1 - depth) opacity
	// (VVessel.cpp: `alpha = (1.0f - alpha) * saturate(scale)`). The first version
	// multiplied depth toward ZERO, which drove the drawn shadow toward FULLY BLACK:
	// the storm was making non-focus vessels' shadows DARKER while the focus vessel's
	// map shadow faded correctly - exactly the two-DeltaGlider screenshot. The fade
	// must push depth toward ONE instead.
	// ORO patch (aa): the storm collapse above is now ONE term of OroGroundShadowFade,
	// with the fog's sun attenuation and transmittance at EACH VESSEL's own position -
	// the first fog round applied the camera's column to every shadow alike, and a fog
	// that had erased a vessel at 150 m still drew its shadow crisp on the grey.

	// render vessel shadows
	VOBJREC *pv;
	for (pv = vobjFirst; pv; pv = pv->next) {
		if (!pv->vobj->IsActive()) continue;
		if (oapiGetObjectType(pv->vobj->Object()) == OBJTP_VESSEL) {
			const VECTOR3 cp = pv->vobj->PosFromCamera();
			const float fade = OroGroundShadowFade(D3DXVECTOR3((float)cp.x, (float)cp.y, (float)cp.z));
			// `depth` is an INVERSE alpha (see above): push it toward ONE to fade
			const float dv = 1.0f - (1.0f - depth) * fade;
			const bool  mainPass = (GetRenderPass() == RENDERPASS_MAINSCENE);   // ORO patch (ab) INSTRUMENT
			if (mainPass) { g_oroDbgDepth = depth; g_oroDbgFade = fade; g_oroDbgDv = dv; }
			if (dv < 0.995f) {
				if (mainPass) g_oroDbgVesDrawn++;
				((vVessel*)(pv->vobj))->RenderGroundShadow(pDevice, hPlanet, dv);
			}
			else if (mainPass) g_oroDbgVesSkip++;
		}
	}

	// reset device parameters
	pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);

	// render particle shadows
	LPDIRECT3DTEXTURE9 tex = 0;
	for (DWORD j=0;j<nstream;j++) pstream[j]->RenderGroundShadow(pDevice, tex);
}


// ===========================================================================================
//
void Scene::RenderMesh(DEVMESHHANDLE hMesh, const oapi::FMATRIX4 *pWorld)
{
	D3D9Mesh *pMesh = (D3D9Mesh *)hMesh;

	const Scene::SHADOWMAPPARAM *shd = GetSMapData();

	float s = float(shd->size);
	float sr = 2.0f * shd->rad / s;

	HR(D3D9Effect::FX->SetMatrix(D3D9Effect::eLVP, &shd->mViewProj));

	if (shd->pShadowMap) {
		HR(D3D9Effect::FX->SetTexture(D3D9Effect::eShadowMap, shd->pShadowMap));
		HR(D3D9Effect::FX->SetVector(D3D9Effect::eSHD, ptr(D3DXVECTOR4(sr, 1.0f / s, float(oapiRand()), 1.0f / shd->depth))));
		HR(D3D9Effect::FX->SetBool(D3D9Effect::eShadowToggle, true));
	}
	else {
		HR(D3D9Effect::FX->SetBool(D3D9Effect::eShadowToggle, false));
	}

	pMesh->SetSunLight(&sunLight);
	pMesh->RenderSimplified(LPD3DXMATRIX(pWorld));
}


// ===========================================================================================
//
bool Scene::WorldToScreenSpace(const VECTOR3 &wpos, oapi::IVECTOR2 *pt, D3DXMATRIX *pVP, float clip)
{
	D3DXVECTOR4 homog;
	D3DXVECTOR3 pos(float(wpos.x), float(wpos.y), float(wpos.z));

	if (pVP) D3DXVec3Transform(&homog, &pos, pVP);
	else D3DXVec3Transform(&homog, &pos, GetProjectionViewMatrix());

	if (homog.w < 0.0f) return false;

	homog.x /= homog.w;
	homog.y /= homog.w;

	bool bClip = false;
	if (homog.x < -clip || homog.x > clip || homog.y < -clip || homog.y > clip) bClip = true;

	if (std::hypot(homog.x, homog.y) < 1e-6) {
		pt->x = viewW / 2;
		pt->y = viewH / 2;
	}
	else {
		pt->x = (long)((float(viewW) * 0.5f * (1.0f + homog.x)) + 0.5f);
		pt->y = (long)((float(viewH) * 0.5f * (1.0f - homog.y)) + 0.5f);
	}

	return !bClip;
}


// ===========================================================================================
//
bool Scene::WorldToScreenSpace2(const VECTOR3& wpos, oapi::FVECTOR2* pt, D3DXMATRIX* pVP, float clip)
{
	D3DXVECTOR4 homog;
	D3DXVECTOR3 pos(float(wpos.x), float(wpos.y), float(wpos.z));

	if (pVP) D3DXVec3Transform(&homog, &pos, pVP);
	else D3DXVec3Transform(&homog, &pos, GetProjectionViewMatrix());

	homog.x /= homog.w;
	homog.y /= homog.w;

	bool bClip = false;
	if (homog.w < 0.0f) bClip = true;
	if (homog.x < -clip || homog.x > clip || homog.y < -clip || homog.y > clip) bClip = true;

	if (std::hypot(homog.x, homog.y) < 1e-6) {
		pt->x = viewW / 2;
		pt->y = viewH / 2;
	}
	else {
		pt->x = (float(viewW) * 0.5f * (1.0f + homog.x)) + 0.5f;
		pt->y = (float(viewH) * 0.5f * (1.0f - homog.y)) + 0.5f;
	}

	return !bClip;
}

// ===========================================================================================
//
void Scene::RenderObjectMarker(oapi::Sketchpad *pSkp, const VECTOR3 &gpos, const std::string& label1, const std::string& label2, int mode, int scale)
{
	VECTOR3 dp (gpos - GetCameraGPos());
	normalise (dp);
	m_celSphere->RenderMarker(pSkp, dp, label1, label2, mode, scale);
}

// ===========================================================================================
//
void Scene::NewVessel(OBJHANDLE hVessel)
{
	CheckVisual(hVessel);
}

// ===========================================================================================
//
void Scene::DeleteVessel(OBJHANDLE hVessel)
{
	VOBJREC *pv = FindVisual(hVessel);
	if (pv) DelVisualRec(pv);
}

// ===========================================================================================
//
// ORO patch (y): walk the scene's particle streams and report the reconstructed
// spec of hVessel's STOCK exhaust streams - the only place a vessel author's own
// PARTICLESTREAMSPEC can be read back from (the core copies it at construction and
// exposes no getter). Read-only; nothing is lent, so 23(k)'s load-window rule does
// not apply.
int Scene::GetExhaustStreamSpec (OBJHANDLE hVessel, int idx, PARTICLESTREAMSPEC* out,
                                 VECTOR3* pos, VECTOR3* dir)
{
	int n = 0;
	for (DWORD i = 0; i < nstream; i++) {
		if (!pstream[i]->OroIsStockExhaust(hVessel)) continue;
		if (n == idx) pstream[i]->OroGetSpec(out, pos, dir);
		n++;
	}
	return n;
}

void Scene::AddParticleStream (class D3D9ParticleStream *_pstream)
{

	D3D9ParticleStream **tmp = new D3D9ParticleStream*[nstream+1];
	if (nstream) {
		memcpy (tmp, pstream, nstream*sizeof(D3D9ParticleStream*));
		delete []pstream;
	}
	pstream = tmp;
	pstream[nstream++] = _pstream;

}

// ===========================================================================================
//
void Scene::DelParticleStream (DWORD idx)
{

	D3D9ParticleStream **tmp;
	if (nstream > 1) {
		DWORD i, j;
		tmp = new D3D9ParticleStream*[nstream-1];
		for (i = j = 0; i < nstream; i++)
			if (i != idx) tmp[j++] = pstream[i];
	} else tmp = 0;
	delete pstream[idx];
	delete []pstream;
	pstream = tmp;
	nstream--;

}

// ===========================================================================================
//
void Scene::InitGDIResources ()
{
	char dbgfnt[64]; sprintf_s(dbgfnt,64,"*%s",Config->DebugFont);
	pAxisFont  = oapiCreateFont(24, false, "Arial", FONT_NORMAL, 0);
	pLabelFont = oapiCreateFont(15, false, "Arial", FONT_NORMAL, 0);
	pDebugFont = oapiCreateFont(Config->DebugFontSize, true, dbgfnt, FONT_NORMAL, 0);

	const int fsize[4] = { 12, 16, 20, 26 };
	for (int i = 0; i < 4; ++i) {
		label_font[i] = gc->clbkCreateFont(fsize[i], true, "Arial", FONT_BOLD);
	}
	//@todo: different pens for different fonts?
}

// ===========================================================================================
//
void Scene::ExitGDIResources ()
{
	oapiReleaseFont(pAxisFont);
	oapiReleaseFont(pLabelFont);
	oapiReleaseFont(pDebugFont);

	for (int i = 0; i < 4; ++i) {
		gc->clbkReleaseFont(label_font[i]);
	}
}

// ===========================================================================================
//
float Scene::GetDepthResolution(float dist) const
{
	return fabs( (Camera.nearplane-Camera.farplane)*(dist*dist) / (Camera.farplane * Camera.nearplane * 16777215.0f) );
}

// ===========================================================================================
//
float Scene::CameraInSpace() const
{
	if (Camera.vProxy) {
		if (Camera.vProxy->HasAtmosphere()) {
			ConstParams* cp = Camera.vProxy->GetScatterConst();
			if (cp)	return 1.0f - exp(-cp->CamAlt * cp->iH.x);
		}
	}
	return 1.0f;
}

// ===========================================================================================
//
void Scene::GetCameraLngLat(double *lng, double *lat) const
{
	if (lng) *lng = Camera.lng;
	if (lat) *lat = Camera.lat;
}

// ===========================================================================================
//
void Scene::PushCamera()
{
	CameraStack.push(Camera);
}

// ===========================================================================================
//
void Scene::PopCamera()
{
	Camera = CameraStack.top();
	CameraStack.pop();
}

// ===========================================================================================
//
FMATRIX4 Scene::PushCameraFrustumLimits(float nearlimit, float farlimit)
{
	FRUSTUM fr = { Camera.nearplane, Camera.farplane };
	FrustumStack.push(fr);
	SetCameraFrustumLimits(nearlimit, farlimit);
	return FMATRIX4(GetProjectionViewMatrix());
}

// ===========================================================================================
//
FMATRIX4 Scene::PopCameraFrustumLimits()
{
	SetCameraFrustumLimits(FrustumStack.top().znear, FrustumStack.top().zfar);
	FrustumStack.pop();
	return FMATRIX4(GetProjectionViewMatrix());
}

// ===========================================================================================
//
void Scene::BeginPass(DWORD dwPass)
{
	PassStack.push(dwPass);
}

// ===========================================================================================
//
void Scene::PopPass()
{
	PassStack.pop();
}

// ===========================================================================================
//
DWORD Scene::GetRenderPass() const
{
	if (PassStack.empty()) return RENDERPASS_MAINSCENE;
	return PassStack.top();
}

// ===========================================================================================
//
D3DXVECTOR3 Scene::GetPickingRay(short xpos, short ypos)
{
	float x = 2.0f*float(xpos) / float(ViewW()) - 1.0f;
	float y = 2.0f*float(ypos) / float(ViewH()) - 1.0f;
	D3DXVECTOR3 vPick = Camera.x * (x / Camera.mProj._11) + Camera.y * (-y / Camera.mProj._22) + Camera.z;
	D3DXVec3Normalize(&vPick, &vPick);
	return vPick;
}

// ===========================================================================================
//
TILEPICK Scene::PickSurface(short xpos, short ypos)
{
	TILEPICK tp; memset(&tp, 0, sizeof(TILEPICK));
	vPlanet *vp = GetCameraProxyVisual();
	if (!vp) return tp;
	D3DXVECTOR3 vRay = GetPickingRay(xpos, ypos);
	vp->PickSurface(vRay, &tp);
	return tp;
}

// ===========================================================================================
//
D3D9Pick Scene::PickScene(short xpos, short ypos)
{
	D3DXVECTOR3 vPick = GetPickingRay(xpos, ypos);

	D3D9Pick result;
	result.dist  = 1e30f;
	result.pMesh = NULL;
	result.vObj  = NULL;
	result.group = -1;
	result.idx = -1;

	for (VOBJREC *pv=vobjFirst; pv; pv=pv->next) {

		if (pv->type!=OBJTP_VESSEL) continue;
		if (!pv->vobj->IsActive()) continue;
		if (!pv->vobj->IsVisible()) continue;

		vVessel *vVes = (vVessel *)pv->vobj;
		double cd = vVes->CamDist();

		if (cd<5e3 && cd>1e-3) {
			D3D9Pick pick = vVes->Pick(&vPick);
			if (pick.pMesh) if (pick.dist<result.dist) result = pick;
		}
	}
	return result;
}

// ===========================================================================================
//
D3D9Pick Scene::PickMesh(DEVMESHHANDLE hMesh, const LPD3DXMATRIX pW, short xpos, short ypos)
{
	D3D9Mesh *pMesh = (D3D9Mesh *)hMesh;
	return pMesh->Pick(pW, NULL, ptr(GetPickingRay(xpos, ypos)));
}

// ===========================================================================================
//
void Scene::GetAdjProjViewMatrix(LPD3DXMATRIX pMP, float znear, float zfar)
{
	float tanap = tan(Camera.aperture);
	ZeroMemory(pMP, sizeof(D3DXMATRIX));
	pMP->_11 = (Camera.aspect / tanap);
	pMP->_22 = (1.0f / tanap);
	pMP->_43 = (pMP->_33 = zfar / (zfar - znear)) * (-znear);
	pMP->_34 = 1.0f;
}

// ===========================================================================================
//
void Scene::SetCameraAperture(float ap, float as)
{
	Camera.aperture = ap;
	Camera.aspect = as;

	float tanap = tan(ap);

	ZeroMemory(&Camera.mProj, sizeof(D3DXMATRIX));

	Camera.mProj._11 = (as / tanap);
	Camera.mProj._22 = (1.0f / tanap);
	Camera.mProj._43 = (Camera.mProj._33 = Camera.farplane / (Camera.farplane-Camera.nearplane)) * (-Camera.nearplane);
	Camera.mProj._34 = 1.0f;

	float x = tanap / as;
	float y = tanap;
	float z = as / tanap;

	Camera.apsq = sqrt(x*x*x*z + y*y);

	Camera.vh   = tan(ap);
	Camera.vw   = Camera.vh/as;
	Camera.vhf  = 1.0f / cos(ap);
	Camera.vwf  = Camera.vhf/as;

	D3DXMatrixMultiply(&Camera.mProjView, &Camera.mView, &Camera.mProj);
	D3D9Effect::SetViewProjMatrix(&Camera.mProjView);
}

// ===========================================================================================
//
void Scene::SetCameraFrustumLimits (double nearlimit, double farlimit)
{
	Camera.nearplane = (float)nearlimit;
	Camera.farplane  = (float)farlimit;
	SetCameraAperture(Camera.aperture, Camera.aspect);
}

// ===========================================================================================
//
bool Scene::IsProxyMesh()
{
	return Camera.vProxy ? Camera.vProxy->IsMesh() : false;
}

// ===========================================================================================
//
bool Scene::CameraPan(VECTOR3 pan, double speed)
{
	DWORD camMode = *(DWORD *)gc->GetConfigParam(CFGPRM_GETCAMERAMODE);
	OBJHANDLE hTgt = oapiCameraTarget();

	if (DebugControls::IsActive()==true && hTgt) {
		if (camMode==1) {
			VECTOR3 pos;
			oapiGetGlobalPos(hTgt, &pos);
			Camera.pos = pos + Camera.relpos;
			Camera.pos += Camera.dir * (pan.z*speed) + _VD3DX(Camera.x) * (pan.x*speed) + _VD3DX(Camera.y) * (pan.y*speed);
			Camera.relpos = Camera.pos - pos;
			return true;
		}
	}
	return false;
}


// ===========================================================================================
//
bool Scene::UpdateCameraFromOrbiter(DWORD dwPass)
{
	MATRIX3 grot;
	VECTOR3 pos;

	DWORD camMode = *(DWORD *)gc->GetConfigParam(CFGPRM_GETCAMERAMODE);

	OBJHANDLE hTgt = oapiCameraTarget();

	if (hTgt) {
		if (DebugControls::IsActive()==false || camMode==0) {
			// Acquire camera information from Orbiter
			oapiGetGlobalPos(hTgt, &pos);
			oapiCameraGlobalPos(&Camera.pos);
			Camera.relpos = Camera.pos - pos;	// camera_relpos is a mesh debugger paramater
		}
		else {
			// Mesh debugger camera mode active
			oapiGetGlobalPos(hTgt, &pos);
			Camera.pos = pos + Camera.relpos; // Compute from target pos and offset
		}
	}
	else {
		// Camera target doesn't exist. (Should not happen)
		oapiCameraGlobalPos(&Camera.pos);
		Camera.relpos = _V(0,0,0);
	}

	oapiCameraGlobalDir(&Camera.dir);
	oapiCameraRotationMatrix(&grot);
	Camera.grot = grot;		// ORO patch (k): keep the double-precision rotation for
							// gcCore::GetRenderCam (mView below is the same, as float)
	D3DXMatrixIdentity(&Camera.mView);
	D3DMAT_SetRotation(&Camera.mView, &grot);

	// note: in render space, the camera is always placed at the origin,
	// so that render coordinates are precise in the vicinity of the
	// observer (before they are translated into D3D single-precision
	// format). However, the orientation of the render space is the same
	// as orbiter's global coordinate system. Therefore there is a
	// translational transformation between orbiter global coordinates
	// and render coordinates.

	for (VOBJREC *pv = vobjFirst; pv; pv = pv->next) pv->vobj->Update(true);

	return SetupInternalCamera(&Camera.mView, NULL, oapiCameraAperture(), double(viewH)/double(viewW));
}



// ===========================================================================================
//
bool Scene::SetupInternalCamera(D3DXMATRIX *mNew, VECTOR3 *gpos, double apr, double asp)
{

	// Update camera orientation if a new matrix is provided
	if (mNew) {
		Camera.mView	  = *mNew;
		Camera.x   = D3DXVECTOR3(Camera.mView._11, Camera.mView._21, Camera.mView._31);
		Camera.y   = D3DXVECTOR3(Camera.mView._12, Camera.mView._22, Camera.mView._32);
		Camera.z   = D3DXVECTOR3(Camera.mView._13, Camera.mView._23, Camera.mView._33);
		Camera.dir = _VD3DX(Camera.z);
	}

	if (gpos) Camera.pos = *gpos;

	Camera.upos = D3DXVEC(unit(Camera.pos));

	// find a logical reference body
	Camera.hObj_proxy = oapiCameraProxyGbody();
	Camera.hNear = NULL;
	Camera.hGravRef = NULL;
	Camera.vGravRef = NULL;

	// find the planet closest to the current camera position
	double closest = 0;
	int n = oapiGetGbodyCount();
	for (int i = 1; i < n; i++) {
		VECTOR3 gp; OBJHANDLE hB = oapiGetGbodyByIndex(i);
		oapiGetGlobalPos(hB, &gp);
		VECTOR3 dst = gp - Camera.pos;
		double l = pow(oapiGetMass(hB), 0.33) / dotp(dst, dst);
		if (l > closest) {
			closest = l;
			Camera.hNear = hB;
		}	
	}

	// find the planet closest to the current camera position
	closest = 0;
	for (int i = 0; i < n; i++) {
		VECTOR3 gp; OBJHANDLE hB = oapiGetGbodyByIndex(i);
		oapiGetGlobalPos(hB, &gp);
		VECTOR3 dst = gp - Camera.pos;
		double l = oapiGetMass(hB) / dotp(dst, dst);
		if (l > closest) {
			closest = l;
			Camera.hGravRef = hB;
		}
	}

	/*if (Camera.hNear) {
		// If the near body is not visible enough, switch to proxy.
		double apr = oapiGetSize(Camera.hNear) / closest;
		if (apr < 4e-3) Camera.hNear = Camera.hObj_proxy;
	}*/

	// find the visual
	if (oapiGetObjectType(Camera.hObj_proxy) == OBJTP_PLANET)
		Camera.vProxy = (vPlanet*)GetVisObject(Camera.hObj_proxy);
	else Camera.vProxy = nullptr;
	
	if (oapiGetObjectType(Camera.hNear) == OBJTP_PLANET)
		Camera.vNear = (vPlanet*)GetVisObject(Camera.hNear);
	else Camera.vNear = nullptr;

	Camera.vGravRef = GetVisObject(Camera.hGravRef);

	
	// Something is very wrong... abort...
	if (Camera.hGravRef == NULL || Camera.hObj_proxy == NULL || Camera.hNear == NULL) {
		assert(false); return false;
	}
	if (Camera.vGravRef == NULL || Camera.vProxy == NULL || Camera.vNear == NULL) {
		return false;
	}

	// Camera altitude over the proxy
	VECTOR3 pos; MATRIX3 grot; double rad;
	oapiGetGlobalPos(Camera.hObj_proxy, &pos);
	oapiGetRotationMatrix(Camera.hObj_proxy, &grot);

	oapiLocalToEqu(Camera.hObj_proxy, tmul(grot, Camera.pos - pos), &Camera.lng, &Camera.lat, &rad);

	Camera.alt_proxy = dist(Camera.pos, pos) - oapiGetSize(Camera.hObj_proxy);

	if (Camera.vProxy->Type() == OBJTP_PLANET) 
		rad = oapiSurfaceElevation(Camera.hObj_proxy, Camera.lng, Camera.lat);
	
	Camera.elev = Camera.alt_proxy - rad;

	// Camera altitude over the proxy
	oapiGetGlobalPos(Camera.hNear, &pos);
	Camera.alt_near = dist(Camera.pos, pos) - oapiGetSize(Camera.hNear);

	// Call SetCameraAparture to update ViewProj Matrix
	SetCameraAperture(float(apr), float(asp));

	// Finally update world matrices from all visuals
	//
	if (gpos) for (VOBJREC *pv = vobjFirst; pv; pv = pv->next) pv->vobj->ReOrigin(Camera.pos);

	return true;
}




// ===========================================================================================
// CUSTOM CAMERA INTERFACE
// ===========================================================================================

int Scene::DeleteCustomCamera(CAMERAHANDLE hCam)
{
	if (!hCam) return 0;
	int iError = CAMERA(hCam)->iError;
	CustomCams.erase(CAMERA(hCam));
	delete CAMERA(hCam);
	camCurrent = CustomCams.cbegin();
	return iError;
}

// ===========================================================================================
//
void Scene::DeleteAllCustomCameras()
{
	for (auto x : CustomCams) delete CAMERA(x);
	CustomCams.clear();
}

// ===========================================================================================
//
CAMERAHANDLE Scene::SetupCustomCamera(CAMERAHANDLE hCamera, OBJHANDLE hVessel, MATRIX3 &mRot, VECTOR3 &pos, double fov, SURFHANDLE hSurf, DWORD flags)
{
	CAMREC *pv = NULL;

	if (!hSurf) return NULL;
	if (Config->CustomCamMode==0) return NULL;
	if (SURFACE(hSurf)->Is3DRenderTarget()==false) return NULL;

	if (hCamera==NULL) {
		pv = new CAMREC; memset(pv, 0, sizeof(CAMREC));
		CustomCams.insert(pv);
		camCurrent = CustomCams.cbegin();
	}
	else {
		pv = (CAMREC *)hCamera;
	}

	if (!pv) return NULL;

	pv->bActive = true;
	pv->dAperture = fov;
	pv->dwFlags = flags;
	pv->hSurface = hSurf;
	pv->mRotation = mRot;
	pv->vPosition = pos;
	pv->hVessel = hVessel;
	pv->iError = 0;

	return (CAMERAHANDLE)pv;
}

// ===========================================================================================
//
void Scene::CustomCameraOnOff(CAMERAHANDLE hCamera, bool bOn)
{
	if (!hCamera) return;
	CAMERA(hCamera)->bActive = bOn;
}

// ===========================================================================================
//
void Scene::RenderCustomCameraView(CAMREC *cCur)
{
	VESSEL *pVes = oapiGetVesselInterface(cCur->hVessel);

	DWORD w = SURFACE(cCur->hSurface)->GetWidth();
	DWORD h = SURFACE(cCur->hSurface)->GetHeight();

	LPDIRECT3DSURFACE9 pSrf = SURFACE(cCur->hSurface)->GetSurface();
	LPDIRECT3DSURFACE9 pDSs = SURFACE(cCur->hSurface)->GetDepthStencil();

	if (!pSrf) cCur->iError = -1;
	if (!pDSs) cCur->iError = -2;

	if (cCur->iError!=0) return;

	MATRIX3 grot;
	VECTOR3 gpos;

	pVes->GetRotationMatrix(grot);
	pVes->Local2Global(cCur->vPosition, gpos);

	D3DXMATRIX mEnv, mGlo;

	D3DXMatrixIdentity(&mGlo);
	D3DMAT_SetRotation(&mGlo, &grot);
	D3DXMatrixIdentity(&mEnv);
	D3DMAT_SetRotation(&mEnv, &cCur->mRotation);
	D3DXMatrixMultiply(&mEnv, &mGlo, &mEnv);

	PushCamera();

	SetCameraFrustumLimits(0.1, 2e7);
	SetupInternalCamera(&mEnv, &gpos, cCur->dAperture, double(h)/double(w));

	
	VOBJREC *pv = NULL;
	std::set<vVessel*> List;
	std::set<vVessel*> Lights;

	for (pv = vobjFirst; pv; pv = pv->next) if (pv->type == OBJTP_VESSEL) List.insert((vVessel *)pv->vobj);
	
	BeginPass(RENDERPASS_CUSTOMCAM);

	gc->PushRenderTarget(pSrf, pDSs, RENDERPASS_CUSTOMCAM);

	RenderSecondaryScene(List, Lights, 0xFF);

	gc->PopRenderTargets();

	PopPass();
	PopCamera();
}


// ===========================================================================================
//
// ===========================================================================================
// ORO patch: the sun hides behind terrain. The glare visibility kernel samples
// GBUF_DEPTH, which holds VESSELS and the COCKPIT only - terrain never writes it -
// so at sundown the glare sprite painted straight over any mountain the sun was
// actually behind (the true disc behind the ridge is background and covered fine;
// what shows through is THIS sprite, drawn after the scene). Rendering terrain
// into the depth pass would cost a full extra terrain draw, so the terrain is
// asked DIRECTLY instead: march the ground toward the sun's azimuth, take the
// highest elevation angle (curvature-dropped), and fade the glare across the
// sun's own apparent diameter as it sinks below that ridge line. A dozen
// elevation samples on the CPU - the base shadow code spends more per frame.
// Inert above 10 km AGL, where the smooth limb takes over; on flat ground the
// ridge line IS the dipped sea horizon, so sunset timing comes out right too.
static float OroSunTerrainVis(const VECTOR3& camPos, OBJHANDLE hProxy, const VECTOR3& usun)
{
	if (!hProxy) return 1.0f;

	double lng, lat, rad;
	oapiGlobalToEqu(hProxy, camPos, &lng, &lat, &rad);
	const double R = oapiGetSize(hProxy);
	const double eyeElv = rad - R;                    // eye height above mean radius
	const double grdElv = oapiSurfaceElevation(hProxy, lng, lat);
	const double hAGL = eyeElv - grdElv;
	if (hAGL > 25e3 || hAGL < -1e3) return 1.0f;      // high above: the limb owns it

	VECTOR3 pgp; oapiGetGlobalPos(hProxy, &pgp);
	const VECTOR3 up = unit(camPos - pgp);
	const double sinel = dotp(usun, up);
	VECTOR3 hdir = usun - up * sinel;                 // horizontal component = azimuth
	const double hl = length(hdir);
	if (hl < 1e-6) return 1.0f;                       // sun overhead
	hdir *= 1.0 / hl;

	// Out to 160 km on a x1.4 ladder (~22 samples): a sunset vista's occluding
	// range can sit tens of km away, and round 2's sparse far samples left
	// tens-of-km GAPS a whole mountain range could hide in - the test saw the
	// valleys either side of his ridge and kept a faint dot alive. The
	// curvature drop keeps far samples honest.
	double maxAng = -1.0;
	for (double d = 150.0; d < 160e3; d *= 1.25) {
		VECTOR3 p = camPos + hdir * d;
		double plng, plat, prad;
		oapiGlobalToEqu(hProxy, p, &plng, &plat, &prad);
		const double e = oapiSurfaceElevation(hProxy, plng, plat);
		const double drop = d * d / (2.0 * R);        // curvature
		const double ang = atan2(e - eyeElv - drop, d);
		if (ang > maxAng) maxAng = ang;
	}

	const double sunAng = asin(sinel < -1.0 ? -1.0 : (sinel > 1.0 ? 1.0 : sinel));
	// Fade to ZERO as the sun's CENTRE reaches the ridge (round 4 - the DIAG run:
	// the old band kept vis ~0.15 alive for minutes with the sun already behind
	// rock, because it modelled the peeking upper limb. A glare sprite is CENTRED
	// on the sun, so once the centre is behind the crest a centred dot is wrong
	// regardless of the limb - and the measured ridge tends to read a shade LOW,
	// which the hard zero also absorbs).
	double t = (sunAng - maxAng) / 0.010;
	return (float)(t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t));
}

void Scene::RenderGlares()
{
	// -------------------------------------------------------------------------------------------------------
	// Render glares for the Sun and local lights
	// -------------------------------------------------------------------------------------------------------

	if (pRenderGlares && pLocalResultsSL)
	{
		static SMVERTEX Vertex[4] = { {-1, -1, 0, 0, 0}, {-1, 1, 0, 0, 1}, {1, 1, 0, 1, 1}, {1, -1, 0, 1, 0} };
		static WORD cIndex[6] = { 0, 2, 1, 0, 3, 2 };
		D3DSURFACE_DESC desc; FVECTOR2 pt;
		struct { D3DXMATRIX	mVP; float4	Pos, Color;	float GPUId, Alpha, Blend; } Const;

		Const.Color = FVECTOR4(1, 1, 1, 1);
		D3DXMatrixOrthoOffCenterLH(&Const.mVP, 0.0f, (float)viewW, (float)viewH, 0.0f, 0.0f, 1.0f);
		pLocalResultsSL->GetDesc(&desc);

		pRenderGlares->ClearTextures();
		pRenderGlares->Setup(pPosTexDecl, false, 1);
		pRenderGlares->SetTextureVS("tVis", pLocalResults, IPF_CLAMP | IPF_POINT); // Set texture containing pre-cumputed visibility factors

		if (Config->bGlares && pSunGlare)
		{
			pRenderGlares->SetTexture("tTex0", pSunGlare, IPF_CLAMP | IPF_LINEAR);
			pRenderGlares->UpdateTextures();

			// Render Sun glare
			VECTOR3 gsun; oapiGetGlobalPos(oapiGetObjectByIndex(0), &gsun);
			double sdst = length(gsun - Camera.pos);
			VECTOR3 usun = (gsun - Camera.pos) / sdst;
			VECTOR3 pos = usun * 10e4;

			if (WorldToScreenSpace2(pos, &pt))
			{
				float cis = 1.0f, glare = float(Config->GFXGlare) * saturate(8.0 * AU / sdst);
				// ORO patch (s) part 2: no sun disc under an overcast, so no glare. Fading
				// the INTENSITY, not disabling the glare system - GBUF_DEPTH (patch g's
				// depth buffer) is created by the glare config flags and must survive.
				{ extern float g_gcStormLight; glare *= (1.0f - g_gcStormLight); }
				// ORO: and no glare through a mountain - see OroSunTerrainVis above.
				// (The ONLY survivor of the 2026-09-01 sun-disc experiments, his
				// ruling: everything else in this path is bit-stock.)
				glare *= OroSunTerrainVis(Camera.pos, Camera.hObj_proxy, usun);
				// ORO patch (aa): and the sun disc dims through the fog above the camera.
				glare *= g_oroFogSunCam;
				FVECTOR4 clr = FVECTOR4(1, 1, 1, 1);

				vPlanet* vp = GetCameraNearVisual();
			
				if (vp && vp->IsActive())
				{
					VECTOR3 crp = vp->CameraPos();
					clr = vp->SunLightColor(crp, 2.0);
					cis = CameraInSpace();
					glare *= pow(clr.MaxRGB(), 0.33f) * cis;				
				}
							
				float cd = length(pt - FVECTOR2(viewW, viewH) * 0.5f) / float(viewW); // Glare distance from a screen center
				float alpha = 2.0f * glare * max(0.5f, 1.0f - cd);
				float size = 300.0f * GetDisplayScale() * pow(alpha, 0.25f);

				Const.GPUId = 0.5f / float(desc.Width);
				Const.Pos = FVECTOR4(pt.x, pt.y, size, size);
				Const.Color.rgb = clr.rgb / (clr.MaxRGB() + 0.0001f);
				Const.Alpha = alpha * 2.0f;
				Const.Blend = sqrt(cis);

				pRenderGlares->SetVSConstants("Const", &Const, sizeof(Const));
				pRenderGlares->SetPSConstants("Const", &Const, sizeof(Const));
				HR(pDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, &cIndex, D3DFMT_INDEX16, &Vertex, sizeof(SMVERTEX)));		
			}
		}

		if (Config->bLocalGlares && pLightGlare)
		{
			pRenderGlares->SetTexture("tTex0", pLightGlare, IPF_CLAMP | IPF_LINEAR);
			pRenderGlares->UpdateTextures();

			// Render glares for local lights
			for (int i = 0; i < nLights; ++i) {
				int GPUId = Lights[i].GPUId;
				if (GPUId >= 0) {
					if (WorldToScreenSpace2(_V(Lights[i].Position), &pt)) {
						float size = 40.0f;
						Const.GPUId = (float(GPUId) + 0.5f) / desc.Width;
						Const.Pos = FVECTOR4(pt.x, pt.y, size, size);
						Const.Alpha = Lights[i].cone;
						Const.Color = Lights[i].Diffuse;
						Const.Blend = 1.0f;
						pRenderGlares->SetVSConstants("Const", &Const, sizeof(Const));
						pRenderGlares->SetPSConstants("Const", &Const, sizeof(Const));
						HR(pDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, &cIndex, D3DFMT_INDEX16, &Vertex, sizeof(SMVERTEX)));
					}
				}
			}
		}
		pRenderGlares->DetachTextures();
	}
}


// ===========================================================================================
//
bool Scene::IsVisibleInCamera(const D3DXVECTOR3 *pCnt, float radius)
{
	float z = Camera.z.x*pCnt->x + Camera.z.y*pCnt->y + Camera.z.z*pCnt->z;
	if (z<(-radius)) return false;
	if (z<0) z=-z;
	float y = Camera.y.x*pCnt->x + Camera.y.y*pCnt->y + Camera.y.z*pCnt->z;
	if (y<0) y=-y;
	if (y-(radius*Camera.vhf) > (Camera.vh*z)) return false;
	float x = Camera.x.x*pCnt->x + Camera.x.y*pCnt->y + Camera.x.z*pCnt->z;
	if (x<0) x=-x;
	if (x-(radius*Camera.vwf) > (Camera.vw*z)) return false;
	return true;
}

// ===========================================================================================
//
bool Scene::CameraDirection2Viewport(const VECTOR3 &dir, int &x, int &y)
{
	D3DXVECTOR3 homog;
	D3DXVECTOR3 idir = D3DXVECTOR3( -float(dir.x), -float(dir.y), -float(dir.z) );
	D3DMAT_VectorMatrixMultiply(&homog, &idir, &Camera.mProjView);
	if (homog.x >= -1.0f && homog.y <= 1.0f && homog.z >= 0.0) {
		if (std::hypot(homog.x, homog.y) < 1e-6) {
			x = viewW / 2, y = viewH / 2;
		} else {
			x = (int)(viewW*0.5f*(1.0f + homog.x));
			y = (int)(viewH*0.5f*(1.0f - homog.y));
		}
		return true;
	}
	return false;
}

// ===========================================================================================
//
void Scene::GlobalExit()
{
	SAFE_RELEASE(FX);
}

// ===========================================================================================
//
void Scene::D3D9TechInit(LPDIRECT3DDEVICE9 pDev, const char *folder)
{
	char name[256];
	sprintf_s(name,256,"Modules/%s/SceneTech.fx", folder);

	// Create the Effect from a .fx file.
	ID3DXBuffer* errors = 0;

	HR(D3DXCreateEffectFromFile(pDev, name, 0, 0, 0, 0, &FX, &errors));

	if (errors) {

		// It's an error
		//
		if (strstr((char*)errors->GetBufferPointer(),"warning")==NULL) {
			LogErr("Effect Error: %s",(char*)errors->GetBufferPointer());
			MessageBoxA(0, (char*)errors->GetBufferPointer(), "SceneTech.fx Error", 0);
			return;
		}

		// It's a warning
		//
		else {
			LogErr("[Effect Warning: %s]",(char*)errors->GetBufferPointer());
			//MessageBoxA(0, (char*)errors->GetBufferPointer(), "CelSphereTech.fx Warning", 0);
		}
	}

	if (FX==0) {
		LogErr("Failed to create an Effect (%s)",name);
		return;
	}

	eLine  = FX->GetTechniqueByName("LineTech");
	eStar  = FX->GetTechniqueByName("StarTech");
	eWVP   = FX->GetParameterByName(0,"gWVP");
	eTex0  = FX->GetParameterByName(0,"gTex0");
	eColor = FX->GetParameterByName(0,"gColor");

	D3D9CelestialSphere::D3D9TechInit(FX);
}

// ===========================================================================================
//
int distcomp (const void *arg1, const void *arg2)
{
	double d1 = ((PList*)arg1)->dist;
	double d2 = ((PList*)arg2)->dist;
	return (d1 > d2 ? -1 : d1 < d2 ? 1 : 0);
}
