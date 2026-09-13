// ==============================================================
// OroBaseAnim.cpp - ORO patch (ag): ANIMATED BASE OBJECTS (2026-09-07)
// Part of the ORO-patched D3D9Client (github.com/dgatsoulis/orbiter-oro).
// Dual licensed under GPL v3 and LGPL v3.
// See OroBaseAnim.h for what this is and why.
// ==============================================================

#include "OroBaseAnim.h"
#include "Mesh.h"
#include "D3D9Surface.h"
#include "D3D9Util.h"
#include "OapiExtension.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <algorithm>

namespace {

	const float ORO_TRK_DS    = 25.0f;  // [m] profile sample spacing along the track
	const int   ORO_TRK_NMAX  = 512;    // sample cap (a 12 km track still gets 25 m)
	const float ORO_TRK_GRADE = 0.08f;  // the maximum grade the rail climbs or descends
	const float ORO_TRK_PYLON = 0.5f;   // [m] rail base above ground beyond which a pylon appears
	const float ORO_TRK_MAXWARP = 100.0f; // the trains run as if time warp were at most this (his cap)
	const float ORO_HR_GDIST  = 200.0f; // [m] the core's approximate girder spacing (Train2::Activate)
	const float ORO_HR_DECK   = 0.4f;   // [m] the deck's thickness (the core's was a zero-thickness sheet)
	const int   ORO_SOL_NMAX  = 3500;   // panels per plant (17 vertices each under the 65535 ceiling)
	const float ORO_SOL_COSTILT = 0.2588f; // cos 75 deg: the panels' maximum tilt (the core went vertical)
	const double ORO_SOL_UPDATE = 60.0; // [s] sim time between aims (the core's SolarPlant::Update cadence)
	const float ORO_SOL_GLINT = 0.999f; // the core's glint cone: camera within acos(0.999) = 2.6 deg of the normal

	// THE CORE'S OWN TABLES (Src/Orbiter/Baseobj.cpp), copied verbatim so a base authored
	// in 2010 looks exactly as its author saw it. The monorail cabin is centred on z = 0
	// and rides with its floor at y = 0.5, straddling the 1 m beam; the hangrail cabin
	// hangs with its roof at y = 0 and its floor at y = -3.
	const NTVERTEX cabin1[30] = {
		{ 2,  4.5, 0,       0,1,0,  0,0.75},
		{-2,  4.5, 0,       0,1,0,  0,0.5},
		{ 2,  4.5, 8,       0,1,0,  0.34375f,0.75f},
		{-2,  4.5, 8,       0,1,0,  0.34375f,0.5f},
		{ 3,  2,  10,       0,0,1,  0.516f,0.75f},
		{-3,  2,  10,       0,0,1,  0.516f,0.5f},
		{ 2.2f,0.5f, 9.5f,  0,-1,0, 0.625f,0.75f},
		{-2.2f,0.5f, 9.5f,  0,-1,0, 0.625f,0.5f},
		{ 2.2f,0.5f, 0.0f,  0,-1,0, 1,0.75f},
		{-2.2f,0.5f, 0.0f,  0,-1,0, 1,0.5f},
		{ 2.2f,0.5f,-9.5f,  0,-1,0, 0.625f,0.75f},
		{-2.2f,0.5f,-9.5f,  0,-1,0, 0.625f,0.5f},
		{ 3,  2, -10,       0,0,-1, 0.516f,0.75f},
		{-3,  2, -10,       0,0,-1, 0.516f,0.5f},
		{ 2,  4.5,-8,       0,1,0,  0.34375f,0.75f},
		{-2,  4.5,-8,       0,1,0,  0.34375f,0.5f},
		{ 2,  4.5, 0,       0,1,0,  0,0.75},
		{-2,  4.5, 0,       0,1,0,  0,0.5},
		{ 2,  4.5,-8,     0.866f,0.5,0,  0.1f,0.75f},
		{ 2,  4.5, 8,     0.866f,0.5,0,  0.9f,0.75f},
		{ 3,  2, -10,     1,0,0,        0,0.9f},
		{ 3,  2,  10,     1,0,0,        1,0.9f},
		{ 2.2f,0.5,-9.5,   0.866f,-0.5,0, 0.05f,1},
		{ 2.2f,0.5, 9.5,   0.866f,-0.5,0, 0.95f,1},
		{-2,  4.5, 8,    -0.866f,0.5,0,  0.1f,0.75f},
		{-2,  4.5,-8,    -0.866f,0.5,0,  0.9f,0.75f},
		{-3,  2,  10,    -1,0,0,        0,0.9f},
		{-3,  2, -10,    -1,0,0,        1,0.9f},
		{-2.2f,0.5, 9.5,  -0.866f,-0.5,0, 0.05f,1},
		{-2.2f,0.5,-9.5,  -0.866f,-0.5,0, 0.95f,1},
	};
	const NTVERTEX cabin2[30] = {
		{ 2, 0,0, 0,1,0,              0,0.75},
		{-2, 0,0, 0,1,0,              0,0.5},
		{ 2, 0,5, 0,0.707f,0.707f,    0.34375f,0.75f},
		{-2, 0,5, 0,0.707f,0.707f,    0.34375f,0.5f},
		{ 2,-2,6, 0,0,1,              0.516f,0.75f},
		{-2,-2,6, 0,0,1,              0.516f,0.5f},
		{ 2,-3,5.5, 0,-0.707f,0.707f, 0.625f,0.75f},
		{-2,-3,5.5, 0,-0.707f,0.707f, 0.625f,0.5f},
		{ 2,-3,0, 0,-1,0,             1,0.75},
		{-2,-3,0, 0,-1,0,             1,0.5},
		{ 2,-3,-5.5, 0,-0.707f,-0.707f, 0.625f, 0.75f},
		{-2,-3,-5.5, 0,-0.707f,-0.707f, 0.625f, 0.5f},
		{ 2,-2,-6, 0,0,-1,            0.516f,0.75f},
		{-2,-2,-6, 0,0,-1,            0.516f,0.5f},
		{ 2, 0,-5, 0,0.707f,-0.707f,  0.34375f,0.75f},
		{-2, 0,-5, 0,0.707f,-0.707f,  0.34375f,0.5f},
		{ 2, 0,0, 0,1,0,              0,0.75f},
		{-2, 0,0, 0,1,0,              0,0.5f},
		{ 2,-2,-6,    1,0,0,          0,0.9f},
		{ 2, 0,-5,    1,0,0,          0.1f,0.75f},
		{ 2,-3,-5.5,  1,0,0,          0.05f,1},
		{ 2, 0,5,     1,0,0,          0.9f,0.75f},
		{ 2,-3,5.5,   1,0,0,          0.95f,1},
		{ 2,-2,6,     1,0,0,          1,0.9f},
		{-2,-2, 6,   -1,0,0,          0,0.9f},
		{-2, 0, 5,   -1,0,0,          0.1f,0.75f},
		{-2,-3, 5.5, -1,0,0,          0.05f,1},
		{-2, 0,-5,   -1,0,0,          0.9f,0.75f},
		{-2,-3,-5.5, -1,0,0,          0.95f,1},
		{-2,-2,-6,   -1,0,0,          1,0.9f}
	};
	const WORD cabin_idx[78] = {           // the core's cabin1_idx, shared by both cabins
		0,1,2,3,2,1,2,3,4,5,4,3,4,5,6,7,6,5,6,7,8,9,8,7,8,9,10,11,10,9,
		10,11,12,13,12,11,12,13,14,15,14,13,14,15,16,17,16,15,
		18,19,20,21,20,19,20,21,22,23,22,21,24,25,26,27,26,25,26,27,28,29,28,27
	};
	// the monorail beam's cross-section (the core's mrail1: a 5 m wide, 1 m tall
	// trapezoid, three faces), plus a bottom the core never needed - its beam lay on the
	// ground; ours stands on pylons and is seen from below.
	struct XSec { float x, y, nx, ny, tv; };
	const XSec mrail1_xs[6] = {
		{-2.5f, 0.0f, -0.707f, 0.707f, 0.0f },
		{-1.5f, 1.0f,  0.0f,   1.0f,   0.15f},
		{ 1.5f, 1.0f,  0.0f,   1.0f,   0.35f},
		{ 2.5f, 0.0f,  0.707f, 0.707f, 0.5f },
		{-2.5f, 0.0f,  0.0f,  -1.0f,   0.0f },   // bottom, left
		{ 2.5f, 0.0f,  0.0f,  -1.0f,   0.5f },   // bottom, right
	};
	// the core's mrail1 (for finding its frozen export): x, y; odd entries sit at the far end
	const float mrail1_core[8][2] = { {-2.5f,0}, {-2.5f,0}, {-1.5f,1}, {-1.5f,1}, {1.5f,1}, {1.5f,1}, {2.5f,0}, {2.5f,0} };
	// the hangrail portal (the core's girder_template, one face): x across the track, the
	// height CLASS (0 = the feet on the ground, 1 = the deck, 2 = the deck + 4), and uv
	struct Portal { float x; int lvl; float tu, tv; };
	const Portal portal_xs[6] = {
		{-10.0f, 0, 0.5f, 0.25f}, {-7.0f, 2, 0.0f, 0.25f}, {-5.0f, 1, 0.0f, 0.5f},
		{  7.0f, 2, 0.5f, 0.25f}, { 5.0f, 1, 0.5f, 0.5f }, {10.0f, 0, 0.0f, 0.5f}
	};

