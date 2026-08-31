// ==============================================================
// Configuration Manager
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2007-2016 Martin Schweiger
//				 2011-2016 Jarmo Nikkanen
// ==============================================================

#include "D3D9Config.h"
#include "Orbitersdk.h"

using std::min;
using std::max;

static const char* cfgfile = "D3D9Client.cfg";

class D3D9Config *Config;				// configuration manager

// ==============================================================

D3D9Config::D3D9Config()
{
	Reset();
	ReadParams();
}

D3D9Config::~D3D9Config ()
{
	WriteParams ();
	delete []DebugFont;
	DebugFont = NULL;
	delete []SolCfg;
	SolCfg = NULL;
}

void D3D9Config::Reset ()
{
	Enable9On12			= 0;
	OrbitalShadowMult   = 0.85;
	PlanetPreloadMode	= 0;
	PlanetLoadFrequency	= 40;
	Anisotrophy			= 4;
	SceneAntialias		= 4;
	DebugLvl			= 1;
	VCNearPlane			= 0.1;
	LightConfig			= 2;
	NearClipPlane		= 0;
	NVPerfHUD			= 0;
	PreLBaseVis			= 0;
	DebugFontSize		= 18;
	UseNormalMap		= 1;
	SketchpadFont		= 1;
	RwyLightAnimate		= 1;
	RwyLightAngle		= 120.0;
	RwyBrightness		= 1.0;
	Convergence			= 0.2;
	Separation			= 65.0;
	SunAngle			= 10.0;
	BumpAmp				= 1.0;
	PlanetGlow			= 1.0;
	EnvMapSize			= 256;
	EnvMapMode			= 1;
	EnvMapFaces			= 1;
	EnableGlass			= 1;
	EnableMeshDbg		= 1;
	ShadowMapMode		= 1;
	ShadowFilter		= 0;
	ShadowMapSize		= 2048;
	FrameRate			= 200.0;
	EnableLimiter		= 0;
	CustomCamMode		= 1;
	TileMipmaps			= 1;
	TextureMips			= 1;
	LODBias				= 0.0;
	MeshRes				= 1;
	MaxTiles			= 1;
	TileDebug			= 0;
	MicroMode			= 1;
	MicroFilter			= 2;
	BlendMode			= 1;
	MicroBias			= 3;
	PostProcess			= 0;
	ShaderDebug			= 0;
	PresentLocation		= 1;
	PlanetTileLoadFlags	= 0x3;
	TerrainShadowing	= 2;
	ParticleLight		= 2;			// ORO patch (x): brightness + colour by default
	ParticleShadow		= 1.0;			// ORO patch (x): stock shadow strength by default
	ParticleTintLead	= 0.10;			// ORO patch (x): the smoke runs ~6 deg of sun ahead of the hull
	ParticleTintSat		= 1.6;			// ORO patch (x): dawn hue deepened (a grey medium washes tint out)
	ParticleTintBloom	= 2.34;			// ORO patch (x): tinted overdrive into the bloom
										// (all three: HIS settled slider positions, 2026-08-30 dawn ascent)
	LabelDisplayFlags	= LABEL_DISPLAY_RECORD | LABEL_DISPLAY_REPLAY;
	CloudMicro			= 1;
	GDIOverlay			= 0;
	gcGUIMode			= 0;
	bAbsAnims			= 0;
	bCloudNormals		= 1;
	bFlats				= 1;
	bGlares				= 1;
	bLocalGlares		= 0;
	DebugBreak			= 0;
	ShaderCacheUse		= 0;
	bIrradiance			= 1;
	bAtmoQuality		= 1;
	NoPlanetAA			= 0;
	
	GFXIntensity = 0.5;
	GFXDistance = 0.8;
	GFXThreshold = 1.1;
	GFXGamma = 1.0;
	GFXSunIntensity = 1.2;
	GFXLocalMax = 0.5;
	GFXGlare = 0.3;

	DisableDriverManagement = 0;
	DisableVisualHelperReadout = 0;

	AtmoCfg["Earth"] = "Earth.atm.cfg";

	SolCfg = new char[64];   strcpy_s(SolCfg,64,"Sol");
	DebugFont = new char[64];   strcpy_s(DebugFont,64,"Fixed");
}

