// ============================================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// licensed under LGPL v2
// Copyright (C) 2022 Jarmo Nikkanen
// ============================================================================

#include "Scatter.hlsl"

struct _Light
{
	float3   position[4];         /* position in world space */
	float3   direction[4];        /* direction in world space */
	float3   diffuse[4];          /* diffuse color of light */
	float3   attenuation[4];      /* Attenuation */
	float4   param[4];            /* range, falloff, theta, phi */
};

#define Range   0
#define Falloff 1
#define Theta   2
#define Phi     3

#define ATMNOISE 0.25
#define GLARE_SIZE 5	// Larger value -> smaller

// ----------------------------------------------------------------------------
// Vertex input layouts from Vertex buffers to vertex shader
// ----------------------------------------------------------------------------

struct TILEVERTEX					// (VERTEX_2TEX) Vertex declaration used for surface tiles and cloud layer
{
	float3 posL     : POSITION0;
	float3 normalL  : NORMAL0;
	float2 tex0     : TEXCOORD0;
	float  elev     : TEXCOORD1;
};

// ----------------------------------------------------------------------------
// Vertex Shader to Pixel Shader datafeeds
// ----------------------------------------------------------------------------

struct TileVS
{
	float4 posH     : POSITION0;
	float2 texUV    : TEXCOORD0;  // Texture coordinate
	float4 camW		: TEXCOORD1;  // Radius in .w
	float3 nrmW		: TEXCOORD2;
#if defined(_SHDMAP)
	float4 shdH     : TEXCOORD3;
#endif
};

struct CldVS
{
	float4 posH     : POSITION0;
	float2 texUV    : TEXCOORD0;  // Texture coordinate
	float3 nrmW		: TEXCOORD1;
	float3 posW		: TEXCOORD2;
};

struct HazeVS
{
	float4 posH    : POSITION0;
	float2 texUV   : TEXCOORD0;
	float3 posW	   : TEXCOORD1;
	float  alpha   : COLOR0;
};


// Note: "bool" is 32-bits in a shaders (max count 16) 
//
struct FlowControlPS
{
	BOOL bInSpace;				// Camera in space (not in atmosphere)
	BOOL bBelowClouds;			// Camera is below cloud layer
	BOOL bOverlay;				// Overlay on/off	
	BOOL bShadows;				// Shadow Map on/off
	BOOL bLocals;				// Local Lights on/off
	BOOL bMicroNormals;			// Micro texture has normals
	BOOL bCloudShd;				// Cloud shadow textures valid and enabled
	BOOL bMask;					// Nightlights/water mask texture is enabled
	BOOL bRipples;				// Water riples texture is enabled
	BOOL bMicroTex;				// Micro textures exists and enabled
	BOOL bPlanetShadow;			// Use spherical approximation for shadow
	BOOL bEclipse;				// Eclipse is occuring
	BOOL bTexture;				// Surface texture exists
};

struct FlowControlVS
{
	BOOL bInSpace;				// Camera in space (not in atmosphere)
	BOOL bSpherical;			// Ignore elevation, render as sphere
	BOOL bElevOvrl;				// ElevOverlay on/off			
};

struct PerObjectParams
{
	float4x4 mWorld;			// World Matrix
	float4x4 mLVP;				// Light-View-Projection
	float4   vSHD;				// Shadow Map Parameters
	float4   vMSc[3];			// Micro Texture offset-scale
	float4	 vTexOff;			// Texture offset-scale
	float4   vCloudOff;			// Cloud texture offset-scale
	float4   vMicroOff;			// Micro texture offset-scale
	float4   vOverlayOff;       // Overlay texture offset-scale
	float4   vOverlayCtrl[4];
	float3	 vEclipse;			// Eclipse caster position (geocentric)
	float	 fEclipse;			// Eclipse data addressing scale factor. (to access tExlipse)
	float	 fAlpha;
	float	 fBeta;
	float	 fTgtScale;
};

uniform extern float gWet;            // ORO patch (s): ground wetness, 0..1
uniform extern float gStorm;          // ORO patch (s) part 2: overcast factor, 0..1
uniform extern float gWetDark;        // ORO patch (s) part 3: wet albedo darkening gain
uniform extern float4 gWetReflPrm;    // ORO patch (s) part 6: 1/W, 1/H, gain, live
uniform extern float4 gWetSwimPrm = {1, 1, 1, 1};  // ORO patch (s): swim amp, swim rate, pool size, pool reach
uniform extern float4 gWetGrainPrm = {1, 1, 0, 0}; // ORO patch (s) part 7: grain opacity, grain size
sampler tWetRefl;                     // ORO patch (s) part 6: the planar mirror
uniform extern PerObjectParams Prm;
uniform extern FlowControlPS Flow;
uniform extern FlowControlVS FlowVS;
uniform extern _Light Lights;			// Note: DX9 doesn't tolerate structure arrays outside FX framework
uniform extern bool Spotlight[4];


sampler tDiff;				// Diffuse texture
sampler tMask;				// Nightlights / Specular mask texture
sampler tCloud;				// 1st Cloud shadow texture
sampler tCloud2;			// 2nd Cloud shadow texture
sampler tCloudMicro;		
sampler tCloudMicroNorm;
sampler tNoise;				//
sampler	tOcean;				// Ocean Normal Map Texture
sampler	tMicroA;
sampler	tMicroB;
sampler	tMicroC;
sampler tGlare;
sampler	tShadowMap;
sampler	tOverlay;
sampler	tMskOverlay;
sampler	tElvOverlay;
sampler	tEclipse;




// ---------------------------------------------------------------------------------------------------
//
float SampleShadows(float2 sp, float pd)
{
	if (sp.x < 0 || sp.y < 0) return 0.0f;	// If a sample is outside border -> fully lit
	if (sp.x > 1 || sp.y > 1) return 0.0f;

	if (pd < 0) pd = 0;
	if (pd > 2) pd = 2;

	float2 dx = float2(Prm.vSHD[1], 0) * 1.5f;
	float2 dy = float2(0, Prm.vSHD[1]) * 1.5f;
	float  va = 0;

	sp -= dy;
	if ((tex2D(tShadowMap, sp - dx).r) > pd) va++;
	if ((tex2D(tShadowMap, sp).r) > pd) va++;
	if ((tex2D(tShadowMap, sp + dx).r) > pd) va++;
	sp += dy;
	if ((tex2D(tShadowMap, sp - dx).r) > pd) va++;
	if ((tex2D(tShadowMap, sp).r) > pd) va++;
	if ((tex2D(tShadowMap, sp + dx).r) > pd) va++;
	sp += dy;
	if ((tex2D(tShadowMap, sp - dx).r) > pd) va++;
	if ((tex2D(tShadowMap, sp).r) > pd) va++;
	if ((tex2D(tShadowMap, sp + dx).r) > pd) va++;

	return va * 0.1111111f;
}

