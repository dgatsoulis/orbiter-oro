// ==============================================================
// VBase.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2007 - 2016 Martin Schweiger
//				 2011 - 2016 Jarmo Nikkanen (D3D9Client modification)  
// ==============================================================

// ==============================================================
// class vBase (implementation)
//
// A vBase is the visual representation of a surface base
// object (a "spaceport" on the surface of a planet or moon,
// usually with runways or landing pads where vessels can
// land and take off.
// ==============================================================

#include "VBase.h"
#include "TileMgr.h"
#include "D3D9Client.h"
#include "D3D9Surface.h"
#include "BeaconArray.h"
#include "RunwayLights.h"
#include "AABBUtil.h"
#include "OrbiterAPI.h"
#include "DebugControls.h"
#include "D3D9Config.h"
#include "VPlanet.h"
#include "OroBaseAnim.h"   // ORO patch (ag): the animated base objects

#pragma warning(push)
#pragma warning(disable : 4838)
#include <xnamath.h>
#pragma warning(pop)

typedef struct {
	float rad;
	float width, length, height;
	D3DXVECTOR3 pos, min, max;
} MeshStats;


void CheckMeshStats(MESHHANDLE hMesh, MeshStats *stats)
{
	int nGrp = oapiMeshGroupCount(hMesh);
	if (nGrp == 0) return;

	XMVECTOR mi = XMLoadFloat3(ptr(XMFLOAT3(1e12f, 1e12f, 1e12f)));
	XMVECTOR mx = -mi;

	for (int i = 0; i < nGrp; i++) {

		MESHGROUPEX *grp = oapiMeshGroupEx(hMesh, i);
		
		for (DWORD v = 0; v < grp->nVtx; v++) {
			XMVECTOR x = XMLoadFloat3((XMFLOAT3 *)&grp->Vtx[v].x);
			mi = XMVectorMin(mi, x);
			mx = XMVectorMax(mx, x);
		}
	}

	XMStoreFloat3((XMFLOAT3 *)&stats->min.x, mi);
	XMStoreFloat3((XMFLOAT3 *)&stats->max.x, mx);

	stats->width = stats->max.x - stats->min.x;
	stats->height = stats->max.y - stats->min.y;
	stats->length = stats->max.z - stats->min.z;
	stats->pos = (stats->max + stats->min) * 0.5f;
	stats->rad = D3DXVec3Length(ptr(stats->max + stats->min)) * 0.5f;
}



