// ==============================================================
// Particle.cpp
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006 - 2016 Martin Schweiger
//				 2011 - 2016 Jarmo Nikkanen (D3D9Client modification)
// ==============================================================

#define STRICT 1

#include "Particle.h"
#include "Scene.h"
#include "D3D9Surface.h"
#include "D3D9Config.h"
#include "VPlanet.h"        // ORO patch (x): SunLightColor - the hull's own dawn tint
#include <stdio.h>

static bool needsetup = true;

static VERTEX_XYZ_TEX evtx[MAXPARTICLE*4]; // vertex list for emissive trail (no normals)
static NTVERTEX       dvtx[MAXPARTICLE*4]; // vertex list for diffusive trail
static WORD            idx[MAXPARTICLE*6]; // index list

static float tu[8*4] = {0.0,0.5,0.5,0.0, 0.5,1.0,1.0,0.5, 0.0,0.5,0.5,0.0, 0.5,1.0,1.0,0.5,
						0.5,0.5,0.0,0.0, 1.0,1.0,0.5,0.5, 0.5,0.5,0.0,0.0, 1.0,1.0,0.5,0.5};

static float tv[8*4] = {0.0,0.0,0.5,0.5, 0.0,0.0,0.5,0.5, 0.5,0.5,1.0,1.0, 0.5,0.5,1.0,1.0,
						0.0,0.5,0.5,0.0, 0.0,0.5,0.5,0.0, 0.5,1.0,1.0,0.5, 0.5,1.0,1.0,0.5};

using namespace oapi;

static PARTICLESTREAMSPEC DefaultParticleStreamSpec = {
	0,                            // flags
	8.0,                          // creation size
	0.5,                          // creation rate
	100,                          // emission velocity
	0.3,                          // velocity randomisation
	8.0,                          // lifetime
	0.5,                          // growth rate
	3.0,                          // atmospheric slowdown
	PARTICLESTREAMSPEC::DIFFUSE,  // render lighting method
	PARTICLESTREAMSPEC::LVL_SQRT, // mapping from level to alpha
	0, 1,						  // lmin and lmax levels for mapping
	PARTICLESTREAMSPEC::ATM_PLOG, // mapping from atmosphere to alpha
	1e-4, 1						  // amin and amax densities for mapping
};

SURFHANDLE D3D9ParticleStream::deftex = 0;
SURFHANDLE D3D9ParticleStream::deftexems = 0;
bool D3D9ParticleStream::bShadows = false;

D3D9ParticleStream::D3D9ParticleStream(GraphicsClient *_gc, PARTICLESTREAMSPEC *pss) : ParticleStream (_gc, pss), D3D9Effect()
{
	pGC = (D3D9Client*)_gc;

	//cam_ref = &gc->GetScene()->GetCameraGPos();
	//src_ref = 0;
	//src_ofs = _V(0,0,0);

	interval = 0.1;
	SetSpecs (pss ? pss : &DefaultParticleStreamSpec);
	t0 = oapiGetSimTime();
	//active = false;
	pfirst = NULL;
	plast = NULL;
	np = 0;
	D3DMAT_Identity(&mWorld);

	if (needsetup) {
		int i, j, k, r, ofs;
		for (i = j = 0; i < MAXPARTICLE; i++) {
			ofs = i*4;
			idx[j++] = ofs;
			idx[j++] = ofs+2;
			idx[j++] = ofs+1;
			idx[j++] = ofs+2;
			idx[j++] = ofs;
			idx[j++] = ofs+3;
			r = rand() & 7;
			for (k = 0; k < 4; k++) {
				evtx[ofs+k].tu = dvtx[ofs+k].tu = tu[r*4+k];
				evtx[ofs+k].tv = dvtx[ofs+k].tv = tv[r*4+k];
			}
		}
		needsetup = false;
	}
}

D3D9ParticleStream::~D3D9ParticleStream()
{
	while (pfirst) {
		ParticleSpec *tmp = pfirst;
		pfirst = pfirst->next;
		delete tmp;
	}
}

void D3D9ParticleStream::GlobalInit (oapi::D3D9Client *gclient)
{
	deftex = SURFACE(gclient->clbkLoadTexture("Contrail1.dds", 0));
	deftexems = SURFACE(gclient->clbkLoadTexture("Contrail1.dds", 0));
	bShadows = *(bool*)gclient->GetConfigParam (CFGPRM_VESSELSHADOWS);
}

void D3D9ParticleStream::GlobalExit ()
{
	DELETE_SURFACE(deftex);
	DELETE_SURFACE(deftexems);
}

void D3D9ParticleStream::SetSpecs(PARTICLESTREAMSPEC *pss)
{
	SetParticleHalflife (pss->lifetime);
	size0 = pss->srcsize;
	speed = pss->v0;
	vrand = pss->srcspread;
	alpha = pss->growthrate;
	beta  = pss->atmslowdown;
	pdensity = pss->srcrate;
	diffuse = (pss->ltype == PARTICLESTREAMSPEC::DIFFUSE);
	lmap  = pss->levelmap;
	lmin  = pss->lmin, lmax = pss->lmax;
	amap  = pss->atmsmap;
	amin  = pss->amin;

	switch (amap) {
		case PARTICLESTREAMSPEC::ATM_PLIN: afac = 1.0/(pss->amax-amin); break;
		case PARTICLESTREAMSPEC::ATM_PLOG: afac = 1.0/log(pss->amax/amin); break;
	}

	if (diffuse) tex = (SURFACE(pss->tex) ? SURFACE(pss->tex) : deftex);
	else		 tex = (SURFACE(pss->tex) ? SURFACE(pss->tex) : deftexems);
}

void D3D9ParticleStream::SetParticleHalflife (double pht)
{
	exp_rate = RAND_MAX/pht;
	stride = max (1, min (20,(int)pht));
	ipht2 = 0.5/pht;
}

void D3D9ParticleStream::SetObserverRef (const VECTOR3 *cam)
{
	LogErr("D3D9ParticleStream::SetObserverRef() NOT IMPLEMENTED");
	//cam_ref = cam;
}

void D3D9ParticleStream::SetSourceRef (const VECTOR3 *src)
{
	LogErr("D3D9ParticleStream::SetSourceRef() NOT IMPLEMENTED");
	//src_ref = src;
}

void D3D9ParticleStream::SetSourceOffset (const VECTOR3 &ofs)
{
	LogErr("D3D9ParticleStream::SetSourceOffset() NOT IMPLEMENTED");
	//src_ofs = ofs;
}

void D3D9ParticleStream::SetIntensityLevelRef (double *lvl)
{
	level = lvl;
}

double D3D9ParticleStream::Level2Alpha(double level) const
{
	switch (lmap) {
		case PARTICLESTREAMSPEC::LVL_FLAT:	return lmin;
		case PARTICLESTREAMSPEC::LVL_LIN:	return level;
		case PARTICLESTREAMSPEC::LVL_SQRT:	return sqrt (level);
		case PARTICLESTREAMSPEC::LVL_PLIN:	return max (0.0, min (1.0, (level-lmin)/(lmax-lmin)));
		case PARTICLESTREAMSPEC::LVL_PSQRT:	return (level <= lmin ? 0 : level >= lmax ? 1 : sqrt ((level-lmin)/(lmax-lmin)));
	}
	return 0; // should not happen
}

