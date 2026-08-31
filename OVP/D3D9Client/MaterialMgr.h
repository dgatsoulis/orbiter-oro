// ==============================================================
// MaterialMgr.h
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ==============================================================

#ifndef __MATERIALMGR_H
#define __MATERIALMGR_H

#include <d3d9.h>
#include <d3dx9.h>

#include "Mesh.h"
#include "D3D9Client.h"
#include "D3D9Util.h"
#include "vObject.h"

#define ENVCAM_OMIT_ATTC		0x0001
#define ENVCAM_OMIT_DOCKS		0x0002
#define ENVCAM_FOCUS			0x0004

// ORO patch (v) 2026: MULTIPLE reflection probes per vessel. The data model always
// allowed for them (BEGIN_CAMERA takes an index, GetCamera takes an index) but the
// implementation was parked at one - "For now just one camera". Un-parked: up to
// MAX_ENVCAM probes, each with its own position, flags and omit lists, and a list of
// MESH GROUP ranges that sample it (GROUPS a b in the camera block). Groups not
// claimed by any camera keep probe 0, so a config with no camera blocks renders
// exactly as before.
#define MAX_ENVCAM				4


/**
 * \brief Storage structure to keep environmental camera information.
 */
struct ENVCAMREC {
	D3DXVECTOR3		lPos;			///< Camera local position
	float			near_clip;		///< Near clip-plane distance
	DWORD			flags;			///< Camera flags
	WORD			nGrpRng;		///< ORO patch (v): number of group ranges below
	WORD *			pGrpRng;		///< ORO patch (v): (first,last) mesh-group index
									///  pairs that sample THIS probe (all meshes)
	bool			bBox;			///< ORO patch (v): box-projected sampling on
	D3DXVECTOR3		bxC;			///< ... proxy box centre, vessel-local [m]
	D3DXVECTOR3		bxE;			///< ... proxy box half-extents, vessel-local [m]
	WORD			nAttc;			///< Number of attachments points in a list
	WORD			nDock;			///< Number of docking ports in a list
	BYTE *			pOmitAttc;		///< Omit attachments
	BYTE *			pOmitDock;		///< Omit vessels in docking ports
};

// ORO patch (v) part 2: a PLANAR REFLECTION surface. Where a probe approximates,
// a mirrored-camera render is EXACT - correct parallax, scale and motion for the
// plane it mirrors about - and the flat near-mirrors that expose probe parallax
// (the shuttle's radiators and door inner faces) are exactly planar. Declared in
// the _ecam file (BEGIN_PLANE/POS/NRM/GRPREF/GROUPS/END_PLANE); POS/NRM are in the
// mesh's BASE pose and ride the reference group's animation transform, so an
// opening door carries its mirror with it.
#define MAX_ENVPLN	2

struct ENVPLNREC {
	D3DXVECTOR3		lPos;			///< a point on the plane, base-pose local [m]
	D3DXVECTOR3		lNrm;			///< plane normal, base-pose local (reflective side)
	int				refMesh;		///< mesh of the animation reference group (-1 = static)
	int				refGrp;			///< ... and the group whose transform carries the plane
	WORD			nGrpRng;		///< group ranges that sample this plane
	WORD *			pGrpRng;		///< (first,last) pairs, as the camera ranges
	float			rDist;			///< assumed reflected-geometry distance [m] for the
									///  CURVATURE warp (RDIST; only matters off-plane)
};

/**
 * \brief Management of custom configurations for vessel materials
 */
class MatMgr {

public:
	// Disable copy construct & copy assign
					MatMgr    (MatMgr const&) = delete;
	MatMgr &		operator= (MatMgr const&) = delete;

					MatMgr(class vObject *vObj, class D3D9Client *_gc);
					~MatMgr();

	//DWORD			NewRecord(const char *name, DWORD midx);
	//void			ClearRecord(DWORD iRec);
	void			RegisterMaterialChange(D3D9Mesh *pMesh, DWORD midx, const D3D9MatExt *pM);
	void			RegisterShaderChange(D3D9Mesh *pMesh, WORD id);
	void			ApplyConfiguration(D3D9Mesh *pMesh);
	bool			SaveConfiguration();
	bool			LoadConfiguration(bool bAppend=false);
	bool			LoadCameraConfig();
	bool			ParseCameraFile(const char* path, class VESSEL* vessel, bool bExt);
	bool			HasMesh(const char *name);
	void			ResetCamera(DWORD idx);

	ENVCAMREC *		GetCamera(DWORD idx);
	DWORD			CameraCount();
	ENVPLNREC *		GetPlane(DWORD idx);		// ORO patch (v) part 2
	DWORD			PlaneCount();
	void			ResetPlane(DWORD idx);

private:

	vObject			*vObj;
	D3D9Client		*gc;
	DWORD			nCameras;	///< ORO patch (v): highest camera index used + 1
	//DWORD			nRec;	///< Number of records
	//DWORD			mRec;	///< Allocated records


	struct SHADER {
		SHADER(string x, WORD i) { name = x; id = i; }
		string name;
		WORD id;
	};

	struct MESHREC {
		WORD shader;
		map<int, D3D9MatExt> material;
	};

	std::map<string, MESHREC> MeshConfig;
	std::list<SHADER> Shaders;

	ENVCAMREC *pCamera;
	ENVPLNREC *pPlane;		// ORO patch (v) part 2: MAX_ENVPLN slots
	DWORD		nPlanes;
};

#endif