vBase::vBase (OBJHANDLE _hObj, const Scene *scene, vPlanet *_vP): vObject (_hObj, scene)
{
	_TRACE;
	DWORD i,j;

	vP = _vP;
	hPlanet = oapiGetBasePlanet(hObj);

	if (!vP) vP = static_cast<vPlanet*>( scene->GetVisObject(hPlanet) );
	
	structure_bs	= NULL;
	structure_as	= NULL;
	nstructure_bs	= 0;
	nstructure_as	= 0;
	oroAnim         = NULL;   // ORO patch (ag)
	tspec			= NULL;
	tilemesh		= NULL;
	numRunwayLights = 0;
	numTaxiLights   = 0;
	runwayLights    = NULL;
	taxiLights		= NULL;
	csun_lights     = RAD * Config->SunAngle;

	// ----------------------------------------------------------------------
	// Compute transformations from local base frame to planet frame and back
	//
	MATRIX3 plrot; VECTOR3 relpos;
	oapiGetRotationMatrix(hPlanet, &plrot);
	oapiGetRotationMatrix(hObj, &mGlobalRot);
	oapiGetRelativePos(hObj, hPlanet, &relpos);
	vLocalPos = tmul(plrot, relpos);

	swap(plrot.m12, plrot.m21);
	swap(plrot.m13, plrot.m31);
	swap(plrot.m23, plrot.m32);

	mGlobalRot = mul(plrot, mGlobalRot);

	D3DXMatrixIdentity(&mGlobalRotDX);
	D3DMAT_SetRotation(&mGlobalRotDX, &mGlobalRot);
	//------------------------------------------------------------------------

	// load surface tiles
	DWORD _ntile = gc->GetBaseTileList (_hObj, &tspec);
	ntile = 0;
	for (i=0; i<_ntile; ++i) {
		// Only count (render) tiles where bit0 is set!
		if (tspec[i].texflag & 0x01) {
			++ntile;
		}
	}

	// Do not render tiles for planets having a new tile format
	if (vP->tilever >= 2) ntile = 0;

	if (ntile) {

		MESHGROUPEX **grps = new MESHGROUPEX*[ntile];
		SURFHANDLE *texs = new SURFHANDLE[ntile];

		for (i = 0, j = 0; i < _ntile; ++i) {
			// Only render tiles where bit0 is set!
			if (tspec[i].texflag & 0x01) {
				DWORD ng = oapiMeshGroupCount(tspec[i].mesh);
				if (ng!=1) LogErr("MeshGroup Count = %u",ng);
				else {
					texs[j] = tspec[i].tex;
					grps[j] = oapiMeshGroupEx(tspec[i].mesh, 0);
				}
				++j;
			}
		}
		tilemesh = new D3D9Mesh(ntile, (const MESHGROUPEX**)grps, texs);	
		delete []grps;
		grps = NULL;
        delete []texs;
		texs = NULL;
	}

	// load meshes for generic structures
	MESHHANDLE *sbs, *sas;
	DWORD nsbs, nsas;
	gc->GetBaseStructures (_hObj, &sbs, &nsbs, &sas, &nsas);

	// ORO patch: complete the <tex>_n night-texture pair for MESH base objects
	// (and the RUNWAY surface) - the core wires the night layer only for the
	// generic object types, so MESH blocks arrived with it empty and the
	// day/night switch below had nothing to flip. See D3D9Mesh::AttachNightTextures.
	if (nstructure_bs = nsbs) {
		structure_bs = new D3D9Mesh*[nsbs];
		for (i = 0; i < nsbs; i++) { structure_bs[i] = new D3D9Mesh(sbs[i]); structure_bs[i]->AttachNightTextures(); }
	}

	if (nstructure_as = nsas) {
		structure_as = new D3D9Mesh*[nsas];
		for (i = 0; i < nsas; i++) { structure_as[i] = new D3D9Mesh(sas[i]); structure_as[i]->AttachNightTextures(); }
	}

	// ORO patch (ag): THE ANIMATED BASE OBJECTS (2026-09-07). The base's own cfg is
	// re-read for TRAIN1 / TRAIN2 / SOLARPLANT blocks (see OroBaseAnim.h for why the
	// core's own machinery never runs under a client); their meshes are APPENDED to
	// the above-shadow structure list, so every consumer of that list - the render,
	// the depth pass (z2), the local-light and cascade casters (z3/ae), the night
	// texture flip, the bounding box - takes them with no further plumbing, and the
	// core's frozen exports are collapsed so nothing draws twice.
	{
		OroBaseAnim *an = new OroBaseAnim(gc, _hObj, hPlanet, GetElevation());
		if (an->MeshCount()) {
			an->HideCoreDuplicates(structure_bs, nstructure_bs, structure_as, nstructure_as);
			const DWORD nNew = nstructure_as + an->MeshCount();
			D3D9Mesh **as2 = new D3D9Mesh*[nNew];
			for (i = 0; i < nstructure_as; i++) as2[i] = structure_as[i];
			for (DWORD k = 0; k < an->MeshCount(); k++) as2[nstructure_as + k] = an->Mesh(k);
			if (structure_as) delete []structure_as;
			structure_as = as2;
			nstructure_as = nNew;
			oroAnim = an;
		}
		else delete an;
	}

	lights = false;
	Tchk = Tlghtchk = oapiGetSimTime()-1.0;

	UpdateBoundingBox();
	
	char name[64];
	oapiGetObjectName(_hObj, name, 64);
	LogAlw("New Base Visual(%s) %s hBase=%s, nsbs=%u, nsas=%u", _PTR(this), name, _PTR(_hObj), nsbs, nsas);

	CreateRunwayLights();
	CreateTaxiLights();
}