int D3D9Config::MaxLights()
{
	if (LightConfig == 0) return 1;
	if (LightConfig <= 2) return 4;
	return 8;
}

bool D3D9Config::ReadParams ()
{
	int i;
	double d;

	FILEHANDLE hFile = oapiOpenFile(cfgfile, FILE_IN_ZEROONFAIL, ROOT);
	if (!hFile) return false;

	if (oapiReadItem_int   (hFile, (char*)"EnableDX12Wrapper", i))		Enable9On12 = max(0, min(1, i));
	if (oapiReadItem_float (hFile, (char*)"FrameRate", d))				FrameRate = max(0.0, min(300.0, d));
	if (oapiReadItem_int   (hFile, (char*)"EnableLimiter", i))			EnableLimiter = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"CustomCamMode", i))			CustomCamMode = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"PlanetPreloadMode", i))		PlanetPreloadMode = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"PlanetTexLoadFreq", i))		PlanetLoadFrequency = max(1, min(1000, i));
	if (oapiReadItem_int   (hFile, (char*)"Anisotrophy", i))			Anisotrophy = max(1, min(16, i));
	if (oapiReadItem_int   (hFile, (char*)"SceneAntialias", i))		SceneAntialias = i;
	if (oapiReadItem_int   (hFile, (char*)"SketchpadFont", i))			SketchpadFont = max(0, min(2, i));
	if (oapiReadItem_int   (hFile, (char*)"PreLoadBaseVisuals", i))	PreLBaseVis = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"EnableNormalMapping", i))	UseNormalMap = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"NearClipPlaneMode", i))		NearClipPlane = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"RwyLightAnimate", i))		RwyLightAnimate = max(0, min(1, i));
	if (oapiReadItem_float (hFile, (char*)"RwyLightAngle", d))			RwyLightAngle = max(10.0, min(180.0, d));
	if (oapiReadItem_float (hFile, (char*)"RwyBrightness", d))			RwyBrightness = max(0.3, min(3.0, d));
	if (oapiReadItem_float (hFile, (char*)"NightLightsAngle", d))		SunAngle = max(0.1, min(20.0, d));
	if (oapiReadItem_float (hFile, (char*)"BumpMapAmplitude", d))		BumpAmp = max(0.1, min(10.0, d));
	if (oapiReadItem_float (hFile, (char*)"PlanetGlow", d))			PlanetGlow = max(0.01, min(2.5, d));
	if (oapiReadItem_int   (hFile, (char*)"EnvMapSize", i))			EnvMapSize = max(64, min(512, i));
	if (oapiReadItem_int   (hFile, (char*)"EnvMapMode", i))			EnvMapMode = max(0, min(3, i));	// ORO patch (v): 3 = Full Scene ORO (exp); stock's own clamp reads it as 2
	if (oapiReadItem_int   (hFile, (char*)"EnvMapFaces", i))			EnvMapFaces = max(1, min(3, i));
	if (oapiReadItem_int   (hFile, (char*)"ShadowMapMode", i))			ShadowMapMode = max(0, min(3, i));
	if (oapiReadItem_int   (hFile, (char*)"ShadowMapFilter", i))	 	ShadowFilter = max(0, min(5, i));
	if (oapiReadItem_int   (hFile, (char*)"ShadowMapSize", i))			ShadowMapSize = max(512, min(4096, i));
	if (oapiReadItem_int   (hFile, (char*)"EnableGlass", i))			EnableGlass = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"TerrainShadowing", i))		TerrainShadowing = max(0, min(2, i));
	if (oapiReadItem_int   (hFile, (char*)"ParticleLight", i))			ParticleLight = max(0, min(2, i));
	if (oapiReadItem_float (hFile, (char*)"ParticleShadow", d))			ParticleShadow = max(0.0, min(1.0, d));
	if (oapiReadItem_float (hFile, (char*)"ParticleTintLead", d))		ParticleTintLead = max(0.0, min(0.2, d));
	if (oapiReadItem_float (hFile, (char*)"ParticleTintSat", d))		ParticleTintSat = max(1.0, min(4.0, d));
	if (oapiReadItem_float (hFile, (char*)"ParticleTintBloom", d))		ParticleTintBloom = max(1.0, min(3.0, d));
	if (oapiReadItem_int   (hFile, (char*)"EnableMeshDbg", i))			EnableMeshDbg = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"TileMipmaps", i))			TileMipmaps = max(0, min(2, i));
	if (oapiReadItem_int   (hFile, (char*)"TextureMips", i))			TextureMips = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"TileDebug", i))				TileDebug = max(0, min(1, i));
	if (oapiReadItem_float (hFile, (char*)"StereoSeparation", d))		Separation = max(10.0, min(100.0, d));
	if (oapiReadItem_float (hFile, (char*)"StereoConvergence", d))		Convergence = max(0.05, min(1.0, d));
	if (oapiReadItem_int   (hFile, (char*)"DebugLvl", i))				DebugLvl = i;
	if (oapiReadItem_float (hFile, (char*)"VCNearPlane", d))			VCNearPlane = max(-1.0, min(1.0, d));
	if (oapiReadItem_int   (hFile, (char*)"LightCongiguration", i))	LightConfig = max(min(4, i), 0); // Old typo stored?
	if (oapiReadItem_int   (hFile, (char*)"LightConfiguration", i))	LightConfig = max(min(4, i), 0); // ...this will override it anyhow
	if (oapiReadItem_int   (hFile, (char*)"DisableDrvMgm", i))			DisableDriverManagement = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"NVPerfHUD", i))				NVPerfHUD = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"DebugLineFontSize", i))		DebugFontSize = i;
	if (oapiReadItem_int   (hFile, (char*)"DisableVisualHelperReadout", i))	DisableVisualHelperReadout = max(0, min(1, i));
	if (oapiReadItem_float (hFile, (char*)"LODBias", d))						LODBias = max(-2.0, min(2.0, d));
	if (oapiReadItem_int   (hFile, (char*)"MeshRes", i))						MeshRes = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"MaxTiles", i))						MaxTiles = max(0, min(2, i));
	if (oapiReadItem_int   (hFile, (char*)"MicroMode", i))						MicroMode = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"MicroFilter", i))					MicroFilter = max(0, min(5, i));
	if (oapiReadItem_int   (hFile, (char*)"BlendMode", i))						BlendMode = max(0, min(2, i));
	if (oapiReadItem_int   (hFile, (char*)"MicroBias", i))						MicroBias = max(0, min(10, i));
	if (oapiReadItem_int   (hFile, (char*)"CloudMicro", i))					CloudMicro = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"PostProcess", i))					PostProcess = max(0, min(2, i));
	if (oapiReadItem_int   (hFile, (char*)"ShaderDebug", i))					ShaderDebug = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"PresentLocation", i))				PresentLocation = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"PlanetTileLoadFlags", i))			PlanetTileLoadFlags = max(1, min(3, i));
	if (oapiReadItem_int   (hFile, (char*)"LabelDisplayFlags", i))				LabelDisplayFlags = max(0, min(3, i));
	if (oapiReadItem_int   (hFile, (char*)"GDIOverlay", i))					GDIOverlay = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"gcGUIMode", i))						gcGUIMode = max(0, min(3, i));
	if (oapiReadItem_int   (hFile, (char*)"AbsoluteAnimations", i))			bAbsAnims = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"NormalmappedClouds", i))			bCloudNormals = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"TerrainFlats", i))					bFlats = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"SunGlare", i))						bGlares = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"LightsGlare", i))					bLocalGlares = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"Irradiance", i))					bIrradiance = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"AtmoQuality", i))					bAtmoQuality = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"DebugBreak", i))					DebugBreak = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"ShaderCacheUse", i))				ShaderCacheUse = max(0, min(1, i));
	if (oapiReadItem_int   (hFile, (char*)"NoPlanetAA", i))					NoPlanetAA = max(0, min(1, i));
	if (oapiReadItem_float (hFile, (char*)"OrbitalShadowMult", d))			    OrbitalShadowMult = max(0.5, min(10.0, d));

	if (oapiReadItem_float (hFile, (char*)"GFXIntensity", d))					GFXIntensity = max(0.0, min(1.0, d));
	if (oapiReadItem_float (hFile, (char*)"GFXDistance", d))					GFXDistance = max(0.0, min(1.0, d));
	if (oapiReadItem_float (hFile, (char*)"GFXThreshold", d))					GFXThreshold = max(0.5, min(2.0, d));
	if (oapiReadItem_float (hFile, (char*)"GFXGamma", d))						GFXGamma = max(0.3, min(2.5, d));
	if (oapiReadItem_float (hFile, (char*)"GFXSunIntensity", d))				GFXSunIntensity = max(0.5, min(2.5, d));
	if (oapiReadItem_float (hFile, (char*)"GFXLocalMax", d))					GFXLocalMax = max(0.001, min(1.0, d));
	if (oapiReadItem_float (hFile, (char*)"GFXGlare", d))						GFXGlare = max(0.1, min(10.0, d));


	oapiReadItem_string (hFile, (char*)"SolCfg", SolCfg);
	oapiReadItem_string (hFile, (char*)"DebugLineFont", DebugFont);

	char Temp[256];
	if (oapiReadItem_string(hFile, (char*)"EarthAtmoCfg", Temp)) AtmoCfg["Earth"] = Temp;

	oapiCloseFile (hFile, FILE_IN_ZEROONFAIL);

	return true;
}


