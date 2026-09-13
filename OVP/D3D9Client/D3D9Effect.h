// ===========================================================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2011 - 2016 Jarmo Nikkanen
// ===========================================================================================

#ifndef __D3D9EFFECT_H
#define __D3D9EFFECT_H

#define D3D9SM_SPHERE	0x01
#define D3D9SM_ARROW	0x02

#include "D3D9Client.h"
#include <d3d9.h> 
#include <d3dx9.h>

// NOTE: a "bool" in HLSL is 32bits (i.e. int)
// Must match with counterpart in D3D9Client.fx

struct TexFlow {
	BOOL Emis;		// Enable Emission Maps
	BOOL Spec;		// Enable Specular Maps
	BOOL Refl;		// Enable Reflection Maps
	BOOL Transl;	// Enable translucent effect
	BOOL Transm;	// Enable transmissive effect
	BOOL Rghn;		// Enable roughness map
	BOOL Norm;		// Enable normal map
	BOOL Metl;		// Enable metalness map
	BOOL Heat;		// Enable heat map
};



using namespace oapi;

class D3D9Effect {

	DWORD d3d9id;

public:
	static void D3D9TechInit(D3D9Client *gc, LPDIRECT3DDEVICE9 pDev, const char *folder);

	/**
	 * \brief Release global parameters
	 */
	static void GlobalExit();

	static void ShutDown();

	D3D9Effect();
	~D3D9Effect();

	static void EnablePlanetGlow(bool bEnabled);
	static void UpdateEffectCamera(OBJHANDLE hPlanet);
	static void InitLegacyAtmosphere(OBJHANDLE hPlanet, float GlobalAmbient);
	static void SetViewProjMatrix(LPD3DXMATRIX pVP);

	static void RenderLines(const D3DXVECTOR3 *pVtx, const WORD *pIdx, int nVtx, int nIdx, const D3DXMATRIX *pW, DWORD color);
	static void RenderTileBoundingBox(const LPD3DXMATRIX pW, VECTOR4 *pVtx, const LPD3DXVECTOR4 color);
	static void RenderBoundingBox(const LPD3DXMATRIX pW, const LPD3DXMATRIX pGT, const D3DXVECTOR4 *bmin, const D3DXVECTOR4 *bmax, const D3DXVECTOR4 *color);
	static void RenderBoundingSphere(const LPD3DXMATRIX pW, const LPD3DXMATRIX pGT, const D3DXVECTOR4 *bs, const D3DXVECTOR4 *color);
	static void RenderBillboard(const LPD3DXMATRIX pW, LPDIRECT3DTEXTURE9 pTex, float alpha = 1.0f);
	static void RenderExhaust(const LPD3DXMATRIX pW, VECTOR3 &cdir, EXHAUSTSPEC *es, SURFHANDLE def);
	static void RenderSpot(float intens, const LPD3DXCOLOR color, const LPD3DXMATRIX pW, SURFHANDLE pTex);
	static void Render2DPanel(const MESHGROUP *mg, const SURFHANDLE pTex, const LPD3DXMATRIX pW, float alpha, float scale, bool additive);
	static void RenderReEntry(const SURFHANDLE pTex, const LPD3DXVECTOR3 vPosA, const LPD3DXVECTOR3 vPosB, const LPD3DXVECTOR3 vDir, float alpha_a, float alpha_b, float size);
	static void RenderArrow(OBJHANDLE hObj, const VECTOR3 *ofs, const VECTOR3 *dir, const VECTOR3 *rot, float size, const D3DXCOLOR *pColor);  
	
	static LPDIRECT3DDEVICE9 pDev;      ///< Static (global) render device
	static LPDIRECT3DVERTEXBUFFER9 VB;  ///< Static (global) Vertex buffer pointer
	
	static D3DXVECTOR4 atm_color;		///< Earth glow color

	// Rendering Technique related parameters
	static ID3DXEffect	*FX;
	static D3D9Client   *gc; ///< The graphics client instance

	static D3D9MatExt	mfdmat;
	static D3D9MatExt	defmat;
	static D3D9MatExt	night_mat;
	static D3D9MatExt	emissive_mat;
	