double D3D9ParticleStream::Atm2Alpha(double prm) const
{
	switch (amap) {
		case PARTICLESTREAMSPEC::ATM_FLAT:	return amin;
		case PARTICLESTREAMSPEC::ATM_PLIN:	return max (0.0, min (1.0, (prm-amin)*afac));
		case PARTICLESTREAMSPEC::ATM_PLOG:	return max (0.0, min (1.0, log(prm/amin)*afac));
	}
	return 0; // should not happen
}

ParticleSpec *D3D9ParticleStream::CreateParticle (const VECTOR3 &pos, const VECTOR3 &vel, double size, double alpha)
{
	ParticleSpec *p = new ParticleSpec;
	p->pos = pos;
	p->vel = vel;
	p->size = size;
	p->alpha0 = alpha;
	p->t0 = oapiGetSimTime();
	p->texidx = (rand() & 7) * 4;
	p->flag = 0;
	p->next = NULL;
	p->prev = plast;
	if (plast) plast->next = p;
	else       pfirst = p;
	plast = p;
	np++;

	if (np > MAXPARTICLE)
		DeleteParticle (pfirst);

	return p;
}

void D3D9ParticleStream::DeleteParticle (ParticleSpec *p)
{
	if (p->prev) p->prev->next = p->next;
	else         pfirst = p->next;
	if (p->next) p->next->prev = p->prev;
	else         plast = p->prev;
	delete p;
	np--;
}

void D3D9ParticleStream::Update ()
{
	ParticleSpec *p, *tmp;
	double dt = oapiGetSimStep();

	for (p = pfirst; p;) {
		if (dt * exp_rate > rand()) {
			tmp = p;
			p = p->next;
			DeleteParticle (tmp);
		} else {
			p->pos += p->vel*dt;
			p = p->next;
		}
	}
}

void D3D9ParticleStream::Timejump()
{
	while (pfirst) {
		ParticleSpec *tmp = pfirst;
		pfirst = pfirst->next;
		delete tmp;
	}
	pfirst = NULL;
	plast = NULL;
	np = 0;
	t0 = oapiGetSimTime();
}

// ORO patch (y): rebuild the PARTICLESTREAMSPEC this stream was constructed from.
// Everything SetSpecs derived inverts exactly; lifetime comes back out of ipht2
// (SetParticleHalflife stores 0.5/lifetime) and amax out of afac. tex is reported
// NULL - the texture handle is client-private and the consumer picks its own.
void D3D9ParticleStream::OroGetSpec(PARTICLESTREAMSPEC* out, VECTOR3* outPos, VECTOR3* outDir) const
{
	if (outPos) *outPos = pos ? *pos : _V(0,0,0);
	if (outDir) *outDir = dir ? *dir : _V(0,0,0);
	if (!out) return;
	memset(out, 0, sizeof(PARTICLESTREAMSPEC));
	out->srcsize     = size0;
	out->srcrate     = pdensity;
	out->v0          = speed;
	out->srcspread   = vrand;
	out->lifetime    = (ipht2 > 0.0) ? 0.5 / ipht2 : 0.0;
	out->growthrate  = alpha;
	out->atmslowdown = beta;
	out->ltype       = diffuse ? PARTICLESTREAMSPEC::DIFFUSE : PARTICLESTREAMSPEC::EMISSIVE;
	out->levelmap    = lmap;
	out->lmin        = lmin;
	out->lmax        = lmax;
	out->atmsmap     = amap;
	out->amin        = amin;
	switch (amap) {
		case PARTICLESTREAMSPEC::ATM_PLIN: out->amax = (afac != 0.0) ? amin + 1.0/afac      : amin; break;
		case PARTICLESTREAMSPEC::ATM_PLOG: out->amax = (afac != 0.0) ? amin * exp(1.0/afac) : amin; break;
		default:                           out->amax = 1.0; break;
	}
	out->tex = NULL;
}

void D3D9ParticleStream::SetDParticleCoords(const VECTOR3 &ppos, double scale, NTVERTEX *vtx)
{
	VECTOR3 cdir = ppos;
	double ux, uy, uz, vx, vy, vz, len;
	if (cdir.y || cdir.z) {
		ux =  0;
		uy =  cdir.z;
		uz = -cdir.y;
		len = scale / sqrt (uy*uy + uz*uz);
		uy *= len;
		uz *= len;
		vx = cdir.y*cdir.y + cdir.z*cdir.z;
		vy = -cdir.x*cdir.y;
		vz = -cdir.x*cdir.z;
		len = scale / sqrt(vx*vx + vy*vy + vz*vz);
		vx *= len;
		vy *= len;
		vz *= len;
	} else {
		ux = 0;
		uy = scale;
		uz = 0;
		vx = 0;
		vy = 0;
		vz = scale;
	}
	vtx[0].x = (float)(ppos.x-ux-vx);
	vtx[0].y = (float)(ppos.y-uy-vy);
	vtx[0].z = (float)(ppos.z-uz-vz);
	vtx[1].x = (float)(ppos.x-ux+vx);
	vtx[1].y = (float)(ppos.y-uy+vy);
	vtx[1].z = (float)(ppos.z-uz+vz);
	vtx[2].x = (float)(ppos.x+ux+vx);
	vtx[2].y = (float)(ppos.y+uy+vy);
	vtx[2].z = (float)(ppos.z+uz+vz);
	vtx[3].x = (float)(ppos.x+ux-vx);
	vtx[3].y = (float)(ppos.y+uy-vy);
	vtx[3].z = (float)(ppos.z+uz-vz);
}

void D3D9ParticleStream::SetEParticleCoords (const VECTOR3 &ppos, double scale, VERTEX_XYZ_TEX *vtx)
{
	VECTOR3 cdir = ppos;
	double ux, uy, uz, vx, vy, vz, len;
	if (cdir.y || cdir.z) {
		ux =  0;
		uy =  cdir.z;
		uz = -cdir.y;
		len = scale / sqrt (uy*uy + uz*uz);
		uy *= len;
		uz *= len;
		vx = cdir.y*cdir.y + cdir.z*cdir.z;
		vy = -cdir.x*cdir.y;
		vz = -cdir.x*cdir.z;
		len = scale / sqrt(vx*vx + vy*vy + vz*vz);
		vx *= len;
		vy *= len;
		vz *= len;
	} else {
		ux = 0;
		uy = scale;
		uz = 0;
		vx = 0;
		vy = 0;
		vz = scale;
	}
	vtx[0].x = (float)(ppos.x-ux-vx);
	vtx[0].y = (float)(ppos.y-uy-vy);
	vtx[0].z = (float)(ppos.z-uz-vz);
	vtx[1].x = (float)(ppos.x-ux+vx);
	vtx[1].y = (float)(ppos.y-uy+vy);
	vtx[1].z = (float)(ppos.z-uz+vz);
	vtx[2].x = (float)(ppos.x+ux+vx);
	vtx[2].y = (float)(ppos.y+uy+vy);
	vtx[2].z = (float)(ppos.z+uz+vz);
	vtx[3].x = (float)(ppos.x+ux-vx);
	vtx[3].y = (float)(ppos.y+uy-vy);
	vtx[3].z = (float)(ppos.z+uz-vz);
}

