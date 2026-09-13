// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2012-2016 Jarmo Nikkanen
// ==============================================================

#ifndef __D3D9CONFIG_H
#define __D3D9CONFIG_H
#include <map>
#include <string>

extern class D3D9Config *Config;

/**
 * \brief Configuration Manager
 *
 * This class provides access to config-parameters that were read from the
 * config file.
 */
class D3D9Config {
public:

	D3D9Config ();
	~D3D9Config ();

	void Reset();
	bool ReadParams();
	void WriteParams();
	int  MaxLights();

	/// Bit flags for "LabelDisplayFlags" parameter.
	static const int LABEL_DISPLAY_RECORD = 0x1; ///< Display label "Record" on active recording session
	static const int LABEL_DISPLAY_REPLAY = 0x2; ///< Display label "Replay" on active playback session

	int Enable9On12;				///< Enable DX9 through DX12
	int PlanetPreloadMode;			///< Planet preload mode setting (0=load on demand, 1=preload)
	int PlanetLoadFrequency;		///< Load frequency for on-demand textures \[Hz\] (1...1000)
	int Anisotrophy;				///< Anisotropic filtering setting \[factor\] (1...16)
	int SceneAntialias;				///< Antialiasing setting \[factor\] (0...)
	int DisableDriverManagement;	///< Disable the D3D9 driver management \[sets the D3DCREATE_DISABLE_DRIVER_MANAGEMENT behavior flag\]  (0=default, 1:disabled)
	int DisableVisualHelperReadout;	///< Disable the hooking of the visual helper windows, to allow access to config parameter that Orbiter core doesn't provide (0=normal mode, 1=disable any hooking)
	int NearClipPlane;				///< Near clip plane mode (0,1)
	int DebugBreak;					///< Enable Debug Break
	int PreLBaseVis;				///< Preload base visuals (0=load on demand, 1=preload)
	int DebugFontSize;				///< Debug font height \[pixel\] (default=18px)
	int UseNormalMap;				///< Enable normal mapping (0,1)
	int SketchpadFont;				///< Sketchpad Font (0=Crisp, 1=Default, 2=Cleartype, 3=Proof Quality)
	int RwyLightAnimate;			///< Runway light animate (0,1)
	double RwyLightAngle;			///< Runway light angle \[deg\] (10...180)
	double RwyBrightness;			///< Runway light brightness (0.3...3.0)
	double VCNearPlane;				///< Virtual cockpit near clip-plane distance \[m\] (-1.0...1.0, default=0.1)
	double Convergence;				///< StereoScopic 3D convergence distance \[m\] (0.05...1.0, default=0.2)
	double Separation;				///< StereoScopic 3D depth of field separation \[m\] (10.0...100.0, default=65)
	double SunAngle;				///< Sun-angle above horizon when night-lights set it \[deg\] (0.1...20.0, default=10)
	double BumpAmp;					///< Bump map amplification setting (0.1...10.0, default=1)
	double PlanetGlow;				///< Intensity of planet glow effect (0.01...2.0, default=0.7)
	double FrameRate;				///< Frame-rate limiter
	double OrbitalShadowMult;		///< Multiplier for cloud shadows for Orbital flight
	int EnableLimiter;				///< Enable frame-rate limiter
	int DebugLvl;					///< Level of debug output 'verbosity'. Higher values create more detailed output (0...4, default=1)
	int LabelDisplayFlags;			///< Label display option flags. For example the "Record" and "Replay" labels (0=all disabled, 1=show record label, 2=show replay label, 3=show both \[default\])
	int LightConfig;				///< Light emitter configuration
	int NVPerfHUD;					///< ??? (0,1)
	int EnvMapSize;					///< Environment map size (64...512)
	int EnvMapMode;					///< Environment map mode (0=disabled, 1=planet only, 2=full scene, 3=full scene ORO exp - patch (v))
	int EnvMapFaces;				///< Number of environment map faces render per frame (1..6, default=1)
	int EnableGlass;				///< Enable improved glass shading (Fresnel reflection)
	int EnableMeshDbg;				///< Enable mesh debugger
	int ShadowMapMode;				///< Shadow Mapping Mode
	int ShadowFilter;				///< Shadow Mapping Filter
	int ShadowMapSize;				///< Shadow Map size
	int TerrainShadowing;			///< Terrain Shadowing mode (0=None, 1=Stencil, 2=Projected, 3=Cascaded (ORO patch ae), default=1)
	int LocalLightShadows;			///< ORO patch (z3): shadow maps for local SPOT lights (0=off/stock, 1=on, default=1)
	int LocalLightShadowPoint;		///< ORO patch (ah) step 5: POINT lights cast too - 0 off, 1 an aimed map (a pseudo-spot fitted to the casters), 2 a five-face cube where five cells are free (default 1)
	int LocalLightShadowMaps;		///< ORO patch (ah) step 4: how many spot lights cast at once - 1, 2, 4 or 6 maps (default 4). Cascaded mode keeps them in the cascade atlas' spare row; the other modes get a small local atlas
	double ShadowDepthTol;			///< ORO patch (ab): stencil ground-shadow soft depth test, base tolerance [m] (default 1)
	double ShadowDepthTolK;			///< ORO patch (ab): ...plus this per metre of distance (default 0.001)
	int ShadowDebug;				///< ORO patch (ab)/(ae) INSTRUMENT: 0 off; 1 colour the stencil sheets by the depth test's verdict; 2 by the signed depth difference; 3 dump the cascade atlas once; 4 = 3 + colour every receiver by the cascade slot it reads; 5 = 3 + black every receiver whose grazing slope exceeds ShadowCascadeSlope (half-dark past half of it). All log once a second.
int LocalLightSelfShadow;		///< ORO patch (ah) step 2 DIAGNOSTIC (2026-09-08, his SSV test): 1 = the emitter's OWN vessel casts into its own shadow map too, spot or point (normally excluded - emitters are authored inside hulls). Hidden key, default 0. 2026-09-13: the containment rule in aimCasters/OroFitCasterSphere used to reject the owner right after this flag admitted it, so for a point light the flag did nothing at all; the owner's own sphere is exempt from that skip now.
	int ShadowCascadeSize;			///< ORO patch (ae): one cascade's map size; the atlas is 3x2 of these (512..4096, default 2048; 4096 = 768 MB and 16384-wide texture caps)
	double ShadowCascadeFar;		///< ORO patch (ae): the far cascade's reach in metres (1000..60000, default 30000)
	int ShadowCascadeSoft;			///< ORO patch (ae): 1 = the coarse cascades take a three-texel tent (soft far shadows), 0 = bilinear everywhere (default 1)
	double ShadowCascadeSlope;		///< ORO patch (ae) THE MOIRE FIX (2026-09-13): the receiver-plane slope clamp in the cascade taps, as the tangent of the grazing angle (1..32, default 16 = 86 deg). It was a literal 4 (76 deg): past that the slope correction stopped growing while the surface's depth-per-texel kept growing as tan, and every receiver at a lower sun shadowed ITSELF - the hatched moire on a hull, a wall or the ground. Hidden key.
	double ShadowCascadeOffset;		///< ORO patch (ae) THE MOIRE FIX: the receiver's normal offset before the lookup, in texels x sin(grazing) (0..8, default 1.5; it was 0.5). Its depth margin is offset x tan - the same growth the acne has - so it beats the clamp error at any angle, and it only ever moves the LOOKUP sideways (<= offset texels), never the depth, so contact shadows stay put. Hidden key.
	int ParticleLight;				///< ORO patch (x): DIFFUSE particle sun lighting (0=Off/stock always-lit, 1=Brightness only, 2=Brightness+colour, default=2)
	double ParticleShadow;			///< ORO patch (x): DIFFUSE particle ground-shadow strength 0..1 (0=no shadow, 1=stock, default=1)
	double ParticleTintLead;		///< ORO patch (x): dawn-hue lead in sin-elevation, 0..0.20 (the smoke reads the sun this much HIGHER than it is, default=0.10)
	double ParticleTintSat;			///< ORO patch (x): dawn-hue depth 1..4 (1 = the hull's own hue, default=1.6)
	double ParticleTintBloom;		///< ORO patch (x): sunlit tinted overdrive 1..3 (brightens + blooms the tinted sun term, 1 = off, default=2.34)
	int CustomCamMode;				///< Custom Camera Mode
	int TileMipmaps;				///< Enable surface tile mipmaps
	int ShaderDebug;				///< Shader Debug Logging enable flag (0=disabled, 1=enabled)
	double LODBias;					///< 3D Terrain resolution bias
	int MeshRes;					///< Tile patch mesh resolution
	int MaxTiles;
	int TileDebug;					///< Enable tile debugger
	int TextureMips;				///< Texture mipmap auto-gen policy
	int PostProcess;				///< Enable post processing effects
	int MicroMode;					///< Surface micro textures enable flag (0=disabled, 1=enabled)
	int MicroFilter;				///< Surface micro texture filter mode (0=Point, 1=Linear ,2=Anisotropic 2x ,3=Anisotropic 4x, 4=Anisotropic 8x, Anisotropic 16x)
	int BlendMode;					///< Surface micro texture light blend mode (0=Soft, 1=Normal, 2=Hard)
	int PresentLocation;			///< PresentScene call-location (0=at clbkDisplayFrame, 1=at clbkRenderScene)
	int ShaderCacheUse;				///< Shader cache usage flag (0=disabled, 1=enabled)
	int MicroBias;					///< Mipmap LOD Bias for surface micro textures
	int CloudMicro;					///< Cloud layer micro textures
	int PlanetTileLoadFlags;		///< Planet Tile Load Flags (0x1=load tiles from directory tree, 0x2=load tiles from compressed archive, 0x3=both \[try directory tree first, then archive\])
	int GDIOverlay;					///< GDI Overlay
	int gcGUIMode;					///< gcGUI Operation Mode
	int bAbsAnims;					///< Absolute animations
	int bCloudNormals;				///< Felix24's Cloud normals implementation test
	int bFlats;						///< Face's terrain flattening
	int bGlares;
	int bLocalGlares;
	int bIrradiance;
	int bAtmoQuality;
	int NoPlanetAA;					///< Disable planet surface anti-aliasing to prevent white pixels at horizon 
	char *DebugFont;				///< Font face for debug lines (default="Fixed")
	char *SolCfg;					///< Solar system to use (default="Sol")
	double GFXIntensity;			///< Post Processing | Light glow intensity (0.0...1.0, default=0.5)
	double GFXDistance;				///< Post Processing | Light glow distance (0.0...1.0, default=0.8)
	double GFXThreshold;			///< Post Processing | Glow threshold (0.5...2.0, default=1.1)
	double GFXGamma;				///< Post Processing | Gamma (0.3...2.5, default=1.0)
	double GFXSunIntensity;			///< Light Configuration| Sunlight Intensity (0.5...2.5, default=1.2)
	double GFXLocalMax;				///< Light Configuration| Local Lights Max (0.001...1.0, default=0.5)
	double GFXGlare;				///< Sun glare intensity| (0.001...1.0, default=0.5)

	std::map<std::string, std::string> AtmoCfg;

private:

};

#endif // !__D3D9CONFIG_H