	char* Trim(char* s)
	{
		while (*s && isspace((unsigned char)*s)) s++;
		size_t n = strlen(s);
		while (n && isspace((unsigned char)s[n-1])) s[--n] = 0;
		return s;
	}

	inline float Clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

	// A growing NTVERTEX/WORD mesh under construction. Tri picks the winding the core's
	// own tables use ((v1 - v0) x (v2 - v0) along the face normal), so the new geometry is
	// culled exactly as the cabins are.
	struct Build {
		std::vector<NTVERTEX> v;
		std::vector<WORD>     i;
		WORD Add(const D3DXVECTOR3& p, const D3DXVECTOR3& n, float tu, float tv)
		{
			NTVERTEX t; t.x = p.x; t.y = p.y; t.z = p.z; t.nx = n.x; t.ny = n.y; t.nz = n.z; t.tu = tu; t.tv = tv;
			v.push_back(t);
			return (WORD)(v.size() - 1);
		}
		void Tri(WORD a, WORD b, WORD c, const D3DXVECTOR3& want)
		{
			const NTVERTEX &A = v[a], &B = v[b], &C = v[c];
			D3DXVECTOR3 e1(B.x - A.x, B.y - A.y, B.z - A.z), e2(C.x - A.x, C.y - A.y, C.z - A.z), x;
			D3DXVec3Cross(&x, &e1, &e2);
			if (D3DXVec3Dot(&x, &want) >= 0.0f) { i.push_back(a); i.push_back(b); i.push_back(c); }
			else                                 { i.push_back(a); i.push_back(c); i.push_back(b); }
		}
		// quad a-b at one station, c-d at the next, normal 'want'
		void Quad(WORD a, WORD b, WORD c, WORD d, const D3DXVECTOR3& want)
		{
			Tri(a, b, d, want);
			Tri(a, d, c, want);
		}
	};

	D3D9Mesh* MakeMesh(const Build& B, SURFHANDLE hTex, float spec = 0.0f, float power = 10.0f)
	{
		if (B.v.empty() || B.i.empty() || B.v.size() > 65000) return NULL;
		MESHGROUPEX g; memset(&g, 0, sizeof(g));
		g.Vtx = (NTVERTEX*)&B.v[0];
		g.Idx = (WORD*)&B.i[0];
		g.nVtx = (DWORD)B.v.size();
		g.nIdx = (DWORD)B.i.size();
		g.MtrlIdx = 0;                      // the client's index is 0-based (Mesh.cpp)
		g.TexIdx = hTex ? 1 : 0;            // Tex[1] in the single-group constructor
		MATERIAL mat; memset(&mat, 0, sizeof(mat));
		mat.diffuse.r = mat.diffuse.g = mat.diffuse.b = mat.diffuse.a = 1.0f;
		mat.ambient.r = mat.ambient.g = mat.ambient.b = mat.ambient.a = 1.0f;
		mat.specular.r = mat.specular.g = mat.specular.b = spec;   // the solar panels' glass; 0 for the trains
		mat.specular.a = mat.emissive.a = 1.0f;
		mat.power = power;
		D3D9Mesh* m = new D3D9Mesh(&g, &mat, SURFACE(hTex));
		if (!m->IsOK()) { delete m; return NULL; }
		m->AttachNightTextures();           // the <tex>_n pair (Monorail_n / Hangrail_n ship)
		return m;
	}

	D3D9Mesh* MakeCabin(const NTVERTEX* tab, SURFHANDLE hTex)
	{
		Build C;
		for (int k = 0; k < 30; k++)
			C.Add(D3DXVECTOR3(tab[k].x, tab[k].y, tab[k].z), D3DXVECTOR3(tab[k].nx, tab[k].ny, tab[k].nz), tab[k].tu, tab[k].tv);
		for (int k = 0; k < 78; k++) C.i.push_back(cabin_idx[k]);
		return MakeMesh(C, hTex);
	}
}

// ===========================================================================================
OroBaseAnim::OroBaseAnim(oapi::D3D9Client *_gc, OBJHANDLE _hBase, OBJHANDLE _hPlanet, double _baseElev)
	: gc(_gc), hBase(_hBase), hPlanet(_hPlanet), baseElev(_baseElev), lastT(-1e9), nMono(0), nHang(0), nSolar(0), nPanels(0)
{
	R = oapiGetSize(hPlanet);
	oapiGetBaseEquPos(hBase, &blng, &blat);
	cosBlat = max(cos(blat), 1e-3);

	char bname[64]; oapiGetObjectName(hBase, bname, 64);
	char path[MAX_PATH];
	if (!FindCfg(path, sizeof(path))) return;

	std::vector<Spec> specs;
	ReadSpecs(path, specs);
	if (specs.empty()) return;

	for (size_t k = 0; k < specs.size(); k++) {
		const Spec& s = specs[k];
		char tname[96], full[MAX_PATH];
		sprintf_s(tname, sizeof(tname), "%s%s", s.tex, strchr(s.tex, '.') ? "" : ".dds");
		SURFHANDLE hTex = (s.tex[0] && gc->TexturePath(tname, full)) ? gc->clbkLoadTexture(tname, 0x8) : NULL;

		if (s.type == 3) {
			Plant P = Plant(); P.sp = s; P.mesh = NULL; P.updT = -1e9; P.nml = D3DXVECTOR3(0, 1, 0);
			BuildSolar(P, hTex);
			if (!P.mesh) continue;
			meshes.push_back(P.mesh);
			plants.push_back(P);
			nSolar++; nPanels += P.npanel;
			continue;
		}

		Train T = Train(); T.sp = s;
		const bool mono = (T.sp.type == 1);
		if (!BuildProfile(T, mono ? 0.0f : T.sp.height)) continue;
		if (mono) BuildMonorail(T, hTex); else BuildHangrail(T, hTex);
		if (!T.ncab) continue;
		for (int j = 0; j < T.ncab; j++) PlaceCabin(T, j);
		trains.push_back(T);
		if (mono) nMono++; else nHang++;
	}
	oapiWriteLogV("D3D9: ORO base '%s' - %d monorail(s), %d hangrail(s), %d solar plant(s) with %d panel(s) revived from %s",
	              bname, nMono, nHang, nSolar, nPanels, path);
}

OroBaseAnim::~OroBaseAnim()
{
	// the meshes belong to vBase's structure list now
}

