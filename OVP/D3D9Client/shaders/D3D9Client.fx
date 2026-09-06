// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ==============================================================

// ----------------------------------------------------------------------------
// D3D9Client rendering techniques for Orbiter Spaceflight simulator
// ----------------------------------------------------------------------------


#define NIGHT_CLOUDS 0.05f          // range(0.0f-0.1f) Cloud ambient level at night
#define CLOUD_INTENSITY 1.8f        // range(0.5f-2.0f)
#define NIGHT_LIGHTS 0.7f           // range(0.2f-1.0f)

struct Mat
{
	float4 diffuse;
	float4 ambient;
	float4 specular;
	float4 emissive;
	float  specPower;
};

struct Mtrl
{
	float4 diffuse;
	float4 specular;
	float3 ambient;
	float3 emissive;
	float3 reflect;
	float3 emission2;
	float3 fresnel;
	float2 roughness;
	float  metalness;
	float4 specialfx;			// x = Heat 
};

struct Sun
{
	float3 Dir;
	float3 Color;			// Color and Intensity of received sunlight 
	float3 Ambient;			// Ambient light level (Base Objects Only, Vessels are using dynamic methods)
	float3 Transmission;	// Visibility through atmosphere (1.0 = fully visible, 0.0 = obscured)
	float3 Inscatter;		// Amount of incattered light from haze
};

struct Light
{
	int      type;       	   /* Is is spotlight */
	float    dst2;			   /* Camera-Light Emitter distance squared */
	float4   diffuse;          /* diffuse color of light */
	float3   position;         /* position in world space */
	float3   direction;        /* direction in world space */
	float3   attenuation;      /* Attenuation */
	float4   param;            /* range, falloff, theta, phi */
};

// Must match with counterpart in D3D9Effect.h

struct Flow
{
	bool Emis;		// Enable Emission Maps
	bool Spec;		// Enable Specular Maps
	bool Refl;		// Enable Reflection Maps
	bool Transl;	// Enble translucent effect
	bool Transm;	// Enable transmissive effect
	bool Rghn;		// Enable roughness map
	bool Norm;		// Enable normal map
	bool Metl;		// Enable metalness map
	bool Heat;		// Enable heat map
};


// Must match with counterpart D3D9Tune in D3D9Util.h

struct Tune
{
	float4 Albe;		// Tune Diffese Maps
	float4 Emis;		// Tune Emission Maps
	float4 Spec;		// Tune Specular Maps
	float4 Refl;		// Tune Reflection Maps
	float4 Transl;		// Tune translucent effect
	float4 Transm;		// Tune transmissive effect
	float4 Norm;		// Tune normal map
	float4 Rghn;		// Tune roughness map
};


#define Range   0
#define Falloff 1
#define Theta   2
#define Phi     3


#define SH_SIZE		0
#define SH_INVSIZE	1

uniform extern float3    kernel[KERNEL_SIZE];

