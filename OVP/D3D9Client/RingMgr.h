// ==============================================================
// RingMgr.h
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2007-2016 Martin Schweiger
// ==============================================================

#ifndef __RINGMGR_H
#define __RINGMGR_H

#include "D3D9Client.h"
#include "VPlanet.h"
#include "Mesh.h"

#define MAXRINGRES 3

// ==============================================================
// class RingManager (interface)
// ==============================================================
/**
 * \brief Rendering of planet rings at different resolutions
 */
class RingManager {
public:
	/**
	 * \brief Constructs a new ring manager object
	 * \param vPlanet planet instance pointer
	 * \param inner_rad inner radius of the ring (>=1) [unit planet radius]
	 * \param outer_rad outer radius of the ring (>inner_rad) [unit planet radius]
	 */
	RingManager (const vPlanet *vplanet, double inner_rad, double outer_rad);

	/**
	 * \brief Destroys the ring manager object
	 */
	~RingManager ();

	/**
	 * \brief Set up global parameters shared by all instances
	 * \param gclient client instance pointer
	 */
	static void GlobalInit (oapi::D3D9Client *gclient);

	void SetMeshRes (DWORD res);

	inline double InnerRad() const { return irad; }
	inline double OuterRad() const { return orad; }

	bool Render (LPDIRECT3DDEVICE9 dev, D3DXMATRIX &mWorld, bool zenable);
	bool RenderNearField (LPDIRECT3DDEVICE9 dev, const D3DXMATRIX &W);   // ORO patch (aj) round 2: the ORO ring on a LOCAL unit disc placed by W (camera-relative, small coordinates - see VPlanet::RenderRingsNearField); false when no ORO look is pushed

protected:
	D3D9Mesh *CreateRing (double irad, double orad, int nsect);
	DWORD LoadTextures ();

private:
	static oapi::D3D9Client *gc;
	const vPlanet *vp;
	D3D9Mesh *mesh[MAXRINGRES];
	D3D9Mesh *meshORO[MAXRINGRES];  // ORO patch (aj): the same disc at 64/128/256 sections - stock keeps its 8/12/16 (a 16-gon inner edge, 1,430 km short at each chord), so the stock look is untouched
	D3D9Mesh *meshNear;             // ORO patch (aj) round 2: a UNIT half-disc for the near-field draw (CreateRing(0, 1, 64) - the inner edge collapses to the centre, a fan)
	LPDIRECT3DTEXTURE9 tex[MAXRINGRES];
	LPDIRECT3DTEXTURE9 pTex;
	DWORD rres, tres, ntex;
	double irad, orad;
};

#endif // !__RINGMGR_H