// -------------------------------------------------------------------------------------------------------------
// Local light sources
//
void LocalLights(
	out float3 diff_out,
	in float3 nrmW,
	in float3 posW)
{
	diff_out = 0;

	if (!Flow.bLocals) return;
	int i;

	// Relative positions
	float3 p[4];
	[unroll] for (i = 0; i < 4; i++) p[i] = posW - Lights.position[i];

	// Square distances
	float4 sd;
	[unroll] for (i = 0; i < 4; i++) sd[i] = dot(p[i], p[i]);

	// Normalize
	sd = rsqrt(sd);
	[unroll] for (i = 0; i < 4; i++) p[i] *= sd[i];

	// Distances
	float4 dst = rcp(sd);

	// Attennuation factors
	float4 att;
	[unroll] for (i = 0; i < 4; i++) att[i] = dot(Lights.attenuation[i].xyz, float3(1.0, dst[i], dst[i] * dst[i]));

	att = rcp(att);

	// Spotlight factors
	float4 spt = 1;
	
	[unroll] for (i = 0; i < 4; i++) {
		spt[i] = (dot(p[i], Lights.direction[i]) - Lights.param[i][Phi]) * Lights.param[i][Theta];
		if (!Spotlight[i]) spt[i] = 1.0f;
	}

	spt = saturate(spt);

	// Diffuse light factors
	float4 dif;
	[unroll] for (i = 0; i < 4; i++) dif[i] = dot(-p[i], nrmW);

	dif = saturate(dif);
	dif *= (att * spt);

	[unroll] for (i = 0; i < 4; i++) diff_out += Lights.diffuse[i].rgb * dif[i];
}


// Render Eclipse ------------------------------------------------------------
//
float GetEclipse(float3 vVrt)
{
	if (Flow.bEclipse)
	{
		float3 b = vVrt - Const.toSun * dot(vVrt, Const.toSun); // Flatten
		float  x = length(Prm.vEclipse - b) * Prm.fEclipse;
		return tex1D(tEclipse, saturate(x)).r;
	}
	return 1.0;
}
	


// ============================================================================
// Render SkyDome and Horizon
// ============================================================================

HazeVS HorizonVS(float3 posL : POSITION0)
{
	// Zero output.
	HazeVS outVS = (HazeVS)0;

	outVS.texUV = posL.xy*10.0;

	posL.xz *= lerp(Prm.vTexOff[0], Prm.vTexOff[1], posL.y);
	posL.y   = lerp(Prm.vTexOff[2], Prm.vTexOff[3], posL.y);

	outVS.posW = mul(float4(posL, 1.0f), Prm.mWorld).xyz;
	outVS.posH = mul(float4(outVS.posW, 1.0f), Const.mVP);

	return outVS;
}


// SkyDome Shader, Renders the sky from with-in atmosphere
//
float4 HorizonPS(HazeVS frg) : COLOR
{
	float fNoise = (tex2Dlod(tNoise, float4(frg.texUV, 0, 0)).r - 0.5f) * 0.03;

	float3 uDir = normalize(frg.posW);

	SkyOut sky = GetSkyColor(uDir);
	
	float ph = dot(uDir, Const.toSun);

	float2  guv = float2(dot(uDir, Const.ZeroAz), dot(uDir, Const.Up)) * GLARE_SIZE + 0.5f;
	float  cGlr = tex2D(tGlare, guv).r * saturate(ph) * Const.SunVis;
	
	float3 color = HDR(sky.ray.rgb * RayPhase(ph) + (sky.mie.rgb + 0.0008f) * MiePhase(ph) * (0.75f + cGlr * Const.cGlare));

	return float4(color + fNoise, sky.ray.a);
}


// Renders the horizon "ring" from space
//
float4 HorizonRingPS(HazeVS frg) : COLOR
{
	float3 uDir = normalize(frg.posW);
	float3 uOrt = normalize(uDir - Const.toCam * dot(uDir, Const.toCam));
	float3 vVrt = Const.CamPos + frg.posW;
	float d = dot(uDir, frg.posW);
	float x = dot(uOrt, Const.SunAz) * 0.5 + 0.5;
	float r = length(vVrt);
	float q = (r - Const.PlanetRad) / Const.AtmoAlt;

	float2 uv = float2(x, q > 0 ? sqrt(q) : 0);

	float4 cRay = tex2D(tSkyRayColor, uv).rgba;
	float3 cMie = tex2D(tSkyMieColor, uv).rgb;

	float ph = dot(uDir, Const.toSun);

	float3 color = HDR(cRay.rgb * RayPhase(ph) + cMie * MiePhase(ph));

	color *= GetEclipse(vVrt);

	return float4(color, cRay.a);
}



// ============================================================================
// Planet Surface Renderer
// ============================================================================

#define AUX_DIST		0	// Vertex distance
#define AUX_NIGHT		1	// Night lights intensity
#define AUX_SLOPE		2   // Terrain slope factor 0.0=flat, 1.0=sloped
#define AUX_RAYDEPTH	3   // Optical depth of a ray

TileVS TerrainVS(TILEVERTEX vrt)
{
	// Zero output.
	TileVS outVS = (TileVS)0;
	float4 vElev = 0;
	float3 vNrmW;
	
	// Apply a world transformation matrix
	float3 vPosW = mul(float4(vrt.posL, 1.0f), Prm.mWorld).xyz;
	float3 vVrt = Const.CamPos + vPosW;
	float3 vPlN = normalize(vVrt);

	if (FlowVS.bElevOvrl)
	{
		// ----------------------------------------------------------
		// Elevation Overlay
		//
		float2 vUVOvl = vrt.tex0.xy * Prm.vOverlayOff.zw + Prm.vOverlayOff.xy;

		// Sample Elevation Map
		vElev = tex2Dlod(tElvOverlay, float4(vUVOvl, 0, 0));

		// Construct world space normal
		vNrmW = float3(vElev.xy, sqrt(saturate(1.0f - dot(vElev.xy, vElev.xy))));
		vNrmW = mul(float4(vNrmW, 0.0f), Prm.mWorld).xyz;

		// Reconstruct Elevation
		vPosW += normalize(Const.CamPos + vPosW) * (vElev.z - vrt.elev) * vElev.w;	
	}
	else {
		vNrmW = mul(float4(vrt.normalL, 0.0f), Prm.mWorld).xyz;
	}

	// Disrecard elevation and make the surface spherical
	if (FlowVS.bSpherical) {
		vPosW = (normalize(Const.CamPos + vPosW) * Const.PlanetRad) - Const.CamPos;
		vNrmW = vPlN;
	}

	outVS.posH = mul(float4(vPosW, 1.0f), Const.mVP);

#if defined(_SHDMAP)
	outVS.shdH = mul(float4(vPosW, 1.0f), Prm.mLVP);
#endif

	outVS.texUV.xy = vrt.tex0.xy;
	outVS.camW = float4(-vPosW, dot(vVrt, vPlN));
	outVS.nrmW = vNrmW;
	
	return outVS;
}