void D3D9ParticleStream::SetShadowCoords(const VECTOR3 &ppos, const VECTOR3 &cdir, double scale, VERTEX_XYZ_TEX *vtx)
{
	double ux, uy, uz, vx, vy, vz, len;

	if (cdir.y || cdir.z) {
		ux =  0;
		uy =  cdir.z;
		uz = -cdir.y;
		len = scale / sqrt (uy*uy + uz*uz);
		uy *= len;
		uz *= len;
		vx = cdir.y*cdir.y + cdir.z*cdir.z;
		vy = -cdir.x*cdir.y;
		vz = -cdir.x*cdir.z;
		len = scale / sqrt(vx*vx + vy*vy + vz*vz);
		vx *= len;
		vy *= len;
		vz *= len;
	}
	else {
		ux = 0;
		uy = scale;
		uz = 0;
		vx = 0;
		vy = 0;
		vz = scale;
	}
	vtx[0].x = (float)(ppos.x-ux-vx);
	vtx[0].y = (float)(ppos.y-uy-vy);
	vtx[0].z = (float)(ppos.z-uz-vz);
	vtx[1].x = (float)(ppos.x-ux+vx);
	vtx[1].y = (float)(ppos.y-uy+vy);
	vtx[1].z = (float)(ppos.z-uz+vz);
	vtx[2].x = (float)(ppos.x+ux+vx);
	vtx[2].y = (float)(ppos.y+uy+vy);
	vtx[2].z = (float)(ppos.z+uz+vz);
	vtx[3].x = (float)(ppos.x+ux-vx);
	vtx[3].y = (float)(ppos.y+uy-vy);
	vtx[3].z = (float)(ppos.z+uz-vz);
}

void D3D9ParticleStream::CalcNormals(const VECTOR3 &ppos, NTVERTEX *vtx)
{
	VECTOR3 cdir = unit (ppos);
	double ux, uy, uz, vx, vy, vz, len;
	if (cdir.y || cdir.z) {
		ux =  0;
		uy =  cdir.z;
		uz = -cdir.y;
		len = 3.0 / sqrt (uy*uy + uz*uz);
		uy *= len;
		uz *= len;
		vx = cdir.y*cdir.y + cdir.z*cdir.z;
		vy = -cdir.x*cdir.y;
		vz = -cdir.x*cdir.z;
		len = 3.0 / sqrt(vx*vx + vy*vy + vz*vz);
		vx *= len;
		vy *= len;
		vz *= len;
	}
	else {
		ux = 0;
		uy = 1.0;
		uz = 0;
		vx = 0;
		vy = 0;
		vz = 1.0;
	}
	static float scale = (float)(1.0/sqrt(19.0));
	vtx[0].nx = scale*(float)(-cdir.x-ux-vx);
	vtx[0].ny = scale*(float)(-cdir.y-uy-vy);
	vtx[0].nz = scale*(float)(-cdir.z-uz-vz);
	vtx[1].nx = scale*(float)(-cdir.x-ux+vx);
	vtx[1].ny = scale*(float)(-cdir.y-uy+vy);
	vtx[1].nz = scale*(float)(-cdir.z-uz+vz);
	vtx[2].nx = scale*(float)(-cdir.x+ux+vx);
	vtx[2].ny = scale*(float)(-cdir.y+uy+vy);
	vtx[2].nz = scale*(float)(-cdir.z+uz+vz);
	vtx[3].nx = scale*(float)(-cdir.x+ux-vx);
	vtx[3].ny = scale*(float)(-cdir.y+uy-vy);
	vtx[3].nz = scale*(float)(-cdir.z+uz-vz);
}

void D3D9ParticleStream::Render(LPDIRECT3DDEVICE9 dev)
{
	if (!pfirst) return;
	if (diffuse) RenderDiffuse(dev);
	else         RenderEmissive(dev);
}

// ------------------------------------------------------------------------------------
// ORO patch (x): lighting for DIFFUSE particle streams.
// Stock disabled the shader's sun term (Particle.fx hardcodes light = 1.0 with the
// original N.L commented out), so smoke rendered fully daylight-lit on the night side
// of a planet. Lighting a translucent billboard is a question about POSITION, not
// orientation - "does sunlight reach this particle?" - so the value is computed on the
// CPU per particle and carried to the shader in the normal channel, which the shader
// no longer reads as a normal. A fully sunlit particle evaluates to exactly 1.0, so
// the daytime look is bit-identical to stock. EMISSIVE streams are untouched.
// ------------------------------------------------------------------------------------
static const double ORO_PRT_TWILIGHT_LO = -0.0175;	// sin(-1 deg): fully dark this far below the (altitude-depressed) horizon
static const double ORO_PRT_TWILIGHT_HI =  0.0872;	// sin(+5 deg): full daylight above this sun elevation
static const double ORO_PRT_FLAME_REACH = 10.0;		// the engine flame fully lights smoke within this many src-sizes (inverse-square beyond)
// The sunrise/sunset TINT: the sun share warms toward gold where the light crossed a
// long air path, dim and confined the way dawn-launch photography shows it.
// ⚠️ A first version sampled vPlanet::GetObjectAtmoParams at the stream's head and
// tail and lerped the normalized hue by age; one flight killed it - the tail
// anchor's low sun normalized to SATURATED red at FULL sun-share brightness and the
// linear age lerp smeared that band across half the trail ("a large reddish band").
// A rising sun is red AND dim, and the band is an altitude slice, not an age
// fraction. ⚠️ A second version keyed the tint to the VISIBILITY coordinate
// (sinel - sinhrz); the STS-108 reference killed that too - at altitude the
// depressed horizon makes a low sun VISIBLE, it does not make its light WHITE, so
// the whole sunlit column stays gold while the sun is low. The tint therefore keys
// on the TRUE sun elevation (the air path), and only the visibility keeps the
// horizon-dip term. Airless worlds get no tint at all: the reddening IS atmospheric
// extinction.
static const double ORO_PRT_TINT_LO  = 0.0;			// sin(0 deg): the deepest tint at/below the TRUE horizon
static const double ORO_PRT_TINT_HI  = 0.156;		// sin(+9 deg): pure white at/above this TRUE sun elevation
// THREE tint stops since 2026-08-30 (his ask): the first direct rays cross the
// longest air path and are the REDDEST, turning GOLD as the sun climbs, then white.
// Red channel stays 1 throughout; the stops are g/b pairs. ⚠️ The g:b RATIO is the
// hue: ~2:1 read as SALMON on his STS-108 comparison; warm needs g >= 3x b.
static const double ORO_PRT_TINT_MID = 0.40;		// band fraction where the GOLD stop peaks
static const double ORO_PRT_TINT_RG  = 0.30;		// green at the RED stop (dawn's first light)
static const double ORO_PRT_TINT_RB  = 0.10;		// blue at the RED stop
static const double ORO_PRT_TINT_G   = 0.62;		// green at the GOLD stop (flown 2026-08-29)
static const double ORO_PRT_TINT_B   = 0.18;		// blue at the GOLD stop
// ⚠️ An altitude term (grazing rays cross thinner air aloft) and a 0.6 strength
// softener were BOTH built and REVERTED on his next flight (2026-08-30): "I actually
// prefer the previous setting. This looks like it goes straight to yellow." The full
// three-stop bands below are the flown-and-kept look; the taste dial is the
// Launchpad's Particle lighting mode (Config->ParticleLight), not a saturation knob.
//
// DIRECTIONAL SPRITE SHADING (patch (x) stage 2, 2026-08-30 - his reference photo:
// the contrail is visibly lit FROM A SIDE, while a single per-particle scalar lights
// every puff from nowhere in particular). In Orbiter's streams the column's visible
// width IS the sprite's width (srcspread is small; the width comes from growthrate),
// so shading one side of each billboard is shading one side of the column. Each
// corner gets a pseudo-normal - the front hemisphere of a sphere bulging at the
// camera - and a WRAP lambert (a condensation cloud scatters; a hard terminator
// across a puff reads as a painted ball). The gradient rides the existing per-vertex
// light channel, so the rasterizer interpolates it across the quad and this stays a
// DLL-only change - Particle.fx is untouched.
// TWO-LIGHT SPLIT: only the SUN term takes the gradient; the ambient floor and the
// engine flame stay omnidirectional, and in colour mode over an atmosphere the
// ambient gets a mild COOL cast - a daytime shadow side is SKY-lit, not black.
// Airless worlds keep a neutral ambient: no blue sky fills the shadows there.
// Mode 0 stays bit-exact stock. The older note's "fully sunlit evaluates to exactly
// 1.0" is now true per PARTICLE and deliberately no longer per PIXEL: modes 1/2
// shade the face, which is the entire point.
static const double ORO_PRT_DIR_WRAP = 0.60;	// wrap width: 0 = hard lambert, larger = flatter
static const double ORO_PRT_DIR_ZC   = 0.80;	// view component of the corner pseudo-normal
                                            	//   (how strongly the hemisphere bulges at you)
