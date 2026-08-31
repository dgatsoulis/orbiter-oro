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