bool InRange(float2 a)
{
	return (a.x > 0.0f && a.x < 1.0f) && (a.y > 0.0f && a.y < 1.0f);
}

float GGX_NDF(float dHN, float rgh)
{
	float r2 = rgh * rgh;
	float dHN2 = dHN * dHN;
	float d = (r2 * dHN2) + (1.0f - dHN2);
	return r2 / (3.14f * d * d);
}


// ORO patch (s), round 12: the pool grain's value noise - periodic (the wrap) so the
// seam law holds, static in the world (no clock).
float OroGrainHash(float2 c, float wrap)
{
	c = fmod(c, wrap);
	return frac(sin(dot(c, float2(127.1f, 311.7f))) * 43758.5453f);
}
float OroGrainNoise(float2 p, float wrap)
{
	float2 i = floor(p), f = frac(p);
	f = f * f * (3.0f - 2.0f * f);
	float a = OroGrainHash(i, wrap),                b = OroGrainHash(i + float2(1, 0), wrap);
	float c = OroGrainHash(i + float2(0, 1), wrap), d = OroGrainHash(i + float2(1, 1), wrap);
	return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float4 TerrainPS(float4 sc : VPOS, TileVS frg) : COLOR
{

	float2 vUVSrf = frg.texUV.xy * Prm.vTexOff.zw + Prm.vTexOff.xy;
	float2 vUVWtr = frg.texUV.xy * Prm.vMicroOff.zw + Prm.vMicroOff.xy;
	float2 vUVCld = frg.texUV.xy * Prm.vCloudOff.zw + Prm.vCloudOff.xy;

	vUVWtr.x += Const.Time / 180.0f;

	float3 cNrm = float3(0.5, 0.5, 1.0);
	float fChA = 0.0f, fChB = 0.0f;

#if defined(_RIPPLES)
	if (Flow.bTexture) cNrm = tex2D(tOcean, vUVWtr).xyz;
#endif

	// Fetch Main Textures
	float4 cTex = float4(0.5, 0.5, 0.5, 1.0);
	if (Flow.bTexture) cTex = tex2D(tDiff, vUVSrf);

	float4 cMsk = float4(0, 0, 0, 1);
	if (Flow.bMask) cMsk = tex2D(tMask, vUVSrf);

#if defined(_DEVTOOLS)
	if (Flow.bOverlay) {
		float2 vUVOvl = frg.texUV.xy * Prm.vOverlayOff.zw + Prm.vOverlayOff.xy;
		if (InRange(vUVOvl)) {
			float4 cOvl = tex2D(tOverlay, vUVOvl);
			float4 cWtr = tex2D(tMskOverlay, vUVOvl);
			cTex.rgb = lerp(cTex.rgb, cOvl.rgb, cOvl.a * Prm.vOverlayCtrl[0].rgb);
			cMsk.rgb = lerp(cMsk.rgb, cWtr.rgb, cOvl.a * Prm.vOverlayCtrl[1].rgb);
			cMsk.a = lerp(cMsk.a, cWtr.a, Prm.vOverlayCtrl[1].a);
		}
	}
#endif

#if defined(_CLOUDSHD)
	if (Flow.bCloudShd) {
		fChA = tex2D(tCloud, vUVCld).a;
		fChB = tex2D(tCloud2, vUVCld - float2(1, 0)).a;
	}
#endif

	float fShadow = 1.0f;

#if defined(_SHDMAP)
	if (Flow.bShadows) {
		frg.shdH.xyz /= frg.shdH.w;
		frg.shdH.z = 1 - frg.shdH.z;
		float2 sp = frg.shdH.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
		float  pd = frg.shdH.z + 0.05f * Prm.vSHD[3];
		fShadow = 1.0f - SampleShadows(sp, pd);
	}
#endif

	float3 cFar, cMed, cLow;

#if defined(_MICROTEX)
	float2 UV = frg.texUV.xy;
	// Create normals
	if (Flow.bMicroTex)
	{
		if (Flow.bMicroNormals) {
			// Normal in .ag luminance in .b
			cFar = tex2D(tMicroC, UV * Prm.vMSc[2].zw + Prm.vMSc[2].xy).agb;	// High altitude micro texture C
			cMed = tex2D(tMicroB, UV * Prm.vMSc[1].zw + Prm.vMSc[1].xy).agb;	// Medimum altitude micro texture B
			cLow = tex2D(tMicroA, UV * Prm.vMSc[0].zw + Prm.vMSc[0].xy).agb;	// Low altitude micro texture A
		}
		else {
			// Color in .rgb no normals
			cFar = tex2D(tMicroC, UV * Prm.vMSc[2].zw + Prm.vMSc[2].xy).rgb;	// High altitude micro texture C
			cMed = tex2D(tMicroB, UV * Prm.vMSc[1].zw + Prm.vMSc[1].xy).rgb;	// Medimum altitude micro texture B
			cLow = tex2D(tMicroA, UV * Prm.vMSc[0].zw + Prm.vMSc[0].xy).rgb;	// Low altitude micro texture A
		}
	}
#endif

	float3 cRfl = 0;
	float3 nvrW = normalize(frg.nrmW);			// Per-pixel surface normal vector
	float3 vRay = normalize(frg.camW.xyz);		// Unit viewing ray
	float3 vVrt = Const.CamPos - frg.camW.xyz;	// Geo-centric pixel position
	float3 vPlN = normalize(vVrt);				// Planet mean normal
	float3 hlvW = normalize(vRay + Const.toSun);
	float   dst = dot(vRay, frg.camW.xyz);		// Pixel to camera distance
	float   rad = frg.camW.w;					// Pixel geo-distance
	float   alt = rad - Const.PlanetRad;		// Pixel altitude over mean radius
	float  fSrf = (1.0 - Const.CamSpace);		// Camera colse to surface ?
	float fMask = (1.0 - cMsk.a);				// Specular Mask
	float  fSpe = 0;
	float fAmpf = 1.0f;
	float fDRS = dot(vRay, Const.toSun);
	float fDPS = dot(vPlN, Const.toSun);		// Mean normal dot sun


#if defined(_WATER)
#if defined(_RIPPLES)

	// Compute world space normal for water rendering
	//
	cNrm.xy = (cNrm.xy - 0.5f) * 2.0f;
	cNrm.z *= Const.wNrmStr;
	cNrm = normalize(cNrm);

	float3 wnrmW = (Const.vTangent * cNrm.r) + (Const.vBiTangent * cNrm.g) + (vPlN * cNrm.b);
	wnrmW = lerp(nvrW, wnrmW, fMask);
	float fDWS = dot(wnrmW, Const.toSun); // Water normal dot sun

	// Render with specular ripples and fresnel water -------------------------
	//
	float fDCH = saturate(dot(vRay, hlvW));
	float fDCN = saturate(dot(vRay, wnrmW));
	float fDHN = dot(hlvW, wnrmW);

	float3 f = 1.0 - float3(fDCH, fDCN, fDWS);
	float3 fFresnel4 = f * f * f;
	float3 fF = (0.15f + fFresnel4 * 0.85f) * fMask * Const.wSpec;

	// Compute specular reflection intensity
	fSpe = GGX_NDF(fDHN, 0.1f + saturate(fDWS) * 0.1f) * fF.y;
	fSpe /= (4.0f * fDCH * max(fDWS, fDCN) + 1e-3);

	// Apply fresnel water only if close enough to a surface
	//
	if (!Flow.bInSpace)
	{
		cRfl = GetAmbient(reflect(-vRay, wnrmW)) * fF.y * fSrf;	
		// Attennuate diffuse texture for fresnel refl.
		cTex.rgb *= saturate(1.0f - f.y * fSrf * fMask) * saturate(1.0f - f.z * fSrf * fMask);
	}

	cTex.rgb = saturate(cTex.rgb + float3(0, 0.55, 1.0) * Const.wBrightness * fMask);

#else
	// Fallback to simple specular reflection
	float fDHN = dot(hlvW, nvrW);
	fSpe = pow(saturate(fDHN), 60.0f) * fMask * 5.0f;
#endif
#endif

	float3 nrmW = nvrW; // Micro normal defaults to vertex normal

	// Render with surface microtextures --------------------------------------
	//
#if defined(_MICROTEX)

	if (Flow.bMicroTex)
	{
		float step1 = smoothstep(15000, 3000, dst);
		step1 *= (step1 * step1);
		float3 cFnl = max(0, min(2, 1.333f * (cFar + cMed + cLow) - 1));

		// Create normals
		if (Flow.bMicroNormals)
		{
			cFnl = cFnl.bbb;

#if defined(_SOFT)
			float2 cMix = (cFar.rg + cMed.rg + cLow.rg) * 0.6666f;			// SOFT BLEND
#endif
#if defined(_MED)
			float2 cMix = (cFar.rg + 0.5f) * (cMed.rg + 0.5f) * (cLow.rg + 0.5f);	// MEDIUM BLEND
			fAmpf = 2.0f;
#endif
#if defined(_HARD)
			float2 cMix = cFar.rg * cMed.rg * cLow.rg * 8.0f;				// HARD BLEND
			fAmpf = 4.0f;
#endif

			float3 cNrm = float3((cMix - 1.0f) * 2.0f, 0) * step1;
			cNrm.z = cos(cNrm.x * cNrm.y * 1.57);

			// Approximate world space normal
			nrmW = normalize((Const.vTangent * cNrm.x) + (Const.vBiTangent * cNrm.y) + (nvrW * cNrm.z));

			// Bend the normal towards sun a bit
			nrmW = normalize(nrmW + Const.toSun * 0.06f);
		}

		// Apply luminance
		cTex.rgb *= lerp(1.0f, cFnl, step1);
	}
#endif


	// Render Eclipse ------------------------------------------------------------
	//
	float fECL = GetEclipse(vVrt);

	float3 cDiffLocal = 0;

#if defined(_LOCALLIGHTS)
	LocalLights(cDiffLocal, nrmW, -frg.camW.xyz);
#endif

#if defined(_NO_ATMOSPHERE)

	float fDNS = saturate(dot(nvrW, Const.toSun));
	float fDCN = saturate(dot(nvrW, Const.toCam));
	float fLvl = 2.0f * fDNS / (fDNS + fDCN + 0.5f);
	float fSHD = 1.0f;

	// Shadowing by planet
	if (Flow.bPlanetShadow) {
		float palt = sqrt(saturate(1.0f - fDPS * fDPS)) * rad - Const.PlanetRad;
		fSHD = fDPS > 0 ? 1.0f : ilerp(Const.MinAlt, Const.MaxAlt, palt);
	}

	// Amplify light and shadows
	fLvl += dot(nvrW - vPlN, Const.toSun) * fLvl * Const.trLS;

	// Add opposition surge
	fLvl += pow(saturate(fDRS), 4.0f) * 0.3f * fDNS;

#if defined(_MICROTEX)
	fLvl += dot(nrmW - nvrW, Const.toSun) * ilerp(0.0, 0.03, fLvl) * fAmpf;
#endif

	fLvl *= fSHD;	// Apply planet shadow
	fLvl *= fECL;	// Apply eclipse

	float3 color = cTex.rgb * LightFX(max(fLvl, 0) * fShadow + cDiffLocal);

	return float4(pow(saturate(color * Const.TrExpo), Const.TrGamma), 1.0f);		// Gamma corrention
#else

	float fShd = 1.0f;

#if defined(_CLOUDSHD)
	// Do we render cloud shadows ?
	if (Flow.bCloudShd) {
		fShd = (vUVCld.x < 1.0 ? fChA : fChB);
		fShd = saturate(1.0 - fShd * Prm.fAlpha);
	}
#endif

	float3 cNgt = 0;
	float3 cNgt2 = 0;
	float fDNS = dot(nvrW, Const.toSun); // Vertex normal dot sun

#if defined(_NIGHTLIGHTS)

	// Night lights ?
	float fNgt = saturate(-fDPS * 4.0f + 0.05f) * Prm.fBeta; // Night lights intensity and 'on' time

	cMsk.b = (cMsk.b > 0.15f ? cMsk.b : 0.0f); // Blue dirt filter

	cNgt = cMsk.rgb * (1 - Const.CamSpace) * fNgt; // Nightlights surface texture illumination term
	cNgt2 = cMsk.rgb * Const.CamSpace * 4.0f * fNgt; // Nightlights orbital visibility

	// ORO patch (m), part 2: CLOUDS BLOT THE CITY LIGHTS UNDER THEM. The orbital
	// lights are rendered 4x overbright into the fp16 chain and then bloomed, so the
	// night-cloud veil (part 1, CloudPS) cannot visibly dim them - 0.26 x 4 is still
	// ~1.0, and they punched through anything short of solid overcast. Attenuate at
	// the SOURCE instead, with the same cloud alpha the day shadows sample (Surfmgr2
	// now binds the cloud tiles for night tiles too). ~92% blocked under a solid
	// deck; the remaining leak is the diffuse glow real cities show through cloud.
#if defined(_CLOUDSHD)
	if (Flow.bCloudShd) {
		float fCld = saturate(vUVCld.x < 1.0 ? fChA : fChB);
		float fBlot = 1.0f - fCld * 0.92f;
		cNgt *= fBlot;
		cNgt2 *= fBlot;
	}
#endif
#endif

	float fNoise = (tex2Dlod(tNoise, float4(frg.texUV.xy * 4.0f * Prm.fTgtScale, 0, 0)).r - 0.5f) * ATMNOISE;

	// Terrain with gamma correction and attennuation
	cTex.rgb = pow(saturate(cTex.rgb), Const.TrGamma) * Const.TrExpo;

	// Evaluate ambient approximation
	float4 cAmb = AmbientApprox(vPlN, false);
	
	LandOut sct = GetLandView(rad, vPlN);

	// Get the color of sunlight and set maximum intensity to 1.0
	float3 cSun = GetSunColor(fDPS, alt);
	float3 cSF = cSun * Const.cSun;
	float fMx = max(max(cSF.r, cSF.g), cSF.b);
	cSF = fMx > 1.0 ? cSF / fMx : cSF;

	float  fL = Const.trLS * 0.3f;
	float  fZ = clamp(dot(nvrW - vPlN, Const.toSun) * Const.trLS, -fL, fL);
	float  fX = 1.0f - pow(1.0f - saturate(fDPS), 2.0f);

	fZ = fZ > 0 ? fZ * 2.0f : fZ;

#if defined(_MICROTEX)
	float  fG = dot(nrmW - nvrW, Const.toSun) * fAmpf;
#else
	float  fG = 0.0f;
#endif

	// Diffuse "lambertian" shading term
	float  fD = lerp(fX + (fG + fZ) * fX, fDPS * fDPS, fMask);

	// Water masking
	float  fM = 0.5f - fMask * 0.25f;

	// Ambient light for terrain
	//					  Color					   Distance				  Altitude factor		   Particle Density		
	float3 cA = normalize(cAmb.rgb + cSF * 4.0f) * cAmb.a * cAmb.g * fM * exp(-alt * Const.iH.r) * Const.rmI.r * 6e5 * Const.TW_Terrain;

	fShd = saturate(fShd + (1.0f - fX));

	// Bake light and shadow terms
	float3 cL = cSF * fD * fShadow * fShd;

	// ORO patch (s) part 2: overcast. The DIRECTIONAL term collapses and the ambient
	// lifts, which is what a storm deck actually does to the light - shadows and the
	// warm cast go with it, instead of a post-process dimming the finished frame.
	cL *= (1.0f - gStorm);
	cA *= (1.0f + gStorm * 2.2f);

	// Lit the texture with various things
	cTex.rgb *= cL * 2.0f + (cA + cDiffLocal + Const.cAmbient * Const.Ambient) * saturate(1.0f + fG + fZ) + cNgt;

	cTex.rgb = max(float3(0, 0, 0), cTex.rgb);

	// Add Reflection
	cTex.rgb += cRfl * 0.75f;

	// Add Specular component
	cTex.rgb += cSun * fSpe * smoothstep(-0.001f, 0.03f, fDPS) * (1.0f - gStorm);   // ORO patch (s) part 2

	// Amplify cloud shadows for orbital views
	float fOrbShd = 1.0f - (1.0f - fShd) * Const.CamSpace * 0.5f;

	// ORO patch (s): WET TERRAIN. See Mesh.fx's BaseTilePS for the full reasoning.
	// ⚠️ It goes in BEFORE the haze and the eclipse, so wet ground is still attenuated by
	// the atmosphere exactly like dry ground - putting it after would have made distant
	// wet terrain punch through the aerial perspective. Exactly zero at gWet 0, so stock
	// content cannot move.
	if (gWet > 0.001f) {
		// ORO patch (s) part 7: STANDING POOLS ON THE TERRAIN - his brief, verbatim:
		// patches all around the vessel, different sizes, that STAY IN PLACE as the
		// vessel moves, new ones appearing ahead and old ones dropping behind. All of
		// that is a COORDINATE CHOICE, not machinery: the lattice lives in the water-
		// microtexture UV mapping (Prm.vMicroOff, WITHOUT line 401's ocean time drift),
		// which the client already keeps continuous across tiles and LODs so the water
		// microtexture is seamless - so the pools are pinned to the ground by
		// construction and the vessel simply drives across them. Spawn/cull for free.
		// Same two-scale sine lattice as the base tiles (BaseTilePS), so runway and
		// apron pool by one law; the two K constants are the pool-size calibration
		// and are a fly-and-report number, not a derivation.
		// Part 7 round 2: THREE lattice scales (the coarse one shifts the fine
		// threshold regionally - sheets here, speckle there: the size variety), the
		// Pool size slider (gWetSwimPrm.z) scaling all three, pools STARTING at 70%
		// rain (his spec - ground soaks first, water stands later), and a Pool reach
		// distance e-fold (gWetSwimPrm.w, ~900 m at 1) so the far field blends out
		// instead of patterning to the horizon.
		// SEAMLESS LATTICE (round 3: he screenshotted the seams). The micro-UV
		// mapping agrees across tiles and LODs only MODULO 1 - all a wrapping
		// texture needs - so fractional-cycle sinusoids jumped phase at every tile
		// boundary. Integer cycles per UV unit (TAU x QK) make the lattice
		// 1-periodic, so a mod-1 jump lands on the same value: no seam, by
		// construction. frac() bounds the sin arguments (precision), legal only
		// because of that same periodicity. The slider quantizes to whole cycles.
		// ROUND 3 ("too squarish, a bit geometric"): the old lattice was a PRODUCT of
		// axis-aligned sinusoids, and x-waves times y-waves can only make rectilinear
		// cells - the maze he screenshotted. It is now an INTERFERENCE SUM of plane
		// waves at spread angles - how isotropic-looking noise is built from periodic
		// parts - so the blobs come out ROUNDED and quasi-random.
		// ⚠️ THE SEAM LAW HOLDS: every wave-vector COMPONENT is an integer number of
		// cycles per UV unit (signed quantizer QS), so the field stays 1-periodic and
		// a mod-1 tile jump lands on the same value. The phases are free randomizers.
		float2 vUVPool = frg.texUV.xy * Prm.vMicroOff.zw + Prm.vMicroOff.xy;
		float  poolK = 1.0f / max(0.35f, gWetSwimPrm.z);
	#define QS(k) (((k) < 0.0f) ? -max(1.0f, floor(-(k) * poolK + 0.5f)) : max(1.0f, floor((k) * poolK + 0.5f)))
		float2 uvT = frac(vUVPool) * 6.2831853f;
		float  p1 = (sin(uvT.x * QS(50.0f)  + uvT.y * QS(17.0f)  + 1.3f)
		           + sin(uvT.x * QS(21.0f)  + uvT.y * QS(47.0f)  + 4.1f)
		           + sin(uvT.x * QS(-33.0f) + uvT.y * QS(40.0f)  + 2.6f)
		           + sin(uvT.x * QS(45.0f)  + uvT.y * QS(-27.0f) + 5.5f)) * 0.42f;
		float  p2 = (sin(uvT.x * QS(9.0f)   + uvT.y * QS(4.0f)   + 0.7f)
		           + sin(uvT.x * QS(-5.0f)  + uvT.y * QS(10.0f)  + 3.9f)
		           + sin(uvT.x * QS(11.0f)  + uvT.y * QS(-2.0f)  + 2.2f)) * 0.55f;
		float  p3 = (sin(uvT.x * QS(3.0f)   + uvT.y * QS(1.0f)   + 1.9f)
		           + sin(uvT.x * QS(-1.0f)  + uvT.y * QS(3.0f)   + 5.0f)
		           + sin(uvT.x * QS(2.0f)   + uvT.y * QS(-2.0f)  + 0.4f)) * 0.55f;
		// GRAIN IN THE REFLECTION (round 12 - the cell flecks were circular dots that
		// jumped state; his circled reference areas are fine IRREGULAR broken-water
		// texture). Two octaves of continuous value noise, thresholded so only the
		// upper patches dig into the reflection - irregular connected blotches,
		// STATIC in the world (no migration, his call), periodic (seam law).
		// the two user knobs: opacity scales the knockout depth; size rescales the
		// lattice - QUANTIZED to whole cells per period so the seam law survives it
		float  GNq = max(16.0f, floor(512.0f / max(0.35f, gWetGrainPrm.y) + 0.5f));
		float2 guv = frac(vUVPool) * GNq;
		float  gn  = 0.62f * OroGrainNoise(guv, GNq)
		           + 0.38f * OroGrainNoise(guv * 2.0f, GNq * 2.0f);
		float  gran = 1.0f - saturate(0.85f * gWetGrainPrm.x) * smoothstep(0.52f, 0.78f, gn);
	#undef QS
		float  pud = saturate(p1 * 1.30f - 0.16f + 0.30f * gWet + p3 * 0.35f)
		           * saturate(p2 * 1.10f + 0.35f + 0.25f * gWet);
		pud = saturate(pud * 1.6f) * saturate((gWet - 0.70f) / 0.30f);
		pud *= exp(-dst / (90.0f + 780.0f * gWetSwimPrm.w * gWetSwimPrm.w));

		cTex.rgb *= saturate(lerp(1.0f, 1.0f - 0.494f * gWetDark, gWet));   // recalibrated x2: full track = old 0..1.3
		cTex.rgb *= saturate(lerp(1.0f, 1.0f - 0.85f  * gWetDark, pud));    // standing water, darker still
		float fresW = pow(1.0f - saturate(dot(nvrW, -vRay)), 5.0f);
		cTex.rgb = lerp(cTex.rgb, cTex.rgb * 1.7f + 0.02f, saturate(fresW * 0.5f * gWet));

		// THE SKY IN THE WATER FILM (look round 2). The terrain had NO sky reflection -
		// only the vessel image - which is why his pools showed ships but never the
		// grey overcast the reference is full of. The film lerps toward the same
		// storm-sky grey the fog uses, scaled by the daylight factor so night pools
		// stay dark; the pools read as light patches against the darkened ground.
		// Part 7: the POOLS mirror the sky strongly at ANY viewing angle - standing
		// water, the base tiles' repartition law - which is what makes them read as
		// light patches against the darkened apron from a standing camera.
		{
			float3 cSkyT = float3(0.42f, 0.45f, 0.49f) * saturate(cAmb.a * 1.3f)
			             * (1.0f + gStorm * 0.35f);
			float  skyF = saturate(saturate((0.30f + 0.70f * fresW) * gWet) * 0.60f
			                     + (0.55f + 0.45f * fresW) * pud * 0.80f * gran);
			cTex.rgb = lerp(cTex.rgb, cSkyT, skyF);

			// POOL SPECULAR (part 7 round 3, his ask): standing water answers CAMERA
			// MOVEMENT - two lobes on the shader's own Blinn half-vector (hlvW, the
			// idiom the stock ground specular uses). The tight lobe is direct sun on
			// water and collapses with the storm like every directional term; the
			// BROAD lobe is the bright cloud around the hidden sun and GROWS with the
			// storm, so pools keep flashing under full overcast. Night: the sun term
			// gates on the stock horizon smoothstep, the sky term goes dark through
			// cSkyT's daylight factor. Scaled by pud alone - damp ground has no free
			// water surface to mirror a light source.
			float pDHN  = saturate(dot(hlvW, nvrW));
			float spSun = pow(pDHN, 90.0f) * (1.0f - gStorm)
			            * smoothstep(-0.001f, 0.03f, fDPS);
			float spSky = pow(pDHN, 8.0f) * (0.30f + 0.70f * gStorm);
			cTex.rgb += (cSun * spSun * 2.2f + cSkyT * spSky * 0.9f)
			          * pud * (0.35f + 0.65f * fresW);
		}

		// THE VESSEL IMAGE IN THE WET GROUND (ORO patch (s) part 6) - terrain edition.
		// Same half-res mirror the base tiles sample; the film above supplies the sky,
		// this supplies the ships. Fast shimmer on Const.Time directly - the previous
		// wobble rode vUVWtr (time/180) at aurora pace, the third recurrence of that
		// critique; rain-pocked water flickers at a few Hz. LOOK ROUND 3 ("the
		// reflection swings too far"): a jitter, not a wave. LOOK ROUND 4: amplitude
		// and cadence are HIS - gWetSwimPrm (x amp, y rate; 1 = this baseline), the
		// Swim size / Swim rate sliders. Part 7: the image concentrates in the POOLS
		// (mild 0.70 film + strong pud term), which is what the reference shows -
		// ships mirrored in the puddles, not evenly across damp ground.
		if (gWetReflPrm.w > 0.5f) {
			float2 ruv = (sc.xy + 0.5f) * gWetReflPrm.xy;
			ruv.x = 1.0f - ruv.x;              // undo the mirror pass's clip-space X flip
			ruv += float2(sin(Const.Time * 10.8f * gWetSwimPrm.y + vUVSrf.x * 43.0f),
			              cos(Const.Time *  8.5f * gWetSwimPrm.y + vUVSrf.y * 43.0f))
			     * (0.0007f * gWetSwimPrm.x);
			float  smr = gWetReflPrm.y * 3.2f;
			float4 cVes = tex2D(tWetRefl, ruv) * 0.5f
			            + tex2D(tWetRefl, ruv + float2(0, smr)) * 0.3f
			            + tex2D(tWetRefl, ruv + float2(0, smr * 2.5f)) * 0.2f;
			float  rStr = cVes.a * saturate(0.30f + 2.2f * fresW)
			            * saturate(0.70f * gWet + 1.25f * pud) * 0.85f * gWetReflPrm.z * gran;
			cTex.rgb = lerp(cTex.rgb, cVes.rgb, saturate(rStr));
		}
	}

	// Add Haze and night lights
	cTex.rgb *= sct.atn.rgb;
	cTex.rgb += (sct.ray.rgb * RayPhase(-fDRS) + sct.mie.rgb * MiePhase(-fDRS)) * fOrbShd * (1.0f + fNoise);

	// ORO patch (s) part 2: STORM FOG. Visibility collapses under heavy rain; exponential
	// in the pixel distance (dst), toward a grey scaled by the ambient daylight factor so
	// it stays dark at night. Sits after the atmospheric haze - storm fog dominates it -
	// and before the eclipse, which still darkens everything. Zero at gStorm 0.
	if (gStorm > 0.001f) {
		float  ffog = (1.0f - exp(-dst * 0.00045f)) * saturate(gStorm * 1.4f);
		float3 cFog = float3(0.40f, 0.43f, 0.47f) * saturate(cAmb.a * 1.2f);
		cTex.rgb = lerp(cTex.rgb, cFog, ffog);
	}

	cTex.rgb *= fECL;	// Apply eclipse
	cTex.rgb += cNgt2;

	return float4(HDR(cTex.rgb), 1.0f);
#endif
}






// ============================================================================
// Planet Cloud Renderer
// ============================================================================

// ORO patch (m): NIGHT-SIDE CLOUD VISIBILITY, seen from above. Stock multiplies the
// cloud alpha by the SQUARE of the twilight ramp (AmbientApprox: 1 day -> 0 night, no
// floor), so night clouds render at alpha 0 - fully invisible - and city lights shine
// through cloud decks that should blot them out. Real night clouds are conspicuous
// from orbit precisely as HOLES in the city-light field (and as the canvas lightning
// lights up). This floors the term with a smooth LERP - day side exactly unchanged
// (factor 1), night side settling at ORO_NIGHT_CLOUD, no slope kink at the
// terminator. The cloud RGB stays sun-lit (near black at night), so what appears is a
// dark veil that dims what is behind it - the real look. Tune by editing the constant
// and restarting the session (this file compiles at session start; no rebuild).
#define ORO_NIGHT_CLOUD 0.5f

CldVS CloudVS(TILEVERTEX vrt)
{
	// Zero output.
	CldVS outVS = (CldVS)0;

	// Apply a world transformation matrix
	float3 vPosW = mul(float4(vrt.posL, 1.0f), Prm.mWorld).xyz;
	float3 vNrmW = mul(float4(vrt.normalL, 0.0f), Prm.mWorld).xyz;

	outVS.posH = mul(float4(vPosW, 1.0f), Const.mVP);
	outVS.nrmW = vNrmW;
	outVS.posW = vPosW;
	outVS.texUV.xy = vrt.tex0.xy;						// Note: vrt.tex0 is un-used (hardcoded in Tile::CreateMesh and varies per tile)

	return outVS;
}


// ============================================================================
// 
float4 CloudPS(CldVS frg) : COLOR
{
	float2 vUVTex = frg.texUV.xy;
	float4 cTex = tex2D(tDiff, vUVTex);
	float3 vRay;
	float3 vPxl;
	float  dRC;
	float  fNrm = 1.0f;

	if (Flow.bBelowClouds) {
		float  rRef = Const.PlanetRad + Const.smi * 0.5f;	// Reference altitude
		float3 vRef = Const.toCam * rRef;
		vRay = normalize(Const.toCam * (Const.CamRad - rRef) + frg.posW); // Viewing ray to the pixel
		dRC = dot(vRay, Const.toCam);
		float  fEca2 = 1.0f - dRC * dRC;			// Ray horizon angle^2
		float  fD = Const.smi * rsqrt(1.0f - Const.ecc * Const.ecc * fEca2); // Distance to ellipse threshold
		vPxl = vRef + vRay * fD;					// Pretend the pixel being closer and lower
	}
	else {
		vRay = normalize(frg.posW);					// Viewing ray to the pixel
		dRC = dot(vRay, Const.toCam);
		vPxl = Const.CamPos + frg.posW;				// Pixel's geocentric location
	}

	float3 vPlN = normalize(vPxl);					// Mean Normal at pixel's locatin
	float3 vVrt = Const.CamPos + frg.posW.xyz;	// Geo-centric pixel position
	float3 nrm = vPlN;

	float dRS = dot(vRay, Const.toSun);
	float dMNus = dot(vPlN, Const.toSun);
	float dMN = saturate(dMNus);					// Mean normal sun angle
	float fPxR = dot(vPxl, vPlN);					// Pixel geo distance
	float fPxA = fPxR - Const.PlanetRad;			// Pixel altitude

	if (!Flow.bBelowClouds) fPxA = Const.CloudAlt;


	// -----------------------------------------------
	// Cloud layer rendering for Earth
	// -----------------------------------------------

#if defined(_CLOUDMICRO)
	float2 vUVMic = frg.texUV.xy * Prm.vMicroOff.zw + Prm.vMicroOff.xy;
	float4 cMic = tex2D(tCloudMicro, vUVMic);
#endif


#if defined(_CLOUDNORMALS)
#if defined(_CLOUDMICRO)

	float4 cMicNorm = tex2D(tCloudMicroNorm, vUVMic);  // Filename "cloud1_norm.dds"

	// Extract normal from transparency (height) data
	// Filter width
	float d = 2.0 / 512.0;

	float x1 = tex2D(tDiff, vUVTex + float2(-d, 0)).a;
	float x2 = tex2D(tDiff, vUVTex + float2(+d, 0)).a;
	nrm.x = (x1 * x1 - x2 * x2);

	float y1 = tex2D(tDiff, vUVTex + float2(0, -d)).a;
	float y2 = tex2D(tDiff, vUVTex + float2(0, +d)).a;
	nrm.y = (y1 * y1 - y2 * y2);

	// Blend in cloud normals only on moderately thick clouds, allowing the highest cloud tops to be smooth.
	nrm.xy = (nrm.xy + saturate((cTex.a * 10.0f) - 3.0f) * saturate(((1.0f - cTex.a) * 10.0f) - 1.0f) * (cMicNorm.rg - 0.5f)); // new

	// Increase normals contrast based on sun-earth angle.
	nrm.xyz = nrm.xyz * (1.0f + (0.5f * dMN));

	nrm.z = sqrt(1.0f - saturate(nrm.x * nrm.x + nrm.y * nrm.y));

	// Approximate world space normal from local tangent space
	nrm = normalize((Const.vTangent * nrm.x) + (Const.vBiTangent * nrm.y) + (vPlN * nrm.z));

	float dCS = dot(nrm, Const.toSun); // Cloud normal sun angle

	// Brighten the lighting model for clouds, based on sun-earth angle. Twice is better.
	// Low sun angles = greater effect. No modulation leads to washed out normals at high sun angles.
	dCS = saturate((1.0f - dMN) * (dCS * (1.0f - dCS)) + dCS);
	dCS = saturate((1.0f - dMN) * (dCS * (1.0f - dCS)) + dCS);

	// With a high sun angle, don't let the dCS go below 0.2 to avoid unnaturally dark edges.
	dCS = lerp(0.2f * dMN, 1.0f, dCS);

	// Effect of normal/sun angle to color
	// Add some brightness (borrowing red channel from sunset attenuation)
	// Adding it to the sun illumination factor, taking care to keep from saturating
	fNrm = dCS +((1.0f - dCS) * 0.2f);
#endif
#endif

#if defined(_CLOUDMICRO)
	float f = cTex.a;
	float g = lerp(1.0f, cMic.a, 1.0f - abs(dot(Const.vPolarAxis, vPlN)));
	float h = (g + 4.0f) * 0.2f;
	cTex.a = saturate(lerp(g, h, f) * f);
#endif

	// Render Eclipse ------------------------------------------------------------
	//
	float fECL = GetEclipse(vVrt);

	if (Flow.bBelowClouds)
	{
		// Get sunlight color
		float3 cSun = GetSunColor(dMNus, fPxA);

		// Get ambient information
		float4 cMlt = AmbientApprox(vPlN);

		cSun *= saturate(dRS + 1.3f);
		float fPh = pow(saturate(1.0f - dRC), 32.0f) * pow(saturate(dRS), 10.0f); // Boost near horizon and close the sun
		cSun *= 1.0f + fPh * 8.0f;

		cSun *= Const.cSun * fNrm;
		cSun *= Const.Clouds;
		cSun += cMlt.rgb * cMlt.a * 0.2f;

		LandOut sct = GetLandView(fPxA + Const.PlanetRad, vPlN);

		cTex.rgb *= cSun;
		cTex.rgb *= sct.atn.rgb;
		cTex.rgb += sct.ray.rgb * 2.0f;
		cTex.rgb *= fECL;

		return float4(HDR(cTex.rgb), saturate(cTex.a));
	}
	else {

		// Get sunlight color
		float3 cSun = GetSunColor(dMN, fPxA);
	
		// Get ambient information
		float4 cAmb = AmbientApprox(dMNus);
		float3 cMSC = Const.RayWave * Const.RayWave * Const.Clouds; // Multiscatter color

		cSun = sqrt(cMSC * cMSC + cSun * cSun * fNrm) * cAmb.a;
		
		LandOut sct = GetLandView(fPxA + Const.PlanetRad, vPlN);

		cTex.rgb *= cSun;
		cTex.rgb *= sct.atn.rgb;
		cTex.rgb += sct.ray.rgb;
		cTex.rgb *= fECL;

		// ORO patch (m): stock was cTex.a * cAmb.a * cAmb.a - alpha 0 past the
		// terminator, night clouds invisible from above. See the note at CloudVS.
		float fNight = ORO_NIGHT_CLOUD + (1.0f - ORO_NIGHT_CLOUD) * cAmb.a * cAmb.a;
		return float4(sqr(HDR(cTex.rgb * 4.0f)), cTex.a * fNight);
	}
}








// ============================================================================
// Gas Giant Renderer
// ============================================================================

TileVS GiantVS(TILEVERTEX vrt)
{
	// Zero output.
	TileVS outVS = (TileVS)0;
	
	// Apply a world transformation matrix
	float3 vPosW = mul(float4(vrt.posL, 1.0f), Prm.mWorld).xyz;
	float3 vNrmW = mul(float4(vrt.normalL, 0.0f), Prm.mWorld).xyz;
	
	outVS.posH = mul(float4(vPosW, 1.0f), Const.mVP);
	outVS.texUV.xy = vrt.tex0.xy;
	outVS.camW = float4(-vPosW, 0);
	outVS.nrmW = vNrmW;

	return outVS;
}

// ============================================================================
//
float4 GiantPS(TileVS frg) : COLOR
{

	float2 vUVSrf = frg.texUV.xy * Prm.vTexOff.zw + Prm.vTexOff.xy;
	
	// Fetch Main Textures
	float4 cTex = tex2D(tDiff, vUVSrf);
	
	float3 nrmW = normalize(frg.nrmW);			// Per-pixel surface normal vector
	float3 vRay = normalize(frg.camW.xyz);		// Unit viewing ray
	float3 vVrt = Const.CamPos - frg.camW.xyz;	// Geo-centric pixel position
	float3 vPlN = normalize(vVrt);				// Planet mean normal
	float  fDPS = dot(vPlN, Const.toSun);
	float3 cSun = saturate((fDPS + 0.1) * 5.0);


	// Render Eclipse ------------------------------------------------------------
	//
	cSun *= GetEclipse(vVrt);

	// Terrain with gamma correction and attennuation
	cTex.rgb = pow(saturate(cTex.rgb), Const.TrGamma) * Const.TrExpo;

	float3 color = cTex.rgb * LightFX(cSun + float3(0.9, 0.9, 1.0) * Const.Ambient);

	return float4(HDR(color), 1.0f);
}


// ============================================================================
// Gas giant cloud layer renderer
// ============================================================================

float4 GiantCloudPS(CldVS frg) : COLOR
{
	float4 cTex = tex2D(tDiff, frg.texUV.xy);
	float3 vPlN = normalize(frg.nrmW);
	float3 vRay = normalize(frg.posW);
	float3 vVrt = Const.CamPos + frg.posW.xyz;	// Geo-centric pixel position
	float  fDPS = dot(vPlN, Const.toSun);    // Planet mean normal sun angle

	float3 cSun = saturate((fDPS + 0.1) * 5.0);

	// Render Eclipse ------------------------------------------------------------
	//
	float fECL = GetEclipse(vVrt);

	cTex.rgb *= LightFX(cSun + float3(1.0, 1.0, 1.0) * Const.Ambient);
	cTex.rgb = pow(saturate(cTex.rgb), Const.TrGamma) * Const.TrExpo;
	cTex.rgb *= fECL;

	return float4(HDR(cTex.rgb), saturate(cTex.a));
}