static const double ORO_PRT_AMB_R    = 0.92;	// the cool sky cast on the ambient term
static const double ORO_PRT_AMB_G    = 0.97;	//   (colour mode + atmosphere only)
static const double ORO_PRT_AMB_B    = 1.05;
// ROUND 5 (2026-08-30, his verdicts on the hull-matched tint): the smoke runs a
// stop AHEAD of the hull (evaluate the hue at a slightly higher sun elevation, so
// when the hull turns rose the column is already orange), the hue is DEEPENED
// (a translucent grey column washes a tint out), and where the hue is deep the
// sun term OVERDRIVES past 1.0 into the fp16 chain - the diffuse pixel shader
// multiplies the light through unclamped, so the Light glow post-process blooms
// it: his "slight bloom". All three scale with tint depth, so neutral daylight
// and the flame-lit engine steam are touched by exactly nothing.
// ROUND 6: all three are LAUNCHPAD SLIDERS now (his ask - tune without a
// recompile): Config->ParticleTintLead (0..0.20 sin-el), Config->ParticleTintSat
// (1..4), Config->ParticleTintBloom (1..3). Read per call below. Defaults are
// HIS settled positions from the dawn-ascent tuning (0.10 / 1.6 / 2.34,
// 2026-08-30 - "Now THAT is what I'm talking about"); round 5's first guesses
// were 0.05 / 1.6 / 1.30, too shy on lead and bloom because the tinted phase
// coincides with the dim phase of the twilight ramp.