// ===========================================================================================
//
VECTOR3 vBase::ToLocal(VECTOR3 pos, double *lng, double *lat) const
{
	double rad;
	VECTOR3 vLoc = mul(mGlobalRot, pos) + vLocalPos;
	if (lng && lat) oapiLocalToEqu(hPlanet, vLoc, lng, lat, &rad);
	return vLoc;
}


// ===========================================================================================
//
VECTOR3 vBase::FromLocal(VECTOR3 pos) const
{
	return tmul(mGlobalRot, pos-vLocalPos);
}

// ===========================================================================================
//
void vBase::FromLocal(VECTOR3 pos, D3DXVECTOR3 *pTgt) const
{
	D3DXVECTOR3 pv(float(pos.x-vLocalPos.x), float(pos.y-vLocalPos.y), float(pos.z-vLocalPos.z));
	D3DXVec3TransformNormal(pTgt, &pv, &mGlobalRotDX);
}

// ===========================================================================================
//
double vBase::GetElevation() const
{
	VECTOR3 bp;
	oapiGetRelativePos(hObj, hPlanet, &bp);
	return length(bp) - oapiGetSize(hPlanet);
}


// ===========================================================================================
//
void vBase::CreateRunwayLights()
{
	const char *file = oapiGetObjectFileName(hObj);
	if (file) numRunwayLights = RunwayLights::CreateRunwayLights(this, scn, file, runwayLights);
	else LogErr("Configuration file not found for object %s", _PTR(hObj));
}

// ===========================================================================================
//
void vBase::CreateTaxiLights()
{
	const char *file = oapiGetObjectFileName(hObj);
	if (file) numTaxiLights = TaxiLights::CreateTaxiLights(hObj, scn, file, taxiLights);
	else LogErr("Configuration file not found for object %s", _PTR(hObj));
}

// ===========================================================================================
//
vBase::~vBase ()
{
	DWORD i;

	if (tilemesh) delete tilemesh;

	if (nstructure_bs) {
		for (i = 0; i < nstructure_bs; i++)	delete structure_bs[i];
		delete []structure_bs;
		structure_bs = NULL;
	}
	if (nstructure_as) {
		for (i = 0; i < nstructure_as; i++)	delete structure_as[i];
		delete []structure_as;
		structure_as = NULL;
	}
	if (oroAnim) { delete oroAnim; oroAnim = NULL; }   // ORO patch (ag): its meshes went with structure_as above

	if (runwayLights) {
		for(i=0; i<(DWORD)numRunwayLights; i++)
		{
			SAFE_DELETE(runwayLights[i]);
		}
		delete[] runwayLights;
		runwayLights = NULL;
	}

	if (taxiLights) {
		for(i=0; i<(DWORD)numTaxiLights; i++)
		{
			SAFE_DELETE(taxiLights[i]);
		}
		delete[] taxiLights;
	}

	if (DebugControls::IsActive()) {
		DebugControls::RemoveVisual(this);
	}
}

// ===========================================================================================
//
DWORD vBase::GetMeshCount()
{
	if (tilemesh) return nstructure_bs + nstructure_as + 1;
	else          return nstructure_bs + nstructure_as;
}

// ===========================================================================================
//
bool vBase::GetMinMaxDistance(float *zmin, float *zmax, float *dmin)
{
	if (bBSRecompute) UpdateBoundingBox();
	
	D3DXMATRIX mWorldView;

	Scene *scn = gc->GetScene();

	D3DXVECTOR4 Field = D9LinearFieldOfView(scn->GetProjectionMatrix());
	
	D3DXMatrixMultiply(&mWorldView, &mWorld, scn->GetViewMatrix());
	
	if (tilemesh) {
		D9ComputeMinMaxDistance(gc->GetDevice(), tilemesh->GetAABB(), &mWorldView, &Field, zmin, zmax, dmin);
	}

	if (nstructure_bs) {
		for (DWORD i = 0; i < nstructure_bs; i++) {
			D9ComputeMinMaxDistance(gc->GetDevice(), structure_bs[i]->GetAABB(), &mWorldView, &Field, zmin, zmax, dmin);
		}
	}

	if (nstructure_as) {
		for (DWORD i = 0; i < nstructure_as; i++) {
			D9ComputeMinMaxDistance(gc->GetDevice(), structure_as[i]->GetAABB(), &mWorldView, &Field, zmin, zmax, dmin);
		}
	}

	return true;
}


