// ===========================================================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2013 - 2016 Jarmo Nikkanen
// ===========================================================================================


#include "MaterialMgr.h"
#include "D3D9Surface.h"
#include "OapiExtension.h"
#include "vVessel.h"
#include "D3D9Config.h"	// ORO patch (v): EnvMapMode gates the experimental file



// ===========================================================================================
//
MatMgr::MatMgr(class vObject *v, class D3D9Client *_gc)
{
	gc = _gc;
	vObj = v;

	// ORO patch (v): allocate all probe slots up front; only camera 0 exists until a
	// config declares more (nCameras tracks the highest index used + 1).
	pCamera = new ENVCAMREC[MAX_ENVCAM];
	nCameras = 1;

	for (DWORD i = 0; i < MAX_ENVCAM; i++) ResetCamera(i);

	// ORO patch (v) part 2: the planar-mirror slots
	pPlane = new ENVPLNREC[MAX_ENVPLN];
	nPlanes = 0;
	for (DWORD i = 0; i < MAX_ENVPLN; i++) ResetPlane(i);

	Shaders.push_back(SHADER("PBR-Old",SHADER_NULL));
	Shaders.push_back(SHADER("Metalness", SHADER_METALNESS));
}


// ===========================================================================================
//
MatMgr::~MatMgr()
{
	MeshConfig.clear();

	if (pCamera) {
		for (DWORD i = 0; i < MAX_ENVCAM; i++) {   // ORO patch (v): every slot
			if (pCamera[i].pOmitAttc) delete[] pCamera[i].pOmitAttc;
			if (pCamera[i].pOmitDock) delete[] pCamera[i].pOmitDock;
			if (pCamera[i].pGrpRng)   delete[] pCamera[i].pGrpRng;
		}
		delete[] pCamera;
	}
	if (pPlane) {
		for (DWORD i = 0; i < MAX_ENVPLN; i++)
			if (pPlane[i].pGrpRng) delete[] pPlane[i].pGrpRng;
		delete[] pPlane;
	}
}


// ===========================================================================================
//
ENVCAMREC * MatMgr::GetCamera(DWORD idx)
{
	// ORO patch (v): the index is honoured now (it used to pin to 0).
	return &pCamera[(idx < MAX_ENVCAM) ? idx : 0];
}


// ===========================================================================================
//
DWORD MatMgr::CameraCount()
{
	return nCameras;   // ORO patch (v): was a hardcoded 1
}


// ===========================================================================================
//
ENVPLNREC * MatMgr::GetPlane(DWORD idx)
{
	return &pPlane[(idx < MAX_ENVPLN) ? idx : 0];   // ORO patch (v) part 2
}


// ===========================================================================================
//
DWORD MatMgr::PlaneCount()
{
	return nPlanes;
}


// ===========================================================================================
//
void MatMgr::ResetPlane(DWORD idx)
{
	pPlane[idx].lPos = D3DXVECTOR3(0, 0, 0);
	pPlane[idx].lNrm = D3DXVECTOR3(0, 1, 0);
	pPlane[idx].refMesh = -1;
	pPlane[idx].refGrp = -1;
	pPlane[idx].nGrpRng = 0;
	pPlane[idx].pGrpRng = NULL;
	pPlane[idx].rDist = 5.0f;
}


// ===========================================================================================
//
void MatMgr::ResetCamera(DWORD idx)
{
	pCamera[idx].near_clip = 0.25f;
	pCamera[idx].lPos = D3DXVECTOR3(0,0,0);
	pCamera[idx].nAttc = 0;
	pCamera[idx].nDock = 0;
	pCamera[idx].flags = ENVCAM_OMIT_ATTC;
	pCamera[idx].pOmitAttc = NULL;
	pCamera[idx].pOmitDock = NULL;
	pCamera[idx].nGrpRng = 0;      // ORO patch (v)
	pCamera[idx].pGrpRng = NULL;
	pCamera[idx].bBox = false;
	pCamera[idx].bxC = D3DXVECTOR3(0, 0, 0);
	pCamera[idx].bxE = D3DXVECTOR3(0, 0, 0);
}
	