void D3D9ParticleStream::RenderDiffuse(LPDIRECT3DDEVICE9 dev)
{
	static D3DMATERIAL9 smokemat = { // emissive material for engine exhaust
		{1,1,1,1},
		{0,0,0,1},
		{0,0,0,1},
		{0.2f,0.2f,0.2f,1},
		0.0
	};
	UINT numPasses=0;
	ParticleSpec *p;
	int i0, j, n, stride = np/16+1;
	float *u, *v;
	NTVERTEX *vtx;

	VECTOR3 camera_gpos = pGC->GetScene()->GetCameraGPos();

	// ORO patch (x): CalcNormals retired - the normal channel now carries the lighting
	// value (see below) and the shader no longer reads a normal from it.
	// CalcNormals(plast->pos - camera_gpos, dvtx);

	// ORO patch (x): per-frame lighting context. The planet is the camera's proxy body
	// (particles are only ever watched from nearby); sun-at-the-global-origin is the
	// same approximation RenderGroundShadow below has always used. The ambient floor is
	// the user's Launchpad ambient, scaled exactly as vVessel::ModLighting scales it,
	// so night smoke settles at the same floor the night hull does. The engine flame
	// lights nearby smoke by DISTANCE FROM THE SOURCE (the newest particle) - reference:
	// night-launch photography; a slow climber keeps its ground cloud lit because the
	// cloud stays near the flame, and engine shutdown kills the glow through level.
	OBJHANDLE hPln = oapiCameraProxyGbody();
	if (hPln && oapiGetObjectType(hPln) == OBJTP_STAR) hPln = NULL;	// camera in deep space: no occluder, stay lit
	VECTOR3 pp = _V(0,0,0); double Rp = 1.0;
	if (hPln) { oapiGetGlobalPos(hPln, &pp); Rp = oapiGetSize(hPln); }
	// ORO patch (x) round 4: the planet VISUAL, for SunLightColor - the same
	// extinction curve that tints the HULL through a dawn. NULL = the fallback band.
	vPlanet* vPl = NULL;
	if (hPln) {
		vObject* vo = pGC->GetScene()->GetVisObject(hPln);
		if (vo && vo->Type() == OBJTP_PLANET) vPl = (vPlanet*)vo;
	}
	double amb = 0.0039 * (double)(*(DWORD*)pGC->GetConfigParam(CFGPRM_AMBIENTLEVEL));
	if (amb > 1.0) amb = 1.0;
	double lvl = level ? *level : 0.0;
	VECTOR3 srcpos = plast->pos;
	double flameR = ORO_PRT_FLAME_REACH * size0;
	if (flameR < 1.0) flameR = 1.0;

	// ORO patch (x): the Launchpad's "Particle lighting" mode (his design, 2026-08-30):
	// 0 = Off, bit-exact stock (diffuse stays fully lit, flame and all skipped);
	// 1 = Brightness only; 2 = Brightness + colour. The sunrise/sunset tint also
	// applies only where there is AIR to redden the light.
	const int  plm    = Config->ParticleLight;
	const bool hasAtm = (hPln && plm >= 2 && oapiGetPlanetAtmConstants(hPln) != NULL);
	// ORO patch (x) round 6: the three dawn-tint dials, now Launchpad sliders
	const double tintLead = Config->ParticleTintLead;
	const double tintSat  = Config->ParticleTintSat;
	const double tintOver = Config->ParticleTintBloom;

	HR(dev->SetVertexDeclaration(pNTVertexDecl));
	HR(FX->SetTechnique(eDiffuseTech));
	HR(FX->SetMatrix(eW, &mWorld));

	if (tex) HR(FX->SetTexture(eTex0, SURFACE(tex)->GetTexture()));

	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));

	for (p = pfirst, vtx = dvtx, n = i0 = 0; p; p = p->next) {

		SetDParticleCoords(p->pos - camera_gpos, p->size, vtx);

		u = tu + p->texidx;
		v = tv + p->texidx;

		// ORO patch (x): sun visibility at THIS particle - per particle, so a long
		// trail can straddle the terminator (lit head, dark tail). The horizon the sun
		// sets behind dips with altitude, which is what keeps an orbital trail lit past
		// the ground terminator and darkens it at the correct shadow entry. Round 2:
		// the sun SHARE warms toward amber over the bottom of the same twilight ramp
		// (thin band, already-dimmed light - see the tunables note); the ambient floor
		// and the engine flame stay white.
		float lR = 1.0f, lG = 1.0f, lB = 1.0f;
		// stage 2 (the two-light split + per-corner gradient - see below): the sun
		// term and the ambient/flame terms kept apart so only the sun can be given
		// a direction. grad stays false where no sunlight reaches the particle.
		bool    grad = false;
		double  ambR = 0.0, ambG = 0.0, ambB = 0.0;
		double  dirR = 0.0, dirG = 0.0, dirB = 0.0;
		double  flame = 0.0;
		double  dOv = 0.0;			// round 5: sunlit bloom excess factor (0 at white)
		VECTOR3 sd = _V(0, 0, 1);
		if (hPln && plm > 0) {
			VECTOR3 rp = p->pos - pp;
			double r  = length(rp);     if (r  < 1.0) r  = 1.0;
			double rs = length(p->pos); if (rs < 1.0) rs = 1.0;
			double sinel  = -dotp(rp, p->pos) / (r * rs);			// sin of sun elevation (sun at the origin)
			double q      = 1.0 - (Rp*Rp)/(r*r);
			double sinhrz = (q > 0.0) ? -sqrt(q) : 0.0;				// sin of the depressed horizon at this altitude
			double t = (sinel - sinhrz - ORO_PRT_TWILIGHT_LO) / (ORO_PRT_TWILIGHT_HI - ORO_PRT_TWILIGHT_LO);
			if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
			double sunsh = (t*t*(3.0 - 2.0*t)) * (1.0 - amb);		// the sun's share over the ambient floor
			double hr = 1.0, hg = 1.0, hb = 1.0;					// tint: the sun's hue at this particle
			if (hasAtm && vPl) {
				// THE HULL'S OWN DAWN (round 4, 2026-08-30 - his four-shot ascent:
				// hull rose -> blood-orange -> gold -> cream while the smoke stayed
				// grey, and his spec: "follow the same convention as the spacecraft
				// body"). SunLightColor is the exact extinction curve that
				// GetObjectAtmoParams feeds the vessel's sunlight, evaluated with
				// two numbers this loop already has (sun elevation + altitude), so
				// hull and smoke read the SAME sun by construction - at every
				// altitude slice of a long trail, which the hand-tuned band could
				// never do. Normalized by its max component, as the hull's cap
				// effectively does in every sunlit regime; the MAGNITUDE stays with
				// the twilight ramp above, which is what keeps the first Color
				// experiment's "saturated red at full brightness" dead.
				// ⚠️ HIS CLARIFICATION, load-bearing: the tint lives in the SUN term
				// ONLY. Near the engines the FLAME adds white light over it, so the
				// steam by the nozzles keeps its own colour and hands over to the
				// sunlit hue downstream, through the smoothed flame tail.
				double altp = r - Rp; if (altp < 0.0) altp = 0.0;
				// round 5: the hue is read a little AHEAD of the hull (his "shift the
				// spectrum to the right" - when the hull turns rose the column already
				// shows orange), and DEEPENED around white - a thin grey medium washes
				// a tint out, so the hull's own hue always reads paler on the smoke
				// than on the skin it matches. Deepening around 1.0 leaves the max
				// channel alone, so the normalization's meaning survives.
				FVECTOR3 sc = vPl->SunLightColor((float)(-(sinel + tintLead)), (float)altp);
				float mx = sc.MaxRGB();
				if (mx > 1e-6f) {
					sc = sc / mx;
					hr = 1.0 + tintSat * ((double)sc.r - 1.0); if (hr < 0.0) hr = 0.0;
					hg = 1.0 + tintSat * ((double)sc.g - 1.0); if (hg < 0.0) hg = 0.0;
					hb = 1.0 + tintSat * ((double)sc.b - 1.0); if (hb < 0.0) hb = 0.0;
				}
			}
			else if (hasAtm && sinel < ORO_PRT_TINT_HI) {
				// The hand-tuned three-stop band survives as the FALLBACK, for a
				// proxy body with no planet visual to ask.
				// TRUE elevation, not the visibility coordinate - see the tunables
				// note: a dipped horizon makes a low sun visible, not white. Three
				// stops (red -> gold -> white), both segments smoothstepped and flat
				// at the joint, so the run has no visible seam.
				double tc = (sinel - ORO_PRT_TINT_LO) / (ORO_PRT_TINT_HI - ORO_PRT_TINT_LO);
				if (tc < 0.0) tc = 0.0;
				if (tc < ORO_PRT_TINT_MID) {						// red -> gold
					double s = tc / ORO_PRT_TINT_MID;
					s = s * s * (3.0 - 2.0 * s);
					hg = ORO_PRT_TINT_RG + (ORO_PRT_TINT_G - ORO_PRT_TINT_RG) * s;
					hb = ORO_PRT_TINT_RB + (ORO_PRT_TINT_B - ORO_PRT_TINT_RB) * s;
				} else {											// gold -> white
					double s = (tc - ORO_PRT_TINT_MID) / (1.0 - ORO_PRT_TINT_MID);
					s = s * s * (3.0 - 2.0 * s);
					hg = ORO_PRT_TINT_G + (1.0 - ORO_PRT_TINT_G) * s;
					hb = ORO_PRT_TINT_B + (1.0 - ORO_PRT_TINT_B) * s;
				}
			}
			// STAGE 2 - the two-light split (see the tunables note above): the sun
			// term takes the per-corner gradient below, the ambient floor and the
			// engine flame stay omnidirectional. In colour mode over an atmosphere
			// the ambient goes mildly cool - the daytime shadow side is SKY-lit.
			ambR = amb; ambG = amb; ambB = amb;
			if (hasAtm) {
				ambR *= ORO_PRT_AMB_R;
				ambG *= ORO_PRT_AMB_G;
				ambB *= ORO_PRT_AMB_B;
				if (ambB > 1.0) ambB = 1.0;
			}
			dirR = sunsh * hr; dirG = sunsh * hg; dirB = sunsh * hb;
			{
				// round 5, the "slight bloom" (his ask): where the hue is DEEP the
				// sun term is allowed past the 1.0 ceiling (up to the Dawn tint
				// bloom slider's factor), into the fp16 chain - the diffuse pixel
				// shader multiplies the light through unclamped, so the Light glow
				// post-process blooms it. Scaled by tint DEPTH: white daylight
				// overdrives by exactly nothing, so the approved daytime look and
				// the engine steam are untouched. Note the excess rides the SUN
				// term, which the twilight ramp holds low exactly when the hue is
				// deep - which is why the slider has real headroom (x3).
				double tintAmt = 1.0 - (hg < hb ? hg : hb);
				if (tintAmt < 0.0) tintAmt = 0.0; else if (tintAmt > 1.0) tintAmt = 1.0;
				dOv = (tintOver - 1.0) * tintAmt;
			}
			if (lvl > 0.0) {
				// SMOOTHED 2026-08-30 (his round: "from the white lit smoke to the
				// orange one in a very short, almost instant cutoff"). Full within
				// the reach as ever; beyond it a LORENTZIAN tail measured from the
				// reach's edge - value AND slope continuous at the boundary, and
				// still half strength one reach further out, where the truncated
				// inverse-square had already fallen to a quarter. Near-engine white
				// and the far tail are unchanged; the night glow carries a little
				// further, which is the same transition seen in the dark.
				double d  = length(p->pos - srcpos);
				double dd = d - flameR; if (dd < 0.0) dd = 0.0;
				flame = lvl * (flameR * flameR) / (flameR * flameR + dd * dd);
			}
			if (sunsh > 0.0005) {
				// direction TOWARD the sun (sun at the origin - RenderGroundShadow's
				// own convention, the same one the visibility above already uses)
				double rsl = length(p->pos); if (rsl < 1.0) rsl = 1.0;
				sd = p->pos * (-1.0 / rsl);
				grad = true;
			}
			// the flat (per-particle) value - the no-sun path, and the fallback.
			// The flame ADDS now instead of max-lifting (the other half of the same
			// smoothing): light adds, and the hard max had a kink exactly where the
			// flame handed over to the sun - which IS the cutoff he reported. Full
			// daylight is untouched: the sum clamps at 1 exactly as the max did.
			double liR = ambR + dirR + flame;
			double liG = ambG + dirG + flame;
			double liB = ambB + dirB + flame;
			// round 5: the WHITE terms keep their old 1.0 ceiling - daylight and the
			// engine steam bit-identical - and only the sun term's tinted EXCESS
			// rides above it, into the bloom (see the dOv note).
			lR = (float)((liR < 1.0 ? liR : 1.0) + dirR * dOv);
			lG = (float)((liG < 1.0 ? liG : 1.0) + dirG * dOv);
			lB = (float)((liB < 1.0 ? liB : 1.0) + dirB * dOv);
		}

		if (grad) {
			// PER-CORNER shading: pseudo-normal = corner offset + a view bulge (the
			// billboard as the front hemisphere of a sphere), wrap lambert against
			// the sun. The corners were just written by SetDParticleCoords, so the
			// offsets come straight off the vertices - orientation-proof: whatever
			// the atlas rotation did, the geometry knows where each corner IS.
			const VECTOR3 cpos = p->pos - camera_gpos;
			const double  cl   = length(cpos);
			const VECTOR3 vc   = (cl > 1.0) ? cpos * (-1.0 / cl) : _V(0, 0, -1);
			for (j = 0; j < 4; j++, vtx++) {
				const VECTOR3 offs = _V((double)vtx->x - cpos.x,
				                        (double)vtx->y - cpos.y,
				                        (double)vtx->z - cpos.z);
				const double ol = length(offs);
				VECTOR3 nj = vc;
				if (ol > 1e-6) nj = unit(offs * (1.0 / ol) + vc * ORO_PRT_DIR_ZC);
				double L = (dotp(nj, sd) + ORO_PRT_DIR_WRAP) / (1.0 + ORO_PRT_DIR_WRAP);
				if (L < 0.0) L = 0.0; else if (L > 1.0) L = 1.0;
				double cR = ambR + dirR * L + flame;	// the flame ADDS, omnidirectional
				double cG = ambG + dirG * L + flame;	//   (see the flat path's note)
				double cB = ambB + dirB * L + flame;
				// ORO patch (x): lighting RGB, not a normal - Particle.fx reads
				// nrmL.xyz per vertex. Round 5: white terms clamp at 1.0 as ever;
				// the tinted sun excess rides above, into the bloom (the flat
				// path's note) - per corner, so the lit side blooms hardest.
				vtx->nx = (float)((cR < 1.0 ? cR : 1.0) + dirR * L * dOv);
				vtx->ny = (float)((cG < 1.0 ? cG : 1.0) + dirG * L * dOv);
				vtx->nz = (float)((cB < 1.0 ? cB : 1.0) + dirB * L * dOv);
				vtx->tu = u[j];
				vtx->tv = v[j];
			}
		} else {
			for (j = 0; j < 4; j++, vtx++) {
				vtx->nx = lR;	// ORO patch (x): lighting RGB, not a normal - Particle.fx reads nrmL.xyz
				vtx->ny = lG;
				vtx->nz = lB;
				vtx->tu = u[j];
				vtx->tv = v[j];
			}
		}

		if (++n == stride || n+i0 == np) {
			float alpha = (float)max (0.1, p->alpha0*(1.0-(oapiGetSimTime()-p->t0)*ipht2));
			HR(FX->SetFloat(eMix, alpha));
			HR(FX->CommitChanges());
			HR(dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, n*4, n*2, idx, D3DFMT_INDEX16, dvtx+i0*4, sizeof(NTVERTEX)));
			i0 += n;
			n = 0;
		}
	}

	HR(FX->EndPass());
	HR(FX->End());
}


