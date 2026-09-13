// ==============================================================
// VBase.h
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2007-2016 Martin Schweiger
// ==============================================================

#ifndef __VBASE_H
#define __VBASE_H

#include "VObject.h"
#include "Mesh.h"

class RunwayLights;
class TaxiLights;


// ==============================================================
// class vBase (interface)
// ==============================================================

/**
 * \brief Visual representation of a surface base.
 *
 * A vBase is the visual representation of a surface base object (a "spaceport"
 * on the surface of a planet or moon, usually with runways or landing pads
 * where vessels can land and take off.
 */
class OroBaseAnim;   // ORO patch (ag): the animated base objects (trains, solar plant) - OroBaseAnim.h

class vBase: public vObject {
	friend class vPlanet;

public:
	vBase (OBJHANDLE _hObj, const Scene *scene, vPlanet *vP=NULL);
	~vBase();

	virtual bool GetMinMaxDistance(float *zmin, float *zmax, float *dmin);
	virtual void UpdateBoundingBox();
	virtual DWORD GetMeshCount();

	bool Update (bool bMainScene);

	double	GetElevation() const;
	vPlanet *GetPlanet() const { return vP; }

	// Convert from a base centric system to time invariant geocentric system 
	// (i.e. vector does not change in time and points to the same fixed surface location) 
	//
	VECTOR3 ToLocal(VECTOR3 pos, double *lng=NULL, double *lat=NULL) const;


	// Convert from time invariant geocentric frame to base centric system (Inverse of the ToLocal)
	//
	VECTOR3 FromLocal(VECTOR3 pos) const;
	void	FromLocal(VECTOR3 pos, D3DXVECTOR3 *pTgt) const;

	void RenderRunwayLights (LPDIRECT3DDEVICE9 dev);
	bool RenderSurface (LPDIRECT3DDEVICE9 dev);
	bool RenderStructures (LPDIRECT3DDEVICE9 dev);
	bool RenderStructureDepth (const LPD3DXMATRIX pVP, int opt = 1);   // ORO patch (z2): opt 1 = GBUF_DEPTH; (z3): opt 0 = a light's shadow map
	int  FitLocalShadowCasters(const D3DXVECTOR3& P, const D3DXVECTOR3& D, float range, float halfCone, float& halfFit, float& farFit);	// ORO patch (ah) step 2
	int  AimLocalShadowCasters(const D3DXVECTOR3& P, float range, D3DXVECTOR3& sum, float& weight);	// ORO patch (ah) step 5: where the visible casters are, seen from a POINT light
	void RenderGroundShadow (LPDIRECT3DDEVICE9 dev, float alpha);

	const SurftileSpec *GetTileDesc() const { return tspec; }

private:

	void CreateRunwayLights();
	void CreateTaxiLights();

	/**
	 * \brief Modify local lighting due to planet shadow or
	 *   atmospheric dispersion.
	 * \param light pointer to D3DLIGHT7 structure receiving modified parameters
	 * \param nextcheck time interval until next lighting check [s]
	 * \return \e true if lighting modifications should be applied, \e false
	 *   if global lighting conditions apply.
	 */
	//bool ModLighting (D3D9Light *light, double &nextcheck);

	double Tchk;               // next update
	double Tlghtchk;           // next lighting update
	double csun_lights;
	DWORD ntile;               // number of surface tiles
	const SurftileSpec *tspec; // list of tile specs
	D3D9Mesh *tilemesh;
	D3D9Mesh **structure_bs;
	D3D9Mesh **structure_as;
	DWORD nstructure_bs, nstructure_as;
	OroBaseAnim *oroAnim;      // ORO patch (ag): the moving base objects; its meshes live in structure_as
	bool lights;               // use nighttextures for base objects
	//bool bLocalLight;          // true if lighting is modified
	class vPlanet *vP;
	VECTOR3 vLocalPos;
	MATRIX3 mGlobalRot;
	D3DXMATRIX mGlobalRotDX;

	int numRunwayLights;
	RunwayLights** runwayLights;

	int numTaxiLights;
	TaxiLights** taxiLights;
};

#endif // !__VBASE_H
