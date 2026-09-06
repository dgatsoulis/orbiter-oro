// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2006-2016 Martin Schweiger
//				 2012-2016 Jarmo Nikkanen
// ==============================================================

// ==============================================================
// Class Scene (interface)
//
// A "Scene" represents the 3-D world as seen from a specific
// viewpoint ("camera"). Each scene therefore has a camera object
// associated with it. The Orbiter core supports a single
// camera, but in principle a graphics client could define
// multiple scenes and render them simultaneously into separate
// windows (or into MFD display surfaces, etc.)
// ==============================================================

#ifndef __SCENE_H
#define __SCENE_H

#include "D3D9Client.h"
#include "CelSphere.h"
#include "VObject.h"
#include <stack>
#include <vector>
#include <list>
#include <set>

class vObject;
class vPlanet;
class D3D9ParticleStream;
class D3D9Text;
class D3D9Pad;

#define GBUF_COLOR				0
#define GBUF_BLUR				1
#define GBUF_TEMP				2
#define GBUF_DEPTH				3
#define GBUF_GDI				4
#define GBUF_COUNT				5	// Buffer count

#define SHM_LOD_COUNT			5

#define TEX_NOISE				0
#define TEX_CLUT				1
#define TEX_COUNT				2

#define RENDERPASS_UNKNOWN		0x0000
#define RENDERPASS_MAINSCENE	0x0001
#define RENDERPASS_ENVCAM		0x0002
#define RENDERPASS_CUSTOMCAM	0x0003
#define RENDERPASS_SHADOWMAP	0x0004
#define RENDERPASS_PICKSCENE	0x0005
#define RENDERPASS_SKETCHPAD	0x0006
#define RENDERPASS_MAINOVERLAY	0x0007
#define RENDERPASS_NORMAL_DEPTH	0x0008

#define RESTORE ((LPDIRECT3DSURFACE9)(-1))
#define CURRENT ((LPDIRECT3DSURFACE9)(-2))

#define RENDERTURN_ENVCAM		0
#define RENDERTURN_CUSTOMCAM	1
#define RENDERTURN_IRRADIANCE   2
#define RENDERTURN_LAST			2

#define SMAP_MODE_FOCUS			1
#define SMAP_MODE_SCENE			2

#define OBJTP_BUILDING			1000

#define CAMERA(x) ((Scene::CAMREC*)x)

class Scene {

	friend class D3D9CelestialSphere;

	// Visual record ===================================================================
	//
	struct VOBJREC {           // linked list of object visuals
		vObject *vobj;         // visual instance
		int	type;
		float apprad;
		VOBJREC *prev, *next;  // previous and next list entry
	} *vobjFirst, *vobjLast;   // first and last list entry


public:

	FVECTOR3 vPickRay;

	struct FRUSTUM {
		float znear;
		float zfar;
	};

	// Custom camera parameters ========================================================
	//
	struct CAMREC {
		MATRIX3		mRotation;
		VECTOR3		vPosition;
		double		dAperture;
		SURFHANDLE	hSurface;
		OBJHANDLE	hVessel;
		DWORD		dwFlags;
		int			iError;
		bool		bActive;
		__gcRenderProc pRenderProc;
		void*		pUser;
	};

	std::set<CAMREC*> CustomCams;
	std::set<CAMREC*>::const_iterator camCurrent{};

	// Camera frustum parameters ========================================================
	//
	struct CAMERA {
		float		aperture;   // aperture [rad]
		float		aspect;     // aspect ratio
		float		nearplane;  // frustum nearplane distance
		float		farplane;   // frustum farplane distance
		float		apsq;
		float		vh, vw, vhf, vwf;

		VECTOR3		pos;		// Global camera position
		VECTOR3		relpos;		// Relative camera position (Used by Mesh Debugger)
		VECTOR3		dir;		// Camera direction

		D3DXVECTOR3 x;			// Camera axis vector
		D3DXVECTOR3 y;			// Camera axis vector
		D3DXVECTOR3 z;			// Camera axis vector
		D3DXVECTOR3 upos;		// Camera position unit vector

		D3DXMATRIX	mView;		// D3DX view matrix for current camera state
		D3DXMATRIX	mProj;		// D3DX projection matrix for current camera state
		D3DXMATRIX	mProjView;	// D3DX combined projection view matrix
		D3DXMATRIX  mProjViewInf; // D3DX combined projection view matrix, far plane at infinity
		MATRIX3		grot;		// ORO patch (k): the oapiCameraRotationMatrix this frame
								// renders with, kept in DOUBLE precision for addons that
								// project world-anchored geometry on the CPU (mView is the
								// same rotation, but demoted to float)
		OBJHANDLE	hTarget;	// Current camera target, Mesh Debugger Related

		OBJHANDLE	hObj_proxy;	// closest celestial body
		vPlanet *	vProxy;		// closest celestial body (visual)
		double		alt_proxy;	// camera distance to surface of hObj_proxy

		OBJHANDLE	hNear;		// closest celestial body
		vPlanet *	vNear;		// closest celestial body (visual)

		OBJHANDLE	hGravRef;	// closest celestial body
		vObject*	vGravRef;	// closest celestial body (visual)

		double		alt_near;
		double		lng, lat, elev;
	};