void D3D9ParticleStream::RenderEmissive(LPDIRECT3DDEVICE9 dev)
{
	static D3DMATERIAL9 smokemat = { // emissive material for engine exhaust
		{0,0,0,1},
		{0,0,0,1},
		{0,0,0,1},
		{1,1,1,1},
		0.0
	};
	UINT numPasses=0;
	ParticleSpec *p = NULL;
	int i0, j, n;
	float *u, *v;
	VERTEX_XYZ_TEX *vtx;

	VECTOR3 camera_gpos = pGC->GetScene()->GetCameraGPos();

	HR(dev->SetVertexDeclaration(pPosTexDecl));
	HR(FX->SetTechnique(eEmissiveTech));
	HR(FX->SetMatrix(eW, &mWorld));

	if (tex) HR(FX->SetTexture(eTex0, SURFACE(tex)->GetTexture()));

	D3DCOLORVALUE color;
	SetMaterial(color);

	HR(FX->SetValue(eColor, &color, sizeof(D3DCOLORVALUE)));

	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(0));

	for (p = pfirst, vtx = evtx, n = i0 = 0; p; p = p->next) {

		SetEParticleCoords(p->pos - camera_gpos, p->size, vtx);

		u = tu + p->texidx;
		v = tv + p->texidx;
		for (j = 0; j < 4; j++, vtx++) {
			vtx->tu = u[j];
			vtx->tv = v[j];
		}

		if (++n == stride || n+i0 == np) {

			float alpha = (float)max (0.1, p->alpha0*(1.0-(oapiGetSimTime()-p->t0)*ipht2));
			HR(FX->SetFloat(eMix, alpha));
			HR(FX->CommitChanges());
			HR(dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, n*4, n*2, idx, D3DFMT_INDEX16, evtx+i0*4, sizeof(VERTEX_XYZ_TEX)));
			i0 += n;
			n = 0;
		}
	}

	HR(FX->EndPass());
	HR(FX->End());
}













// =======================================================================

// ORO patch (o): the construction-time latch (gcCore.cpp).
extern bool gcExemptLatch();

ExhaustStream::ExhaustStream (oapi::GraphicsClient *_gc, OBJHANDLE hV,
	const double *srclevel, const VECTOR3 *thref, const VECTOR3 *thdir,
	PARTICLESTREAMSPEC *pss)
: D3D9ParticleStream (_gc, pss)
{
	Attach (hV, thref, thdir, srclevel);
	hPlanet = 0;
	bExempt = gcExemptLatch();          // ORO patch (o)
}

