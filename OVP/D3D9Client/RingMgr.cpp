// ==============================================================
// RingMgr.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2007 - 2016 Martin Schweiger
//				 2011 - 2106 Jarmo Nikkanen (D3D9Client modification)  
// ==============================================================

// ==============================================================
// class RingManager (implementation)
// ==============================================================

#define D3D_OVERLOADS
#include "RingMgr.h"
#include "D3D9Catalog.h"

using namespace oapi;

void ReleaseTex(LPDIRECT3DTEXTURE9 pTex);



RingManager::RingManager (const vPlanet *vplanet, double inner_rad, double outer_rad)
{
	vp = vplanet;
	irad = inner_rad;
	orad = outer_rad;
	rres = (DWORD)-1;
	tres = 0;
	ntex = 0;
	pTex = NULL;

	for (DWORD i = 0; i < MAXRINGRES; i++) {
		mesh[i] = 0;
		meshORO[i] = 0;   // ORO patch (aj)
		meshNear = 0;     // ORO patch (aj) round 2
		tex[i] = 0;
	}
}

RingManager::~RingManager ()
{
	DWORD i;
	for (i = 0; i < 3; i++)	if (mesh[i]) delete mesh[i];
	for (i = 0; i < 3; i++)	if (meshORO[i]) delete meshORO[i];   // ORO patch (aj)
	if (meshNear) delete meshNear;                                   // ORO patch (aj) round 2
	for (i = 0; i < ntex; i++) ReleaseTex(tex[i]);
	if (pTex) pTex->Release();
}

void RingManager::GlobalInit(D3D9Client *gclient)
{
	gc = gclient;
}

void RingManager::SetMeshRes(DWORD res)
{
	if (res != rres) {
		rres = res;
		if (!mesh[res])	mesh[res] = CreateRing (irad, orad, 8+res*4);
		if (!ntex) ntex = LoadTextures();
		tres = min (rres, ntex-1);
	}
}


DWORD RingManager::LoadTextures ()
{
	char fname[128] = { '\0' };
	char temp[128];
	char path[MAX_PATH] = { '\0' };


	oapiGetObjectName (vp->Object(), fname, ARRAYSIZE(fname));

	LPDIRECT3DDEVICE9 pDev = gc->GetDevice();

	const D3DCAPS9 *caps = gc->GetHardwareCaps();

	int size = max(min((int)caps->MaxTextureWidth, 8192), 2048);

	sprintf_s(temp, ARRAYSIZE(temp), "%s_ring_%d.dds", fname, size);
	if (gc->TexturePath(temp, path) &&
	    D3DXCreateTextureFromFileExA(pDev, path, 0, 0, D3DFMT_FROM_FILE, 0, D3DFMT_FROM_FILE, D3DPOOL_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, &pTex) == S_OK)
	{
		LogAlw("High resolution ring texture loaded [%s]", path);
	}
	
	// Fallback for old method
	strcat_s(fname, ARRAYSIZE(fname), "_ring.tex");
	
	return LoadPlanetTextures(fname, tex, 0, MAXRINGRES);
}