	// Screen space sun visual parameters ==================================================
	//
	struct SUNVISPARAMS {
		float		brightness;
		bool		visible;
		D3DXVECTOR2 position;
		D3DXCOLOR	color;
	};

	struct SHADOWMAPPARAM {
		LPDIRECT3DTEXTURE9 pShadowMap;
		D3DXMATRIX	mProj, mView, mViewProj;
		D3DXVECTOR3	pos;
		D3DXVECTOR3	ld;
		float		rad;
		float		dist;
		float		depth;
		int			lod;
		int			size;
	} smap;

	// ORO patch (z3): the LOCAL-LIGHT shadow map - one perspective depth map rendered
	// from the frame's strongest shadow-casting SPOT light (vessels + base structures
	// as casters), consumed by the terrain shader so a spotlight beam is carved by a
	// building and a vessel standing in the beam casts onto the ground. idx = the
	// scene light (Lights[]) being shadowed, -1 = no map this frame.
	struct LOCALSHADOWPARAM {
		LPDIRECT3DTEXTURE9 pShadowMap;
		D3DXMATRIX	mViewProj;
		D3DXVECTOR3	pos;		// light position, camera-centred world
		float		range;		// light range [m]
		int			idx;		// scene light index, -1 = none
		int			size;		// map size in texels
		float		texel;		// world texel size per metre of light distance
								// (2 tan(fov/2) / size) - drives the receivers'
								// NORMAL-OFFSET, the grazing-acne cure
		float		kdepth;		// near*far/(far-near): converts metres-along-ray to
								// the map's 1-z/w units - drives the receivers'
								// TEXEL-FOOTPRINT bias (grazing, exact, no fade)
		bool		terrainOK;	// 2026-09-05, the DAY SPLIT: false while the sun is
								// up. Gates ONLY the terrain receiver's sampler
								// borrow (Surfmgr2) - the daylight conflict was
								// always the borrow's, never the test's. Vessels
								// bind both maps with headroom and receive day and
								// night; casters and registration key off idx alone.
	} lsmap;

	const LOCALSHADOWPARAM * GetLocalShadowData() const { return &lsmap; }

	// ⛔ ORO patch (z3) round 3 (the BASE SUN MAP - draped building shadows onto
	// terrain via a coarse ortho map bound into the TerrainShadowing-2 slots) was
	// BUILT, FLOWN AND REVERTED 2026-09-03, his call after three fix rounds: the
	// per-tile slot steal kept trading one artifact for another (vessel shadows
	// eaten in tile bites, volume-fit shimmer, transparent flickering building
	// shadows). The honest route needs a receiver path that does not fight the
	// stock tile bindings - parked with the stencil-shadow revert of patch (z).

	// ORO patch (z3) round 2c: a terrain tile registers itself as a local-light
	// shadow CASTER (called from the Surfmgr2 tile render; drawn into the NEXT
	// frame's map - terrain does not move, so one frame stale is free). The
	// buffers are AddRef'd here so an LRU-evicted tile cannot dangle - the 23m
	// law applied to VRAM.
	void RegisterLclShadowTile(LPDIRECT3DVERTEXBUFFER9 pVB, LPDIRECT3DINDEXBUFFER9 pIB,
	                           DWORD nv, DWORD nf, const D3DXMATRIX* pW,
	                           const D3DXVECTOR3* pBs, float bsRad, OBJHANDLE hPlanet) const;
	// ORO patch (ab): TERRAIN WRITES GBUF_DEPTH NOW. The registry above feeds the depth
	// pass as well as the local map, so the tile render registers every nearby tile
	// whenever the buffer exists (it exists with the glares - the 20g rule).
	bool WantsTerrainDepth() const { return ptgBuffer[GBUF_DEPTH] != NULL; }

	static void D3D9TechInit(LPDIRECT3DDEVICE9 pDev, const char *folder);

	/**
	 * \brief Release global parameters
	 */
	static void GlobalExit();

	Scene (oapi::D3D9Client *_gc, DWORD w, DWORD h);
	~Scene ();

	/**
	 * \brief Get a pointer to the client
	 */
	//inline const oapi::D3D9Client *GetClient() const { return gc; }
	inline oapi::D3D9Client *GetClient() const { return gc; }

	void OnOptionChanged(int cat, int item);

	const D3D9Sun *GetSun() const { return &sunLight; }
	const D3D9Light *GetLight(int index) const;
	const D3D9Light *GetLights() const { return Lights; }
	DWORD GetLightCount() const { return nLights; }
	D3D9Pad* GetPooledSketchpad(int id);
	void RecallDefaultState();
	float GetDisplayScale() const { return fDisplayScale; }
	void CreateSunGlare();


	DWORD GetRenderPass() const;
	DWORD GetRenderFlags() const { return RenderFlags; }
	void BeginPass(DWORD dwPass);
	void PopPass();

	inline DWORD GetStencilDepth() const { return stencilDepth; }
	inline const SHADOWMAPPARAM * GetSMapData() const { return &smap; }
	/**
	 * \brief Get the ambient background colour
	 */
	inline D3DCOLOR GetBgColour() const { return bg_rgba; }

	/**
	 * \brief Get the viewport dimension (width)
	 */
	inline const DWORD ViewW() const { return viewW; }