ExhaustStream::ExhaustStream (oapi::GraphicsClient *_gc, OBJHANDLE hV,
	const double *srclevel, const VECTOR3 &ref, const VECTOR3 &_dir,
	PARTICLESTREAMSPEC *pss)
: D3D9ParticleStream (_gc, pss)
{
	Attach (hV, ref, _dir, srclevel);
	hPlanet = 0;
	bExempt = gcExemptLatch();          // ORO patch (o)
}

// ORO patch (n): the per-vessel exhaust suppression (gcCore.cpp) covers the exhaust
// PARTICLE streams too - the billboard gate alone would leave smoke and contrails
// puffing from an engine whose flame an addon has replaced. Suppression stops
// EMISSION only: particles already in flight expire naturally (the patch-(e) rule -
// suppressed vessels fall through to the else below, which resets t0, so lifting the
// suppression does not release a burst of backlogged particles).
extern bool gcIsExhaustStreamSuppressed(OBJHANDLE hVessel);
// ORO patch (o): ... unless THIS stream was exempted (an addon's own replacement
// stream, added to a vessel whose stock exhaust it is suppressing).
extern bool gcExemptLatch();

void ExhaustStream::Update ()
{
	D3D9ParticleStream::Update();

	double simt = oapiGetSimTime();
	double dt = oapiGetSimStep();
	double alpha0;

	VESSEL *vessel = (hRef ? oapiGetVesselInterface (hRef) : 0);

	if (np) {
		ParticleSpec *p;
		double lng, lat, r1, r2, rad, pref, slow;
		int i;
		if (vessel) hPlanet = vessel->GetSurfaceRef();
		if (hPlanet) {
			VECTOR3 pp;
			oapiGetGlobalPos (hPlanet, &pp);
			rad = oapiGetSize (hPlanet);
			VECTOR3 dv = pp-plast->pos; // gravitational dv
			double d = length (dv);
			dv *= GGRAV * oapiGetMass(hPlanet)/(d*d*d) * dt;

			ATMPARAM prm;
			oapiGetPlanetAtmParams (hPlanet, d, &prm);
			if (prm.rho) {
				pref = sqrt(prm.rho) / 1.1371;
				slow = exp(-beta*pref*dt);
				dv *= exp(-prm.rho*2.0); // reduce gravitational effect in atmosphere (buoyancy)
			} else {
			//	pref = 0.0;
				slow = 1.0;
			}
			oapiGlobalToEqu (hPlanet, pfirst->pos, &lng, &lat, &r1);
			VECTOR3 av1 = oapiGetWindVector (hPlanet, lng, lat, r1-rad, 3);
			oapiGlobalToEqu (hPlanet, plast->pos, &lng, &lat, &r2);
			VECTOR3 av2 = oapiGetWindVector (hPlanet, lng, lat, r2-rad, 3);
			VECTOR3 dav = (av2-av1)/np;
			double r = oapiGetSize (hPlanet);
			if (vessel) r += vessel->GetSurfaceElevation();

			for (p = pfirst, i = 0; p; p = p->next, i++) {
				p->vel += dv;
				VECTOR3 av = dav*i + av1; // atmosphere velocity
				VECTOR3 vv = p->vel-av;   // velocity difference
				p->vel = vv*slow + av;
				p->size += alpha * dt;

				VECTOR3 s (p->pos - pp);
				if (length(s) < r) {
					VECTOR3 dp = s * (r/length(s)-1.0);
					p->pos += dp;

					static double dv_scale = length(vv)*0.2;
					VECTOR3 dv = {((double)rand()/(double)RAND_MAX-0.5)*dv_scale,
								  ((double)rand()/(double)RAND_MAX-0.5)*dv_scale,
								  ((double)rand()/(double)RAND_MAX-0.5)*dv_scale};
					dv += vv;

					normalise(s);
					VECTOR3 vv2 = dv - s*dotp(s,dv);
					if (length(vv2)) vv2 *= 0.5*length(vv)/length(vv2);
					vv2 += s*(((double)rand()/(double)RAND_MAX)*dv_scale);
					p->vel = vv2*1.0/*2.0*/+av;
					double r = (double)rand()/(double)RAND_MAX;
					p->pos += (vv2-vv) * dt * r;
					//p->size *= (1.0+r);
				}
			}
		}
	}

	if (level && *level > 0 && vessel && (!gcIsExhaustStreamSuppressed(hRef) || bExempt)
	    && (alpha0 = Level2Alpha(*level) * Atm2Alpha (vessel->GetAtmDensity())) > 0.01) {
		if (simt > t0+interval) {
			VECTOR3 vp, vv;
			MATRIX3 vR;
			vessel->GetRotationMatrix (vR);
			vessel->GetGlobalPos (vp);
			vessel->GetGlobalVel (vv);
			VECTOR3 vr = mul (vR, *dir) * (-speed);
			while (simt > t0+interval) {
				// create new particle
				double dt = simt-t0-interval;
				double dv_scale = speed*vrand; // exhaust velocity randomisation
				VECTOR3 dv = {((double)rand()/(double)RAND_MAX-0.5)*dv_scale,
						      ((double)rand()/(double)RAND_MAX-0.5)*dv_scale,
							  ((double)rand()/(double)RAND_MAX-0.5)*dv_scale};
				ParticleSpec *p = CreateParticle (mul (vR, *pos) + vp + (vr+dv)*dt,
					vv + vr+dv, size0, alpha0);
				p->size += alpha * dt;

				if (diffuse && hPlanet && bShadows) { // check for shadow render
					double lng, lat, alt;
					static const double eps = 1e-2;
					oapiGlobalToEqu (hPlanet, p->pos, &lng, &lat, &alt);
					//planet->GlobalToEquatorial (MakeVector(p->pos), lng, lat, alt);
					alt -= oapiGetSize(hPlanet);
					if (vessel) alt -= vessel->GetSurfaceElevation();
					if (alt*eps < vessel->GetSize()) p->flag |= 1; // render shadow
				}

				// determine next interval (pretty hacky)
				t0 += interval;
				if (speed > 10) {
					interval = max (0.015, size0 / (pdensity * (0.1*vessel->GetAirspeed() + size0)));
				} else {
					interval = 1.0/pdensity;
				}
				interval *= (double)rand()/(double)RAND_MAX + 0.5;
			}
		}
	} else t0 = simt;

}