// ===========================================================================================
// The base's own cfg. The core names a base by its file stem and scans the planet's
// base folders (Config\<planet>\Base by default, or the DIR entries of the planet cfg's
// BEGIN_SURFBASE block, each with optional PERIOD / CONTEXT limiters), so the lookup
// mirrors Planet::ScanBases. A single-base "name: lng lat" line resolves to
// Config\<name>.cfg, the last candidate.
bool OroBaseAnim::FindCfgFor(OBJHANDLE hBase, OBJHANDLE hPlanet, char *path, size_t cap, bool quiet)
{
	char bname[64], pname[64];
	oapiGetObjectName(hBase, bname, 64);
	oapiGetObjectName(hPlanet, pname, 64);

	char cfg[MAX_PATH];
	strcpy_s(cfg, sizeof(cfg), OapiExtension::GetConfigDir());
	if (!cfg[0]) strcpy_s(cfg, sizeof(cfg), "Config\\");
	size_t n = strlen(cfg);
	if (cfg[n-1] != '\\' && cfg[n-1] != '/') strcat_s(cfg, sizeof(cfg), "\\");

	// A folder holds the base if <name>.cfg is there, or - the common case, his Brighton
	// Beach flight: the object is NAMED inside its file ("Name = Brighton Beach" in
	// Brighton.cfg) - if any BASE-V2.0 file in it carries that Name line.
	auto tryDir = [&](const char* dir, char* out, size_t outCap) -> bool {
		FILE* f = NULL;
		sprintf_s(out, outCap, "%s%s\\%s.cfg", cfg, dir, bname);
		if (fopen_s(&f, out, "rt") == 0 && f) { fclose(f); return true; }
		char pat[MAX_PATH]; sprintf_s(pat, sizeof(pat), "%s%s\\*.cfg", cfg, dir);
		WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(pat, &fd);
		if (h == INVALID_HANDLE_VALUE) { out[0] = 0; return false; }
		bool found = false;
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
			char fp[MAX_PATH]; sprintf_s(fp, sizeof(fp), "%s%s\\%s", cfg, dir, fd.cFileName);
			if (fopen_s(&f, fp, "rt") != 0 || !f) continue;
			char line[256]; int n = 0; bool v2 = false;
			while (n < 40 && fgets(line, sizeof(line), f)) {
				char* p = Trim(line);
				if (!p[0]) continue;
				if (n++ == 0) { v2 = (_strnicmp(p, "BASE-V2.0", 9) == 0); if (!v2) break; continue; }
				if (_strnicmp(p, "Name", 4) == 0 && (p[4] == ' ' || p[4] == '\t' || p[4] == '=')) {
					char* e = strchr(p, '=');
					if (e && _stricmp(Trim(e + 1), bname) == 0) found = true;
					break;
				}
			}
			fclose(f);
			if (found) { strcpy_s(out, outCap, fp); break; }
		} while (FindNextFileA(h, &fd));
		FindClose(h);
		if (!found) out[0] = 0;
		return found;
	};

	char dir[MAX_PATH];
	sprintf_s(dir, sizeof(dir), "%s\\Base", pname);
	if (tryDir(dir, path, cap)) return true;

	FILE* f = NULL;
	char pcfg[MAX_PATH];
	sprintf_s(pcfg, sizeof(pcfg), "%s%s.cfg", cfg, pname);
	if (fopen_s(&f, pcfg, "rt") == 0 && f) {
		const double mjd = oapiGetSimMJD();
		char line[256]; bool in = false; char fallback[MAX_PATH] = "";
		while (fgets(line, sizeof(line), f)) {
			char* p = Trim(line);
			if (!in) { if (_strnicmp(p, "BEGIN_SURFBASE", 14) == 0) in = true; continue; }
			if (_strnicmp(p, "END_SURFBASE", 12) == 0) break;
			if (_strnicmp(p, "DIR", 3) != 0) continue;
			char* d = Trim(p + 3);
			bool contextLimited = false, periodOK = true;
			char* cp = strstr(d, "PERIOD");
			if (cp) {
				double a, b;
				if (sscanf_s(cp + 6, "%lf%lf", &a, &b) == 2 && (a > mjd || b < mjd)) periodOK = false;
			}
			char* cc = strstr(d, "CONTEXT");
			if (cc) contextLimited = true;                 // the scenario context is not readable from here
			char* cut = cp; if (cc && (!cut || cc < cut)) cut = cc;
			if (cut) { *cut = 0; d = Trim(d); }
			if (!periodOK) continue;
			char cand[MAX_PATH];
			if (tryDir(d, cand, sizeof(cand))) {
				if (!contextLimited) { strcpy_s(path, cap, cand); fclose(f); return true; }
				if (!fallback[0]) strcpy_s(fallback, sizeof(fallback), cand);
			}
		}
		fclose(f);
		if (fallback[0]) { strcpy_s(path, cap, fallback); return true; }
	}

	sprintf_s(path, cap, "%s%s.cfg", cfg, bname);
	if (fopen_s(&f, path, "rt") == 0 && f) { fclose(f); return true; }
	path[0] = 0;
	if (!quiet) oapiWriteLogV("D3D9: ORO base '%s' - its cfg was not found under %s (no animated objects looked for)", bname, cfg);
	return false;
}

// ===========================================================================================
// Does a BASE-V2.0 file carry a block of the given type (a line whose first token is it)?
bool OroBaseAnim::CfgHasBlock(const char *path, const char *block)
{
	FILE* f = NULL;
	if (fopen_s(&f, path, "rt") != 0 || !f) return false;
	char line[256], tok[64];
	bool first = true, found = false;
	while (!found && fgets(line, sizeof(line), f)) {
		char* p = Trim(line);
		if (!p[0]) continue;
		if (first) { first = false; if (_strnicmp(p, "BASE-V2.0", 9) != 0) break; continue; }
		if (sscanf_s(p, "%63s", tok, (unsigned)sizeof(tok)) == 1 && !_stricmp(tok, block)) found = true;
	}
	fclose(f);
	return found;
}

// ===========================================================================================
// See the header. Every base of every body: if its cfg has a SOLARPLANT block, ask the
// core for its structures - Base::ExportBaseStructures runs ScanObjectMeshes, which calls
// Activate() on each of the base's objects (once: objmsh_valid), so SolarPlant's
// destructor delete[]s allocations instead of stack garbage.
void OroBaseAnim::ArmCoreSolarPlants(oapi::D3D9Client *gc)
{
	int armed = 0, scanned = 0;
	const DWORD nb = oapiGetGbodyCount();
	for (DWORD b = 0; b < nb; b++) {
		OBJHANDLE hP = oapiGetGbodyByIndex(b);
		if (!hP) continue;
		const DWORD nBase = oapiGetBaseCount(hP);
		for (DWORD i = 0; i < nBase; i++) {
			OBJHANDLE hB = oapiGetBaseByIndex(hP, i);
			if (!hB) continue;
			char path[MAX_PATH];
			if (!FindCfgFor(hB, hP, path, sizeof(path), true)) continue;
			scanned++;
			if (!CfgHasBlock(path, "SOLARPLANT")) continue;
			MESHHANDLE *sbs = NULL, *sas = NULL; DWORD nsbs = 0, nsas = 0;
			gc->GetBaseStructures(hB, &sbs, &nsbs, &sas, &nsas);
			armed++;
		}
	}
	if (armed)
		oapiWriteLogV("D3D9: ORO base anim - %d solar-plant base(s) of %d activated before teardown (the core's SolarPlant frees pointers its constructor never set)", armed, scanned);
}

