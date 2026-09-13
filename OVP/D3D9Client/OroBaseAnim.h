// ==============================================================
// OroBaseAnim.h - ORO patch (ag): ANIMATED BASE OBJECTS (2026-09-07/08)
// Part of the ORO-patched D3D9Client (github.com/dgatsoulis/orbiter-oro).
// Dual licensed under GPL v3 and LGPL v3.
// ==============================================================
//
// THE PRE-TERRAIN BASE OBJECTS, REVIVED. Orbiter's base definition files carry three
// object types that MOVE - TRAIN1 (a monorail cabin shuttling between two ends),
// TRAIN2 (two cabins hanging under a girder rail on legs) and SOLARPLANT (a panel
// array tracking the sun) - and every one of them has been dead under a graphics
// client since the client split: the only code that ever animated them lives in the
// core's INLINE renderer (Src/Orbiter/VBase.cpp calls each object's Update() and the
// D3D7 draw routines of the two that render themselves). A client receives a base only
// through GetBaseStructures(): compiled meshes, copied once, every generic object
// sharing a texture merged into one group. So today the monorail cabin sits frozen at
// one end of its beam, the hangrail's cabins float frozen in the air with no rail under
// them (the rail is inline-only), and a solar plant is invisible. The core does lift the
// two track ENDS to the terrain (Base::Setup calls Train::Setup) but draws a straight
// chord between them, so a track cuts through a rise and hangs over a dip - the
// pre-terrain model, unchanged since 2010.
//
// THE REVIVAL (the patch (h) shape - the client reads the file itself): this class
// re-reads the base's own cfg for those three block types, builds its OWN meshes for
// the moving and missing parts, appends them to vBase's above-shadow structure list -
// so they get local lights, fog, the depth buffer and the cascade atlas as casters and
// receivers for free - animates them every frame in vBase::Update with the core's own
// motion laws on sim time, and collapses the core's frozen exports so nothing draws
// twice.
//
// TERRAIN - the design his questions settled (2026-09-07): the ground stays the ground
// and THE STRUCTURE ADAPTS. Flattening a strip along a track was rejected (stock
// elevation tiles are ~1.2 km per texel almost everywhere, so a 10 m track would flatten
// a kilometre-wide plateau; it would change the physics ground for every vessel nearby;
// and it is the wrong model - railways cross terrain on viaducts and cuttings, monorails
// are pylon-mounted by design). So the terrain is sampled along the track (base-local,
// curvature included), the rail profile is the LOWEST line that stays at or above the
// ground everywhere under a maximum GRADE - the slope-limited upper envelope, two
// passes - lightly smoothed so the cabin does not kink, and pylons drop from the beam
// to the local ground wherever it leaves it. On a flat apron the beam lies on the ground
// exactly as in 2010; over a dip it bridges on legs; over a rise it climbs early enough
// to stay under the grade. The SOLAR PLANT takes the same rule per panel: each panel
// stands at its authored height above the ground UNDER IT (a hillside plant follows the
// hill, the way a real tracker farm does - one post per panel), and each stand's three
// feet are cut to the ground under each foot. Visual only, as in 2010: no collision, no
// boarding.
//
// SOLARPLANT is the one object the core never exported at all (it was INLINE-ONLY with
// OBJSPEC_UPDATEVERTEX), so there is nothing to collapse: the client simply gains it.
// Its laws are the core's: 16 x 8 m panels at SCALE on 10 x SCALE stands, aimed at the
// sun every 60 s of sim time with the long axis tilting and the short axis horizontal,
// and the 2010 GLINT - a panel whose normal points at the camera (within 2.6 deg) swaps
// to the texture's bright column, because a mirror aimed at the sun reflects it straight
// back along its normal. Two deliberate departures: the tilt is capped at 75 deg (the
// core stood the panels VERTICAL under a horizon-clamped sun), and the stand's apex sits
// just under the panel's pivot (the core's poked 4 x SCALE through the panel face).
// ==============================================================

#ifndef __OROBASEANIM_H
#define __OROBASEANIM_H

#include "D3D9Client.h"
#include <vector>

class D3D9Mesh;

class OroBaseAnim {
public:
	OroBaseAnim(oapi::D3D9Client *gc, OBJHANDLE hBase, OBJHANDLE hPlanet, double baseElev);
	~OroBaseAnim();

	// The meshes to append to the base's above-shadow structure list. Ownership passes
	// to that list (vBase deletes them with the rest); this class keeps non-owning
	// pointers for the animation.
	DWORD      MeshCount() const { return (DWORD)meshes.size(); }
	D3D9Mesh*  Mesh(DWORD i) const { return meshes[i]; }
	int        TrainCount() const { return (int)trains.size(); }
	int        PlantCount() const { return (int)plants.size(); }

	// Collapse the core's frozen exports (the cabin in the over-shadow generic mesh,
	// the beam in the under-shadow one) so they do not draw beside ours.
	void       HideCoreDuplicates(D3D9Mesh **bs, DWORD nbs, D3D9Mesh **as, DWORD nas);