// ===========================================================================================
//
void vBase::UpdateBoundingBox()
{
	bBSRecompute = false;
	
	if (tilemesh || nstructure_bs || nstructure_as) D9InitAABB(&BBox);
	else D9ZeroAABB(&BBox);

	
	if (tilemesh) D9AddAABB(tilemesh->GetAABB(), NULL, &BBox);
		
	if (nstructure_bs) {
		for (DWORD i = 0; i < nstructure_bs; i++) {
			D9AddAABB(structure_bs[i]->GetAABB(), NULL, &BBox);
		}
	}

	if (nstructure_as) {
		for (DWORD i = 0; i < nstructure_as; i++) {
			D9AddAABB(structure_as[i]->GetAABB(), NULL, &BBox);
		}
	}

	D9UpdateAABB(&BBox);
}


// ===========================================================================================
//
bool vBase::Update (bool bMainScene)
{
	_TRACE;
	if (!active) return false;
	if (!vObject::Update(bMainScene)) return false;

	double simt = oapiGetSimTime();

	if (fabs(simt-Tlghtchk)>0.1 || oapiGetPause()) {
		VECTOR3 rpos = gpos - vP->GlobalPos();
		sunLight = vP->GetObjectAtmoParams(rpos);
		Tlghtchk = simt;
	}

	// ORO patch (ag): the trains move on sim time (paused = still; warp = faster, as in
	// 2010); the solar panels aim at the sun and glint at the camera. Both directions go
	// over in BASE-LOCAL coordinates: vObject's sundir and camera-relative position
	// through the base's own rotation (grot maps base-local to global, so tmul inverts).
	if (oroAnim) oroAnim->Update(simt, tmul(grot, sundir), tmul(grot, -cpos));

	if (fabs(simt-Tchk)>1.0) {
		VECTOR3 pos, sdir;
		MATRIX3 rot;
		oapiGetGlobalPos (hObj, &pos); normalise(pos);
		oapiGetRotationMatrix (hObj, &rot);
		sdir = tmul (rot, -pos);
		double csun = sdir.y;
		// (A twilight RAMP was built here 2026-09-01 and REVERTED on his flight
		//  the same evening: "when someone is in a building, they turn on the
		//  lights at dusk... that is a flip, not a ramp." Interior lights are
		//  switched by people, not faded by the sun - the stock flip stays.)
		// ORO patch (ac): the addon may FORCE the night state (BASE LIGHTS - low
		// visibility is when an airfield switches its lights on). Off = stock flip.
		extern bool g_gcBaseLightsForce;
		bool night = (csun < csun_lights) || g_gcBaseLightsForce;
		if (lights != night) {
			DWORD i;
			for (i = 0; i < nstructure_bs; i++)	structure_bs[i]->SetTexMixture (1, night ? 1.0f:0.0f);
			for (i = 0; i < nstructure_as; i++)	structure_as[i]->SetTexMixture (1, night ? 1.0f:0.0f);
			lights = night;
		}
		Tchk = simt;
	}
	return true;
}