// ============================================================================
// ORO patch (j): WRITE THE CFG WITH RAW WIN32 I/O, ATOMICALLY.
//
// The stock implementation wrote through oapiOpenFile(FILE_OUT) - i.e. a
// `new std::ofstream` constructed INSIDE orbiter.exe. When WriteParams runs
// from ExitModule at application close (~D3D9Config), the core's C++ static
// objects (iostream/locale machinery) are ALREADY DESTROYED: the C-level open
// underneath still succeeds - which TRUNCATES D3D9Client.cfg - and the C++
// layer then rips on the dead locale facets. Measured on 2026-08-08 with a
// raw-Win32 tracer: EXCEPTION 0xC0000005 with the instruction pointer inside
// ASCII string bytes ("bad "), i.e. a call through a clobbered runtime
// pointer, swallowed by the loader so the process still "exits cleanly".
// Net effect: EVERY normal exit left a 0-byte cfg and all video settings
// silently reverted to defaults on the next launch. (The session-start write
// in clbkCreateRenderWindow worked - statics alive - but the next Launchpad
// close destroyed the file again.)
//
// Fix: no C++ streams, no oapi file I/O - plain kernel32 CreateFile/WriteFile
// (safe at any teardown stage), and an ATOMIC REPLACE (write .new, then
// MoveFileEx over the real file) so no failure mode can ever leave a
// truncated cfg again: if anything goes wrong the previous file survives.
// Behavioural note for the upstream report: this is a stock Orbiter 2024 bug,
// reproducible with an unpatched client - the patch only changes HOW the same
// items are written, not which or where.
// ============================================================================
void D3D9Config::WriteParams ()
{
	char buf[8192]; int n = 0;
#define WP_INT(name, val) n += sprintf_s(buf + n, sizeof(buf) - (size_t)n, "%s = %d\r\n",  name, (int)(val))
#define WP_FLT(name, val) n += sprintf_s(buf + n, sizeof(buf) - (size_t)n, "%s = %g\r\n",  name, (double)(val))
#define WP_STR(name, val) n += sprintf_s(buf + n, sizeof(buf) - (size_t)n, "%s = %s\r\n",  name, (const char*)(val))

	WP_INT("EnableDX12Wrapper", Enable9On12);
	WP_FLT("FrameRate", FrameRate);
	WP_INT("EnableLimiter", EnableLimiter);
	WP_INT("CustomCamMode", CustomCamMode);
	WP_INT("PlanetPreloadMode", PlanetPreloadMode);
	WP_INT("PlanetTexLoadFreq", PlanetLoadFrequency);
	WP_INT("Anisotrophy", Anisotrophy);
	WP_INT("SceneAntialias", SceneAntialias);
	WP_INT("SketchpadFont", SketchpadFont);
	WP_INT("PreLoadBaseVisuals", PreLBaseVis);
	WP_INT("EnableNormalMapping", UseNormalMap);
	WP_INT("NearClipPlaneMode", NearClipPlane);
	WP_INT("RwyLightAnimate", RwyLightAnimate);
	WP_FLT("RwyLightAngle", RwyLightAngle);
	WP_FLT("RwyBrightness", RwyBrightness);
	WP_FLT("NightLightsAngle", SunAngle);
	WP_FLT("BumpMapAmplitude", BumpAmp);
	WP_FLT("PlanetGlow", PlanetGlow);
	WP_INT("EnvMapSize", EnvMapSize);
	WP_INT("EnvMapMode", EnvMapMode);
	WP_INT("EnvMapFaces", EnvMapFaces);
	WP_INT("ShadowMapMode", ShadowMapMode);
	WP_INT("ShadowMapFilter", ShadowFilter);
	WP_INT("ShadowMapSize", ShadowMapSize);
	WP_INT("TerrainShadowing", TerrainShadowing);
	WP_INT("ParticleLight", ParticleLight);
	WP_FLT("ParticleShadow", ParticleShadow);
	WP_FLT("ParticleTintLead", ParticleTintLead);
	WP_FLT("ParticleTintSat", ParticleTintSat);
	WP_FLT("ParticleTintBloom", ParticleTintBloom);
	WP_INT("EnableGlass", EnableGlass);
	WP_INT("EnableMeshDbg", EnableMeshDbg);
	WP_INT("TileMipmaps", TileMipmaps);
	WP_INT("TextureMips", TextureMips);
	WP_INT("TileDebug", TileDebug);
	WP_FLT("StereoSeparation", Separation);
	WP_FLT("StereoConvergence", Convergence);
	WP_INT("DebugLvl", DebugLvl);
	WP_FLT("VCNearPlane", VCNearPlane);
	WP_INT("LightConfiguration", LightConfig);
	WP_INT("DisableDrvMgm", DisableDriverManagement);
	WP_INT("NVPerfHUD", NVPerfHUD);
	WP_INT("DebugLineFontSize", DebugFontSize);
	WP_INT("DisableVisualHelperReadout", DisableVisualHelperReadout);
	WP_FLT("LODBias", LODBias);
	WP_INT("MeshRes", MeshRes);
	WP_INT("MaxTiles", MaxTiles);
	WP_INT("MicroMode", MicroMode);
	WP_INT("MicroFilter", MicroFilter);
	WP_INT("BlendMode", BlendMode);
	WP_INT("MicroBias", MicroBias);
	WP_INT("CloudMicro", CloudMicro);
	WP_INT("PostProcess", PostProcess);
	WP_INT("ShaderDebug", ShaderDebug);
	WP_INT("PresentLocation", PresentLocation);
	WP_INT("PlanetTileLoadFlags", PlanetTileLoadFlags);
	WP_INT("LabelDisplayFlags", LabelDisplayFlags);
	WP_INT("GDIOverlay", GDIOverlay);
	WP_INT("gcGUIMode", gcGUIMode);
	WP_INT("AbsoluteAnimations", bAbsAnims);
	WP_INT("NormalmappedClouds", bCloudNormals);
	WP_INT("TerrainFlats", bFlats);
	WP_INT("SunGlare", bGlares);
	WP_INT("LightsGlare", bLocalGlares);
	WP_INT("Irradiance", bIrradiance);
	WP_INT("AtmoQuality", bAtmoQuality);
	WP_INT("DebugBreak", DebugBreak);
	WP_INT("ShaderCacheUse", ShaderCacheUse);
	WP_INT("NoPlanetAA", NoPlanetAA);

	WP_FLT("OrbitalShadowMult", OrbitalShadowMult);
	WP_FLT("GFXIntensity", GFXIntensity);
	WP_FLT("GFXDistance", GFXDistance);
	WP_FLT("GFXThreshold", GFXThreshold);
	WP_FLT("GFXGamma", GFXGamma);
	WP_FLT("GFXSunIntensity", GFXSunIntensity);
	WP_FLT("GFXLocalMax", GFXLocalMax);
	WP_FLT("GFXGlare", GFXGlare);

	WP_STR("SolCfg", SolCfg);
	WP_STR("DebugLineFont", DebugFont);
	WP_STR("EarthAtmoCfg", AtmoCfg["Earth"].c_str());

#undef WP_INT
#undef WP_FLT
#undef WP_STR

	if (n <= 0 || n >= (int)sizeof(buf)) return;         // formatting failed: keep the old file

	char tmpname[MAX_PATH];
	sprintf_s(tmpname, MAX_PATH, "%s.new", cfgfile);

	HANDLE h = CreateFileA(tmpname, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return;                // cannot write: keep the old file

	DWORD w = 0;
	BOOL ok = WriteFile(h, buf, (DWORD)n, &w, NULL) && (w == (DWORD)n);
	if (ok) ok = FlushFileBuffers(h);
	CloseHandle(h);

	if (ok) MoveFileExA(tmpname, cfgfile, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	else    DeleteFileA(tmpname);                         // partial temp: discard, keep the old file
}

// Stock implementation kept for reference (DO NOT CALL AT EXIT - see patch (j)
// note above; `new ofstream` inside the core crashes after static teardown):
static void WriteParams_stock_disabled (D3D9Config*)
{
#if 0
	FILEHANDLE hFile = oapiOpenFile (cfgfile, FILE_OUT, ROOT);

	oapiWriteItem_int   (hFile, (char*)"EnableDX12Wrapper", Enable9On12);
	oapiWriteItem_float (hFile, (char*)"FrameRate", FrameRate);
	oapiWriteItem_int   (hFile, (char*)"EnableLimiter", EnableLimiter);
	oapiWriteItem_int   (hFile, (char*)"CustomCamMode", CustomCamMode);
	oapiWriteItem_int   (hFile, (char*)"PlanetPreloadMode", PlanetPreloadMode);
	oapiWriteItem_int   (hFile, (char*)"PlanetTexLoadFreq", PlanetLoadFrequency);
	oapiWriteItem_int   (hFile, (char*)"Anisotrophy", Anisotrophy);
	oapiWriteItem_int   (hFile, (char*)"SceneAntialias", SceneAntialias);
	oapiWriteItem_int   (hFile, (char*)"SketchpadFont", SketchpadFont);
	oapiWriteItem_int   (hFile, (char*)"PreLoadBaseVisuals", PreLBaseVis);
	oapiWriteItem_int   (hFile, (char*)"EnableNormalMapping", UseNormalMap);
	oapiWriteItem_int   (hFile, (char*)"NearClipPlaneMode", NearClipPlane);
	oapiWriteItem_int   (hFile, (char*)"RwyLightAnimate", RwyLightAnimate);
	oapiWriteItem_float (hFile, (char*)"RwyLightAngle", RwyLightAngle);
	oapiWriteItem_float (hFile, (char*)"RwyBrightness", RwyBrightness);
	oapiWriteItem_float (hFile, (char*)"NightLightsAngle", SunAngle);
	oapiWriteItem_float (hFile, (char*)"BumpMapAmplitude", BumpAmp);
	oapiWriteItem_float (hFile, (char*)"PlanetGlow", PlanetGlow);
	oapiWriteItem_int   (hFile, (char*)"EnvMapSize", EnvMapSize);
	oapiWriteItem_int   (hFile, (char*)"EnvMapMode", EnvMapMode);
	oapiWriteItem_int   (hFile, (char*)"EnvMapFaces", EnvMapFaces);
	oapiWriteItem_int   (hFile, (char*)"ShadowMapMode", ShadowMapMode);
	oapiWriteItem_int   (hFile, (char*)"ShadowMapFilter", ShadowFilter);
	oapiWriteItem_int   (hFile, (char*)"ShadowMapSize", ShadowMapSize);
	oapiWriteItem_int   (hFile, (char*)"TerrainShadowing", TerrainShadowing);
	oapiWriteItem_int   (hFile, (char*)"ParticleLight", ParticleLight);
	oapiWriteItem_float (hFile, (char*)"ParticleShadow", ParticleShadow);
	oapiWriteItem_float (hFile, (char*)"ParticleTintLead", ParticleTintLead);
	oapiWriteItem_float (hFile, (char*)"ParticleTintSat", ParticleTintSat);
	oapiWriteItem_float (hFile, (char*)"ParticleTintBloom", ParticleTintBloom);
	oapiWriteItem_int   (hFile, (char*)"EnableGlass", EnableGlass);
	oapiWriteItem_int   (hFile, (char*)"EnableMeshDbg", EnableMeshDbg);
	oapiWriteItem_int   (hFile, (char*)"TileMipmaps", TileMipmaps);
	oapiWriteItem_int   (hFile, (char*)"TextureMips", TextureMips);
	oapiWriteItem_int   (hFile, (char*)"TileDebug", TileDebug);
	oapiWriteItem_float (hFile, (char*)"StereoSeparation", Separation);
	oapiWriteItem_float (hFile, (char*)"StereoConvergence", Convergence);
	oapiWriteItem_int   (hFile, (char*)"DebugLvl", DebugLvl);
	oapiWriteItem_float (hFile, (char*)"VCNearPlane", VCNearPlane);
	oapiWriteItem_int   (hFile, (char*)"LightConfiguration", LightConfig);
	oapiWriteItem_int   (hFile, (char*)"DisableDrvMgm", DisableDriverManagement);
	oapiWriteItem_int   (hFile, (char*)"NVPerfHUD", NVPerfHUD);
	oapiWriteItem_int   (hFile, (char*)"DebugLineFontSize", DebugFontSize);
	oapiWriteItem_int   (hFile, (char*)"DisableVisualHelperReadout", DisableVisualHelperReadout);
	oapiWriteItem_float (hFile, (char*)"LODBias", LODBias);
	oapiWriteItem_int   (hFile, (char*)"MeshRes", MeshRes);
	oapiWriteItem_int	(hFile, (char*)"MaxTiles", MaxTiles);
	oapiWriteItem_int   (hFile, (char*)"MicroMode", MicroMode);
	oapiWriteItem_int   (hFile, (char*)"MicroFilter", MicroFilter);
	oapiWriteItem_int   (hFile, (char*)"BlendMode", BlendMode);
	oapiWriteItem_int   (hFile, (char*)"MicroBias", MicroBias);
	oapiWriteItem_int	(hFile, (char*)"CloudMicro", CloudMicro);
	oapiWriteItem_int   (hFile, (char*)"PostProcess", PostProcess);
	oapiWriteItem_int   (hFile, (char*)"ShaderDebug", ShaderDebug);
	oapiWriteItem_int   (hFile, (char*)"PresentLocation", PresentLocation);
	oapiWriteItem_int   (hFile, (char*)"PlanetTileLoadFlags", PlanetTileLoadFlags);
	oapiWriteItem_int   (hFile, (char*)"LabelDisplayFlags", LabelDisplayFlags);
	oapiWriteItem_int   (hFile, (char*)"GDIOverlay", GDIOverlay);
	oapiWriteItem_int	(hFile, (char*)"gcGUIMode", gcGUIMode);
	oapiWriteItem_int   (hFile, (char*)"AbsoluteAnimations", bAbsAnims);
	oapiWriteItem_int   (hFile, (char*)"NormalmappedClouds", bCloudNormals);
	oapiWriteItem_int   (hFile, (char*)"TerrainFlats", bFlats);
	oapiWriteItem_int	(hFile, (char*)"SunGlare", bGlares);
	oapiWriteItem_int	(hFile, (char*)"LightsGlare", bLocalGlares);
	oapiWriteItem_int	(hFile, (char*)"Irradiance", bIrradiance);
	oapiWriteItem_int	(hFile, (char*)"AtmoQuality", bAtmoQuality);
	oapiWriteItem_int	(hFile, (char*)"DebugBreak", DebugBreak);
	oapiWriteItem_int	(hFile, (char*)"ShaderCacheUse", ShaderCacheUse);
	oapiWriteItem_int	(hFile, (char*)"NoPlanetAA", NoPlanetAA);

	oapiWriteItem_float (hFile, (char*)"OrbitalShadowMult", OrbitalShadowMult);
	oapiWriteItem_float (hFile, (char*)"GFXIntensity", GFXIntensity);
	oapiWriteItem_float (hFile, (char*)"GFXDistance", GFXDistance);
	oapiWriteItem_float (hFile, (char*)"GFXThreshold", GFXThreshold);
	oapiWriteItem_float (hFile, (char*)"GFXGamma", GFXGamma);
	oapiWriteItem_float (hFile, (char*)"GFXSunIntensity", GFXSunIntensity);
	oapiWriteItem_float (hFile, (char*)"GFXLocalMax", GFXLocalMax);
	oapiWriteItem_float (hFile, (char*)"GFXGlare", GFXGlare);

	oapiWriteItem_string (hFile,(char*) "SolCfg", SolCfg);
	oapiWriteItem_string (hFile,(char*) "DebugLineFont", DebugFont);

	oapiWriteItem_string(hFile, (char*)"EarthAtmoCfg", (char *)AtmoCfg["Earth"].c_str());

	oapiCloseFile (hFile, FILE_OUT);
#endif
}