// -------------------------------------------------------------------------
uniform extern float4x4  gW;			    // World matrix
uniform extern float4x4  gLVP;			    // Light view projection
uniform extern float4x4  gVP;			    // Combined View and Projection matrix
uniform extern float4x4  gGrpT;	            // Mesh group transformation matrix
uniform extern float4    gAttennuate;       // (Mesh Constant Fog) Attennuation of fragment color
uniform extern float4    gInScatter;        // (Mesh Constant Fog) In scattering light
uniform extern float4    gColor;            // General purpose color parameter
uniform extern float4    gFogColor;         // Distance fog color in "Legacy" implementation
uniform extern float4    gAtmColor;         // Earth glow color
uniform extern float4    gTexOff;			// Texture offsets used by surface manager
uniform extern float4    gRadius;           // PlanetRad, AtmOuterLimit, CameraRad, CameraAlt
uniform extern float4    gSHD;				// ShadowMap data
uniform extern float3    gCameraPos;        // Planet relative camera position, Unit vector
uniform extern float3    gNorth;
uniform extern float3    gEast;
uniform extern Sun		 gSun;				// Sun light direction
uniform extern Mat       gMat;			    // Material input structure  TODO:  Remove all reference to this. Use gMtrl
uniform extern Mat       gWater;			// Water material input structure
uniform extern Mtrl      gMtrl;			    // Material input structure
uniform extern Tune      gTune;			    // Texture tuning parameters
uniform extern Light	 gLights[MAX_LIGHTS];
uniform extern bool		 gLightsEnabled;
uniform extern bool      gTuneEnabled;
uniform extern bool      gModAlpha;		    // Configuration input
uniform extern bool      gFullyLit;			// Always fully lit bypass lighting calculations
uniform extern bool      gTextured;			// Enable Diffuse Texturing
uniform extern bool      gFresnel;			// Enable fresnel material
uniform extern bool      gPBRSw;			// Legacy / PBR Switch
uniform extern bool      gRghnSw;			// Roughness converter switch
uniform extern bool      gNight;			// Nighttime/Daytime
uniform extern bool      gShadowsEnabled;	// Enable shadow maps
uniform extern bool      gEnvMapEnable;		// Enable Environment mapping
uniform extern bool		 gInSpace;			// True if a mesh is located in space
uniform extern bool		 gNoColor;			// No color flag
uniform extern bool		 gBaseBuilding;
uniform extern bool		 gOITEnable;
uniform extern int       gSpecMode;
uniform extern int       gHazeMode;
uniform extern float     gProxySize;		// Cosine of the angular size of the Proxy Gbody. (one half)
uniform extern float	 gInvProxySize;		// = 1.0 / (1.0f-gProxySize)
uniform extern float     gPointScale;
uniform extern float     gDistScale;
uniform extern float     gFogDensity;
uniform extern float     gTime;
uniform extern float     gMix;				// General purpose parameter (multible uses)
// ORO patch (p): VC SHADOW DEPTH, 0..1. Stock self-shadowing multiplies the SUN
// term only, so a fully shadowed pixel keeps its material AMBIENT and EMISSIVE - and
// a virtual cockpit is authored with plenty of both, which is why VC shadows read as
// a faint grey smudge no external setting can deepen. This lets the shadow take the
// AMBIENT term with it, by a user-set fraction. EMISSIVE IS DELIBERATELY UNTOUCHED:
// a lit instrument panel does not care what is between it and the sun, and dimming
// MFDs as a canopy shadow sweeps over them reads as a bug. Scene.cpp raises this for
// the COCKPIT PASS ONLY and clears it after, so exterior shading is bit-for-bit stock;
// 0 is also exactly stock, so the whole patch is inert until an addon asks for it.
uniform extern float     gVCShdDepth;
uniform extern float     gSurfWet;        // ORO patch (s): ground wetness, 0..1
uniform extern float     gWetTime;        // ORO patch (s): real-time sparkle clock, wraps hourly
uniform extern float     gWetGlint;       // ORO patch (s) part 5: hull glint gain, 0..2
uniform extern texture   gWetReflTex;     // ORO patch (s) part 6: the wet-ground planar mirror
uniform extern float4    gWetReflPrm;     // ORO patch (s) part 6: 1/W, 1/H, gain, live
uniform extern float4    gWetSwimPrm = {1, 1, 1, 1};  // ORO patch (s): swim amp, swim rate, pool size, pool reach (user sliders)
uniform extern float4    gWetGrainPrm = {1, 1, 0, 0};  // ORO patch (s) part 7: grain opacity, grain size (user sliders)
sampler WetReflS = sampler_state
{
	Texture = <gWetReflTex>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
	AddressU = CLAMP;
	AddressV = CLAMP;
};
// ============================================================================
// ORO patch (aa): THE AIR - FOG (two analytic height-fog layers) and SNOW COVER.
// ----------------------------------------------------------------------------
// The client computes the constants once per frame (Scene.cpp, OroFogFrame): two slabs
// with an exponential density profile, the fog's colour in DISPLAY space, the sun lobe,
// the ambient lift and the on/off K (0 for the cockpit interior draw). Every consumer
// calls OroFog() with the pixel's camera-relative position, attenuates its direct sun by
// sunAtt, lifts its ambient by gFogLift * (1 - sunAtt), and lerps its FINAL colour toward
// col by (1 - T) - after any tone curve, so every shader family lands on the same grey.
// Exactly inert at density 0. This block is kept byte-identical between D3D9Client.fx
// and NewPlanet.hlsl: the two shader roots do not share an include.
// ============================================================================
uniform extern float4 gFogPrm[6];   // [0] xyz = planet-up at the camera, w = camera geocentric radius
                                    // [1],[2] layer 0/1: hc (camera height over the base), hTop, iH, dens
                                    // [3],[4] layer 0/1: exp(-hTop*iH), dens/(iH*sinSunElev), 0, 0
                                    // [5] x = K (1 fog on, 0 off)
uniform extern float4 gFogClr[3];   // [0] fog colour, [1] sun lobe colour, [2].w = ambient lift gain
uniform extern float4 gSnow;        // cover 0..1, snow-line altitude (m), 1/line width, 0
#define gFogCam  gFogPrm[0]
#define gFogL0   gFogPrm[1]
#define gFogL1   gFogPrm[2]
#define gFogM0   gFogPrm[3]
#define gFogM1   gFogPrm[4]
#define gFogK    gFogPrm[5].x
#define gFogSunCam gFogPrm[5].y   // the camera's own sun transmittance (for paths that cannot afford the per-pixel one)
#define gFogCol  gFogClr[0]
#define gFogSun  gFogClr[1]
#define gFogLift gFogClr[2].w

// Optical depth of one layer along the segment from the camera (height hc over the layer
// base, in L.x) to a pixel at height hp, length d. The segment is clipped to the slab
// 0..hTop and the height taken linear along it - exact for a straight ray at fog range.
float OroFogTau(float4 L, float hp, float d)
{
	float dens = L.w * gFogK;
	float hc = L.x, top = L.y, iH = L.z;
	float dh = hp - hc;
	float idh = (abs(dh) > 1e-3f) ? 1.0f / dh : 0.0f;
	float t0 = 0.0f, t1 = 1.0f;
	if (hc > top) { t0 = (hp >= top) ? 1.0f : (top - hc) * idh; }
	else if (hp > top) t1 = (top - hc) * idh;
	if (hc < 0.0f) { t0 = (hp <= 0.0f) ? 1.0f : max(t0, -hc * idh); }
	else if (hp < 0.0f) t1 = min(t1, -hc * idh);
	float span = max(t1 - t0, 0.0f);
	float h0 = hc + dh * t0, h1 = hc + dh * t1;
	float e0 = exp(-h0 * iH), e1 = exp(-h1 * iH);
	float k  = (h1 - h0) * iH;
	float mean = (abs(k) > 1e-3f) ? (e0 - e1) / k : 0.5f * (e0 + e1);
	return dens * d * span * mean;
}

// Sun attenuation at height hp inside one layer: the column above it to the top, over
// the sine of the sun's elevation (both folded into M.y on the CPU).
float OroFogSunTau(float4 L, float4 M, float hp)
{
	float e = exp(-clamp(hp, 0.0f, L.y) * L.z);
	return M.y * max(e - M.x, 0.0f);
}