	/**
	 * \brief Get the viewport dimension (height)
	 */
	inline const DWORD ViewH() const { return viewH; }

	bool UpdateCamVis();
	void Initialise ();

	/**
	 * \brief Update camera position, visuals, etc.
	 */
	void Update();

	/**
	 * \brief Render the whole main scene
	 */
	void RenderMainScene();

	/**
	 * \brief Returns screen space sun visual parameters for Lens Flare rendering.
	*/
	SUNVISPARAMS GetSunScreenVisualState();

	/**
	 * \brief Gets sun diffuse colour (accounting for atmospheric shift)
	 */
	D3DXCOLOR GetSunDiffColor();

	/**
	 * \brief Render a secondary scene. (Env Maps, Shadow Maps, MFD Camera Views)
	 */
	void RenderSecondaryScene(std::set<class vVessel*> &RndList, std::set<class vVessel*> &AdditionalLightsList, DWORD flags = 0xFF);
	int RenderShadowMap(D3DXVECTOR3 &pos, D3DXVECTOR3 &ld, float rad, bool bInternal = false, bool bListExists = false);

	bool IntegrateIrradiance(vVessel *vV, LPDIRECT3DCUBETEXTURE9 pSrc, LPDIRECT3DTEXTURE9 pOut);
	bool RenderBlurredMap(LPDIRECT3DDEVICE9 pDev, LPDIRECT3DCUBETEXTURE9 pSrc);
	void RenderMesh(DEVMESHHANDLE hMesh, const oapi::FMATRIX4 *pWorld);

	LPDIRECT3DSURFACE9 GetIrradianceDepthStencil() const { return pIrradDS; }
	LPDIRECT3DSURFACE9 GetEnvDepthStencil() const { return pEnvDS; }
	LPDIRECT3DSURFACE9 GetBuffer(int id) const { return psgBuffer[id]; }
	// ORO patch (g): the shader-readable scene depth texture (camera-space linear depth
	// in .a, cockpit included - filled in RENDERPASS_NORMAL_DEPTH). NULL when SunGlare is
	// off (bGlares), so callers must degrade. Lets an addon depth-clip its own screen-space
	// geometry (aurora / reentry plasma) against the real scene instead of painting over it.
	LPDIRECT3DTEXTURE9 GetDepthTexture() const { return ptgBuffer[GBUF_DEPTH]; }
	LPDIRECT3DTEXTURE9 GetSunTexture() const { return pSunTex; }
	LPDIRECT3DTEXTURE9 GetSunGlareAtm() const { return pSunGlareAtm; }

	/**
	 * \brief Render any shadows cast by vessels on planet surfaces
	 * \param hPlanet handle of planet to cast shadows on
	 * \param depth shadow darkness parameter (0=none, 1=black)
	 * \note Uses stencil buffering if available and requested. Otherwise shadows
	 *   are pure black.
	 * \note Requests for any planet other than that closest to the camera
	 *   are ignored.
	 */
	void RenderVesselShadows(OBJHANDLE hPlanet, float depth) const;

	/**
	 * \brief Create a visual for a new vessel if within visual range.
	 * \param hVessel vessel object handle
	 */
	void NewVessel (OBJHANDLE hVessel);

	/**
	 * \brief Delete a vessel visual prior to destruction of the logical vessel.
	 * \param hVessel vessel object handle
	 */
	void DeleteVessel (OBJHANDLE hVessel);

	void AddParticleStream (class D3D9ParticleStream *_pstream);

	// ORO patch (y): count / read back the reconstructed specs of a vessel's STOCK
	// exhaust particle streams (reentry streams and ORO's own exempted replacement
	// streams are not counted). Returns the count; fills the outputs when idx is
	// valid. pos/dir are the stream's attach point and thrust direction, VESSEL frame.
	int GetExhaustStreamSpec (OBJHANDLE hVessel, int idx, PARTICLESTREAMSPEC* out,
	                          VECTOR3* pos, VECTOR3* dir);
	void DelParticleStream (DWORD idx);

	void AddLocalLight(const LightEmitter *le, const vObject *vo);
	void ClearLocalLights();

	/**
	 * \brief Get object radius in pixels using oapiCameraGlobalPos()
	 * \param hObj object handle
	 */
	double GetObjectAppRad(OBJHANDLE hObj) const;

	/**
	 * \brief Get object radius in pixels using a custom camera location.
	 * \param hObj object handle
	 */
	double GetObjectAppRad2(OBJHANDLE hObj) const;

	// Picking Functions ============================================================================================================
	//
	D3DXVECTOR3		GetPickingRay(short x, short y);
	D3D9Pick		PickScene(short xpos, short ypos);
	TILEPICK		PickSurface(short xpos, short ypos);
	D3D9Pick		PickMesh(DEVMESHHANDLE hMesh, const LPD3DXMATRIX pW, short xpos, short ypos);

	void			ClearOmitFlags();
	bool			IsRendering() const { return bRendering; }