// ===========================================================================================
//
bool vBase::RenderSurface(LPDIRECT3DDEVICE9 dev)
{
	if (!active) return false;
	if (!IsVisible()) return false;

	pCurrentVisual = this;

	// ORO patch (z) round 2: the surface objects obey the depth buffer now
	// (runways/pads painted through mountains was the stock symptom), and the
	// price is that they sit near-coplanar with the terrain that wrote that
	// depth - flown round 1 z-fought hard. A camera-ward depth bias settles
	// the coplanar contest in the decal's favour while staying orders of
	// magnitude too small to see through real terrain: the constant part is
	// ~300 ticks of a 24-bit buffer, the slope-scaled part is what carries
	// grazing angles, where a ground plane's depth slope per pixel is huge.
	// Cleared below - these states are not in any pass block, so nothing
	// else in the frame resets them for us.
	const float fBias = -0.00002f, fSlope = -2.0f;
	dev->SetRenderState(D3DRS_DEPTHBIAS, *(DWORD*)&fBias);
	dev->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *(DWORD*)&fSlope);

	// ORO patch (ac): the night-light glow gain, scoped to THIS base's draws (the vessel
	// shaders serve hulls too, and a hull's cabin lights must not bloom because a runway
	// does). Restored to 1 before the function returns.
	{ extern float g_gcBaseLightsGlow; if (D3D9Effect::eBaseGlow) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseGlow, g_gcBaseLightsGlow); }
	if (D3D9Effect::eBaseLocal) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseLocal, 1.0f);   // ORO A5: base-local drop glint
	// ORO 2026-09-11: THIS bracket only - the below-shadow surfaces ARE ground, and they
	// take the terrain's wet darkening instead of a hull's. The above-shadow brackets
	// deliberately leave it at 0: a hangar wall is not ground. See gBaseGround.
	if (D3D9Effect::eBaseGround) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseGround, 1.0f);

	// render tiles
	if (tilemesh) {
		uCurrentMesh = 0; // Used for debugging
		tilemesh->SetSunLight(&sunLight);
		tilemesh->RenderBaseTile(&mWorld);
		++uCurrentMesh;
	}

	// render generic objects under shadows
	if (nstructure_bs) {
		for (DWORD i = 0; i < nstructure_bs; ++i) {
			structure_bs[i]->SetSunLight(&sunLight);
			structure_bs[i]->Render(&mWorld, RENDER_BASEBS);
			++uCurrentMesh;
		}
	}

	// ORO 2026-09-10: the wet-ground look on RUNWAYS, PADS and TAXIWAYS. The below-shadow
	// structures take the vessel shader path and so never had the tile's pools, sky film
	// or mirrored vessel (his report: "a vessel landed on a textured runway doesn't get
	// reflected"). Drawn OVER each one, here, inside this same depth-bias bracket so it
	// z-tests as the surface does. See WetOverlayTech in Mesh.fx. Nothing when dry.
	{
		extern float g_gcSurfaceWet;
		if (nstructure_bs && g_gcSurfaceWet > 0.01f)
			for (DWORD i = 0; i < nstructure_bs; ++i) structure_bs[i]->RenderWetOverlay(&mWorld);
	}

	if (D3D9Effect::eBaseGlow) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseGlow, 1.0f);   // ORO patch (ac)
	if (D3D9Effect::eBaseLocal) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseLocal, 0.0f);  // ORO A5: back to UV keying for vessels
	if (D3D9Effect::eBaseGround) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseGround, 0.0f); // ORO 2026-09-11: hull darkening for everything else
	// ORO patch (z): clear the depth bias - see the note at the top
	dev->SetRenderState(D3DRS_DEPTHBIAS, 0);
	dev->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);

	return true;
}


// ===========================================================================================
//
bool vBase::RenderStructures(LPDIRECT3DDEVICE9 dev)
{
	if (!active) return false;
	if (!IsVisible()) return false;

	pCurrentVisual = this;
	uCurrentMesh = 0; // Used for debugging

	if (tilemesh) uCurrentMesh++;
	uCurrentMesh += nstructure_bs;

	{ extern float g_gcBaseLightsGlow; if (D3D9Effect::eBaseGlow) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseGlow, g_gcBaseLightsGlow); }   // ORO patch (ac)
	if (D3D9Effect::eBaseLocal) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseLocal, 1.0f);   // ORO A5: base-local drop glint

	// render generic objects above shadows
	for (DWORD i=0; i<nstructure_as; i++) {
		FVECTOR3 bs = structure_as[i]->GetBoundingSpherePos();
		FVECTOR3 qw = TransformCoord(bs, mWorld);
		D3D9Sun sp = vP->GetObjectAtmoParams(qw._V() + vP->CameraPos());
		structure_as[i]->SetSunLight(&sp);
		structure_as[i]->Render(&mWorld, RENDER_BASE);
		++uCurrentMesh;
	}
	if (D3D9Effect::eBaseGlow) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseGlow, 1.0f);   // ORO patch (ac)
	if (D3D9Effect::eBaseLocal) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseLocal, 0.0f);  // ORO A5: back to UV keying for vessels
	return true;
}