// ===========================================================================================
// The three block types, parsed exactly as the core's Train1::Read / Train2::Read /
// SolarPlant::Read do: one token per line, terminated by a line whose first token is END.
// Defaults are the core's constructors': ends (0,0,0)-(1000,0,0), 30 m/s, a 100 m slow
// zone, a hangrail 11 m high.
void OroBaseAnim::ReadSpecs(const char *path, std::vector<Spec> &out)
{
	FILE* f = NULL;
	if (fopen_s(&f, path, "rt") != 0 || !f) return;
	char line[256], tok[64];
	bool first = true;
	while (fgets(line, sizeof(line), f)) {
		char* p = Trim(line);
		if (!p[0]) continue;
		if (first) { first = false; if (_strnicmp(p, "BASE-V2.0", 9) != 0) break; continue; }
		if (sscanf_s(p, "%63s", tok, (unsigned)sizeof(tok)) != 1) continue;
		const bool t1 = !_stricmp(tok, "TRAIN1"), t2 = !_stricmp(tok, "TRAIN2"), sol = !_stricmp(tok, "SOLARPLANT");
		if (!t1 && !t2 && !sol) continue;

		Spec sp; memset(&sp, 0, sizeof(sp));
		sp.type = t1 ? 1 : (t2 ? 2 : 3);
		sp.e2[0] = 1000.0f;
		sp.maxspeed = 30.0f; sp.slowzone = 100.0f; sp.height = 11.0f; sp.tuscale = 1.0f;
		sp.scale = 1.0f; sp.sepx = sp.sepz = 40.0f; sp.nrow = sp.ncol = 2;   // the core's SolarPlant()
		while (fgets(line, sizeof(line), f)) {
			char* q = Trim(line);
			if (!q[0]) continue;
			if (sscanf_s(q, "%63s", tok, (unsigned)sizeof(tok)) != 1) continue;
			if (!_stricmp(tok, "END")) break;
			if      (!_stricmp(tok, "END1"))     sscanf_s(q + 4, "%f%f%f", &sp.e1[0], &sp.e1[1], &sp.e1[2]);
			else if (!_stricmp(tok, "END2"))     sscanf_s(q + 4, "%f%f%f", &sp.e2[0], &sp.e2[1], &sp.e2[2]);
			else if (!_stricmp(tok, "MAXSPEED")) sscanf_s(q + 8, "%f", &sp.maxspeed);
			else if (!_stricmp(tok, "SLOWZONE")) sscanf_s(q + 8, "%f", &sp.slowzone);
			else if (!_stricmp(tok, "HEIGHT"))   sscanf_s(q + 6, "%f", &sp.height);
			else if (!_stricmp(tok, "POS"))      sscanf_s(q + 3, "%f%f%f", &sp.pos[0], &sp.pos[1], &sp.pos[2]);
			else if (!_stricmp(tok, "SCALE"))    sscanf_s(q + 5, "%f", &sp.scale);
			else if (!_stricmp(tok, "SPACING"))  sscanf_s(q + 7, "%f%f", &sp.sepx, &sp.sepz);
			else if (!_stricmp(tok, "GRID"))     sscanf_s(q + 4, "%d%d", &sp.nrow, &sp.ncol);
			else if (!_stricmp(tok, "ROT"))    { sscanf_s(q + 3, "%f", &sp.rot); sp.rot *= (float)RAD; }
			else if (!_stricmp(tok, "TEX"))      sscanf_s(q + 3, "%63s%f", sp.tex, (unsigned)sizeof(sp.tex), &sp.tuscale);
		}
		out.push_back(sp);
	}
	fclose(f);
}

// ===========================================================================================
// The ground under a base-local (x, z), in base-local y: the planet's elevation service
// (the one the physics uses, flattening included) minus the base's own elevation, minus
// the curvature drop of the sphere below the base's tangent plane - 1.2 m over Brighton
// Beach's 1.3 km on the Moon, enough to bury a beam end. Base-local axes per the core's
// Base::Rel_EquPos: +z east, +x south.
double OroBaseAnim::GroundY(double x, double z) const
{
	const double lng = blng + z / (R * cosBlat);
	const double lat = blat - x / R;
	const double e = oapiSurfaceElevation(hPlanet, lng, lat);
	return e - baseElev - (x * x + z * z) / (2.0 * R);
}

// ===========================================================================================
// The track: the core's Train::Init numbers, then the terrain profile - ground sampled
// every ~25 m along the horizontal chord, the RAIL LINE = the slope-limited upper
// envelope of the ground plus 'lift' (0 for a monorail beam that lies on the ground, the
// deck HEIGHT for a hangrail) - never below it, never steeper than the grade - a 1-2-1
// smoothing over three samples so the cabin does not kink at the take-off points,
// clamped back so a crest cannot bury it.
bool OroBaseAnim::BuildProfile(Train &T, float lift)
{
	const Spec& s = T.sp;
	const float dx = s.e2[0] - s.e1[0], dy = s.e2[1] - s.e1[1], dz = s.e2[2] - s.e1[2];
	T.length = sqrtf(dx * dx + dy * dy + dz * dz);
	T.lenH   = sqrtf(dx * dx + dz * dz);
	if (T.lenH < 30.0f || T.length < 30.0f) return false;    // no room for the 10 m end margins
	T.dirx = dx / T.lenH; T.dirz = dz / T.lenH;
	const double ph = atan2((double)dx, (double)dz);
	T.cosph = (float)cos(ph); T.sinph = (float)sin(ph);
	T.minpos = 10.0f;
	T.maxpos = T.length - 10.0f;
	float slow = Clampf(s.slowzone, T.minpos + 1.0f, T.length * 0.5f);
	T.speedfac = (max(s.maxspeed, 1.0f) - 1.0f) / (slow - T.minpos);
	T.sp.slowzone = slow;

	int N = (int)ceil(T.lenH / ORO_TRK_DS) + 1;
	if (N < 2) N = 2; if (N > ORO_TRK_NMAX) N = ORO_TRK_NMAX;
	T.ds = T.lenH / (float)(N - 1);
	T.gy.assign(N, 0.0f); T.p.assign(N, 0.0f);
	for (int i = 0; i < N; i++) {
		const double u = (double)i / (double)(N - 1);
		const double x = s.e1[0] + dx * u, z = s.e1[2] + dz * u;
		T.gy[i] = (float)(GroundY(x, z) + (s.e1[1] + dy * u));
		T.p[i] = T.gy[i] + lift;
	}
	const float step = ORO_TRK_GRADE * T.ds;
	for (int i = 1; i < N; i++)       T.p[i] = max(T.p[i], T.p[i-1] - step);
	for (int i = N - 2; i >= 0; i--)  T.p[i] = max(T.p[i], T.p[i+1] - step);
	std::vector<float> q(T.p);
	for (int i = 1; i < N - 1; i++)   q[i] = 0.25f * (T.p[i-1] + 2.0f * T.p[i] + T.p[i+1]);
	for (int i = 0; i < N; i++)       T.p[i] = max(q[i], T.gy[i] + lift);
	return true;
}