	// Custom Camera Interface ======================================================================================================
	//
	CAMERAHANDLE	SetupCustomCamera(CAMERAHANDLE hCamera, OBJHANDLE hVessel, MATRIX3 &mRot, VECTOR3 &pos, double fov, SURFHANDLE hSurf, DWORD flags);
	int				DeleteCustomCamera(CAMERAHANDLE hCamera);
	void			DeleteAllCustomCameras();
	void			CustomCameraOnOff(CAMERAHANDLE hCamera, bool bOn);
	void			RenderCustomCameraView(CAMREC *cCur);


	// Camera Matrix Access =========================================================================================================
	//
	void			   GetAdjProjViewMatrix(LPD3DXMATRIX mP, float znear, float zfar);
	const LPD3DXMATRIX GetProjectionViewMatrix() const { return (LPD3DXMATRIX)&Camera.mProjView; }
	const LPD3DXMATRIX GetProjectionMatrix() const { return (LPD3DXMATRIX)&Camera.mProj; }
	const LPD3DXMATRIX GetViewMatrix() const { return (LPD3DXMATRIX)&Camera.mView; }


	// Main Camera Interface =========================================================================================================
	//
	void			SetCameraAperture(float _ap, float _as);
	void			SetCameraFrustumLimits(double nearlimit, double farlimit);
	float			GetDepthResolution(float dist) const;
	float			CameraInSpace() const;

					// Acquire camera information from the Orbiter and initialize internal camera setup
	bool			UpdateCameraFromOrbiter(DWORD dwPass);

					// Manually initialize client's internal camera setup
	bool			SetupInternalCamera(D3DXMATRIX *mView, VECTOR3 *pos, double apr, double asp);

					// Pan Camera in a mesh debugger
	bool			CameraPan(VECTOR3 pan, double speed);

					// Check if a sphere located in pCnt (relative to cam) with a specified radius is visible in a camera
	bool			IsVisibleInCamera(const D3DXVECTOR3 *pCnt, float radius);
	bool			IsProxyMesh();
	bool            CameraDirection2Viewport(const VECTOR3 &dir, int &x, int &y);
	double			GetTanAp() const { return tan(Camera.aperture); }
	float			GetCameraAspect() const { return (float)Camera.aspect; }
	float			GetCameraFarPlane() const { return Camera.farplane; }
	float			GetCameraNearPlane() const { return Camera.nearplane; }
	float			GetCameraAperture() const { return (float)Camera.aperture; }
	VECTOR3			GetCameraGPos() const { return Camera.pos; }
	// ORO patch (k): snapshot of the camera the CURRENT frame is being rendered with -
	// position, rotation and tan(aperture) in the exact form oapiCamera* hand out, but read
	// AT RENDER TIME. Module pre/post-step hooks both run before Orbiter updates the camera,
	// so anything a module projects there is one full step stale; world-anchored geometry at
	// close range (the ORO reentry trail) visibly jumps by one frame of camera travel.
	// Refreshed by UpdateCameraFromOrbiter at the top of every pass that renders the scene.
	// ORO patch (u): while the wet-mirror render proc runs, GetRenderCam reports the
	// MIRRORED camera instead of the real one. An addon drawing screen-space geometry
	// into the reflection needs the camera that pass is rendering with, and it already
	// asks this function for it - so the whole plumbing is a substitution, not a second
	// API. See the pass in Scene.cpp for how the two fields are built.
	bool			bMirrorCam;
	VECTOR3			mirrorCamPos;
	MATRIX3			mirrorCamRot;

	void			GetRenderCam(VECTOR3* p, MATRIX3* r, double* t) const {
						if (bMirrorCam) { *p = mirrorCamPos; *r = mirrorCamRot; }
						else            { *p = Camera.pos;   *r = Camera.grot;  }
						*t = tan((double)Camera.aperture); }
	VECTOR3			GetCameraGDir() const { return Camera.dir; }
	OBJHANDLE		GetCameraProxyBody() const { return Camera.hObj_proxy; }
	vPlanet *		GetCameraProxyVisual() const { return Camera.vProxy; }
	double			GetCameraAltitude() const { return Camera.alt_proxy; }
	OBJHANDLE		GetCameraNearBody() const { return Camera.hNear; }
	vPlanet *		GetCameraNearVisual() const { return Camera.vNear; }
	double			GetCameraNearAltitude() const { return Camera.alt_near; }
	double			GetCameraElevation() const { return Camera.elev; }
	void			GetCameraLngLat(double *lng, double *lat) const;
	bool			WorldToScreenSpace(const VECTOR3& rdir, oapi::IVECTOR2* pt, D3DXMATRIX* pVP = NULL, float clip = 1.0f);
	bool			WorldToScreenSpace2(const VECTOR3& rdir, oapi::FVECTOR2* pt, D3DXMATRIX* pVP = NULL, float clip = 1.0f);

	DWORD			GetFrameId() const { return dwFrameId; }

	const D3DXVECTOR3 *GetCameraX() const { return &Camera.x; }
	const D3DXVECTOR3 *GetCameraY() const { return &Camera.y; }
	const D3DXVECTOR3 *GetCameraZ() const { return &Camera.z; }

	const CAMERA *	GetCamera() const { return &Camera; }

	void			PushCamera();	// Push current camera onto a stack
	void			PopCamera();	// Restore a camera from a stack
	FMATRIX4		PushCameraFrustumLimits(float nearlimit, float farlimit);
	FMATRIX4		PopCameraFrustumLimits();
	