// posW = the pixel's camera-relative position, toSun = unit vector toward the sun.
// T = transmittance camera->pixel, sunAtt = share of the direct sun reaching the pixel,
// col = what the fog puts in front of it (display space).
void OroFog(float3 posW, float3 toSun, out float T, out float sunAtt, out float3 col)
{
	T = 1.0f; sunAtt = 1.0f; col = 0.0f;
	// K (0 for the cockpit interior draw) governs only the AIR between the pixel and
	// the eye - it rides dens inside OroFogTau, so T is 1 in the cabin. The sun's column
	// above the pixel is real wherever the pixel is, so sunAtt does NOT take K: the sun
	// entering the cabin crossed the fog, and the panel shadows soften with it.
	[branch] if ((gFogL0.w + gFogL1.w) > 0.0f) {
		float  d   = length(posW);
		float3 ray = posW / max(d, 1e-3f);
		float  rp  = length(gFogCam.xyz * gFogCam.w + posW);   // pixel geocentric radius
		float  dr  = rp - gFogCam.w;                           // pixel height minus camera height
		float  hp0 = gFogL0.x + dr, hp1 = gFogL1.x + dr;
		T = exp(-(OroFogTau(gFogL0, hp0, d) + OroFogTau(gFogL1, hp1, d)));
		sunAtt = exp(-(OroFogSunTau(gFogL0, gFogM0, hp0) + OroFogSunTau(gFogL1, gFogM1, hp1)));
		float ph = saturate(dot(ray, toSun));
		ph *= ph; ph *= ph; ph *= ph;                          // pow 8: a tight lobe around the sun
		col = gFogCol.rgb + gFogSun.rgb * ph;
	}
}

// SNOW COVER (dormant at gSnow.x 0; the look is tuned in the snow round). Up-facing
// surfaces whiten above the snow line, patches filling in as the cover rises through a
// static thresholded value noise. nuv = any static 2D coordinate of the surface.
float OroSnowNoise(float2 p)
{
	float2 i = floor(p), f = frac(p);
	f = f * f * (3.0f - 2.0f * f);
	float a = frac(sin(dot(i,                float2(127.1f, 311.7f))) * 43758.5453f);
	float b = frac(sin(dot(i + float2(1, 0), float2(127.1f, 311.7f))) * 43758.5453f);
	float c = frac(sin(dot(i + float2(0, 1), float2(127.1f, 311.7f))) * 43758.5453f);
	float d = frac(sin(dot(i + float2(1, 1), float2(127.1f, 311.7f))) * 43758.5453f);
	return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}
float OroSnowMask(float3 nrmW, float3 up, float alt, float2 nuv)
{
	float c = gSnow.x;
	float slope = smoothstep(0.30f, 0.75f, dot(nrmW, up));
	float lineF = saturate((alt - gSnow.y) * gSnow.z + 0.5f);
	float n     = 0.65f * OroSnowNoise(nuv) + 0.35f * OroSnowNoise(nuv * 3.1f + 7.3f);
	float edge  = smoothstep(1.0f - c * 1.15f - 0.12f, 1.0f - c * 1.15f + 0.12f, n);
	return (c > 0.0f) ? slope * lineF * edge : 0.0f;
}
#define ORO_SNOW_ALBEDO float3(0.86f, 0.88f, 0.93f)