// ===========================================================================================
// The monorail: the beam along the profile (the core's cross-section, mitred at every
// sample so it is one continuous surface), end caps, pylons wherever the beam leaves the
// ground, and the cabin as its own mesh at the origin - moved by a mesh transform every
// frame, never by vertex edits.
void OroBaseAnim::BuildMonorail(Train &T, SURFHANDLE hTex)
{
	const Spec& s = T.sp;
	const int N = (int)T.p.size();
	const D3DXVECTOR3 right(T.cosph, 0.0f, -T.sinph);          // the core's r11, r21, r31
	const D3DXVECTOR3 hfwd(T.dirx, 0.0f, T.dirz);

	Build B;
	std::vector<WORD> ring(N * 6);
	D3DXVECTOR3 fwd0, fwdN;
	for (int i = 0; i < N; i++) {
		const double u = (double)i / (double)(N - 1);
		const D3DXVECTOR3 base((float)(s.e1[0] + (s.e2[0] - s.e1[0]) * u), T.p[i], (float)(s.e1[2] + (s.e2[2] - s.e1[2]) * u));
		const float slope = (i == 0)     ? (T.p[1] - T.p[0]) / T.ds
		                  : (i == N - 1) ? (T.p[N-1] - T.p[N-2]) / T.ds
		                  :                (T.p[i+1] - T.p[i-1]) / (2.0f * T.ds);
		D3DXVECTOR3 fwd(T.dirx, slope, T.dirz); D3DXVec3Normalize(&fwd, &fwd);
		D3DXVECTOR3 up; D3DXVec3Cross(&up, &fwd, &right);      // the core's basis: up = forward x right
		if (i == 0) fwd0 = fwd;
		if (i == N - 1) fwdN = fwd;
		const float tu = s.tuscale * (float)u;
		for (int k = 0; k < 6; k++) {
			const XSec& c = mrail1_xs[k];
			const D3DXVECTOR3 p = base + right * c.x + up * c.y;
			D3DXVECTOR3 n = right * c.nx + up * c.ny; D3DXVec3Normalize(&n, &n);
			ring[i * 6 + k] = B.Add(p, n, tu, c.tv);
		}
	}
	const D3DXVECTOR3 upW(0, 1, 0), dnW(0, -1, 0);
	for (int i = 0; i < N - 1; i++) {
		const WORD* a = &ring[i * 6]; const WORD* b = &ring[(i + 1) * 6];
		B.Quad(a[0], a[1], b[0], b[1], upW - right);
		B.Quad(a[1], a[2], b[1], b[2], upW);
		B.Quad(a[2], a[3], b[2], b[3], upW + right);
		B.Quad(a[5], a[4], b[5], b[4], dnW);
	}
	// end caps: the trapezoid closed at both ends
	{
		const D3DXVECTOR3 n0 = -fwd0, n1 = fwdN;
		WORD c0[4], c1[4];
		for (int k = 0; k < 4; k++) {
			const NTVERTEX& v0 = B.v[ring[k]];
			const NTVERTEX& v1 = B.v[ring[(N - 1) * 6 + k]];
			c0[k] = B.Add(D3DXVECTOR3(v0.x, v0.y, v0.z), n0, 0.0f, mrail1_xs[k].tv);
			c1[k] = B.Add(D3DXVECTOR3(v1.x, v1.y, v1.z), n1, 0.0f, mrail1_xs[k].tv);
		}
		B.Tri(c0[0], c0[1], c0[2], n0); B.Tri(c0[0], c0[2], c0[3], n0);
		B.Tri(c1[0], c1[1], c1[2], n1); B.Tri(c1[0], c1[2], c1[3], n1);
	}
	// pylons: a 0.8 m square column from half a metre below the local ground up to the
	// beam base, wherever the beam has left the ground
	for (int i = 0; i < N; i++) {
		const float h = T.p[i] - T.gy[i];
		if (h <= ORO_TRK_PYLON) continue;
		const double u = (double)i / (double)(N - 1);
		const D3DXVECTOR3 c((float)(s.e1[0] + (s.e2[0] - s.e1[0]) * u), 0.0f, (float)(s.e1[2] + (s.e2[2] - s.e1[2]) * u));
		const float y0 = T.gy[i] - 0.5f, y1 = T.p[i] + 0.05f;
		const D3DXVECTOR3 ex = right * 0.4f, ez = hfwd * 0.4f;
		const D3DXVECTOR3 faceN[4] = { right, -right, hfwd, -hfwd };
		const D3DXVECTOR3 faceA[4] = { ex - ez, -ex + ez, ez + ex, -ez - ex };
		const D3DXVECTOR3 faceB[4] = { ex + ez, -ex - ez, ez - ex, -ez + ex };
		for (int f = 0; f < 4; f++) {
			const D3DXVECTOR3 pa = c + faceA[f], pb = c + faceB[f];
			WORD a0 = B.Add(D3DXVECTOR3(pa.x, y0, pa.z), faceN[f], 0.0f,  0.5f);
			WORD b0 = B.Add(D3DXVECTOR3(pb.x, y0, pb.z), faceN[f], 0.05f, 0.5f);
			WORD a1 = B.Add(D3DXVECTOR3(pa.x, y1, pa.z), faceN[f], 0.0f,  0.0f);
			WORD b1 = B.Add(D3DXVECTOR3(pb.x, y1, pb.z), faceN[f], 0.05f, 0.0f);
			B.Quad(a1, b1, a0, b0, faceN[f]);
		}
	}
	D3D9Mesh* rail = MakeMesh(B, hTex);
	if (rail) meshes.push_back(rail);

	T.ncab = 0; T.cabY = 0.0f; T.latOfs[0] = 0.0f;
	T.cab[0] = MakeCabin(cabin1, hTex);
	if (T.cab[0]) { meshes.push_back(T.cab[0]); T.ncab = 1; T.cpos[0] = T.minpos; T.cvel[0] = 1.0f; }   // the core's Activate()
	else if (rail) { meshes.pop_back(); delete rail; }
}

// ===========================================================================================
// The hangrail: the core's gantry PORTALS every ~200 m (Train2::Activate: a flat
// trapezoidal arch, feet 20 m apart on the ground, a 4 m lintel above the deck - here
// each portal's feet sit on the LOCAL ground and its lintel on the local deck height, so
// the legs are cut to the terrain), the 10 m wide DECK following the profile at HEIGHT
// above the ground (the core's zero-thickness sheet, given 0.4 m so it reads from below),
// and TWO cabins hanging under it one lane apart, starting at opposite ends.
void OroBaseAnim::BuildHangrail(Train &T, SURFHANDLE hTex)
{
	const Spec& s = T.sp;
	const int N = (int)T.p.size();
	const D3DXVECTOR3 right(T.cosph, 0.0f, -T.sinph);
	const D3DXVECTOR3 hfwd(T.dirx, 0.0f, T.dirz);
	const D3DXVECTOR3 upW(0, 1, 0), dnW(0, -1, 0);
	auto lerpAt = [&](const std::vector<float>& a, float st) -> float {
		const float fi = st / T.ds; int i = (int)floorf(fi); if (i > N - 2) i = N - 2; if (i < 0) i = 0;
		return a[i] + (a[i+1] - a[i]) * (fi - (float)i);
	};
	auto posAt = [&](float st, float y) -> D3DXVECTOR3 { return D3DXVECTOR3(s.e1[0] + T.dirx * st, y, s.e1[2] + T.dirz * st); };

	Build B;
	// the portals
	const int   ng  = (int)(T.length / ORO_HR_GDIST) + 1;
	const float seg = T.lenH / (float)ng;
	for (int k = 0; k <= ng; k++) {
		const float st = seg * (float)k;
		const float g = lerpAt(T.gy, st) - 0.5f, d = lerpAt(T.p, st);
		const float lvlY[3] = { g, d, d + 4.0f };
		for (int f = 0; f < 2; f++) {
			const D3DXVECTOR3 n = f ? hfwd : -hfwd;
			WORD v[6];
			for (int q = 0; q < 6; q++)
				v[q] = B.Add(posAt(st, lvlY[portal_xs[q].lvl]) + right * portal_xs[q].x, n, portal_xs[q].tu, portal_xs[q].tv);
			for (int q = 0; q < 4; q++) B.Tri(v[q], v[q+1], v[q+2], n);   // the core's 6-vertex strip
		}
	}
	// the deck along the profile: top, bottom and the two edges
	std::vector<WORD> ring(N * 8);
	for (int i = 0; i < N; i++) {
		const float st = T.ds * (float)i;
		const D3DXVECTOR3 base = posAt(st, T.p[i]);
		const float slope = (i == 0)     ? (T.p[1] - T.p[0]) / T.ds
		                  : (i == N - 1) ? (T.p[N-1] - T.p[N-2]) / T.ds
		                  :                (T.p[i+1] - T.p[i-1]) / (2.0f * T.ds);
		D3DXVECTOR3 fwd(T.dirx, slope, T.dirz); D3DXVec3Normalize(&fwd, &fwd);
		D3DXVECTOR3 up; D3DXVec3Cross(&up, &fwd, &right);
		const D3DXVECTOR3 L = base - right * 5.0f, Rr = base + right * 5.0f, dn = -up * ORO_HR_DECK;
		const float tu = 6.0f * st / seg;                          // the core's 6 repeats per span
		ring[i*8+0] = B.Add(L,       up,     tu, 0.0f);
		ring[i*8+1] = B.Add(Rr,      up,     tu, 0.25f);
		ring[i*8+2] = B.Add(L + dn,  -up,    tu, 0.0f);
		ring[i*8+3] = B.Add(Rr + dn, -up,    tu, 0.25f);
		ring[i*8+4] = B.Add(L,       -right, tu, 0.25f);
		ring[i*8+5] = B.Add(L + dn,  -right, tu, 0.35f);
		ring[i*8+6] = B.Add(Rr,      right,  tu, 0.25f);
		ring[i*8+7] = B.Add(Rr + dn, right,  tu, 0.35f);
	}
	for (int i = 0; i < N - 1; i++) {
		const WORD* a = &ring[i * 8]; const WORD* b = &ring[(i + 1) * 8];
		B.Quad(a[0], a[1], b[0], b[1], upW);
		B.Quad(a[3], a[2], b[3], b[2], dnW);
		B.Quad(a[4], a[5], b[4], b[5], -right);
		B.Quad(a[6], a[7], b[6], b[7], right);
	}
	D3D9Mesh* rail = MakeMesh(B, hTex);
	if (rail) meshes.push_back(rail);

	// the two cabins: the core's lanes (+2.5 / -2.5 across), roofs 1 m under the deck,
	// one starting at each end and running toward the other
	T.ncab = 0; T.cabY = -1.0f;
	for (int j = 0; j < 2; j++) {
		D3D9Mesh* c = MakeCabin(cabin2, hTex);
		if (!c) break;
		meshes.push_back(c);
		T.cab[j] = c;
		T.latOfs[j] = j ? -2.5f : 2.5f;
		T.cpos[j] = j ? T.maxpos : T.minpos;
		T.cvel[j] = j ? -1.0f : 1.0f;
		T.ncab = j + 1;
	}
	if (T.ncab < 2) {                                         // all or nothing
		for (int j = 0; j < T.ncab; j++) { meshes.pop_back(); delete T.cab[j]; T.cab[j] = NULL; }
		if (rail) { meshes.erase(std::find(meshes.begin(), meshes.end(), rail)); delete rail; }
		T.ncab = 0;
	}
}