// ===========================================================================================
// ORO patch (z2): base structures join the depth-normal buffer (2026-09-02, his
// light-through-the-hangar screenshot). GBUF_DEPTH held VESSELS + COCKPIT only, so every
// consumer that asks it a visibility question was blind to buildings: the sun and
// local-light glare kernels painted their sprites straight through a hangar, and the
// Sketchpad depth clip (patch g) let addon geometry draw in front of structures it should
// vanish behind. Same RenderShadowMap(opt 1) path the vessels use - the function is
// mesh-generic, and patch (f) part 2's transparent-caster skip rides along, so a glass
// wall correctly fails to occlude.
// ⚠️ ABOVE-SHADOW STRUCTURES ONLY, deliberately: the ground-level sets (tilemesh,
// structure_bs - runways, aprons, pads) are coplanar with TERRAIN, which never writes
// this buffer either. Admitting one side of that contest would make addon geometry clip
// against an apron but not the grass beside it - worse than staying out entirely.
// ORO patch (z3): grew an opt parameter. opt 1 (default) is the (z2) behaviour above;
// opt 0 renders the same above-shadow set into a LIGHT's shadow map via the
// SHADER_SHADOWMAP technique, so a spotlight beam is carved by a hangar. The
// ground-level exclusion is just as right there - coplanar-with-terrain geometry
// shadowing the terrain it lies on would be pure acne.
// ===========================================================================================
bool vBase::RenderStructureDepth(const LPD3DXMATRIX pVP, int opt)
{
	if (!active) return false;
	// ⚠️ IsVisible() is a CAMERA test and belongs to opt 1 ONLY (GBUF_DEPTH is the
	// camera's own screen-space buffer). A LIGHT-space map (opt 0) must never
	// view-cull its casters: rotating the camera until the base left the frustum
	// dropped every structure out of the shadow maps and the whole draped shadow
	// strobed with the view direction - his daylight rotation test, 2026-09-02.
	if (opt == 1 && !IsVisible()) return false;
	for (DWORD i = 0; i < nstructure_as; i++)
		structure_as[i]->RenderShadowMap(&mWorld, pVP, opt);
	return true;
}

// ORO patch (ah) step 2: the above-shadow structures join the spot map's CASTER FIT - each
// mesh's bounding sphere, taken to world through mWorld (rotation + translation, no scale).
// Returns how many intersect the beam. Same list RenderStructureDepth draws, so a hangar
// that can cast is a hangar the fitted frustum keeps.
// ORO patch (ah) step 5: a POINT light has no axis, so its map is AIMED at the casters - the
// solid-angle-weighted direction of the visible above-shadow structures within range.
int vBase::AimLocalShadowCasters(const D3DXVECTOR3& P, float range, D3DXVECTOR3& sum, float& weight)
{
	if (!active) return 0;
	int n = 0;
	for (DWORD i = 0; i < nstructure_as; i++) {
		if (!structure_as[i]) continue;
		D3DXVECTOR3 cl = structure_as[i]->GetBoundingSpherePos();
		float bsr = structure_as[i]->GetBoundingSphereRadius();
		D3DXVECTOR3 c; D3DXVec3TransformCoord(&c, &cl, &mWorld);
		D3DXVECTOR3 rel = c - P;
		const float d = D3DXVec3Length(&rel);
		if (d > range + bsr || d <= bsr + 0.01f) continue;        // out of reach, or the light is inside it
		if (!scn->IsVisibleInCamera(&c, bsr)) continue;
		const float w = (bsr * bsr) / (d * d);
		sum += rel * (w / d); weight += w; n++;
	}
	return n;
}