	// Techniques ----------------------------------------------------
	static D3DXHANDLE	eVesselTech;     ///< Vessel exterior, surface bases
	static D3DXHANDLE	eSimple;
	static D3DXHANDLE	eBBTech;         ///< Bounding Box Tech
	static D3DXHANDLE	eTBBTech;        ///< Bounding Box Tech
	static D3DXHANDLE	eBSTech;         ///< Bounding Sphere Tech
	static D3DXHANDLE   eExhaust;        ///< Render engine exhaust texture
	static D3DXHANDLE   eSpotTech;       ///< Vessel beacons
	static D3DXHANDLE   ePanelTech;      ///< Used to draw a new style 2D panel
	static D3DXHANDLE   ePanelTechB;     ///< Used to draw a new style 2D panel
	static D3DXHANDLE	eBaseTile;
	static D3DXHANDLE	eWetOverlay;     ///< ORO 2026-09-10: the wet-ground look over runways/pads/taxiways (Mesh.fx WetOverlayTech)
	static D3DXHANDLE	eRingTech;       ///< Planet rings technique
	static D3DXHANDLE	eRingTech2;      ///< Planet rings technique
	static D3DXHANDLE	eRingTechORO;    ///< ORO patch (aj): the ORO ring (linear radius, real tau, lit/unlit faces)
	static D3DXHANDLE	eShadowTech;     ///< Vessel ground shadows
	static D3DXHANDLE	eGeometry;
	static D3DXHANDLE	eBaseShadowTech; ///< Used to draw transparent surface without texture
	static D3DXHANDLE	eBeaconArrayTech;
	static D3DXHANDLE	eArrowTech;      ///< (Grapple point) arrows
	static D3DXHANDLE	eAxisTech;
	static D3DXHANDLE	ePlanetTile;
	static D3DXHANDLE	eCloudTech;
	static D3DXHANDLE	eCloudShadow;
	static D3DXHANDLE	eSkyDomeTech;
	static D3DXHANDLE	eDiffuseTech;
	static D3DXHANDLE	eEmissiveTech;
	static D3DXHANDLE	eHazeTech;
	static D3DXHANDLE	eSimpMesh;

	// Transformation Matrices ----------------------------------------
	static D3DXHANDLE	eVP;         ///< Combined View & Projection Matrix
	static D3DXHANDLE	eW;          ///< World Matrix
	static D3DXHANDLE	eLVP;        ///< Light view projection
	static D3DXHANDLE	eGT;         ///< MeshGroup transformation matrix

	// Lighting related parameters ------------------------------------
	static D3DXHANDLE   eMtrl;
	static D3DXHANDLE   eTune;
	static D3DXHANDLE	eMat;        ///< Material
	static D3DXHANDLE	eWater;      ///< Water
	static D3DXHANDLE	eSun;        ///< Sun
	static D3DXHANDLE	eLights;     ///< Additional light sources
	static D3DXHANDLE	eLclShdP;    ///< ORO patch (ah) step 4: per spot map - origin xyz, w = cell*10 + tan(fov/2)
	static D3DXHANDLE	eLclShdD;    ///< ORO patch (ah) step 4: per spot map - axis xyz, w = range
	static D3DXHANDLE	eLclShd;     ///< ORO patch (z3) 2b / (ah) 4: live maps, cells per row, first row's v, cell size in texels
	static D3DXHANDLE	eLclAtl;     ///< ORO patch (ah) step 4: cell width/height in uv, one texel in uv
	static D3DXHANDLE	eLclShmTex;  ///< ORO patch (z3) 2b: the local-light shadow map
	static D3DXHANDLE	eKernel;
	static D3DXHANDLE	eAtmoParams;