	// Per frame, main thread: advance every cabin on sim time and re-place its mesh;
	// aim the solar panels at the sun and glint them at the camera. Both directions
	// are BASE-LOCAL (vBase hands them over: the base's rotation applied to vObject's
	// sundir and to the camera's position relative to the base).
	void       Update(double simt, const VECTOR3 &sunLocal, const VECTOR3 &camLocal);

	// THE CORE'S SOLARPLANT CORRUPTS THE HEAP AT SESSION CLOSE (found 2026-09-08, his
	// four exits in a row at 0xC0000374). SolarPlant's constructor never initialises its
	// geometry pointers (ppos, Vtx, Idx, flash, ShVtx, ShIdx - Src/Orbiter/Baseobj.cpp),
	// Activate() allocates them, and the destructor's Deactivate() delete[]s them
	// UNCONDITIONALLY - the trains guard theirs with dyndata, the plant guards nothing.
	// Under a graphics client a base's objects are only ever activated when the client
	// asks for that base's structures, i.e. when its VISUAL is created (apparent radius
	// > 2, or the focus vessel's own planet with PreLBaseVis), so a SOLARPLANT block in
	// any base the session never visited - a plant on the Moon while you fly at Mars -
	// is destroyed with garbage pointers at Base::~Base. The 2010 inline renderer
	// activated every object of every base at start, which is why nobody ever saw it.
	// The core cannot be patched from here, but the trigger can be pulled from here:
	// called at the top of clbkCloseSession, this asks the core for the structures of
	// every base whose cfg carries a SOLARPLANT block, which runs Activate() on every
	// object of those bases, so the destructor frees real allocations. Done at CLOSE,
	// not at start: Base::Setup (the terrain lift, the mesh elevations) has run by then,
	// and a base the user did visit is untouched (the core's objmsh_valid guard).
	static void ArmCoreSolarPlants(oapi::D3D9Client *gc);

private:
	struct Spec {
		int   type;                 // 1 = TRAIN1, 2 = TRAIN2, 3 = SOLARPLANT
		float e1[3], e2[3];         // trains: as authored, base-local metres, y above the base plane
		float maxspeed, slowzone, height, tuscale;
		float pos[3], scale, sepx, sepz, rot;   // solar: the core's POS / SCALE / SPACING / ROT (radians)
		int   nrow, ncol;                       // solar: GRID
		char  tex[64];
	};
	struct Train {
		Spec  sp;
		float length, minpos, maxpos, speedfac;   // the core's Train::Init
		float lenH;                                // horizontal chord length
		float dirx, dirz;                          // horizontal unit direction end1 -> end2
		float cosph, sinph;                        // the core's yaw: ph = atan2(dx, dz)
		float ds;                                  // sample spacing along the horizontal chord
		std::vector<float> gy, p;                  // per sample, base-local y: ground (+ authored y), and
		                                           //   THE RAIL LINE - the monorail's beam base, the
		                                           //   hangrail's deck top
		int   ncab;                                // 1 (monorail) or 2 (hangrail, one per lane)
		float cpos[2], cvel[2];                    // cabin state, the core's
		float latOfs[2];                           // each cabin's offset across the track (hangrail +-2.5)
		float cabY;                                // cabin origin relative to the rail line (hangrail -1)
		D3D9Mesh *cab[2];                          // the moving meshes (owned by vBase's list)
	};
	struct Plant {
		Spec  sp;
		int   npanel;
		float dx, dz;                              // half-extents: 8 x SCALE along the tilting axis, 4 x SCALE across
		std::vector<D3DXVECTOR3> ppos;             // each panel's pivot, base-local, on its own ground
		std::vector<char>        flash;            // each panel's glint state (the texture column it shows)
		D3DXVECTOR3 nml;                           // the panels' current normal
		double      updT;                          // the next aim, sim time (the core's 60 s cadence)
		D3D9Mesh   *mesh;                          // one group: panels (8 vertices each, edited) + stands (static)
	};

	oapi::D3D9Client *gc;
	OBJHANDLE hBase, hPlanet;
	double    baseElev, R, blng, blat, cosBlat;
	double    lastT;
	std::vector<Train>     trains;
	std::vector<Plant>     plants;
	std::vector<D3D9Mesh*> meshes;
	int       nMono, nHang, nSolar, nPanels;

	bool   FindCfg(char *path, size_t cap) const { return FindCfgFor(hBase, hPlanet, path, cap, false); }
	static bool FindCfgFor(OBJHANDLE hBase, OBJHANDLE hPlanet, char *path, size_t cap, bool quiet);
	static bool CfgHasBlock(const char *path, const char *block);
	void   ReadSpecs(const char *path, std::vector<Spec> &out);
	double GroundY(double x, double z) const;
	bool   BuildProfile(Train &T, float lift);
	void   BuildMonorail(Train &T, SURFHANDLE hTex);
	void   BuildHangrail(Train &T, SURFHANDLE hTex);
	void   PlaceCabin(Train &T, int j);
	static void MoveCabin(Train &T, int j, float dt);
	void   BuildSolar(Plant &P, SURFHANDLE hTex);
	void   AimPanels(Plant &P, const D3DXVECTOR3 &sun);
	void   GlintPanels(Plant &P, const D3DXVECTOR3 &cam, bool sunUp);
};

#endif // !__OROBASEANIM_H