int vBase::FitLocalShadowCasters(const D3DXVECTOR3& P, const D3DXVECTOR3& D, float range, float halfCone, float& halfFit, float& farFit)
{
	if (!active) return 0;
	int n = 0;
	for (DWORD i = 0; i < nstructure_as; i++) {
		if (!structure_as[i]) continue;
		D3DXVECTOR3 cl = structure_as[i]->GetBoundingSpherePos();
		float bsr = structure_as[i]->GetBoundingSphereRadius();
		D3DXVECTOR3 c; D3DXVec3TransformCoord(&c, &cl, &mWorld);
		D3DXVECTOR3 rel = c - P;
		if (D3DXVec3Length(&rel) > range + bsr) continue;
		if (!scn->IsVisibleInCamera(&c, bsr)) continue;		// visible receivers only - see Scene.cpp's fit
		if (OroFitCasterSphere(rel, bsr, D, halfCone, halfFit, farFit)) n++;
	}
	return n;
}




// ===========================================================================================
//
void vBase::RenderRunwayLights(LPDIRECT3DDEVICE9 dev)
{
	if (!active) return;
	if (!IsVisible()) return;

	pCurrentVisual = this;

	{ extern float g_gcBaseLightsGlow; if (D3D9Effect::eBaseGlow) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseGlow, g_gcBaseLightsGlow); }   // ORO patch (ac)
	if (D3D9Effect::eBaseLocal) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseLocal, 1.0f);   // ORO A5: base-local drop glint
	{ extern float g_gcBaseLightsHalo; if (D3D9Effect::eBaseHalo) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseHalo, g_gcBaseLightsHalo); }   // ORO patch (ac) part 2

	for(int i=0; i<numRunwayLights; i++)
	{
		if (scn->GetRenderPass() == RENDERPASS_MAINSCENE) runwayLights[i]->Update(vP);
		runwayLights[i]->Render(dev, &mWorld, lights);
	}

	for(int i=0; i<numTaxiLights; i++)
	{
		taxiLights[i]->Render(dev, &mWorld, lights);
	}
	if (D3D9Effect::eBaseGlow) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseGlow, 1.0f);   // ORO patch (ac)
	if (D3D9Effect::eBaseLocal) D3D9Effect::FX->SetFloat(D3D9Effect::eBaseLocal, 0.0f);  // ORO A5: back to UV keying for vessels
	
	if (DebugControls::IsActive()) {
		DWORD flags = *(DWORD*)gc->GetConfigParam(CFGPRM_GETDEBUGFLAGS);
		if (flags&DBG_FLAGS_SELVISONLY && this!=DebugControls::GetVisual()) return; // Used for debugging
		if (flags&DBG_FLAGS_BOXES) {
			D3DXMATRIX id;
			D3D9Effect::RenderBoundingBox(&mWorld, D3DXMatrixIdentity(&id), &BBox.min, &BBox.max, ptr(D3DXVECTOR4(1,0,1,0.75f)));
		}
	}
}