bool RingManager::Render(LPDIRECT3DDEVICE9 dev, D3DXMATRIX &mWorld, bool front)
{
	MATRIX3 grot;
	static D3DXMATRIX imat;
	D3DXVECTOR3 q(mWorld._11, mWorld._21, mWorld._31);
	float scale = D3DXVec3Length(&q);
	
	oapiGetRotationMatrix(vp->Object(), &grot);
	
	VECTOR3 gdir; oapiCameraGlobalDir(&gdir);

	VECTOR3 yaxis =  mul(grot, _V(0,1,0));
	VECTOR3 xaxis = unit(crossp(gdir, yaxis));
	VECTOR3 zaxis = unit(crossp(xaxis, yaxis));

	if (!front) {
		xaxis = -xaxis;
		zaxis = -zaxis;
	}

	D3DXVECTOR3 x(float(xaxis.x), float(xaxis.y), float(xaxis.z)); 
	D3DXVECTOR3 y(float(yaxis.x), float(yaxis.y), float(yaxis.z)); 
	D3DXVECTOR3 z(float(zaxis.x), float(zaxis.y), float(zaxis.z)); 

	D3DXMATRIX World = mWorld;

	x*=scale; y*=scale;	z*=scale;

	D3DMAT_FromAxisT(&World, &x, &y, &z);

	float rad = float(vp->GetSize());

	// ORO patch (aj) part 5 - A STOCK BUG, and the ring has been lit by an UNINITIALISED
	// SUN. RenderRings/RenderRings2 both push the ring mesh's own D3D9Mesh::sunLight into
	// gSun - but NOTHING ever calls SetSunLight on a ring mesh (every other mesh in the
	// client gets one: Scene, VBase, VVessel, vPlanet's own), and neither D3D9Mesh::Null()
	// nor any constructor initialises that member. So gSun.Dir was whatever the heap block
	// happened to hold. With it at zero - which is what a fresh block gives, and what his
	// 2026-09-12 screenshot shows - two things in RingTech/RingTech2 collapse:
	//   the PLANET'S SHADOW: da = dot(normalize(pp), gSun.Dir) = 0, so r = |pp|, which for
	//     any ring fragment is >= irad, far past gRadius[1]; smoothstep returns 1 and sh is
	//     1 everywhere. No shadow on the rings, ever.
	//   the UNLIT FACE: dot(nrmW,CamW) * dot(nrmW,gSun.Dir) = 0, never > 0, so the backside
	//     dimming never applies and the rings stay fully bright seen from the dark side.
	// Both are visible in Orbiter 2016 and were gone in 2024. The planet's own surface is
	// drawn from gc->GetScene()->GetSun() (SurfMgr.cpp:91/145), so taking the ring's sun
	// from the same place makes the ring agree with the globe it surrounds by construction.
	// !! This bit the ORO ring too - RingTechOROPS reads gSun.Dir for its sun obliquity,
	// its lit/unlit split AND the planet shadow, so without this the new shader's first
	// flight would have looked wrong for a reason that was never in the new shader.
	const D3D9Sun* pSun = gc->GetScene()->GetSun();

	// ORO patch (aj): when the addon has pushed a look + profile for this planet, draw the
	// ORO ring - on a finer disc (the stock one's inner edge is a 16-gon), through the
	// per-pixel radial path whatever texture format the planet shipped (this is what puts
	// Uranus's narrow rings on the per-pixel path). The uniforms and the profile texture
	// were bound by vPlanet::Render's bracket, which also serves the planet's shadow pass.
	if (gcGetRingLook(vp->Object())) {
		if (!meshORO[rres]) meshORO[rres] = CreateRing(irad, orad, 64 << rres);   // 64 / 128 / 256
		meshORO[rres]->SetSunLight(pSun);
		meshORO[rres]->RenderRingsORO(&World);
		return true;
	}

	mesh[rres]->SetSunLight(pSun);
	if (pTex) {
		mesh[rres]->RenderRings2(&World, pTex, float(irad)*rad, float(orad)*rad);
	}
	else mesh[rres]->RenderRings(&World, tex[tres]);
	return true;
}