// ORO patch (ab): the scene's normal+depth buffer (GBUF_DEPTH, camera DISTANCE in .a,
// 0 = sky) for the stencil ground shadows' SOFT depth test - see ShadowTechPS in
// Mesh.fx. gSceneDepthPrm = 1/W, 1/H, tolerance base (m; 0 = test off), tolerance/m.
uniform extern texture   gSceneDepth;
uniform extern float4    gSceneDepthPrm;
uniform extern float     gOroDbg = 0.0f;   // ORO patch (ab) INSTRUMENT: ShadowDebug mode for Mesh.fx ShadowTechPS
// ORO patch (ae): THE CASCADED SUN SHADOW ATLAS (Terrain shadowing mode 3). NINE ortho
// maps in ONE texture, ALL SHARING ONE LIGHT BASIS (round 7): slot 0 the focus vessel's
// box, slots 6-8 the boxes of the three nearest other vessels (each hull alone, each
// box riding its hull - crisp from any distance), slots 1-5 camera frustum slices
// (50 / 450 / 2000 / 8000 m / the far limit) holding everything else. Every box is an
// ortho projection along the same light with the same up, so a receiver projects into
// light space ONCE (u, v, depth - metres, camera-relative) and each slot is an offset
// and a scale: two float4 per slot instead of a matrix, which is what lets the mesh
// family see every slot it needs within ps_3_0's 224 constant registers (six full
// matrices were X4507 under his effect set - and so were nine compact slots with a
// pushed uv rect each, and so was a three-vector basis). THIS FAMILY SEES SEVEN SLOTS,
// in ITS OWN ORDER: 0 = the focus box, 1-3 = the cascades to 2 km (a hull or a hangar
// beyond that is pixels), 4-6 = the three hull boxes (Scene.cpp remaps them on push).
// gCascBasis = U, V across the light (U.w carries ShadowDebug; L, the direction the
// light travels, is -U x V - LookAt's axes are orthonormal and right-handed);
// gCascA[k] = (centre u, centre v, near-plane depth, 1/depth range); gCascTx = the seven
// texels in metres, four to a register (a dead slot has texel 0); gCascSplit = the split
// distances; gCascAtlas = (1/atlas W, 1/atlas H, ON, far). The slots' uv rectangles are
// FIXED by the atlas layout (Scene.cpp's table: cascades 1-3 full-size along the top
// row, the hull boxes and cascades 4-5 half-size along the second) and are arithmetic
// in the lookups. Scene.cpp fills them after the pass and zeroes ON at frame top and in
// the cockpit pass. The terrain (its own compile unit) takes all nine, in atlas order.
uniform extern float4    gCascBasis[2];
uniform extern float4    gCascA[7];
uniform extern float4    gCascTx[2];
uniform extern float4    gCascSplit = {0, 0, 0, 0};
uniform extern float4    gCascAtlas = {0, 0, 0, 0};
uniform extern texture   gCascMap;
// ORO patch (ac): BASE LIGHTS glow - a gain on the night textures' emission and the
// runway/taxiway light sprites, set by vBase around ITS draws only (1 = stock, and 1
// for every vessel). Past 1 the excess rides the fp16 chain into the bloom.
uniform extern float     gBaseGlow = 1.0f;
// ORO patch (ac) part 2: the HALO gain - a lamp in fog is not brighter, it is a soft
// aureole whose size grows with the optical depth between it and the eye (BeaconArray.fx).
uniform extern float     gBaseHalo = 1.0f;
sampler SceneDepthS = sampler_state
{
	Texture = <gSceneDepth>;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

uniform extern float     gWetDark;        // ORO patch (s) part 3: wet albedo darkening
                                          // gain, 1 = the designed look (pushed with a 1
                                          // default from C++, so an absent addon changes
                                          // nothing).
uniform extern float     gStorm;          // ORO patch (s) part 2: overcast factor, 0..1.
                                          // Collapses the directional sun into a lifted
                                          // ambient in every consumer below - under a
                                          // storm deck there is no sun disc, so there are
                                          // no sharp shadows, no warm light, no speculars.
uniform extern float 	 gMtrlAlpha;
uniform extern float	 gGlowConst;
uniform extern float	 gNightTime;		// 1 for nighttime, 0 for daytime
uniform extern Flow		 gCfg;

// Textures -----------------------------------------------------------------

uniform extern texture   gTex0;			    // Diffuse texture
uniform extern texture   gTex1;			    // Nightlights
uniform extern texture   gTex3;				// Normal Map / Cloud Microtexture
uniform extern texture   gSpecMap;			// Specular Map
uniform extern texture   gRghnMap;			// Roughness Map
uniform extern texture   gEmisMap;	    	// Emission Map
uniform extern texture   gEnvMapA;	    	// Environment Map (Mirror clear)
uniform extern texture   gEnvMapB;	    	// Environment Map (Mipmapped with different levels of blur)
uniform extern texture   gReflMap;   		// Reflectivity Map
uniform extern texture   gMetlMap;   		// Metalness Map
uniform extern texture   gHeatMap;   		// Heat Map
uniform extern texture   gTranslMap;		// Translucence Map
uniform extern texture   gTransmMap;		// Transmittance Map
uniform extern texture   gShadowMap;	    // Shadow Map
uniform extern texture   gIrradianceMap;    // Irradiance Map

// Legacy Atmosphere --------------------------------------------------------

uniform extern float     gGlobalAmb;        // Global Ambient Level
uniform extern float     gSunAppRad;        // Sun apparent size (Radius / Distance)
uniform extern float     gDispersion;
uniform extern float     gAmbient0;


// ----------------------------------------------------------------------------
// Vertex layouts
// ----------------------------------------------------------------------------

struct MESH_VERTEX {                        // D3D9Client Mesh vertex layout
	float3 posL   : POSITION0;
	float3 nrmL   : NORMAL0;
	float3 tanL   : TANGENT0;
	float3 tex0   : TEXCOORD0;
};

struct NTVERTEX {                           // Orbiter Mesh vertex layout
	float3 posL     : POSITION0;
	float3 nrmL     : NORMAL0;
	float2 tex0     : TEXCOORD0;
};

struct TILEVERTEX {                         // Vertex declaration used for surface tiles and cloud layer
	float3 posL     : POSITION0;
	float3 normalL  : NORMAL0;
	float2 tex0     : TEXCOORD0;
	float  elev     : TEXCOORD1;
};

struct HZVERTEX {
	float3 posL     : POSITION0;
	float4 color    : COLOR0;
	float2 tex0     : TEXCOORD0;
};

struct POSTEX {
	float3 posL     : POSITION0;
	float2 tex0     : TEXCOORD0;
};

struct SHADOW_VERTEX {
	float4 posL     : POSITION0;
};


// ----------------------------------------------------------------------------
// Vertex shader outputs
// ----------------------------------------------------------------------------

struct SimpleVS
{
	float4 posH     : POSITION0;
	float2 tex0     : TEXCOORD0;
	float3 nrmW     : TEXCOORD1;
	float3 toCamW   : TEXCOORD2;
};

struct HazeVS
{
	float4 posH    : POSITION0;
	float4 color   : TEXCOORD0;
	float2 tex0    : TEXCOORD1;
};

struct BShadowVS
{
	float4 posH    : POSITION0;
	float2 dstW    : TEXCOORD0;
	float  alpha   : TEXCOORD1;
};

struct ShadowTexVS
{
	float4 posH    : POSITION0;
	float2 dstW    : TEXCOORD0;
	float3 tex0	   : TEXCOORD1;
	float  dist    : TEXCOORD2;	// ORO patch (ab): the projected sheet's camera distance
};

// ----------------------------------------------------------------------------
// Texture Sampler implementations
// ----------------------------------------------------------------------------

sampler IrradS = sampler_state      // Irradiance map sampler
{
	Texture = <gIrradianceMap>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler CascS = sampler_state        // ORO patch (ae): the cascade atlas (point, clamp - PCF in the shader)
{
	Texture = <gCascMap>;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler ShadowS = sampler_state      // Shadow map sampler
{
	Texture = <gShadowMap>;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = POINT;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler WrapS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler ClampS = sampler_state      // Base tile sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler SpecS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gSpecMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler EmisS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gEmisMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler ReflS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gReflMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler MetlS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gMetlMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler HeatS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gHeatMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler RghnS = sampler_state       // Primary Mesh texture sampler
{
	Texture = <gRghnMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler TranslS = sampler_state       // Translucence texture sampler
{
	Texture = <gTranslMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};
sampler TransmS = sampler_state       // Transmittance texture sampler
{
	Texture = <gTransmMap>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler Tex1S = sampler_state       // Secundary mesh texture sampler (i.e. night texture)
{
	Texture = <gTex1>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler Nrm0S = sampler_state       // Normal Map Sampler
{
	Texture = <gTex3>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler MFDSamp = sampler_state     // Virtual Cockpit MFD screen sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler Panel0S = sampler_state     // Sampler for mesh based panels, Panel MFDs. Must be compatible with Non-power of two conditional due to MFD screens.
{
	Texture = <gTex0>;
	MinFilter = POINT;
	MagFilter = LINEAR;
	MipFilter = NONE;
	AddressU  = CLAMP;
	AddressV  = CLAMP;
};

sampler SimpleS = sampler_state       // Sampler used for SimpleTech. (Star, VC HUD)
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	MipMapLODBias = 0;
	AddressU = CLAMP; // Modified for RC29 to fix the line issue in top-right corner
	AddressV = CLAMP;
};

sampler ExhaustS = sampler_state
{
	Texture = <gTex0>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler RingS = sampler_state       // Planetary rings sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler EnvMapAS = sampler_state
{
	Texture = <gEnvMapA>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW = CLAMP;
};

// ============================================================================
// ORO patch (v): BOX-PROJECTED (parallax-corrected) environment sampling.
// A cube map stores the world as seen from ONE point; sampling it by raw
// reflection DIRECTION paints nearby geometry as if it stood at infinity -
// magnified and unanchored, the "zoomed in" look of any probe placed inside
// the thing it reflects (the Atlantis payload bay was the reporting case).
// The correction is the standard one: intersect the reflection ray with a
// proxy BOX around the probed volume and sample the cube TOWARD THE HIT
// POINT, so near geometry lands at the right place and scale from any eye
// position. The box (an OBB in the vessel's own axes) and the probe position
// arrive per probe from the _ecam config via the per-group swap in
// D3D9Mesh::Render; gEnvBoxC.w = 0 - the default, and every probe without a
// BOX line - bypasses the whole thing, so stock content cannot move.
// All vectors are camera-centred world, the shaders' native space.
// ============================================================================
uniform extern float4 gEnvBoxC;    // xyz box centre, w: 1 = box projection on
uniform extern float4 gEnvBoxX;    // xyz unit axis, w half-extent [m]
uniform extern float4 gEnvBoxY;
uniform extern float4 gEnvBoxZ;
uniform extern float4 gEnvPrbP;    // xyz probe position

// ORO patch (v) part 2: the PLANAR mirror - the mirrored-scene RT for groups a
// vessel's _ecam config assigns to a reflection plane. Sampled at the pixel's own
// SCREEN position (the mirrored view-projection makes a real point land exactly
// where the main camera sees its virtual image - the wet-ground mirror's law),
// with the pass's clip-space X flip undone here. gPlnCtl: xy = 1/screen,
// z = enable. Alpha 0 in the RT means "nothing reflected here" and the probe
// keeps that pixel, so planet and sky still arrive from the cube.
uniform extern texture   gPlnMap;
uniform extern float4    gPlnCtl;
// ORO patch (v) 2b: the mirror PLANE's equation (N, d) in camera-centred world,
// and gPlnCtl.w = RDIST [m]. Together they let the pixel shader re-aim the
// planar sample along the pixel's TRUE reflected ray - the curvature warp.
uniform extern float4    gPlnEq;

// ORO patch (w): PLANET-SHINE SHADOWS - a depth map of the focus assembly along
// the planet direction (the sun shadow-map machinery, reused). gPShnSHD:
// x = enable, y = texel size, z = world bias [m], w = 1/depth. With x = 0 the
// glow term below is bit-exact stock.
uniform extern float4x4  gPShnLVP;
uniform extern float4    gPShnSHD;
uniform extern texture   gPShnMap;

sampler PShnS = sampler_state
{
	Texture = <gPShnMap>;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

// Lit factor for the planet-shine term at posW (camera-centred world). The map
// stores 1 - z/w, the sun shadow-map convention; 4-tap PCF softens the edge -
// planet light is a huge area source, a hard edge would be a lie.
float PShineShadow(float3 posW)
{
	if (gPShnSHD.x < 0.5f) return 1.0f;
	// orthographic projection: w is exactly 1, no divide needed
	float3 h = mul(float4(posW, 1.0f), gPShnLVP).xyz;
	h.xy = h.xy * float2(0.5f, -0.5f) + 0.5f;
	if (any(saturate(h) != h)) return 1.0f;
	float pd = (1.0f - h.z) + gPShnSHD.z * gPShnSHD.w;
	float lit = step(tex2D(PShnS, h.xy).r, pd);
	lit += step(tex2D(PShnS, h.xy + gPShnSHD.yy).r, pd);
	return lit * 0.5f;
}


sampler PlnMapS = sampler_state
{
	Texture = <gPlnMap>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = NONE;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

float3 EnvDir(float3 posW, float3 R)
{
	if (gEnvBoxC.w < 0.5f) return R;
	// into the box frame
	float3 rp = posW - gEnvBoxC.xyz;
	float3 rl = float3(dot(rp, gEnvBoxX.xyz), dot(rp, gEnvBoxY.xyz), dot(rp, gEnvBoxZ.xyz));
	float3 rd = float3(dot(R,  gEnvBoxX.xyz), dot(R,  gEnvBoxY.xyz), dot(R,  gEnvBoxZ.xyz));
	// slab test, exit distance only (the surface is inside or near the box; a
	// zero direction component yields +-inf slabs, which IEEE handles for us)
	float3 ext = float3(gEnvBoxX.w, gEnvBoxY.w, gEnvBoxZ.w);
	float3 tm = max((ext - rl) / rd, (-ext - rl) / rd);
	float  t  = max(min(tm.x, min(tm.y, tm.z)), 0.0f);
	// the hit, re-aimed from the PROBE rather than the surface
	float3 pp = gEnvPrbP.xyz - gEnvBoxC.xyz;
	float3 pl = float3(dot(pp, gEnvBoxX.xyz), dot(pp, gEnvBoxY.xyz), dot(pp, gEnvBoxZ.xyz));
	float3 dl = (rl + rd * t) - pl;
	return normalize(dl.x * gEnvBoxX.xyz + dl.y * gEnvBoxY.xyz + dl.z * gEnvBoxZ.xyz + 1e-6f);
}

sampler EnvMapBS = sampler_state
{
	Texture = <gEnvMapB>;
	MinFilter = LINEAR;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	AddressU = CLAMP;
	AddressV = CLAMP;
	AddressW = CLAMP;
};


// Planet surface samplers ----------------------------------------------------

sampler Planet0S = sampler_state    // Planet/Cloud diffuse texture sampler
{
	Texture = <gTex0>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler Planet1S = sampler_state    // Planet nightlights/specular mask sampler
{
	Texture = <gTex1>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

sampler Planet3S = sampler_state    // Planet/Cloud micro texture sampler
{
	Texture = <gTex3>;
	MinFilter = ANISOTROPIC;
	MagFilter = LINEAR;
	MipFilter = LINEAR;
	MaxAnisotropy = ANISOTROPY_MACRO;
	AddressU = WRAP;
	AddressV = WRAP;
};



// ----------------------------------------------------------------------------
// Atmospheric Haze implementation
//
// att = attennuation, ins = inscatter, depth = pixel depth [0 to 1],
// posW = camera centric world space position of the vertex
// ----------------------------------------------------------------------------

void AtmosphericHaze(out float4 att, out float4 ins, in float depth, in float3 posW)
{
	if (gHazeMode==0) {
		att = 1;
		ins = 0;
		return;
	}
	else if (gHazeMode==1) {
		att = gAttennuate;
		ins = gInScatter;
		return;
	}
	else if (gHazeMode==2) {
		float fogFact = 1.0f / exp(max(0,depth) * gFogDensity);
		att = fogFact;
		ins = half4((1.0f-fogFact) * gFogColor.rgb, 0.0f);
		return;
	}
}


// ----------------------------------------------------------------------------
// Legacy sun color on planet surface. Used for planet surface, base tiles and
// buildings.  See SurfaceLighting() in D3D9Util.cpp
// ----------------------------------------------------------------------------

void LegacySunColor(out float4 diff, out float ambi, out float nigh, in float3 normalW)
{
	float   h = dot(-gSun.Dir, normalW);
	float   s = saturate((h+gSunAppRad)/(2.0f*gSunAppRad));
	float3 r0 = 1.0 - float3(0.65, 0.75, 1.0) * gDispersion;

	if (gDispersion!=0) { // case 1: planet has atmosphere
		float3 di = (r0 + (1.0-r0) * saturate(h*5.780)) * s;
		float  ni = (h+0.242)*2.924;
		float  am = saturate(max(gAmbient0*saturate(ni)-0.05, gGlobalAmb));

		diff = float4(di*(1.0-am*0.5),1);
		ambi = am;
		nigh = saturate(-ni-0.2);
	}
	else { // case 2: planet has no atmosphere
		diff = float4(r0*s, 1);
		ambi = gGlobalAmb;
		nigh = 0;
	}
}



// ----------------------------------------------------------------------------
// Vertex shader implementations
// ----------------------------------------------------------------------------


SimpleVS BasicVS(NTVERTEX vrt)
{
	SimpleVS outVS = (SimpleVS)0;
	float3 posW  = mul(float4(vrt.posL, 1.0f), gW).xyz;
	outVS.posH   = mul(float4(posW, 1.0f), gVP);
	outVS.nrmW   = mul(float4(vrt.nrmL, 0.0f), gW).xyz;
	outVS.toCamW = -posW;
	outVS.tex0   = vrt.tex0;
	return outVS;
}



// ----------------------------------------------------------------------------
// PixelShader Implementations
// ----------------------------------------------------------------------------

float4 SimpleTechPS(SimpleVS frg) : COLOR
{
	float4 c = tex2D(SimpleS, frg.tex0);
	return float4(c.rgb, c.a * gMix);
}

float4 PanelTechPS(SimpleVS frg) : COLOR
{
	float4 cTex = tex2D(SimpleS, frg.tex0);
	return float4(cTex.rgb, cTex.a*gMix);
}

float4 PanelTechBPS(SimpleVS frg) : COLOR
{
	float4 cTex = tex2D(Panel0S, frg.tex0);
	return float4(cTex.rgb, cTex.a*gMix);
}

float4 ExhaustTechPS(SimpleVS frg) : COLOR
{
	float4 c = tex2D(ExhaustS, frg.tex0);
	return float4(c.rgb, c.a*gMix);
}

float4 SpotTechPS(SimpleVS frg) : COLOR
{
	return (tex2D(SimpleS, frg.tex0) * gColor) * gMix;
}

// ----------------------------------------------------------------------------
// ORO patch (s): the rain-drop GLINT, as ONE shared function - because the first build
// put it inline in PBR_PS alone and a mesh with no advanced textures renders through
// RenderFast (Mesh.cpp:1453), a third shader path entirely: one vessel sparkled and its
// neighbour stayed bone dry. The fifth time in this project a rule was written and not
// swept ((r)'s own rule, "behaviour must not depend on which path a mesh takes"). A
// shared helper cannot drift between paths.
// Distance-aware: a drop cell is fixed in mesh UV, so up close one blob covers many
// pixels and reads as a headlight. Near the camera the blob TIGHTENS and DIMS toward
// fine speckle; the far look is unchanged.
float WetSparkle(float2 tex0, float3 nrmW, float dist)
{
	if (gSurfWet < 0.001f) return 0.0f;
	float2 uvS  = tex0 * 34.0f;
	float2 cell = floor(uvS);
	float  hc   = frac(sin(dot(cell, float2(127.1f, 311.7f))) * 43758.5453f);
	float  uu   = gWetTime * (1.0f / 0.55f) + hc;      // the splash-ring cadence
	float  ep   = floor(uu);
	float  tt   = frac(uu);
	float  h2   = frac(sin(dot(cell + ep * 7.13f, float2(269.5f, 183.3f))) * 43758.5453f);
	float  on   = step(h2, 0.34f);
	float  env  = on * saturate(tt * 9.0f) * exp(-tt * 5.5f);
	float2 fp   = frac(uvS) - float2(0.25f + 0.5f * frac(h2 * 13.7f),
	                                 0.25f + 0.5f * frac(h2 * 41.3f));
	float  distF = saturate(dist * (1.0f / 45.0f));
	float  blob  = saturate(1.0f - dot(fp, fp) * lerp(34.0f, 11.0f, distF));
	float  upF   = saturate(dot(nrmW, normalize(gCameraPos)) * 0.6f + 0.4f);
	return env * blob * upF * gSurfWet * lerp(0.30f, 1.0f, distF) * gWetGlint;
}

// ORO patch (ae): THE CASCADED SUN SHADOW ATLAS - the receiver side, shared by every
// family in this effect (Mesh.fx is included before Vessel.fx, so it lives here).
// Returns the LIT factor (1 = lit) for a camera-relative world position. Slot 0 is the
// focus vessel's own box (the crispest) and wins whenever the point lies inside it;
// otherwise the frustum cascade the point's distance falls in. Bias = a normal offset
// of 1.5 texels plus the texel-footprint depth bias (the (z3) law). Four-tap PCF at
// 1.5 texels, clamped inside the slot so a neighbouring cascade can never bleed in.
// ps_3_0 cannot index a constant array dynamically, so the slot is picked by selects.
// WARNING - NO UNIFORM-ONLY BRANCHES IN HERE: the vessel technique sits at ps_3_0's sixteen
// boolean registers, and a branch on a constant alone takes one (X4550 at the first
// compile). Every condition below carries a varying, and the ON flag is folded into
// the depth compare, so an unbound atlas (modes 0-2, the cockpit pass) reads black and
// everything comes back lit. The calls are gated by _CASCADE (mode 3 only), so the
// other modes pay nothing at all.
// One lookup in one slot (round 7). l = the receiver in light space (metres), ln = its
// normal in the same basis, g = THE RECEIVER PLANE'S DEPTH SLOPE (metres of depth per
// metre across the light plane, straight from the normal: N.U / N.L) - so every tap
// compares the map against the depth the receiver's OWN surface has at that texel. The
// slope term that used to be a blind bias (2.5 texels x tan - 24 m at a 2 m texel under
// a 20-degree sun, which is what ate a landed ShuttleA's 4 m of clearance on his
// round-6 flight) is exact now; what remains is under a texel, for curvature and the
// rasteriser. Half a texel of normal offset, grazing only. Bilinear PCF: the four
// texels round the sample, each compared, weighted by the sub-texel position.
// WARNING - THIS FAMILY IS COMPILED BY THE LEGACY EFFECT FRONT END (fx_2_0), which
// packs constants far worse than fxc's standalone compiler and spends them per INLINED
// copy of a lookup: round 6 sat at c220 of 224 under his effect set, and every extra
// tap or literal since was X4507. Two lookups is the budget - the cascade and one hull
// box - so no wide tent here and no [branch]; the terrain has both.
float OroCascTapC(float4 A, float tx, float2 uvo, float2 sc, float3 l, float3 ln, float2 g, float sn, float grz, float on)
{
	tx = max(tx, 1.0e-6f);
	float3 p  = l + ln * (0.5f * tx * sn);
	float  invR = 2.0f * gCascAtlas.x / (tx * sc.x);                   // r = texel x slot size / 2, slot size = sc.x x W
	float2 sp = (p.xy - A.xy) * (invR * 0.5f) + 0.5f;
	float  z  = (p.z - A.z) * A.w;
	if (on < 0.5f || any(sp < 0.0f) || any(sp > 1.0f) || z < 0.0f || z > 1.0f) return 1.0f;
	float2 tuv = gCascAtlas.xy;                                        // one atlas texel, in uv
	float2 uv  = clamp(uvo + sp * sc, uvo + tuv * 1.5f, uvo + sc - tuv * 1.5f);
	float2 tc  = uv / tuv - 0.5f;                                      // atlas texels, centres at integers
	float2 fl  = floor(tc), fr = tc - fl;
	float2 b   = (fl + 0.5f) * tuv;                                    // texel fl's centre
	float2 dz  = g * (tx * A.w);                                       // the plane's depth per texel step, map units
	float  z0  = z + dot(g, (b - uv) / tuv * tx) * A.w;                // the plane's depth at texel fl
	float  pd  = 1.0f - z0 + tx * (0.5f + 0.5f * grz) * A.w;
	float s00 = (tex2Dlod(CascS, float4(b, 0, 0)).r > pd) ? 1.0f : 0.0f;
	float s10 = (tex2Dlod(CascS, float4(b.x + tuv.x, b.y, 0, 0)).r > pd - dz.x) ? 1.0f : 0.0f;
	float s01 = (tex2Dlod(CascS, float4(b.x, b.y + tuv.y, 0, 0)).r > pd - dz.y) ? 1.0f : 0.0f;
	float s11 = (tex2Dlod(CascS, float4(b + tuv, 0, 0)).r > pd - dz.x - dz.y) ? 1.0f : 0.0f;
	return 1.0f - lerp(lerp(s00, s10, fr.x), lerp(s01, s11, fr.x), fr.y);
}

// does a HALF-SIZE box (a hull's) contain the point? 1 or 0 - the per-pixel pick below
float OroCascIn(float4 A, float tx, float3 l)
{
	float  k  = 6.0f * gCascAtlas.x / max(tx, 1.0e-6f);                // invR / 2 for a half-size slot (sc.x = 1/6)
	float2 sp = (l.xy - A.xy) * k + 0.5f;
	float  z  = (l.z - A.z) * A.w;
	return (tx > 0.0f && all(sp > 0.0f) && all(sp < 1.0f) && z > 0.0f && z < 1.0f) ? 1.0f : 0.0f;
}

float OroCascadeShadow(float3 posW, float3 nrmW, float3 toSun)
{
	float3 U  = gCascBasis[0].xyz, V = gCascBasis[1].xyz, L = -cross(U, V);
	float3 l  = float3(dot(posW, U), dot(posW, V), dot(posW, L));
	float3 ln = float3(dot(nrmW, U), dot(nrmW, V), dot(nrmW, L));
	float  nl = saturate(dot(nrmW, toSun));
	float  sn = sqrt(saturate(1.0f - nl * nl));
	float  grz = min(sn / max(nl, 0.05f), 4.0f);
	float2 g  = clamp(ln.xy / max(nl, 0.05f), -4.0f, 4.0f);           // the receiver plane's depth slope (N.L = -nl)
	float  d  = length(posW);
	float  on = gCascAtlas.z * ((d <= gCascSplit.z) ? 1.0f : 0.0f);   // the mesh family's cascades end at slot 3
	// the cascade by distance: slots 1-3 sit full-size along the atlas' top row
	float4 A = gCascA[1]; float tx = gCascTx[0].y; float si = 1.0f;
	if (d > gCascSplit.x) { A = gCascA[2]; tx = gCascTx[0].z; si = 2.0f; }
	if (d > gCascSplit.y) { A = gCascA[3]; tx = gCascTx[0].w; si = 3.0f; }
	float lit = OroCascTapC(A, tx, float2((si - 1.0f) / 3.0f, 0.0f), float2(1.0f / 3.0f, 0.5f), l, ln, g, sn, grz, on);
	// the hull boxes - the focus vessel's (slot 0) and the three nearest others' (4-6) -
	// each holding one hull alone, vessel-anchored, DISJOINT from the cascades (those
	// hulls are left out of them), so the receiver takes the min: no seam, and a parked
	// hull's shadow is rasterised on a grid that rides it whatever the camera does. ONE
	// lookup, in the box that contains the point - focus first, then the nearest others
	// (two hulls' light-space footprints overlap only when they are parked within a
	// hull of each other). Half-size, second row, at columns 0, 3, 4, 5.
	float4 Ah = gCascA[0]; float th = gCascTx[0].x; float xh = 0.0f;
	float  inh = OroCascIn(Ah, th, l);
	float  i4  = OroCascIn(gCascA[4], gCascTx[1].x, l);
	if (i4 > 0.5f && inh < 0.5f) { Ah = gCascA[4]; th = gCascTx[1].x; xh = 3.0f; inh = 1.0f; }
	float  i5  = OroCascIn(gCascA[5], gCascTx[1].y, l);
	if (i5 > 0.5f && inh < 0.5f) { Ah = gCascA[5]; th = gCascTx[1].y; xh = 4.0f; inh = 1.0f; }
	float  i6  = OroCascIn(gCascA[6], gCascTx[1].z, l);
	if (i6 > 0.5f && inh < 0.5f) { Ah = gCascA[6]; th = gCascTx[1].z; xh = 5.0f; inh = 1.0f; }
	lit = min(lit, OroCascTapC(Ah, th, float2(xh / 6.0f, 0.5f), float2(1.0f / 6.0f, 0.25f), l, ln, g, sn, grz, gCascAtlas.z * inh));
	// INSTRUMENT (ShadowDebug 4): the slot bands, folded in ARITHMETICALLY
	float band = (inh > 0.5f) ? 0.15f : 0.15f + 0.17f * si;
	return lerp(lit, band, saturate(gOroDbg - 3.5f));
}

#include "Particle.fx"
#include "Mesh.fx"
#include "Vessel.fx"
#include "HorizonHaze.fx"
#include "Planet.fx"
#include "BeaconArray.fx"


BShadowVS ArrowTechVS(float3 posL : POSITION0)
{
	// Zero output.
	BShadowVS outVS = (BShadowVS)0;
	float3 posW = mul(float4(posL, 1.0f), gW).xyz; // Apply world transformation matrix
	outVS.posH = mul(float4(posW, 1.0f), gVP); // Apply view projection matrix
	return outVS;
}

float4 ArrowTechPS(BShadowVS frg) : COLOR
{
	return gColor;
}


// This is used for rendering grapple points ----------------------------------
//
technique ArrowTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 ArrowTechVS();
		pixelShader = compile ps_3_0 ArrowTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = false;
		ZEnable = true;
	}
}


// This is used for many simple renderings ------------------------------------
//
technique SimpleTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 SimpleTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
	}
}

// This is used for 2DPanel and Glass cockpit ---------------------------------
//
technique PanelTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 PanelTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
	}
}

technique PanelTechB
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 PanelTechBPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
	}
}


// Thil will render exhaust textures ------------------------------------------
//
technique ExhaustTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 ExhaustTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = false;
		ZEnable = true;
	}
}

// This is used for rendering beacons -----------------------------------------
//
technique SpotTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BasicVS();
		pixelShader  = compile ps_3_0 SpotTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = false;
		ZEnable = true;
	}
}