// ===========================================================================================
// The core's Train::MoveCabin, verbatim: 1 m/s at the ends, a linear ramp across the
// slow zone to max speed, reverse 10 m short of each end.
void OroBaseAnim::MoveCabin(Train &T, int j, float dt)
{
	const float slow = T.sp.slowzone;
	float &cpos = T.cpos[j], &cvel = T.cvel[j];
	cpos += cvel * dt;
	if (cpos < slow) {
		if (cpos < T.minpos) { cpos = T.minpos; cvel = 1.0f; }
		else cvel = ((cpos - T.minpos) * T.speedfac + 1.0f) * (cvel > 0.0f ? 1.0f : -1.0f);
	} else if (cpos > T.length - slow) {
		if (cpos > T.maxpos) { cpos = T.maxpos; cvel = -1.0f; }
		else cvel = ((T.maxpos - cpos) * T.speedfac + 1.0f) * (cvel > 0.0f ? 1.0f : -1.0f);
	}
}

// ===========================================================================================
// A cabin on the profile: position along the chord plus its lane offset, height from
// the rail line, pitch from the local grade, yaw from the chord. The matrix rows are the
// core's own basis (right, up, forward) in D3DX's row-vector convention.
void OroBaseAnim::PlaceCabin(Train &T, int j)
{
	if (j >= T.ncab || !T.cab[j]) return;
	const Spec& s = T.sp;
	const int N = (int)T.p.size();
	const float u = Clampf(T.cpos[j] / T.length, 0.0f, 1.0f);
	const float fi = u * (float)(N - 1);
	int i = (int)floorf(fi); if (i > N - 2) i = N - 2; if (i < 0) i = 0;
	const float t = fi - (float)i;
	const float y = T.p[i] + (T.p[i+1] - T.p[i]) * t + T.cabY;
	const float slope = (T.p[i+1] - T.p[i]) / T.ds;
	const float costh = 1.0f / sqrtf(1.0f + slope * slope), sinth = slope * costh;
	const float x = s.e1[0] + (s.e2[0] - s.e1[0]) * u + T.cosph * T.latOfs[j];
	const float z = s.e1[2] + (s.e2[2] - s.e1[2]) * u - T.sinph * T.latOfs[j];

	D3DXMATRIX m; D3DXMatrixIdentity(&m);
	m._11 = T.cosph;          m._12 = 0.0f;   m._13 = -T.sinph;          // right
	m._21 = -T.sinph * sinth; m._22 = costh;  m._23 = -T.cosph * sinth;  // up
	m._31 = T.sinph * costh;  m._32 = sinth;  m._33 = T.cosph * costh;   // forward
	m._41 = x;                m._42 = y;      m._43 = z;
	T.cab[j]->SetTransform(-1, &m);
}

// ===========================================================================================
// THE SOLAR PLANT - the core's SolarPlant::Activate geometry on the terrain. A grid of
// nrow x ncol panel pivots at SPACING, rotated by ROT about POS; each pivot sits
// 10 x SCALE above the ground UNDER IT (plus the authored POS y), so the plant follows a
// hillside one post at a time. One mesh, one group: the panels first (8 vertices each -
// a front and a back face, 16 x 8 m at SCALE, built flat and aimed by AimPanels), then
// the stands (the core's three-sided pyramid, 5 x SCALE across, its feet cut to the
// ground under each foot, one normal per face). The core's uv layout is kept so a 2010
// texture reads as authored: the panel's front is the left quarter's top half, its back
// the bottom half, the glint variant one quarter to the right, the stand in between.
void OroBaseAnim::BuildSolar(Plant &P, SURFHANDLE hTex)
{
	const Spec& s = P.sp;
	const int nrow = max(1, min(s.nrow, 512)), ncol = max(1, min(s.ncol, 512));
	P.npanel = min(nrow * ncol, ORO_SOL_NMAX);
	const float sc = max(s.scale, 0.05f);
	const float cr = cosf(s.rot), sr = sinf(s.rot);
	P.dx = 8.0f * sc; P.dz = 4.0f * sc;
	P.ppos.resize(P.npanel);
	P.flash.assign(P.npanel, 0);

	int idx = 0;
	for (int i = 0; i < nrow && idx < P.npanel; i++) {
		const float x = s.sepx * (i - 0.5f * (nrow - 1));
		for (int j = 0; j < ncol && idx < P.npanel; j++, idx++) {
			const float z = s.sepz * (j - 0.5f * (ncol - 1));
			const float px = s.pos[0] + x * cr - z * sr;
			const float pz = s.pos[2] + x * sr + z * cr;
			P.ppos[idx] = D3DXVECTOR3(px, (float)GroundY(px, pz) + s.pos[1] + 10.0f * sc, pz);
		}
	}

	Build B;
	const D3DXVECTOR3 up(0, 1, 0), dn(0, -1, 0);
	for (int i = 0; i < P.npanel; i++) {
		// flat, the core's corner order (-a-b, -a+b, +a+b, +a-b); AimPanels rotates the
		// same four corners, and a rotation keeps the winding chosen here
		const D3DXVECTOR3& c = P.ppos[i];
		const D3DXVECTOR3 q[4] = { D3DXVECTOR3(c.x - P.dx, c.y, c.z - P.dz), D3DXVECTOR3(c.x - P.dx, c.y, c.z + P.dz),
		                           D3DXVECTOR3(c.x + P.dx, c.y, c.z + P.dz), D3DXVECTOR3(c.x + P.dx, c.y, c.z - P.dz) };
		WORD f[4], b[4];
		f[0] = B.Add(q[0], up, 0.0f,  0.0f); f[1] = B.Add(q[1], up, 0.25f, 0.0f);
		f[2] = B.Add(q[2], up, 0.25f, 0.5f); f[3] = B.Add(q[3], up, 0.0f,  0.5f);
		b[0] = B.Add(q[0], dn, 0.0f,  0.5f); b[1] = B.Add(q[1], dn, 0.25f, 0.5f);
		b[2] = B.Add(q[2], dn, 0.25f, 1.0f); b[3] = B.Add(q[3], dn, 0.0f,  1.0f);
		B.Tri(f[0], f[1], f[2], up); B.Tri(f[2], f[3], f[0], up);
		B.Tri(b[2], b[1], b[0], dn); B.Tri(b[0], b[3], b[2], dn);
	}
	const float v1 = 2.89f * sc, v2 = 2.5f * sc, v3 = 1.44f * sc;
	for (int i = 0; i < P.npanel; i++) {
		const D3DXVECTOR3& c = P.ppos[i];
		auto foot = [&](float fx, float fz) -> D3DXVECTOR3 {
			const float wx = c.x + fx * cr - fz * sr, wz = c.z + fx * sr + fz * cr;
			return D3DXVECTOR3(wx, (float)GroundY(wx, wz) - 0.3f, wz);
		};
		const D3DXVECTOR3 apex(c.x, c.y - 0.1f * sc, c.z);
		const D3DXVECTOR3 f1 = foot(v2, -v3), f2 = foot(-v2, -v3), f3 = foot(0.0f, v1);
		auto face = [&](const D3DXVECTOR3& fa, const D3DXVECTOR3& fb, float tua, float tub) {
			D3DXVECTOR3 e1 = fa - apex, e2 = fb - apex, n;
			D3DXVec3Cross(&n, &e1, &e2); D3DXVec3Normalize(&n, &n);
			const D3DXVECTOR3 out((fa.x + fb.x) * 0.5f - c.x, 0.0f, (fa.z + fb.z) * 0.5f - c.z);   // away from the post
			if (D3DXVec3Dot(&n, &out) < 0.0f) n = -n;
			const WORD ia = B.Add(apex, n, 0.375f, 0.5f), ib = B.Add(fa, n, tua, 1.0f), ic = B.Add(fb, n, tub, 1.0f);
			B.Tri(ia, ib, ic, n);
		};
		face(f1, f2, 0.25f, 0.5f);
		face(f2, f3, 0.5f,  0.375f);
		face(f3, f1, 0.375f, 0.25f);
	}
	P.mesh = MakeMesh(B, hTex, 0.35f, 30.0f);   // glass: a real highlight beside the 2010 glint column
}