// ===========================================================================================
//
void MatMgr::RegisterMaterialChange(D3D9Mesh *pMesh, DWORD midx, const D3D9MatExt *pM)
{
	if (!pMesh || !pM) return;
	MeshConfig[pMesh->GetName()].material[midx] = *pM;
}

// ===========================================================================================
//
void MatMgr::RegisterShaderChange(D3D9Mesh *pMesh, WORD id)
{
	if (!pMesh) return;
	for (auto y : Shaders) if (y.id == id) {
		MeshConfig[pMesh->GetName()].shader = id;
		break;
	}
}


// ===========================================================================================
//
void MatMgr::ApplyConfiguration(D3D9Mesh *pMesh)
{
	if (pMesh==NULL) return;

	const char *name = pMesh->GetName();

	LogAlw("Applying custom configuration to a mesh (%s)",name);

	if (MeshConfig.count(name)) 
	{
		pMesh->SetDefaultShader(MeshConfig[name].shader);

		for (auto x : MeshConfig[name].material) 
		{
			auto rec = x.second;
			
			if (x.first >= int(pMesh->GetMaterialCount())) {
				LogErr("MatMgr::ApplyConfiguration: Matrial Idx out of range [%s.msh]", name);
				continue;
			}

			D3D9MatExt Mat;
			auto RecMat = x.second;
			DWORD flags = RecMat.ModFlags;

			if (!pMesh->GetMaterial(&Mat, x.first)) continue;

			if (flags&D3D9MATEX_AMBIENT) Mat.Ambient = RecMat.Ambient;
			if (flags&D3D9MATEX_DIFFUSE) Mat.Diffuse = RecMat.Diffuse;
			if (flags&D3D9MATEX_EMISSIVE) Mat.Emissive = RecMat.Emissive;
			if (flags&D3D9MATEX_REFLECT) Mat.Reflect = RecMat.Reflect;
			if (flags&D3D9MATEX_SPECULAR) Mat.Specular = RecMat.Specular;
			if (flags&D3D9MATEX_FRESNEL) Mat.Fresnel = RecMat.Fresnel;
			if (flags&D3D9MATEX_EMISSION2) Mat.Emission2 = RecMat.Emission2;
			if (flags&D3D9MATEX_ROUGHNESS) Mat.Roughness = RecMat.Roughness;
			if (flags&D3D9MATEX_METALNESS) Mat.Metalness = RecMat.Metalness;

			Mat.ModFlags = flags;

			pMesh->SetMaterial(&Mat, x.first);

			LogBlu("Material %u setup applied to mesh (%s) Flags=0x%X", x.first, name, flags);
		}
	}
}

// ===========================================================================================
//
bool MatMgr::HasMesh(const char *name)
{
	if (MeshConfig.count(name)) return true;
	return false;
}

// ===========================================================================================
//
void parse_vessel_classname(char *lbl)
{
	int i = -1;
	while (lbl[++i]!=0) if (lbl[i]=='/' || lbl[i]=='\\') lbl[i]='_';
}