	// Visual Management =========================================================================================================
	//
	void			GetLVLH(vVessel *vV, D3DXVECTOR3 *up, D3DXVECTOR3 *nr, D3DXVECTOR3 *cp);
	class vObject *	GetVisObject(OBJHANDLE hObj) const;
	class vVessel *	GetFocusVisual() const { return vFocus; }
	void			CheckVisual(OBJHANDLE hObj);
	double			GetFocusGroundAltitude() const;
	double			GetTargetGroundAltitude() const;
	double			GetTargetElevation() const;
	std::set<vVessel *> GetVessels(double max_dst, bool bActive = true);

	// Locate the visual for hObj in the list if present, or return
	// NULL if not found

protected:

	/**
	 * \brief Render a single marker at a given global position
	 * \param hDC device context
	 * \param gpos global position (ecliptic frame)
	 * \param label1 label above marker
	 * \param label2 label below marker
	 * \param mode marker shape
	 * \param scale marker size
	 */
	void RenderObjectMarker(oapi::Sketchpad *pSkp, const VECTOR3 &gpos, const std::string& label1, const std::string& label2, int mode, int scale);

	void RenderGlares();

private:
	void		ComputeLocalLightsVisibility();
	DWORD		GetActiveParticleEffectCount();
	float		ComputeNearClipPlane();
	void		VisualizeCubeMap(LPDIRECT3DCUBETEXTURE9 pCube, int mip);
	VOBJREC *	FindVisual (OBJHANDLE hObj) const;
	void		RenderVesselMarker(vVessel *vV, D3D9Pad *pSketch);

	// Locate the visual for hObj in the list if present, or return
	// NULL if not found

	void DelVisualRec (VOBJREC *pv);
	void DeleteAllVisuals();
	// Delete entry pv from the list of visuals

	VOBJREC *AddVisualRec (OBJHANDLE hObj);
	// Add an entry for object hObj in the list of visuals

	VECTOR3 SkyColour ();
	// Sky background colour based on atmospheric parameters of closest planet

	void InitGDIResources();
	void ExitGDIResources();

	void FreePooledSketchpads();      ///< Release pooled Sketchpad instances



	// Scene variables ================================================================
	//
	oapi::D3D9Client* gc;
	LPDIRECT3DDEVICE9 pDevice; // render device
	DWORD viewW, viewH;        // render viewport size
	DWORD stencilDepth;        // stencil buffer bit depth
	D3D9CelestialSphere* m_celSphere; // celestial sphere background
	DWORD iVCheck;             // index of last object checked for visibility
	bool  bLocalLight;         // enable local light sources
	bool  surfLabelsActive;    // v.2 surface labels activated?

	OBJHANDLE hSun;

	D3D9ParticleStream **pstream; // list of particle streams
	DWORD                nstream; // number of streams


	D3DCOLOR bg_rgba;          // ambient background colour

	// GDI resources ====================================================================
	//
	oapi::Font *label_font[4];

	std::list<vVessel *> RenderList;
	std::list<vVessel *> SmapRenderList;
	std::list<vVessel *> Casters;
	std::stack<CAMERA>	CameraStack;
	std::stack<DWORD>	PassStack;
	std::stack<FRUSTUM> FrustumStack;


	CAMERA		Camera;
	D3D9Light*	Lights;
	// ORO patch (z3): who each scene light belongs to. D3D9Light does not keep its
	// vObject, and the local-light shadow pass must EXCLUDE the emitter's own vessel
	// from the caster set - emitter positions are routinely authored INSIDE the hull
	// (the DG dock light sits in the nose), so an honest self-shadow would black the
	// whole beam out. Parallel to Lights[], maintained by Add/ClearLocalLights.
	const vObject* LightOwners[MAX_SCENE_LIGHTS];
	D3D9Sun	    sunLight;

	VECTOR3		sky_color;
	double      bglvl;

	float		fDisplayScale;
	float		lmaxdst2;
	DWORD		nLights;
	DWORD		nplanets;		// Number of distance sorted planets to render
	DWORD		dwTurn;
	DWORD		dwFrameId;
	DWORD		camIndex;
	DWORD		RenderFlags;
	bool		bRendering;

	oapi::Font *pAxisFont;
	oapi::Font *pLabelFont;
	oapi::Font *pDebugFont;

	SurfNative *pLblSrf;

	class ImageProcessing *pLightBlur, *pBlur, *pGDIOverlay, *pIrradiance, *pVisDepth, *pCreateGlare;
	class ShaderClass *pLocalCompute, *pRenderGlares;

	class vVessel *vFocus;
	VOBJREC *vobjEnv, *vobjIrd;
	double dVisualAppRad;

	FVECTOR2 DepthSampleKernel[57];

	LPDIRECT3DTEXTURE9 pSunTex, pLightGlare, pSunGlare, pSunGlareAtm;
	LPDIRECT3DTEXTURE9 pLocalResults;
	LPDIRECT3DSURFACE9 pLocalResultsSL;

	// Blur Sampling Kernel ==============================================================
	LPDIRECT3DCUBETEXTURE9 pBlrTemp[5];
	LPDIRECT3DCUBETEXTURE9 pIrradTemp;
	LPDIRECT3DTEXTURE9 pIrradTemp2, pIrradTemp3;