// =======================================================================
// ORO patch (aj) round 2: THE NEAR-FIELD DISC. The ring mesh is 137,000 km across, and a
// fragment's camera-relative position (CamW, what the close-up's grain and specks are
// built from) is interpolated from vertices 100,000 km away in float32: ~8 m of error,
// re-rounded every frame the camera moves. At 25 km that is under a pixel; at 1 km it is
// ten, and flight 5 watched the grain "vibrate left-right" and the specks shake. So the
// near field is drawn on a UNIT half-disc placed by a matrix whose scale is the reach
// (a few tens of km) and whose translation is the camera's foot point on the plane
// (metres): every vertex is small, so every interpolated position is exact to
// millimetres. Drawn twice with the x/z rows negated, the ring manager's own flip, for
// the full disc. Same shader, same bracket: RingTechOROPS takes the radius from the
// camera-relative position plus gCameraPos, never from the mesh, which is what makes a
// disc that knows nothing about the ring's radii draw the right ring.
//
bool RingManager::RenderNearField(LPDIRECT3DDEVICE9 dev, const D3DXMATRIX &W)
{
	if (!gcGetRingLook(vp->Object())) return false;
	if (!meshNear) meshNear = CreateRing(0.0, 1.0, 64);   // a fan: the inner edge collapses to the centre
	meshNear->SetSunLight(gc->GetScene()->GetSun());
	D3DXMATRIX W1 = W, W2 = W;
	W2._11 = -W._11; W2._12 = -W._12; W2._13 = -W._13;
	W2._31 = -W._31; W2._32 = -W._32; W2._33 = -W._33;
	meshNear->RenderRingsORO(&W1);
	meshNear->RenderRingsORO(&W2);
	return true;
}

// =======================================================================
// CreateRing
// Creates mesh for rendering planetary ring system. Creates a ring
// with nsect quadrilaterals. Smoothing the corners of the mesh is
// left to texture transparency. Nsect should be an even number.
// Disc is in xz-plane centered at origin facing up. Size is such that
// a ring of inner radius irad (>=1) and outer radius orad (>irad)
// can be rendered on it.

D3D9Mesh *RingManager::CreateRing(double irad, double orad, int nsect)
{
	int i, j;

	MESHGROUPEX *grp = new MESHGROUPEX; 
	
	memset(grp,0,sizeof(MESHGROUPEX));
	
	int count = nsect/2 + 1;
	grp->nVtx = 2*count;
	grp->nIdx = 6*(count-1);
	grp->Idx = new WORD[grp->nIdx+12];
	grp->Vtx = new NTVERTEX[grp->nVtx+4];
	grp->TexIdx = 1;
	
	NTVERTEX *Vtx = grp->Vtx;
	WORD *Idx = grp->Idx;

	double alpha = PI/(double)nsect;
	float nrad = (float)(orad/cos(alpha)); // distance for outer nodes
	float ir = (float)irad;
	float fo = (float)(0.5*(1.0-orad/nrad));
	float fi = (float)(0.5*(1.0-irad/nrad));

	for (i = j = 0; i < count; i++) {
		double phi = i*2.0*alpha;
		float cosp = (float)cos(phi), sinp = (float)sin(phi);
		Vtx[i*2].x = nrad*cosp;  Vtx[i*2+1].x = ir*cosp;
		Vtx[i*2].z = nrad*sinp;  Vtx[i*2+1].z = ir*sinp;
		Vtx[i*2].y = Vtx[i*2+1].y = 0.0;
		Vtx[i*2].nx = Vtx[i*2+1].nx = Vtx[i*2].nz = Vtx[i*2+1].nz = 0.0;
		Vtx[i*2].ny = Vtx[i*2+1].ny = 1.0;

		if (!(i&1)) Vtx[i*2].tu = fo,  Vtx[i*2+1].tu = fi;  //fac;
		else        Vtx[i*2].tu = 1.0f-fo,  Vtx[i*2+1].tu = 1.0f-fi; //1.0f-fac;

		//Vtx[i*2].tv = 0.05f, Vtx[i*2+1].tv = 1.00f;
		Vtx[i*2].tv = 0.0f, Vtx[i*2+1].tv = 1.00f;

		if ((DWORD)j<=grp->nIdx-6) {
			Idx[j++] = i*2;
			Idx[j++] = i*2+1;
			Idx[j++] = i*2+2;
			Idx[j++] = i*2+3;
			Idx[j++] = i*2+2;
			Idx[j++] = i*2+1;
		}
	}

	MATERIAL mat = {{1,1,1,1},{0,0,0,1},{0,0,0,1},{0,0,0,0},20.0f};

	D3D9Mesh *msh = new D3D9Mesh(grp, &mat, NULL);

	delete grp->Idx;
	delete grp->Vtx;
	delete grp;
	return msh;
}

oapi::D3D9Client *RingManager::gc = 0;