// ===========================================================================================
//
bool MatMgr::LoadConfiguration(bool bAppend)
{
	_TRACE;

	char cbuf[256];
	char path[256];
	char classname[256];
	char meshname[64];
	char shadername[64];

	OBJHANDLE hObj = vObj->GetObjectA();

	if (oapiGetObjectType(hObj)!=OBJTP_VESSEL) return false; 

	const char *cfgdir = OapiExtension::GetConfigDir();

	VESSEL *vessel = oapiGetVesselInterface(hObj);
	strcpy_s(classname, 256, vessel->GetClassNameA());
	parse_vessel_classname(classname);

	AutoFile file;

	if (file.IsInvalid()) {
		sprintf_s(path, 256, "%sGC\\%s.cfg", cfgdir, classname);
		fopen_s(&file.pFile, path, "r");	
	}

	if (file.IsInvalid()) return true;

	LogAlw("Reading a custom configuration file for a vessel %s (%s)", vessel->GetName(), vessel->GetClassNameA());
	
	DWORD n = 0;
	int mat_idx = -1;

	while (fgets2(cbuf, 256, file.pFile, 0x0A)>=0) 
	{	
		float a, b, c, d;
		
		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "MESH", 4)) {
			mat_idx = -1;
			if (sscanf_s(cbuf, "MESH %s", meshname, 64)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			if (strncmp(meshname, "???", 3) == 0) meshname[0] = 0;
			if (HasMesh(meshname) && bAppend) meshname[0] = 0; // Mesh is loaded already skip all entries related to it.
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (meshname[0] == 0) continue;  // Do not continue without a valid mesh

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "SHADER", 6)) {
			MeshConfig[meshname].shader = SHADER_NULL;
			if (sscanf_s(cbuf, "SHADER %s", shadername, 64) != 1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			for (auto x : Shaders)
				if (string(shadername) == x.name) {
					MeshConfig[meshname].shader = x.id;
					LogOapi("NewShader [%s]=%hX", meshname, x.id);
				}
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "MATERIAL", 8)) {
			if (sscanf_s(cbuf, "MATERIAL %d", &mat_idx)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (mat_idx == -1) continue;  // Do not continue without a valid material idx

		auto &Mat = MeshConfig[meshname].material[mat_idx];

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "SPECULAR", 8)) {
			if (sscanf_s(cbuf, "SPECULAR %f %f %f %f", &a, &b, &c, &d)!=4) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Specular = D3DXVECTOR4(a, b, c, d);
			Mat.ModFlags |= D3D9MATEX_SPECULAR;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "DIFFUSE", 7)) {
			if (sscanf_s(cbuf, "DIFFUSE %f %f %f %f", &a, &b, &c, &d)!=4) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Diffuse = D3DXVECTOR4(a, b, c, d);
			Mat.ModFlags |= D3D9MATEX_DIFFUSE;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "EMISSIVE", 8)) {
			if (sscanf_s(cbuf, "EMISSIVE %f %f %f", &a, &b, &c)!=3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Emissive = D3DXVECTOR3(a, b, c);
			Mat.ModFlags |= D3D9MATEX_EMISSIVE;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "EMISSION2", 9)) {
			if (sscanf_s(cbuf, "EMISSION2 %f %f %f", &a, &b, &c) != 3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Emission2 = D3DXVECTOR3(a, b, c);
			Mat.ModFlags |= D3D9MATEX_EMISSION2;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "AMBIENT", 7)) {
			if (sscanf_s(cbuf, "AMBIENT %f %f %f", &a, &b, &c)!=3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Ambient = D3DXVECTOR3(a, b, c);
			Mat.ModFlags |= D3D9MATEX_AMBIENT;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "REFLECT", 7)) {
			if (sscanf_s(cbuf, "REFLECT %f %f %f", &a, &b, &c) != 3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Reflect = D3DXVECTOR3(a, b, c);
			Mat.ModFlags |= D3D9MATEX_REFLECT;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "FRESNEL", 7)) {
			if (sscanf_s(cbuf, "FRESNEL %f %f %f", &a, &b, &c) != 3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			if (b < 10.0f) b = 1024.0f;
			Mat.Fresnel = D3DXVECTOR3(a, c, b);
			Mat.ModFlags |= D3D9MATEX_FRESNEL;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "ROUGHNESS", 9)) {
			int cnt = sscanf_s(cbuf, "ROUGHNESS %f %f", &a, &b);
			if (cnt == 1) Mat.Roughness = D3DXVECTOR2(a, 1.0f);
			else if (cnt == 2)  Mat.Roughness = D3DXVECTOR2(a, b);
			else LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.ModFlags |= D3D9MATEX_ROUGHNESS;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "SMOOTHNESS", 10)) {
			int cnt = sscanf_s(cbuf, "SMOOTHNESS %f %f", &a, &b);
			if (cnt == 1) Mat.Roughness = D3DXVECTOR2(a, 1.0f);
			else if (cnt == 2)  Mat.Roughness = D3DXVECTOR2(a, b);
			else LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.ModFlags |= D3D9MATEX_ROUGHNESS;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "METALNESS", 9)) {
			if (sscanf_s(cbuf, "METALNESS %f", &a) != 1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			Mat.Metalness = a;
			Mat.ModFlags |= D3D9MATEX_METALNESS;
			continue;
		}
	}

	return true;
}