// ===========================================================================================
// The core's SolarPlant::Update: the panel normal is the sun direction, the long axis
// tilts toward it (tht = acos n.y), the short axis stays horizontal (phi = atan2(n.z,
// n.x)); the two half-axes are the rotation's first and third columns. Ours caps the
// tilt at 75 deg (the core clamped n.y at 0 and stood the panels vertical), keeping the
// sun's azimuth so they wait for it at the horizon. Pushed through EditGroup with an
// index list: only the 8 x npanel panel vertices move, the stands never do.
void OroBaseAnim::AimPanels(Plant &P, const D3DXVECTOR3 &sun)
{
	D3DXVECTOR3 n = sun;
	if (D3DXVec3Length(&n) < 1e-6f) n = D3DXVECTOR3(0, 1, 0);
	D3DXVec3Normalize(&n, &n);
	if (n.y < ORO_SOL_COSTILT) {
		const float hl = sqrtf(n.x * n.x + n.z * n.z);
		const float hs = sqrtf(1.0f - ORO_SOL_COSTILT * ORO_SOL_COSTILT);
		n = (hl < 1e-6f) ? D3DXVECTOR3(0, 1, 0) : D3DXVECTOR3(n.x / hl * hs, ORO_SOL_COSTILT, n.z / hl * hs);
	}
	P.nml = n;
	const double tht = acos((double)Clampf(n.y, -1.0f, 1.0f)), phi = atan2((double)n.z, (double)n.x);
	const float ct = (float)cos(tht), st = (float)sin(tht), cp = (float)cos(phi), sp = (float)sin(phi);
	const D3DXVECTOR3 a(cp * ct * P.dx, -st * P.dx, sp * ct * P.dx);   // the tilting half-axis
	const D3DXVECTOR3 b(-sp * P.dz, 0.0f, cp * P.dz);                  // the horizontal half-axis

	std::vector<NTVERTEX> V(P.npanel * 8);
	std::vector<WORD>     I(P.npanel * 8);
	for (int i = 0; i < P.npanel; i++) {
		const D3DXVECTOR3& c = P.ppos[i];
		const D3DXVECTOR3 q[4] = { c - a - b, c - a + b, c + a + b, c + a - b };
		for (int k = 0; k < 8; k++) {
			NTVERTEX& v = V[i * 8 + k]; memset(&v, 0, sizeof(v));
			const D3DXVECTOR3& p = q[k & 3];
			const float sgn = (k < 4) ? 1.0f : -1.0f;
			v.x = p.x; v.y = p.y; v.z = p.z;
			v.nx = sgn * n.x; v.ny = sgn * n.y; v.nz = sgn * n.z;
			I[i * 8 + k] = (WORD)(i * 8 + k);
		}
	}
	GROUPEDITSPEC ges; memset(&ges, 0, sizeof(ges));
	ges.flags = GRPEDIT_VTXCRD | GRPEDIT_VTXNML; ges.Vtx = &V[0]; ges.nVtx = (DWORD)V.size(); ges.vIdx = &I[0];
	P.mesh->EditGroup(0, &ges);
}

// ===========================================================================================
// The core's glint (SolarPlant::Render): a panel whose normal points within 2.6 deg of the
// camera shows the texture's bright column - the sun it is aimed at reflects straight
// back along that normal. State per panel, so the vertex buffer is touched only when a
// panel's glint changes; only with the sun up (a night-lit column in the dark would be
// a lie, and the core never ran at night under a client anyway).
void OroBaseAnim::GlintPanels(Plant &P, const D3DXVECTOR3 &cam, bool sunUp)
{
	std::vector<NTVERTEX> V; std::vector<WORD> I;
	for (int i = 0; i < P.npanel; i++) {
		D3DXVECTOR3 d = cam - P.ppos[i];
		D3DXVec3Normalize(&d, &d);
		const bool on = sunUp && D3DXVec3Dot(&d, &P.nml) > ORO_SOL_GLINT;
		if (on == (P.flash[i] != 0)) continue;
		P.flash[i] = on ? 1 : 0;
		const float ofs = on ? 0.25f : 0.0f;
		for (int k = 0; k < 4; k++) {
			NTVERTEX v; memset(&v, 0, sizeof(v));
			v.tu = ((k == 1 || k == 2) ? 0.25f : 0.0f) + ofs;
			V.push_back(v); I.push_back((WORD)(i * 8 + k));
		}
	}
	if (V.empty()) return;
	GROUPEDITSPEC ges; memset(&ges, 0, sizeof(ges));
	ges.flags = GRPEDIT_VTXTEXU; ges.Vtx = &V[0]; ges.nVtx = (DWORD)V.size(); ges.vIdx = &I[0];
	P.mesh->EditGroup(0, &ges);
}

// ===========================================================================================
void OroBaseAnim::Update(double simt, const VECTOR3 &sunLocal, const VECTOR3 &camLocal)
{
	// Sim time, CAPPED at 100x real time (his call, 2026-09-07: "more than that they
	// become a blur... even if the user has time-warp x100000, the trains should react as
	// if it was x100"). The cap rides the REAL frame time, so paused is still 0, a
	// scenario skip cannot teleport a cabin, and a long step is integrated in 0.1 s
	// sub-steps so the end ramps and reversals are never skipped over.
	float dt = (lastT < -1e8) ? 0.0f : (float)(simt - lastT);
	lastT = simt;
	const float cap = (float)(oapiGetSysStep() * ORO_TRK_MAXWARP);
	dt = Clampf(dt, 0.0f, max(cap, 0.0f));
	for (size_t k = 0; k < trains.size(); k++)
		for (int j = 0; j < trains[k].ncab; j++) {
			float left = dt;
			while (left > 0.0f) {
				const float h = min(left, 0.1f);
				MoveCabin(trains[k], j, h);
				left -= h;
			}
			PlaceCabin(trains[k], j);
		}

	// The panels follow the SUN, not a clock, so no warp cap: they are re-aimed every
	// 60 s of sim time (the core's cadence - a quarter degree of sun on Earth, invisible)
	// and at any warp that means every frame at most. The glint is checked every frame.
	if (!plants.empty()) {
		const D3DXVECTOR3 sun((float)sunLocal.x, (float)sunLocal.y, (float)sunLocal.z);
		const D3DXVECTOR3 cam((float)camLocal.x, (float)camLocal.y, (float)camLocal.z);
		const bool sunUp = sunLocal.y > 0.0;
		for (size_t k = 0; k < plants.size(); k++) {
			Plant& P = plants[k];
			if (simt >= P.updT) { AimPanels(P, sun); P.updT = simt + ORO_SOL_UPDATE; }
			GlintPanels(P, cam, sunUp);
		}
	}
}