	// Auxiliary params ----------------------------------------------
	static D3DXHANDLE   eModAlpha;     ///< BOOL multiply material alpha with texture alpha
	static D3DXHANDLE	eFullyLit;     ///< BOOL
	static D3DXHANDLE	eFlow;		   ///< BOOL
	static D3DXHANDLE	eShadowToggle; ///< BOOL
	static D3DXHANDLE	eEnvMapEnable; ///< BOOL
	static D3DXHANDLE	eInSpace;      ///< BOOL
	static D3DXHANDLE	eNoColor;      ///< BOOL
	static D3DXHANDLE	eLightsEnabled;///< BOOL
	static D3DXHANDLE	eBaseBuilding; ///< BOOL
	static D3DXHANDLE	eTuneEnabled;  ///< BOOL
	static D3DXHANDLE	eFresnel;	   ///< BOOL
	static D3DXHANDLE   eSwitch;	   ///< BOOL
	static D3DXHANDLE   eRghnSw;	   ///< BOOL
	static D3DXHANDLE	eTextured;	   ///< BOOL
	static D3DXHANDLE	eOITEnable;	   ///< BOOL
	static D3DXHANDLE	eInvProxySize;
	static D3DXHANDLE	eMix;          ///< FLOAT Auxiliary factor/multiplier
	static D3DXHANDLE	eVCShdDepth;   ///< FLOAT ORO patch (p): VC shadow ambient bite, 0..1
	static D3DXHANDLE	eSurfWet;      ///< FLOAT ORO patch (s): ground wetness, 0..1
	static D3DXHANDLE	eStorm;        ///< FLOAT ORO patch (s) part 2: overcast factor, 0..1
	static D3DXHANDLE	eWetDark;      ///< FLOAT ORO patch (s) part 3: wet albedo darkening, 0..2
	static D3DXHANDLE	eWetTime;      ///< FLOAT ORO patch (s): real-time clock for the rain sparkle
	static D3DXHANDLE	eWetGlint;     ///< FLOAT ORO patch (s) part 5: hull glint gain, 0..2
	static D3DXHANDLE	eFogPrm;       ///< FLOAT4[6] ORO patch (aa): fog camera/layer constants
	static D3DXHANDLE	eFogClr;       ///< FLOAT4[3] ORO patch (aa): fog colour, sun lobe, ambient lift
	static D3DXHANDLE	eSnow;         ///< FLOAT4 ORO patch (aa): snow cover, line, 1/width
	static D3DXHANDLE	eSceneDepth;   ///< TEXTURE ORO patch (ab): GBUF_DEPTH for the soft shadow test
	static D3DXHANDLE	eSceneDepthPrm;///< FLOAT4 ORO patch (ab): 1/W, 1/H, tolerance base, tolerance/m
	static D3DXHANDLE	eOroDbg;       ///< FLOAT ORO patch (ab) INSTRUMENT: ShadowDebug mode
	static D3DXHANDLE	eCascBasis;    ///< FLOAT4[2] ORO patch (ae): the shared light basis U, V (L = -U x V in the shader)
	static D3DXHANDLE	eCascA;        ///< FLOAT4[7] ORO patch (ae): per slot centre u, v, near depth, 1/range - focus, cascades 1-3, three hull boxes
	static D3DXHANDLE	eCascTx;       ///< FLOAT4[2] ORO patch (ae): those seven slots' texels (m), four to a register
	static D3DXHANDLE	eCascSplit;    ///< FLOAT4 ORO patch (ae): split distances
	static D3DXHANDLE	eCascAtlas;    ///< FLOAT4 ORO patch (ae): 1/atlas, slot scale, ON
	static D3DXHANDLE	eCascMap;      ///< TEXTURE ORO patch (ae): the atlas
	static D3DXHANDLE	eBaseGlow;     ///< FLOAT ORO patch (ac): base night-light glow gain, 1 = stock
	static D3DXHANDLE	eBaseHalo;     ///< FLOAT ORO patch (ac) part 2: the fog aureole gain, 1 = designed
	static D3DXHANDLE	eBaseLocal;    ///< FLOAT ORO 2026-09-10 (A5): 1 during base structure draws - drop glint in base-local metres
	static D3DXHANDLE	eBaseGround;   ///< FLOAT ORO 2026-09-11: 1 during BELOW-SHADOW base surfaces (runway/pad/taxiway) - ground wet darkening
	static D3DXHANDLE	eWetReflTex;   ///< TEXTURE ORO patch (s) part 6: the planar mirror
	static D3DXHANDLE	eWetReflPrm;   ///< FLOAT4 ORO patch (s) part 6: 1/W, 1/H, gain, live
	static D3DXHANDLE	eWetSwimPrm;   ///< FLOAT4 ORO patch (s) part 6: swim amp scale, rate scale, 0, 0
	static D3DXHANDLE	eWetGrainPrm;  ///< FLOAT4 ORO patch (s) part 7: grain opacity, grain size, 0, 0
	static D3DXHANDLE	eRingPrm;      ///< FLOAT4 ORO patch (aj): blend, density trim, lit brightness, backlit glow
	static D3DXHANDLE	eRingRad;      ///< FLOAT4 ORO patch (aj): irad [m], orad [m], 1/(orad-irad), 0
	static D3DXHANDLE	eRingShd;      ///< FLOAT4 ORO patch (aj): ring plane normal (world) xyz, 0 - the planet's ring shadow
	static D3DXHANDLE	eRingProf;     ///< TEXTURE ORO patch (aj): the profile, N x 1, linear in radius, A = encoded tau
	static D3DXHANDLE	eRingPrm2;     ///< FLOAT4 ORO patch (aj) round 2: detail amp, particle amp, along-track cell offset, radial cell offset
	static D3DXHANDLE	eRingPrm3;     ///< FLOAT4 ORO patch (aj) round 2: cells/m along track, cells/m radial, along-track cell [m], radial cell [m]
	static D3DXHANDLE	eRingAxR;      ///< FLOAT4 ORO patch (aj) round 2: the camera's RADIAL direction in the ring plane (world) xyz, 0
	static D3DXHANDLE	eRingAxT;      ///< FLOAT4 ORO patch (aj) round 2: the camera's ALONG-TRACK direction in the ring plane (world) xyz, 0
	static D3DXHANDLE	eRingCut;      ///< FLOAT4 ORO patch (aj) round 2: camera forward (world) xyz + the near-field seam depth [m] (sign = which side of the seam this draw is; 0 = none)
	static D3DXHANDLE	eRingPrm4;     ///< FLOAT4 ORO patch (aj) round 2: the look's lanes 12..15 - relief, reserved x3
	static D3DXHANDLE   eColor;        ///< Auxiliary color input
	static D3DXHANDLE   eFogColor;     ///< Fog color input
	static D3DXHANDLE   eTexOff;       ///< Surface tile texture offsets
	static D3DXHANDLE	eSpecularMode;
	static D3DXHANDLE	eHazeMode;
	static D3DXHANDLE   eTime;         ///< FLOAT Simulation elapsed time
	static D3DXHANDLE	eExposure;
	static D3DXHANDLE	eCameraPos;	
	static D3DXHANDLE   eNorth;
	static D3DXHANDLE	eEast;
	static D3DXHANDLE   eDistScale;
	static D3DXHANDLE   eGlowConst;
	static D3DXHANDLE   eRadius;
	static D3DXHANDLE	eFogDensity;
	static D3DXHANDLE	ePointScale;
	static D3DXHANDLE	eAtmColor;
	static D3DXHANDLE	eProxySize;
	static D3DXHANDLE	eMtrlAlpha;
	static D3DXHANDLE	eAttennuate;
	static D3DXHANDLE	eInScatter;
	static D3DXHANDLE	eSHD;
	static D3DXHANDLE	eNight;