// ===========================================================================================
//
bool MatMgr::SaveConfiguration()
{
	_TRACE;
	bool bIfStatement = false;

	char path[256];
	char classname[256];
	
	
	OBJHANDLE hObj = vObj->GetObjectA();

	if (oapiGetObjectType(hObj)!=OBJTP_VESSEL) return false; 

	VESSEL *vessel = oapiGetVesselInterface(hObj);
	const char *cfgdir = OapiExtension::GetConfigDir();

	strcpy_s(classname, 256, vessel->GetClassNameA());
	parse_vessel_classname(classname);

	AutoFile file;
	sprintf_s(path, 256, "%sGC\\%s.cfg", cfgdir, classname);
	
	// If the target file contains configurations those are not loaded into the editor,
	// Load them before overwriting the file
	LoadConfiguration(true);

	fopen_s(&file.pFile, path, "w");

	if (file.IsInvalid()) {
		LogErr("Failed to write a file");
		return false;
	}

	fprintf(file.pFile, "CONFIG_VERSION 3\n");

	for (auto x : MeshConfig) 
	{		
		string current = x.first;

		fprintf(file.pFile,"; =============================================\n");
		fprintf(file.pFile, "MESH %s\n", current.c_str());

		for (auto y : Shaders) if (y.id == x.second.shader) fprintf(file.pFile, "SHADER %s\n", y.name.c_str());
		
		for (auto rec : x.second.material) 
		{		
			DWORD flags = rec.second.ModFlags;
			D3D9MatExt *pM = &rec.second;

			if (flags==0) continue;

			fprintf(file.pFile,"; ---------------------------------------------\n");
			fprintf(file.pFile,"MATERIAL %u\n", rec.first);
					
			if (flags&D3D9MATEX_AMBIENT)  fprintf(file.pFile,"AMBIENT %f %f %f\n", pM->Ambient.x, pM->Ambient.y, pM->Ambient.z);
			if (flags&D3D9MATEX_DIFFUSE)  fprintf(file.pFile,"DIFFUSE %f %f %f %f\n", pM->Diffuse.x, pM->Diffuse.y, pM->Diffuse.z, pM->Diffuse.w);
			if (flags&D3D9MATEX_SPECULAR) fprintf(file.pFile,"SPECULAR %f %f %f %f\n", pM->Specular.x, pM->Specular.y, pM->Specular.z, pM->Specular.w);
			if (flags&D3D9MATEX_EMISSIVE) fprintf(file.pFile,"EMISSIVE %f %f %f\n", pM->Emissive.x, pM->Emissive.y, pM->Emissive.z);
			if (flags&D3D9MATEX_REFLECT)  fprintf(file.pFile,"REFLECT %f %f %f\n", pM->Reflect.x, pM->Reflect.y, pM->Reflect.z);
			if (flags&D3D9MATEX_FRESNEL)  fprintf(file.pFile,"FRESNEL %f %f %f\n", pM->Fresnel.x, pM->Fresnel.z, pM->Fresnel.y);
			if (flags&D3D9MATEX_EMISSION2) fprintf(file.pFile, "EMISSION2 %f %f %f\n", pM->Emission2.x, pM->Emission2.y, pM->Emission2.z);
			if (flags&D3D9MATEX_ROUGHNESS) fprintf(file.pFile, "SMOOTHNESS %f %f\n", pM->Roughness.x, pM->Roughness.y);
			if (flags&D3D9MATEX_METALNESS) fprintf(file.pFile, "METALNESS %f\n", pM->Metalness);		
		}
	}
	return true;
}