// ===========================================================================================
// The core exported every cabin (30 vertices, contiguous, over-shadow) and the monorail's
// beam (8, under-shadow) into the base's merged generic meshes at the positions its own
// Activate() computed - from the authored ends LIFTED to the terrain by Base::Setup.
// Recompute those positions with the same arithmetic from the authored ends, find the
// runs by x/z (the lift is a y offset we do not need to reproduce), and collapse them to
// one point far underground. A run that is not found is logged and left alone: the
// frozen 2010 cabin then draws beside ours, which is exactly today's state and nothing
// worse.
void OroBaseAnim::HideCoreDuplicates(D3D9Mesh **bs, DWORD nbs, D3D9Mesh **as, DWORD nas)
{
	for (size_t k = 0; k < trains.size(); k++) {
		const Train& T = trains[k];
		const Spec& s = T.sp;
		const bool mono = (s.type == 1);
		const double sinth = (s.e2[1] - s.e1[1]) / T.length, costh = cos(asin(sinth));
		const double ph = atan2((double)(s.e2[0] - s.e1[0]), (double)(s.e2[2] - s.e1[2]));
		const double cosph = cos(ph), sinph = sin(ph);
		const double r11 = cosph, r12 = -sinph * sinth, r13 = sinph * costh;
		const double r21 = 0.0,   r22 = costh,          r23 = sinth;
		const double r31 = -sinph, r32 = -cosph * sinth, r33 = cosph * costh;
		auto place = [&](double vx, double vy, double vz) -> D3DXVECTOR3 {
			return D3DXVECTOR3((float)(vx * r11 + vy * r12 + vz * r13 + s.e1[0]),
			                   (float)(vx * r21 + vy * r22 + vz * r23 + s.e1[1]),
			                   (float)(vx * r31 + vy * r32 + vz * r33 + s.e1[2]));
		};

		struct Run {
			D3D9Mesh** list; DWORD n; D3DXVECTOR3 w[30]; int cnt; const char* what; bool done;
			float bestWorst; D3DXVECTOR3 bestD0; int bestMesh, bestGrp, bestVtx;   // the closest miss, for the log
			void Reset() { done = false; bestWorst = 1e9f; bestD0 = D3DXVECTOR3(0, 0, 0); bestMesh = bestGrp = bestVtx = -1; }
		};
		Run runs[3]; int nrun = 0;
		if (mono) {
			Run& c = runs[nrun++]; c.list = as; c.n = nas; c.cnt = 30; c.what = "cabin"; c.Reset();
			for (int i = 0; i < 30; i++) c.w[i] = place(cabin1[i].x, cabin1[i].y, cabin1[i].z + T.minpos);
			Run& b = runs[nrun++]; b.list = bs; b.n = nbs; b.cnt = 8; b.what = "beam"; b.Reset();
			for (int i = 0; i < 8; i++) b.w[i] = place(mrail1_core[i][0], mrail1_core[i][1], (i % 2) ? T.length : 0.0);
		} else {
			for (int j = 0; j < 2; j++) {
				Run& c = runs[nrun++]; c.list = as; c.n = nas; c.cnt = 30; c.what = j ? "second cabin" : "first cabin"; c.Reset();
				const double ox = j ? -2.5 : 2.5, oy = s.height - 1.0, oz = j ? T.maxpos : T.minpos;
				for (int i = 0; i < 30; i++) c.w[i] = place(cabin2[i].x + ox, cabin2[i].y + oy, cabin2[i].z + oz);
			}
		}

		for (int r = 0; r < nrun; r++) {
			Run& run = runs[r];
			for (DWORD m = 0; m < run.n && !run.done; m++) {
				D3D9Mesh* M = run.list[m];
				if (!M || !M->IsOK()) continue;
				for (DWORD g = 0; g < M->GetGroupCount() && !run.done; g++) {
					const DWORD nv = M->GetVertexCount((int)g);
					if (nv < (DWORD)run.cnt || nv > 200000) continue;
					std::vector<NTVERTEX> V(nv);
					GROUPREQUESTSPEC grs; memset(&grs, 0, sizeof(grs));
					grs.Vtx = &V[0]; grs.nVtx = nv;
					if (M->GetGroup(g, &grs) != 0) continue;
					for (DWORD j = 0; j + run.cnt <= nv; j++) {
						// Base::Setup lifts each track END to the terrain before the export, and
						// unequal lifts TILT the chord: SetCabin rotates every cabin about END 1
						// by asin((end2.y - end1.y) / length) with the LIFTED ends. His Brighton
						// flight showed what that does to an x/z match: the far cabin collapsed
						// with a 16.85 m lift, i.e. a 0.77 deg tilt over the 1260 m chord, and a
						// cabin hanging 11 m under the girder is carried ~0.15 m along the track
						// by that tilt (11 m x sin 0.77 deg). At the far end the chord's own
						// foreshortening (1250 m x (1 - cos), ~0.11 m the other way) nearly
						// cancelled it and the old 0.1 m test passed; at the near end nothing
						// cancels and the first cabin missed. So the test is on the run's SHAPE:
						// the first vertex may sit metres from the untilted reconstruction (5 m
						// across, 100 m up), but every other vertex must sit at its template
						// offset from it, within half a metre plus 2% of the span (a 0.77 deg
						// tilt moves a 12 m cabin's own offsets by 0.16 m). A cabin's y offsets
						// are tested too (it is lifted as one piece); the beam's are not - its
						// two ends are lifted by different amounts. The worst candidate error is
						// kept for the log line, so a miss names its residual instead of a bare
						// "not found" (the instrument-first rule).
						const NTVERTEX& v0 = V[j];
						const float d0x = v0.x - run.w[0].x, d0y = v0.y - run.w[0].y, d0z = v0.z - run.w[0].z;
						if (fabsf(d0x) > 5.0f || fabsf(d0z) > 5.0f || fabsf(d0y) > 100.0f) continue;
						float worst = 0.0f;    // the run's largest deviation in units of its tolerance
						for (int q = 1; q < run.cnt; q++) {
							const NTVERTEX& v = V[j + q];
							const float wx = run.w[q].x - run.w[0].x, wy = run.w[q].y - run.w[0].y, wz = run.w[q].z - run.w[0].z;
							const float tol = 0.5f + 0.02f * sqrtf(wx * wx + wy * wy + wz * wz);
							float dev = max(fabsf((v.x - v0.x) - wx), fabsf((v.z - v0.z) - wz));
							if (run.cnt == 30) dev = max(dev, fabsf((v.y - v0.y) - wy));
							if (dev / tol > worst) worst = dev / tol;
							if (worst > 50.0f) break;   // hopeless - the usual case for an unrelated vertex
						}
						if (worst < run.bestWorst) {
							run.bestWorst = worst; run.bestD0 = D3DXVECTOR3(d0x, d0y, d0z);
							run.bestMesh = (int)m; run.bestGrp = (int)g; run.bestVtx = (int)j;
						}
						if (worst >= 1.0f) continue;
						const float dyMin = d0y;
						std::vector<NTVERTEX> sink(run.cnt);
						std::vector<WORD>     idx(run.cnt);
						for (int q = 0; q < run.cnt; q++) {
							memset(&sink[q], 0, sizeof(NTVERTEX));
							sink[q].y = -100.0f;
							idx[q] = (WORD)(j + q);
						}
						GROUPEDITSPEC ges; memset(&ges, 0, sizeof(ges));
						ges.flags = GRPEDIT_VTXCRD; ges.Vtx = &sink[0]; ges.nVtx = (DWORD)run.cnt; ges.vIdx = &idx[0];
						M->EditGroup(g, &ges);
						oapiWriteLogV("D3D9: ORO base anim - %s %d: the core's frozen %s collapsed (the core had lifted it %.2f m)",
						              mono ? "monorail" : "hangrail", (int)k, run.what, dyMin);
						run.done = true;
						break;
					}
				}
			}
			if (!run.done) {
				if (run.bestMesh < 0)
					oapiWriteLogV("D3D9: ORO base anim - %s %d: the core's frozen %s was not found in the merged mesh (left as is): "
					              "no vertex within 5 m of the expected (%.1f, %.1f, %.1f)",
					              mono ? "monorail" : "hangrail", (int)k, run.what, run.w[0].x, run.w[0].y, run.w[0].z);
				else
					oapiWriteLogV("D3D9: ORO base anim - %s %d: the core's frozen %s was not found in the merged mesh (left as is): "
					              "closest run mesh %d group %d vtx %d, first vertex off by (%.2f, %.2f, %.2f) m, worst shape deviation %.1f x tolerance",
					              mono ? "monorail" : "hangrail", (int)k, run.what, run.bestMesh, run.bestGrp, run.bestVtx,
					              run.bestD0.x, run.bestD0.y, run.bestD0.z, run.bestWorst);
			}
		}
	}
}