// ===========================================================================================
//
void vBase::RenderGroundShadow(LPDIRECT3DDEVICE9 dev, float alpha)
{
	if (!nstructure_as) return; // nothing to do
	if (!active) return;
	if (!IsVisible()) return;
	if (Config->TerrainShadowing == 0) return;

	pCurrentVisual = this;

	VECTOR3 sd;
	oapiGetGlobalPos(hObj, &sd); normalise(sd);
	
	MATRIX3 mRot;
	oapiGetRotationMatrix(hObj, &mRot);
	D3DXVECTOR3 lsun = D3DXVEC(tmul(mRot, sd));

	if (lsun.y > -0.07f) return;

	float scale = (-lsun.y - 0.07f) * 25.0f;
	scale = (1.0f - alpha) * saturate(scale);
	

	// build shadow projection matrix
	D3DXMATRIX mProj;
	
	OBJHANDLE hPlanet = oapiGetBasePlanet(hObj); 
	double prad = oapiGetSize(hPlanet);
	D3DXVECTOR4 param = D9OffsetRange(prad, 30e3);

	for (DWORD i=0; i<nstructure_as; i++) {
		
		if (structure_as[i]->HasShadow()) {

			double a, b, el0, el1, el2;
			double d = atan(1.0 / prad);
			VECTOR3 va, vb, vc;
			VECTOR3 q = _V(structure_as[i]->BBox.bs);
			float rad = structure_as[i]->BBox.bs.w;

			if (rad<250.0f) ToLocal(q, &a, &b);	
			else ToLocal(_V(0,0,0), &a, &b);

			if (vP->GetElevation(a, b, &el0) <= 0) el0 = oapiSurfaceElevation(hPlanet, a, b);
			if (vP->GetElevation(a + d, b, &el1) <= 0) el1 = oapiSurfaceElevation(hPlanet, a + d, b);
			if (vP->GetElevation(a, b + d, &el2) <= 0) el2 = oapiSurfaceElevation(hPlanet, a, b + d);

			oapiEquToLocal(hPlanet, a, b, el0 + prad, &va);
			oapiEquToLocal(hPlanet, a + d, b, el1 + prad, &vb);
			oapiEquToLocal(hPlanet, a, b + d, el2 + prad, &vc);

			va = FromLocal(va);
			vb = FromLocal(vb);
			vc = FromLocal(vc);
				
			VECTOR3 n = -unit(crossp(vb - va, vc - va));

			D3DXVECTOR3 hn = D3DXVEC(n); 
				
			float zo = float(-dotp(va, n));
			float nd = D3DXVec3Dot(&hn, &lsun);
			hn /= nd;
			float ofs = zo / nd;

			mProj._11 = 1.0f - (float)(lsun.x*hn.x);
			mProj._12 = -(float)(lsun.y*hn.x);
			mProj._13 = -(float)(lsun.z*hn.x);
			mProj._14 = 0;
			mProj._21 = -(float)(lsun.x*hn.y);
			mProj._22 = 1.0f - (float)(lsun.y*hn.y);
			mProj._23 = -(float)(lsun.z*hn.y);
			mProj._24 = 0;
			mProj._31 = -(float)(lsun.x*hn.z);
			mProj._32 = -(float)(lsun.y*hn.z);
			mProj._33 = 1.0f - (float)(lsun.z*hn.z);
			mProj._34 = 0;
			mProj._41 = -(float)(lsun.x*ofs);
			mProj._42 = -(float)(lsun.y*ofs);
			mProj._43 = -(float)(lsun.z*ofs);
			mProj._44 = 1;
				
			D3DXVECTOR4 nrml = D3DXVECTOR4(float(n.x), float(n.y), float(n.z), zo);

			// ORO patch (aa): the storm collapse and the fog reach this shadow too, at the
			// STRUCTURE's own position (a base spans kilometres, so per base would be
			// wrong for everything but the middle). Stock drew these at full strength
			// under any overcast and through any fog - which is what left the buildings'
			// shadows floating in a fog that had erased the buildings. See
			// OroGroundShadowFade in Scene.cpp; 1.0 with no storm and no fog.
			float fscale = scale;
			{
				extern float OroGroundShadowFade(const D3DXVECTOR3& posW);
				D3DXVECTOR3 bsl(structure_as[i]->BBox.bs.x, structure_as[i]->BBox.bs.y, structure_as[i]->BBox.bs.z), bsw;
				D3DXVec3TransformCoord(&bsw, &bsl, &mWorld);   // base-local -> camera-relative
				fscale *= OroGroundShadowFade(bsw);
			}
			{
				extern int g_oroDbgStrDrawn, g_oroDbgStrSkip;   // ORO patch (ab) INSTRUMENT
				if (fscale < 0.005f) g_oroDbgStrSkip++; else g_oroDbgStrDrawn++;
			}
			if (fscale < 0.005f) continue;

			structure_as[i]->RenderShadowsEx(fscale, &mProj, &mWorld, &nrml, &param);
		}
	}
}