	// Deferred Experiment ===============================================================
	//
	LPDIRECT3DSURFACE9 psgBuffer[GBUF_COUNT];
	LPDIRECT3DTEXTURE9 ptgBuffer[GBUF_COUNT];
	LPDIRECT3DSURFACE9 pOffscreenTarget;
	LPDIRECT3DTEXTURE9 pTextures[TEX_COUNT];

	LPDIRECT3DSURFACE9 pEnvDS, pIrradDS, pDepthNormalDS;
	LPDIRECT3DTEXTURE9 ptWetRefl;		// ORO patch (s) part 6: wet-ground planar reflection, half res
	// ORO patch (w): PLANET-SHINE SHADOWS - a depth map of the focus vessel's
	// attachment assembly along the PLANET direction, so Earth glow cannot light
	// the inside of a closed payload bay. Own target: the sun's LOD targets are
	// repainted by the sun's own pass right after (the patch-(f) reuse trap).
	SHADOWMAPPARAM pshn;
	LPDIRECT3DTEXTURE9 ptPShn;
	LPDIRECT3DSURFACE9 psPShn;
	std::set<const class vVessel*> pshnSet;	// this frame's receiver set (the assembly)
	bool EnsurePShnTarget();
	// ORO patch (w) part 2: a COPY of the focus vessel's SUN shadow map, taken after
	// the main scene renders it. Stock binds the sun map in the MAIN SCENE only, so
	// every secondary render (probe cubes, both mirror passes) draws the assembly
	// fully sunlit - invisible in stock (probes exclude self), glaring the moment an
	// interior probe renders its own closed bay. The copy is one frame stale, which
	// is bounded and invisible; the LOD target itself is repainted by other passes.
	SHADOWMAPPARAM sunCpy;
	LPDIRECT3DTEXTURE9 ptSunCpy;
	LPDIRECT3DSURFACE9 psSunCpy;
	bool sunCpyLive;
	bool EnsureSunCpyTarget();
	LPDIRECT3DTEXTURE9 ptRflPln[2];		// ORO patch (v) part 2: vessel planar mirrors, half res
	LPDIRECT3DSURFACE9 psRflPln[2];		// ... their level-0 surfaces
	const class vVessel* pRflPlnVes;	// ... whose planes they hold this frame
	int nRflPlnLive;					// ... bitmask of planes rendered this frame
	D3DXVECTOR4 rflPlnEq[2];			// ... plane equation (N, d), camera-centred world
	float rflPlnDist[2];				// ... RDIST per plane [m] (the curvature warp)
	LPDIRECT3DSURFACE9 psWetRefl;		// ORO patch (s) part 6: its level-0 surface (the RT binding)
	LPDIRECT3DSURFACE9 psWetReflDS;		// ORO patch (s) part 6: its depth-stencil
	bool bWetReflLive;					// ORO patch (s) part 6: the RT holds this frame's mirror
	// ORO patch (z3): the local-light shadow map's DEDICATED target. Not one of the
	// sun's LOD targets on purpose - those are repainted by other passes within the
	// frame (the patch-(f)/(w) reuse trap), and a dedicated map needs no copy-out.
	LPDIRECT3DTEXTURE9 ptLclShm;
	LPDIRECT3DSURFACE9 psLclShm;
	LPDIRECT3DSURFACE9 psLclShmDS;
	void RenderLocalLightShadowMap();
	// ORO patch (z3) round 2c: terrain casters - registered tiles (AddRef'd VB/IB,
	// released when they age out) + the tiny depth-only tile shader (TileShdVS/PS
	// in NewPlanet.hlsl). ⚠️ Entries PERSIST for ~2 s after last sighting: the
	// registration source is the CAMERA's rendered tile set, and without the TTL a
	// camera rotation churned the caster list and strobed the beam's shadows (his
	// daylight rotation test). cpos = the camera's GLOBAL position at registration,
	// so a stale entry's camera-relative mW can be re-anchored when the camera has
	// TRANSLATED since (world orientation is fixed; only the origin moves).
	struct LCLTILECASTER {
		LPDIRECT3DVERTEXBUFFER9 pVB;
		LPDIRECT3DINDEXBUFFER9 pIB;
		DWORD nv, nf;
		D3DXMATRIX mW;
		DWORD stamp;		// dwFrameId at last registration
		// ORO patch (ab) rounds 3+4 (2026-09-06): THE TILE RIDES ITS PLANET. A stale entry
		// used to be re-anchored by the camera's translation through the GLOBAL frame -
		// but Earth carries the terrain ~150 m per stepped frame at 30 km/s (invariant
		// 21a's barycentric lesson), so every stale tile sat that far off on the frames
		// the sim stepped and exact on the others: the KSC blink. Round 3 re-anchored
		// relative to the planet's POSITION and the blink survived only at high time
		// warp, where the planet also ROTATES between registration and draw and a
		// kilometre-wide tile's corners swing metres. So the tile is stored where it
		// actually lives - in the planet's own frame, origin and basis rows, in DOUBLES
		// (the planet centre is ~6400 km off, past float's metre) - and each consumer
		// rebuilds its camera-relative matrix from the planet's CURRENT rotation and
		// position (OroTileFromPlanet). Exact at any warp; mW stays as the fallback.
		OBJHANDLE hPlanet;	// the tile's planet
		VECTOR3 lpos;		// tile origin, planet-local
		VECTOR3 lrow[3];	// the matrix's three basis rows, planet-local (D3DX is row-vector)
		D3DXVECTOR3 bs;		// ORO patch (ab): bounding-sphere centre, camera-relative at registration
		float bsRad;		//   ...and its radius - the consumers filter by range now, not the registrar
	};
	mutable std::vector<LCLTILECASTER> LclTiles;	// mutable: fed from the CONST tile render
	class ShaderClass* pTileShd;
	class ShaderClass* pTileDepth;	// ORO patch (ab): the tile normal+depth shader (TileDepthVS/PS)
	void RenderLocalLightShadowMap2(const std::vector<LCLTILECASTER>& tiles);
	// ORO patch (ab) round 4: the planet-frame store and its inverse - see LCLTILECASTER
	static void OroTileToPlanet(const D3DXMATRIX& mW, const VECTOR3& cam, LCLTILECASTER& t);
	static void OroTileFromPlanet(const LCLTILECASTER& t, const VECTOR3& cam, D3DXMATRIX& mW);
public:
	// ORO patch (s) part 6: the terrain shader samples the same mirror as the base tiles
	LPDIRECT3DTEXTURE9 GetWetReflTex() const { return bWetReflLive ? ptWetRefl : NULL; }
	// ORO patch (v) part 2: the vessel planar-mirror targets. Filled only for the
	// vessel whose planes rendered this frame; everyone else gets zero, which is
	// what keeps the mirrored RT off every other hull.
	int GetRflPlaneTex(const class vVessel* v, LPDIRECT3DTEXTURE9* out, D3DXVECTOR4* eq, float* dist, int mx) const {
		if (v != pRflPlnVes || !pRflPlnVes) return 0;
		int n = 0;
		for (int i = 0; i < 2 && n < mx; i++) {
			out[n]  = (nRflPlnLive & (1 << i)) ? ptRflPln[i] : NULL;
			if (eq)   eq[n]   = rflPlnEq[i];
			if (dist) dist[n] = rflPlnDist[i];
			n++;
		}
		return n;
	}
	bool IsWetReflLive() const { return bWetReflLive; }
	// ORO patch (ae): the cascade atlas for the terrain (Surfmgr2) - live only after
	// RenderCascadeShadows ran this frame
	bool CascadesLive() const { return cascLive; }
	// round 8: the local light's shadow map sits in the atlas' spare row this frame
	// (cell 0 of row 3, half-size) - the terrain samples it there beside the sun's
	// cascades, no sampler slot borrowed, so the (z3) day gate no longer applies
	bool CascadeHasLocal() const { return cascLclLive; }
	LPDIRECT3DTEXTURE9 GetCascadeAtlas() const { return ptCasc; }
	void GetCascadeConstants(D3DXVECTOR4* basis, D3DXVECTOR4* A, D3DXVECTOR4* tx, D3DXVECTOR4* split, D3DXVECTOR4* atl, bool live) const;
	// ORO patch (w): the planet-shine shadow map, for assembly members this frame.
	const SHADOWMAPPARAM* GetPShn(const class vVessel* v) const {
		return (ptPShn && pshnSet.count(v)) ? &pshn : NULL;
	}
	// ORO patch (w) part 2: last frame's sun shadow map, for assembly members in
	// secondary passes (the main scene binds the live map itself).
	const SHADOWMAPPARAM* GetSunShdCopy(const class vVessel* v) const {
		return (sunCpyLive && ptSunCpy && pshnSet.count(v)) ? &sunCpy : NULL;
	}
private:
	// ORO patch (ae): THE CASCADED SUN SHADOW ATLAS (2026-09-06, terrain shadowing mode 3
	// "Cascaded (ORO)") - one truth for the sun's shadows. Four ortho maps in ONE R32F
	// texture (2x2 slots of ShadowCascadeSize): slot 0 fitted to the focus vessel's
	// bounding sphere (stock's fit, the crispest, the grid riding the hull; since round
	// 5 it carries the focus vessel ALONE), slots 1-3 to camera frustum slices (bounding
	// spheres, texel-SNAPPED in the planet's frame so the grid never swims as the camera
	// moves; every caster BUT the focus vessel - its whole shadow lives in slot 0, in
	// light space a silhouette never leaves its own box). Casters: vessels
	// (vVessel::Render under RENDERPASS_SHADOWMAP with smap.mViewProj swapped),
	// base structures (RenderBaseDepth opt 0), terrain tiles (the (z3)/(ab) registry,
	// rebuilt in the planet's frame). Receivers: terrain, base tiles, and the whole mesh
	// family (hulls, structures, runways, pads) through one sampler each. In mode 3 the
	// stencil sheets are not drawn at all; the stock per-vessel maps stay for the hull's
	// own crisp self-shadow, the cascade term is min()'d onto them.
	// ROUND 2 (his Brighton Beach flight: nothing beyond 600 m, blobs at 22 m texels):
	// SIX slots. The atlas is 2 x cascSize square; in quarter-atlas units the two near
	// cascades take 2x2 (full size), the focus box and the three far cascades 1x1, and
	// the bottom-right quarter is spare (the light atlas, later). Splits 50 / 450 / 2000
	// / 8000 m, far = ShadowCascadeFar. The focus slot is ALWAYS fitted.
	// ROUND 7 (his Brighton Beach telephoto flight, 450 m at FOV 30: the base straddling
	// the 450 m split, and a landed ShuttleA with NO shadow beyond it): NINE slots in a
	// 3 x 2 atlas (6144 x 4096 at 2048). Slot 3 (450-2000 m) is full-size now (1 m
	// texels); slots 6-8 are the boxes of the THREE NEAREST OTHER VESSELS, each hull
	// alone, vessel-anchored like slot 0 - a parked hull's shadow is crisp from any
	// distance, which is what the focus vessel already had. EVERY SLOT SHARES ONE LIGHT
	// BASIS (U, V across the light, L along it): a receiver projects into light space
	// once, in metres, and each slot is an offset and a scale - two float4 (A = centre
	// u, centre v, near-plane depth, 1/range; B = atlas u, v offset, slot uv width,
	// texel) instead of a matrix, so the mesh family sees all nine within ps_3_0's 224
	// constant registers. And THE RECEIVER-PLANE DEPTH BIAS: the receiver's normal gives
	// the exact depth its own surface has at each neighbouring texel (g = N.U / N.L per
	// metre), so the slope term that used to be a blind bias (2.5 texels x tan - 24 m at
	// slot 3 under a low sun, which is what ate the ShuttleA's 4 m of clearance) is exact
	// and what remains is under a texel. The bottom row (six cells) is spare - the light
	// atlas, later.
	struct CASCADE {
		D3DXMATRIX  mView, mVP;
		D3DXVECTOR3 c;
		float r, zf, texel, range;
		int   px, py, size;     // the slot's rect in the atlas, pixels
		float uvx, uvy, scale;  // ...and in uv
		D3DXVECTOR4 A, B;       // round 7: the receiver's compact form (FitCascade)
		bool  live;
	};
	CASCADE casc[9];
	D3DXVECTOR3 cascU, cascV, cascL;   // the frame's light basis, shared by every slot
	class vVessel* cascVes[3];         // the hulls in slots 6-8 this frame
	// ROUND 5 - THE LATTICE ORIGIN (his round-4 flight: a fast shimmer at every edge up
	// close, a slow one on the far cascades). The snap quantised the light-space
	// coordinates of the box centre measured from the PLANET'S CENTRE, in a light basis
	// fixed in the global frame - and the planet rotates. A lattice anchored 1737 km
	// away and rotated by omega sweeps the terrain at omega*R: 3.5 m/s on the Moon
	// (130 texels/s on the near slots = a random rasterisation phase every frame; 0.1-2
	// texels/s on the far slots = the slow crawl he saw), 460 m/s on Earth. The lattice
	// must be anchored NEAR the scene: each snapped slot keeps its own planet-local
	// anchor and walks it to the camera every frame in WHOLE lattice steps (the phase is
	// preserved, so the walk never pops), so the rotation is applied about a point at
	// most a texel from the camera and a receiver D metres away sees the lattice move at
	// D*omega - a texel every ten seconds on Earth's far slots, nothing on the Moon.
	// Slot 0 is vessel-anchored (stock's fit, no snap) and carries the FOCUS VESSEL
	// ONLY; slots 1-5 carry everything else; receivers min() the two. A parked hull's
	// shadow is then rasterised identically every frame, whatever the camera does.
	VECTOR3   cascAnch[9];      // planet-local lattice origin per slot
	float     cascAnchTexel[9]; // the texel it was laid down for (a new texel = a new lattice)
	bool      cascAnchOK[9];
	OBJHANDLE cascAnchPlanet;
	float     cascDbgAnchD;     // INSTRUMENT: slot 1's anchor-to-camera distance
	float cascSplit[4];
	D3DXVECTOR4 cascAtl;    // the last atlas vector pushed (1/A, -, on, far) - restored after the cockpit pass
	LPDIRECT3DTEXTURE9 ptCasc;
	LPDIRECT3DSURFACE9 psCasc, psCascDS;
	int  cascSize;
	bool cascLive;
	bool cascLclLive;
	void RenderCascadeShadows();
	// margin = the depth window's extension TOWARD the light (casters outside the box),
	// reach = its extension PAST the box along the light (receivers beyond the caster)
	bool FitCascade(int i, const D3DXVECTOR3& c0, float r, float margin, float reach, bool snap);
	void RenderCascadeCasters(int i);
	LPDIRECT3DSURFACE9 psShmDS[SHM_LOD_COUNT];
	LPDIRECT3DSURFACE9 psShmRT[SHM_LOD_COUNT];
	LPDIRECT3DTEXTURE9 ptShmRT[SHM_LOD_COUNT];

	LocalLightsCompute LLCBuf[MAX_SCENE_LIGHTS + 1];

	// Rendering Technique related parameters ============================================
	//
	static ID3DXEffect	*FX;
	static D3DXHANDLE	eLine;
	static D3DXHANDLE	eStar;
	static D3DXHANDLE	eWVP;
	static D3DXHANDLE	eColor;
	static D3DXHANDLE	eTex0;

};

#endif // !__SCENE_H