void ExhaustStream::RenderGroundShadow (LPDIRECT3DDEVICE9 dev, LPDIRECT3DTEXTURE9 &prevtex)
{
	if (!diffuse || !hPlanet || !pfirst) return;
	if (Config->TerrainShadowing == 0) return;
	// ORO patch (x): the Launchpad's diffuse-particle shadow strength (his ask,
	// 2026-08-30 - a low sun stretched a smoke column's shadow into a near-black
	// band across the ground). Scales the shadow alpha INCLUDING its 0.1 floor;
	// 1 = stock exactly, 0 = no shadow drawn at all.
	const double shdK = Config->ParticleShadow;
	if (shdK < 0.005) return;

	ParticleSpec *p = pfirst;

	VESSEL *vessel = (hRef ? oapiGetVesselInterface (hRef) : 0);

	double R;
	float *u, *v, alpha;
	int n, j, i0;
	VECTOR3 sd, hn;

	VERTEX_XYZ_TEX *vtx;

	VECTOR3 pp,gcam;
	oapiGetGlobalPos (hPlanet, &pp);
	gcam = pGC->GetScene()->GetCameraGPos();

	R = oapiGetSize(hPlanet);
	if (vessel) R += vessel->GetSurfaceElevation();
	sd = unit(p->pos);  // shadow projection direction
	VECTOR3 pv0 = p->pos - pp;   // rel. particle position
	// calculate the intersection of the vessel's shadow with the planet surface
	double fac1 = dotp (sd, pv0);
	if (fac1 > 0.0) return;       // shadow doesn't intersect planet surface
	double arg  = fac1*fac1 - (dotp (pv0, pv0) - R*R);
	if (arg <= 0.0) return;       // shadow doesn't intersect with planet surface
	double a = -fac1 - sqrt(arg);
	VECTOR3 shp = sd*a;           // projection point in global frame
	hn = unit (shp + pv0);        // horizon normal in global frame

	HR(dev->SetVertexDeclaration(pPosTexDecl));
	HR(FX->SetTechnique(eEmissiveTech));
	HR(FX->SetMatrix(eW, &mWorld));

	if (tex) FX->SetTexture(eTex0, SURFACE(tex)->GetTexture());

	UINT numPasses = 0;

	HR(FX->Begin(&numPasses, D3DXFX_DONOTSAVESTATE));
	HR(FX->BeginPass(1));

	for (p = pfirst, vtx = evtx, n = i0 = 0; p; p = p->next) {

		if (!(p->flag & 1)) continue;

		VECTOR3 pvr = p->pos - pp;   // rel. particle position

		// calculate the intersection of the vessel's shadow with the planet surface
		double fac1 = dotp (sd, pvr);
		if (fac1 > 0.0) break;       // shadow doesn't intersect planet surface
		double arg  = fac1*fac1 - (dotp (pvr, pvr) - R*R);
		if (arg <= 0.0) break;       // shadow doesn't intersect with planet surface
		double a = -fac1 - sqrt(arg);

		SetShadowCoords (p->pos - gcam + sd*a, -hn, p->size, vtx);

		u = tu + p->texidx;
		v = tv + p->texidx;
		for (j = 0; j < 4; j++, vtx++) {
			vtx->tu = u[j];
			vtx->tv = v[j];
		}
		if (++n == stride || n+i0 == np) {
			alpha = (float)(shdK * max (0.1, 0.60 * p->alpha0*(1.0-(oapiGetSimTime()-p->t0)*ipht2)));
			if (alpha>0.01f) {
				HR(FX->SetFloat(eMix, alpha));
				HR(FX->CommitChanges());
				HR(dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, n*4, n*2, idx, D3DFMT_INDEX16, evtx+i0*4, sizeof(VERTEX_XYZ_TEX)));
			}
			i0 += n;
			n = 0;
		}
	}

	HR(FX->EndPass());
	HR(FX->End());
}


// =======================================================================

ReentryStream::ReentryStream (oapi::GraphicsClient *_gc, OBJHANDLE hV, PARTICLESTREAMSPEC *pss)
: D3D9ParticleStream (_gc, pss)
{
	llevel = 1.0;
	Attach (hV, _V(0,0,0), _V(0,0,0), &llevel);
	hPlanet = 0;
}

void ReentryStream::SetMaterial (D3DCOLORVALUE &col)
{
	// should be heating-dependent
	col.r = 1.0f;
	col.g = 0.7f;
	col.b = 0.5f;
}

// ORO patch (e): the per-vessel reentry suppression (gcCore.cpp) must cover the
// reentry PARTICLE streams too - the core hands EVERY vessel a default "plasmastream"
// (Vessel::SetDefaultReentryStream), and vessels like Atlantis add their own, so an
// addon replacing the reentry visuals gets orphan puffs from ~95 km without this.
// Suppression stops EMISSION only: particles already in flight expire naturally.
extern bool gcIsReentrySuppressed(OBJHANDLE hVessel);

void ReentryStream::Update ()
{
	D3D9ParticleStream::Update ();
	VESSEL *vessel = (hRef ? oapiGetVesselInterface (hRef) : 0);

	double simt = oapiGetSimTime();
	double simdt = oapiGetSimStep();
	double friction = vessel
	                ? 0.5 * pow(vessel->GetAtmDensity(), 0.6)
	                      * pow(vessel->GetAirspeed()  , 3  )
	                : 0.0;
	double alpha0;

	if (np) {
		ParticleSpec *p;
		double lng, lat, r1, r2, rad;
		int i;
		if (vessel) hPlanet = vessel->GetSurfaceRef();
		if (hPlanet) {
			rad = oapiGetSize (hPlanet);
			oapiGlobalToEqu (hPlanet, pfirst->pos, &lng, &lat, &r1);
			VECTOR3 av1 = oapiGetWindVector (hPlanet, lng, lat, r1-rad, 3);
			oapiGlobalToEqu (hPlanet, plast->pos, &lng, &lat, &r2);
			VECTOR3 av2 = oapiGetWindVector (hPlanet, lng, lat, r2-rad, 3);
			VECTOR3 dav = (av2-av1)/np;
			// double r = oapiGetSize (hPlanet);

			for (p = pfirst, i = 0; p; p = p->next, i++) {
				VECTOR3 av = dav*i + av1;
				VECTOR3 vv = p->vel-av;
				double slow = exp(-beta*simdt);
				p->vel = vv*slow + av;
				p->size += alpha * simdt;
			}
		}
	}

	if (friction > 0 && !gcIsReentrySuppressed(hRef) && (alpha0 = Atm2Alpha (friction)) > 0.01) {
		// (suppressed vessels fall through to the else, which resets t0 - so lifting
		// the suppression does not release a burst of backlogged particles)
		if (simt > t0+interval) {
			VECTOR3 vp, vv, av;
			vessel->GetGlobalPos (vp);
			vessel->GetGlobalVel (vv);

			if (hPlanet) {
				double lng, lat, r, rad;
				rad = oapiGetSize (hPlanet);
				oapiGlobalToEqu (hPlanet, vp, &lng, &lat, &r);
				av = oapiGetWindVector (hPlanet, lng, lat, r-rad, 3);
			} else
				av = vv;

			while (simt > t0+interval) {
				// create new particle
				double dt = simt-t0-interval;
				double ebt = exp(-beta*dt);
				double dv_scale = vessel->GetAirspeed()*vrand; // exhaust velocity randomisation
				VECTOR3 dv = {((double)rand()/(double)RAND_MAX-0.5)*dv_scale,
						      ((double)rand()/(double)RAND_MAX-0.5)*dv_scale,
							  ((double)rand()/(double)RAND_MAX-0.5)*dv_scale};
				VECTOR3 dx = (vv-av) * (1.0-ebt)/beta + av*dt;
				CreateParticle (vp + dx - vv*dt, (vv+dv-av)*ebt + av, size0, alpha0);
				// determine next interval
				t0 += interval;
				interval = max (0.015, size0 / (pdensity * (0.1*vessel->GetAirspeed() + size0)));
				interval *= (double)rand()/(double)RAND_MAX + 0.5;
			}
		}
	} else t0 = simt;
}