// ===========================================================================================
//
// ORO patch (v): reflections read TWO files. <class>_ecam.cfg is the file stock
// D3D9 also opens - parsed with STOCK grammar and stock semantics in EVERY mode,
// so its content means exactly what it always meant, on either client.
// <class>_ecam_oro.cfg is the EXPERIMENTAL sidecar (indexed probes, GROUPS, BOX,
// planar mirrors): stock never builds that filename, so nothing in it can reach
// a stock client - and this client reads it only in "Full Scene ORO (exp)".
bool MatMgr::LoadCameraConfig()
{
	_TRACE;

	char path[256];
	char classname[256];

	OBJHANDLE hObj = vObj->GetObjectA();

	if (oapiGetObjectType(hObj)!=OBJTP_VESSEL) return false;

	const char *cfgdir = OapiExtension::GetConfigDir();

	VESSEL *vessel = oapiGetVesselInterface(hObj);
	strcpy_s(classname, 256, vessel->GetClassNameA());
	parse_vessel_classname(classname);

	OroHangTrace("lcc enter %s mode=%d", classname, Config->EnvMapMode);
	bool bAny = false;

	sprintf_s(path, 256, "%sGC\\%s_ecam.cfg", cfgdir, classname);
	if (ParseCameraFile(path, vessel, false)) bAny = true;

	if (Config->EnvMapMode >= 3) {
		sprintf_s(path, 256, "%sGC\\%s_ecam_oro.cfg", cfgdir, classname);
		if (ParseCameraFile(path, vessel, true)) bAny = true;
	}

	OroHangTrace("lcc exit %s cams=%u planes=%u", classname, nCameras, nPlanes);
	// ORO patch (v): the breadcrumb that separates "config never parsed" from
	// every failure downstream of it.
	if (bAny) {
		DWORD ranges = 0;
		for (DWORD i = 0; i < nCameras; i++) ranges += pCamera[i].nGrpRng;
		oapiWriteLogV("D3D9: env cameras (%s): %u probe(s), %u group range(s), %u plane(s)%s.",
		              classname, nCameras, ranges, nPlanes,
		              (Config->EnvMapMode >= 3) ? " [exp]" : "");
	}

	return true;
}