	// Textures --------------------------------------------------------
	static D3DXHANDLE	eTex0;    ///< Primary texture
	static D3DXHANDLE	eTex1;    ///< Secondary texture
	static D3DXHANDLE	eTex3;    ///< Tertiary texture
	static D3DXHANDLE	eSpecMap;
	static D3DXHANDLE	eEmisMap;
	static D3DXHANDLE	eEnvMapA;
	static D3DXHANDLE	eEnvBoxC, eEnvBoxX, eEnvBoxY, eEnvBoxZ, eEnvPrbP;	// ORO patch (v)
	static D3DXHANDLE	ePlnMap, ePlnCtl, ePlnEq;	// ORO patch (v) part 2: planar mirror
	static D3DXHANDLE	ePShnMap, ePShnLVP, ePShnSHD;	// ORO patch (w): planet-shine shadows
	static D3DXHANDLE	eEnvMapB;
	static D3DXHANDLE	eReflMap;
	static D3DXHANDLE	eMetlMap;
	static D3DXHANDLE	eHeatMap;
	static D3DXHANDLE	eRghnMap;
	static D3DXHANDLE	eTranslMap;
	static D3DXHANDLE	eTransmMap;
	static D3DXHANDLE	eShadowMap;
	static D3DXHANDLE	eIrradMap;

	// Legacy Atmosphere -----------------------------------------------
	static D3DXHANDLE	eGlobalAmb;	 
	static D3DXHANDLE	eSunAppRad;	 
	static D3DXHANDLE	eAmbient0;	 
	static D3DXHANDLE	eDispersion;	  
};

#endif // !__D3D9EFFECT_H