// bExt=false: STOCK grammar, stock semantics byte for byte (BEGIN_CAMERA forces
// camera 0 and clears its flags; ORO keywords fall to "Invalid Line" exactly as
// stock would say). bExt=true: the ORO grammar on top - indexed cameras, GROUPS,
// BOX, and the planar-mirror blocks.
bool MatMgr::ParseCameraFile(const char* path, class VESSEL* vessel, bool bExt)
{
	char cbuf[256];

	AutoFile file;

	fopen_s(&file.pFile, path, "r");

	if (file.IsInvalid()) return false;

	LogAlw("Reading a camera configuration file for a vessel %s (%s)", vessel->GetName(), vessel->GetClassNameA());
	OroHangTrace("parse enter %s ext=%d", path, (int)bExt);
	int traceLn = 0;

	DWORD iattc = 0;
	DWORD idock = 0;
	DWORD igrp  = 0;    // ORO patch (v): group-range pairs accumulated for this camera
	DWORD camera = 0;
	int   plane  = -1;   // ORO patch (v) part 2: >= 0 while inside a PLANE block
	WORD  grplist[128];

	BYTE attclist[256];
	BYTE docklist[256];

	while(fgets2(cbuf, 256, file.pFile, 0x08)>=0) 
	{	
		if ((++traceLn % 500) == 0) OroHangTrace("parse SPINNING? %s line %d", path, traceLn);
		float a, b, c;
		DWORD id;
		// ORO patch (v): fgets2 cuts ';' comments and returns the EMPTY remainder as a
		// valid line - skip it, or every comment/blank line spams "Invalid Line"
		if (!cbuf[0]) continue;

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "END_CAMERA", 10)) {

			if (iattc) pCamera[camera].pOmitAttc = new BYTE[iattc];
			if (idock) pCamera[camera].pOmitDock = new BYTE[idock];

			if (iattc) memcpy(pCamera[camera].pOmitAttc, attclist, iattc);
			if (idock) memcpy(pCamera[camera].pOmitDock, docklist, idock);

			pCamera[camera].nAttc = WORD(iattc);
			pCamera[camera].nDock = WORD(idock);

			// ORO patch (v): the group ranges that sample this probe
			if (igrp) {
				pCamera[camera].pGrpRng = new WORD[igrp];
				memcpy(pCamera[camera].pGrpRng, grplist, igrp * sizeof(WORD));
			}
			pCamera[camera].nGrpRng = WORD(igrp / 2);

			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "BEGIN_CAMERA", 12)) {
			if (sscanf_s(cbuf, "BEGIN_CAMERA %u", &camera)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			if (!bExt) camera = 0; // For now just one camera  (STOCK semantics, verbatim)
			else {
				// ORO patch (v): the index is real here ("For now just one camera"
				// un-parked). Clamped - out of range used to write PAST the array.
				if (camera >= MAX_ENVCAM) { LogErr("BEGIN_CAMERA %u out of range in (%s)", camera, path); camera = MAX_ENVCAM - 1; }
				plane = -1;   // a camera block ends any plane block
				// per-block accumulators reset HERE - with one camera they happened to
				// start at zero; with several, a second block inherits the first's.
				iattc = 0; idock = 0; igrp = 0;
			}
			if (camera + 1 > nCameras) nCameras = camera + 1;
			pCamera[camera].flags = 0; // Clear default flags
			continue;
		}

		// --------------------------------------------------------------------------------------------
		// ORO patch (v): "BOX cx cy cz hx hy hz" - the proxy volume (vessel-local centre
		// + half-extents) for box-projected sampling of this probe. Without it the probe
		// samples by raw direction, exactly as before - and a probe placed INSIDE the
		// geometry it reflects then paints that geometry magnified, which is the
		// artifact the box exists to cure.
		if (bExt && !strncmp(cbuf, "BOX", 3)) {
			float hx, hy, hz;
			if (sscanf_s(cbuf, "BOX %g %g %g %g %g %g", &a, &b, &c, &hx, &hy, &hz) != 6) { LogErr("Invalid Line in (%s): %s", path, cbuf); continue; }
			pCamera[camera].bxC = D3DXVECTOR3(a, b, c);
			pCamera[camera].bxE = D3DXVECTOR3(hx, hy, hz);
			pCamera[camera].bBox = true;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		// ORO patch (v): "GROUPS a b" - mesh groups a..b (inclusive) sample this probe.
		// Repeatable; applies to every mesh of the vessel (single-mesh hulls in practice).
		if (bExt && !strncmp(cbuf, "GROUPS", 6)) {
			DWORD ga, gb;
			if (sscanf_s(cbuf, "GROUPS %u %u", &ga, &gb) != 2) { LogErr("Invalid Line in (%s): %s", path, cbuf); continue; }
			if (igrp + 2 <= 128) { grplist[igrp++] = WORD(ga); grplist[igrp++] = WORD(gb); }
			continue;
		}

		// --------------------------------------------------------------------------------------------
		// ORO patch (v) part 2: PLANAR REFLECTION surfaces. BEGIN_PLANE n / POS x y z /
		// NRM x y z / GRPREF mesh group / GROUPS a b / END_PLANE. POS and NRM are in the
		// mesh BASE pose; when GRPREF names an animated group, the plane rides that
		// group's transform - an opening bay door carries its mirror with it.
		if (bExt && !strncmp(cbuf, "BEGIN_PLANE", 11)) {
			DWORD pidx = 0;
			if (sscanf_s(cbuf, "BEGIN_PLANE %u", &pidx) != 1) { LogErr("Invalid Line in (%s): %s", path, cbuf); continue; }
			if (pidx >= MAX_ENVPLN) { LogErr("BEGIN_PLANE %u out of range in (%s)", pidx, path); pidx = MAX_ENVPLN - 1; }
			plane = int(pidx);
			if (pidx + 1 > nPlanes) nPlanes = pidx + 1;
			igrp = 0;
			continue;
		}
		if (bExt && !strncmp(cbuf, "END_PLANE", 9)) {
			if (plane >= 0 && igrp) {
				pPlane[plane].pGrpRng = new WORD[igrp];
				memcpy(pPlane[plane].pGrpRng, grplist, igrp * sizeof(WORD));
				pPlane[plane].nGrpRng = WORD(igrp / 2);
			}
			plane = -1; igrp = 0;
			continue;
		}
		if (plane >= 0 && !strncmp(cbuf, "POS", 3)) {
			if (sscanf_s(cbuf, "POS %g %g %g", &a, &b, &c) != 3) { LogErr("Invalid Line in (%s): %s", path, cbuf); continue; }
			pPlane[plane].lPos = D3DXVECTOR3(a, b, c);
			continue;
		}
		if (plane >= 0 && !strncmp(cbuf, "NRM", 3)) {
			if (sscanf_s(cbuf, "NRM %g %g %g", &a, &b, &c) != 3) { LogErr("Invalid Line in (%s): %s", path, cbuf); continue; }
			D3DXVECTOR3 n(a, b, c);
			D3DXVec3Normalize(&n, &n);
			pPlane[plane].lNrm = n;
			continue;
		}
		if (plane >= 0 && !strncmp(cbuf, "RDIST", 5)) {
			if (sscanf_s(cbuf, "RDIST %g", &a) != 1) { LogErr("Invalid Line in (%s): %s", path, cbuf); continue; }
			// 0 = the FLAT mirror verbatim (the pixel's own screen position - the
			// pre-warp behaviour, kept as an exact A/B); positive floors at 0.5 m
			pPlane[plane].rDist = (a <= 0.01f) ? 0.0f : max(0.5f, a);
			continue;
		}
		if (plane >= 0 && !strncmp(cbuf, "GRPREF", 6)) {
			DWORD rm, rg;
			if (sscanf_s(cbuf, "GRPREF %u %u", &rm, &rg) != 2) { LogErr("Invalid Line in (%s): %s", path, cbuf); continue; }
			pPlane[plane].refMesh = int(rm);
			pPlane[plane].refGrp  = int(rg);
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "LPOS", 4)) {
			if (sscanf_s(cbuf, "LPOS %g %g %g", &a, &b, &c)!=3) LogErr("Invalid Line in (%s): %s", path, cbuf);
			pCamera[camera].lPos = D3DXVECTOR3(a,b,c);
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "OMITATTC", 8)) {
			if (sscanf_s(cbuf, "OMITATTC %u", &id)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			attclist[iattc++] = BYTE(id);
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "OMITDOCK", 8)) {
			if (sscanf_s(cbuf, "OMITDOCK %u", &id)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			docklist[idock++] = BYTE(id);
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "CLIPDIST", 8)) {
			if (sscanf_s(cbuf, "CLIPDIST %g", &a)!=1) LogErr("Invalid Line in (%s): %s", path, cbuf);
			pCamera[camera].near_clip = a;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "OMIT_ALL_ATTC", 13)) {
			pCamera[camera].flags |= ENVCAM_OMIT_ATTC;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "DO_NOT_OMIT_FOCUS", 17)) {
			pCamera[camera].flags |= ENVCAM_FOCUS;
			continue;
		}

		// --------------------------------------------------------------------------------------------
		if (!strncmp(cbuf, "OMIT_ALL_DOCKS", 14)) {
			pCamera[camera].flags |= ENVCAM_OMIT_DOCKS;
			continue;
		}

		if (cbuf[0]!=';') LogErr("Invalid Line in (%s): %s", path, cbuf);
	}

	OroHangTrace("parse exit %s lines=%d", path, traceLn);
	return true;
}



