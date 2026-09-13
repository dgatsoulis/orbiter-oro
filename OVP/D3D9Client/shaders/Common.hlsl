
#define KERNEL_RADIUS 2.0f
#define SHADOW_THRESHOLD 0.1f       // 0.3 to 7.0



// ============================================================================
//
float4 Paraboloidal_LVLH(sampler s, float3 i)
{
	float z = dot(gCameraPos, i);
	float2 p = float2(dot(gEast, i), dot(gNorth, i)) / (1.0f + abs(z));
	p *= float2(0.2273f, 0.4545f);
	float4 A = tex2D(s, p + float2(0.25f, 0.5f));
	float4 B = tex2D(s, p + float2(0.75f, 0.5f));
	return lerp(A, B, smoothstep(-0.03, 0.03, z));
}

float3 Sq(float3 x)
{
	return x*x;
}

float4 Sq(float4 x)
{
	return x*x;
}


// ==========================================================================================================
// Local light sources
// ==========================================================================================================


float3 Light_fx(float3 x)
{
	return saturate(x);  //1.5 - exp2(-x.rgb)*1.5f;
}


// ==========================================================================================================
// ORO patch (z3) round 2b / (ah) step 4: the LOCAL-LIGHT shadow maps reach the vessels. Up to
// LCLMAPS perspective depth maps a frame - one per shadow-casting SPOT light, or per CLUSTER of
// lights standing together (Scene::RenderLocalLightShadowMap) - read from ONE texture: the scratch
// map (one map live, full size), the local atlas (modes 0-2) or the cascade atlas' spare row
// (Cascaded), as gLclShd/gLclAtl describe. A light names the map that shadows it in its OWN struct:
// gLights[i].diffuse.a = cell + 1 (0 = none; the scene assigns it, Mesh.cpp copies the struct), so
// no per-mesh slot is needed. A map's frame is DERIVED here from two float4 - (origin, cell*10 +
// tan(fov/2)) and (axis, range) - exactly as the CPU built it (LookAtRH + PerspectiveFovRH, near =
// ORO_LCL_NF x range), which is what lets six maps cost twelve registers instead of six matrices.
// Same depth convention as the sun's map (ShdMapPS stores 1 - z/w); tex2Dlod because the calls sit
// behind comparisons on float uniforms - dynamic flow, and ps_3_0 refuses divergent gradients there
// (the map has one mip anyway). Returns the LIT factor.
// ==========================================================================================================

#ifndef LCLMAPS
#define LCLMAPS 1
#endif
#define ORO_LCL_NF 0.0075f
#ifndef ORO_LCL_SHDBLOCKS
#define ORO_LCL_SHDBLOCKS 2
#endif
// blocks of four lights that test shadows (two = the eight strongest; four would put 16x over the 4096-slot cap)

uniform extern float4   gLclShdP[LCLMAPS];			// per map: origin xyz, w = cell * 10 + tan(fov/2)
uniform extern float4   gLclShdD[LCLMAPS];			// per map: axis xyz (unit), w = range (the far plane)
uniform extern float4   gLclShd = {0, 1, 0, 1};		// live maps, cells per row, first row's v, cell size in texels
uniform extern float4   gLclAtl = {1, 1, 0, 0};		// cell width and height in uv, one texel in uv
uniform extern texture  gLclShmTex;

sampler LclShmS = sampler_state
{
	Texture = <gLclShmTex>;
	MinFilter = POINT;
	MagFilter = POINT;
	MipFilter = NONE;
	AddressU = CLAMP;
	AddressV = CLAMP;
};

float OroLclShadow(float3 posW, float3 nrmW, float idx, float dstL, float nl)
{
	// the map this light owns (idx = cell + 1), by select - a uniform-indexed array is not natively
	// addressable in ps_3_0, and the compiler's own chain would be this one
	// a CUBE light (ORO patch (ah) step 5, index 100 + base cell) keeps its five faces at cells
	// base..base+4; face 0 (down) carries the origin, the axis -up and the shared tan(fov/2). The
	// face is picked here from the dominant axis of the light-to-pixel vector in the cube's basis,
	// rebuilt from -up with the CPU's own rule - so one fetch serves all five faces.
#ifdef _LCLCUBE
	const bool  cube = (idx > 99.5f);
#else
	const bool  cube = false;							// no cube lights compiled in: the pick below folds away
#endif
	const float mi   = cube ? (idx - 99.0f) : idx;			// the map to fetch, 1-based
	float4 P  = gLclShdP[0];
	float4 Dm = gLclShdD[0];
#if LCLMAPS > 1
	[unroll] for (int k = 1; k < LCLMAPS; k++) {
		if (abs(mi - (float)(k + 1)) < 0.5f) { P = gLclShdP[k]; Dm = gLclShdD[k]; }
	}
#endif
	float cell = floor(P.w * 0.1f);
	const float  t = P.w - cell * 10.0f;						// tan(fov/2)
	const float  f = Dm.w;
	float3 D = Dm.xyz;

	// NORMAL-OFFSET (tpm = world texel per metre of light distance): push the receiver point
	// ~2 map texels out along its surface normal - the ordinary-acne cure.
	const float tpm = 2.0f * t / gLclShd.w;
	posW += nrmW * (dstL * tpm * 2.0f);
	const float3 rel = posW - P.xyz;

#ifdef _LCLCUBE
	if (cube) {
		const float3 up  = -D;
		const float3 aux = (abs(up.y) < 0.9f) ? float3(0, 1, 0) : float3(1, 0, 0);
		const float3 X   = normalize(cross(up, aux));
		const float3 Z   = cross(X, up);
		const float3 a   = float3(dot(rel, X), dot(rel, up), dot(rel, Z));
		const float3 aa  = abs(a);
		if (aa.y >= aa.x && aa.y >= aa.z) {
			if (a.y > 0.0f) return 1.0f;							// the sky face has no map: lit
		}
		else if (aa.x >= aa.z) { D = (a.x > 0.0f) ? X : -X; cell += (a.x > 0.0f) ? 1.0f : 2.0f; }
		else                   { D = (a.z > 0.0f) ? Z : -Z; cell += (a.z > 0.0f) ? 3.0f : 4.0f; }
	}
#endif

	const float3 upv = (abs(D.y) < 0.9f) ? float3(0, 1, 0) : float3(1, 0, 0);
	const float3 R   = normalize(cross(D, upv));				// LookAtRH: x = cross(up, -D)
	const float3 U   = cross(R, D);								//           y = cross(-D, x)
	const float  d   = dot(rel, D);
	if (d < 0.001f) return 1.0f;								// behind the light: lit
	const float2 xy  = float2(dot(rel, R), dot(rel, U)) / (d * t);
	if (abs(xy.x) > 1.0f || abs(xy.y) > 1.0f) return 1.0f;		// outside the map: lit
	float2 sp = xy * float2(0.5f, -0.5f) + 0.5f;

	// the receiver's depth as the caster wrote it - 1 - z/w for near = ORO_LCL_NF x far - with
	// the RELATIVE bias, then the TEXEL-FOOTPRINT BIAS - the grazing cure: clear exactly the
	// ray-depth span one PCF-widened texel covers on this surface (c x f = near x far / (far - near))
	const float c  = ORO_LCL_NF / (1.0f - ORO_LCL_NF);
	float pd = c * (f / d - 1.0f);
	pd = pd * 1.012f + 0.0008f;
	const float grz = sqrt(saturate(1.0f - nl * nl)) / max(nl, 0.05f);
	pd += (tpm * 3.0f) * grz * (c * f) / max(dstL, 1.0f);

	// the cell in the texture, and 2x2 BILINEAR PCF inside it (ORO patch (ah) step 2: the four
	// texels around the sample, weighted by the sub-texel position - smooth edges; the clamp keeps
	// every tap in the cell)
	const float  col = cell - gLclShd.y * floor(cell / gLclShd.y);
	const float  row = floor(cell / gLclShd.y);
	const float2 uvo = float2(col * gLclAtl.x, gLclShd.z + row * gLclAtl.y);
	const float2 ts  = gLclAtl.zw;
	sp = clamp(uvo + sp * gLclAtl.xy, uvo + ts * 2.0f, uvo + gLclAtl.xy - ts * 2.0f);
	float2 tx = sp / ts - 0.5f;
	float2 fr = frac(tx);
	float2 b  = (floor(tx) + 0.5f) * ts;
	float s00 = (tex2Dlod(LclShmS, float4(b, 0, 0)).r > pd) ? 1.0f : 0.0f;
	float s10 = (tex2Dlod(LclShmS, float4(b + float2(ts.x, 0), 0, 0)).r > pd) ? 1.0f : 0.0f;
	float s01 = (tex2Dlod(LclShmS, float4(b + float2(0, ts.y), 0, 0)).r > pd) ? 1.0f : 0.0f;
	float s11 = (tex2Dlod(LclShmS, float4(b + ts, 0, 0)).r > pd) ? 1.0f : 0.0f;
	return 1.0f - lerp(lerp(s00, s10, fr.x), lerp(s01, s11, fr.x), fr.y);
}


void LocalLights(
	out float3 diff_out,
	out float3 spec_out,
	in float3 nrmW,
	in float3 posW,
	in float sp,
	uniform int x,
	uniform bool bSpec)
{

	float3 posWN = normalize(-posW);
	float3 p[4];
	float4 spe;
	int i;

	// Relative positions
	[unroll] for (i = 0; i < 4; i++) p[i] = posW - gLights[i + x].position.xyz;

	// Square distances
	float4 sd;
	[unroll] for (i = 0; i < 4; i++) sd[i] = dot(p[i], p[i]);

	// Normalize
	sd = rsqrt(sd);
	[unroll] for (i = 0; i < 4; i++) p[i] *= sd[i];

	// Distances
	float4 dst = rcp(sd);

	// Attennuation factors
	float4 att;
	[unroll] for (i = 0; i < 4; i++) att[i] = dot(gLights[i + x].attenuation.xyz, float3(1.0, dst[i], dst[i] * dst[i]));

	att = rcp(att);

	// Spotlight factors
	float4 spt;
	[unroll] for (i = 0; i < 4; i++) {
		spt[i] = (dot(p[i], gLights[i + x].direction.xyz) - gLights[i + x].position.w) * gLights[i + x].direction.w;
		if (gLights[i + x].attenuation.w == 0) spt[i] = 1.0f;
	}

	spt = saturate(spt);

	// Diffuse light factors
	float4 dif;
	[unroll] for (i = 0; i < 4; i++) dif[i] = dot(-p[i], nrmW);

	dif = saturate(dif);
	dif *= (att*spt);

	// Specular lights factors
	if (bSpec) {

		[unroll] for (i = 0; i < 4; i++) spe[i] = dot(reflect(p[i], nrmW), posWN) * (dif[i] > 0);

		spe = pow(saturate(spe), sp);
		spe *= (att*spt);
	}

	// ORO patch (z3) round 2b / (ah) step 4: every light of this block that owns a shadow map
	// loses its light behind a caster. The map index rides the light's own struct (diffuse.a =
	// cell + 1), so no slot uniform is needed; the branches are on constants - coherent, and a
	// few instructions for the common mesh no shadowed light reaches. ONLY THE FIRST TWO BLOCKS
	// (a mesh's eight strongest lights) test shadows: a ps_3_0 shader is validated against the
	// device's instruction-slot cap - 4096 on NVIDIA (tools/fxeff reads and checks it) - and the
	// tests inlined for sixteen lights put every pass over it (4758 slots: the hulls vanished at
	// 20 fps with nothing logged, 2026-09-09). A spot ranked below eight on a mesh is faint on
	// it. x is a compile-time block offset, so this folds away for blocks 8 and 12.
	if (x < ORO_LCL_SHDBLOCKS * 4) {
		const float4 sidx = float4(gLights[x].diffuse.a, gLights[x + 1].diffuse.a, gLights[x + 2].diffuse.a, gLights[x + 3].diffuse.a);
		[branch] if (gLclShd.x > 0.5f && any(sidx > 0.5f)) {
			float4 m = float4(1, 1, 1, 1);
			[unroll] for (i = 0; i < 4; i++) {
				[branch] if (sidx[i] > 0.5f) m[i] = OroLclShadow(posW, nrmW, sidx[i], dst[i], dot(-p[i], nrmW));
			}
			dif *= m;
			if (bSpec) spe *= m;
		}
	}

	diff_out = 0;
	spec_out = 0;

	[unroll] for (i = 0; i < 4; i++) diff_out += gLights[i + x].diffuse.rgb * dif[i];

	if (bSpec) {
		[unroll] for (i = 0; i < 4; i++) spec_out += gLights[i + x].diffuse.rgb * spe[i];
	}
}


void LocalLightsBeckman(
	out float3 diff_out,
	out float3 spec_out,
	in float3 nrmW,
	in float3 posW,
	in float fRgh,
	uniform int x,
	uniform bool bSpec)
{

	float3 camW = normalize(-posW);
	float3 p[4];
	float3 H[4];
	float4 spe;
	float4 dHN;
	int i;

	// Relative positions
	[unroll] for (i = 0; i < 4; i++) p[i] = posW - gLights[i + x].position.xyz;

	// Square distances
	float4 sd;
	[unroll] for (i = 0; i < 4; i++) sd[i] = dot(p[i], p[i]);

	// Normalize
	sd = rsqrt(sd);
	[unroll] for (i = 0; i < 4; i++) p[i] *= sd[i];

	// Distances
	float4 dst = rcp(sd);

	if (bSpec) {

		// Halfway Vectors
		float4 hd;
		[unroll] for (i = 0; i < 4; i++) H[i] = (camW - p[i]);
		[unroll] for (i = 0; i < 4; i++) hd[i] = dot(H[i], H[i]);

		hd = rsqrt(hd);

		[unroll] for (i = 0; i < 4; i++) H[i] *= hd[i];
		[unroll] for (i = 0; i < 4; i++) dHN[i] = dot(H[i], nrmW);
	}

	

	// Attennuation factors
	float4 att;
	[unroll] for (i = 0; i < 4; i++) att[i] = dot(gLights[i + x].attenuation.xyz, float3(1.0, dst[i], dst[i] * dst[i]));

	att = rcp(att);

	// Spotlight factors
	float4 spt;
	[unroll] for (i = 0; i < 4; i++) {
		spt[i] = (dot(p[i], gLights[i + x].direction.xyz) - gLights[i + x].position.w) * gLights[i + x].direction.w;
		if (gLights[i + x].attenuation.w == 0) spt[i] = 1.0f;
	}

	spt = saturate(spt);

	// Diffuse light factors
	float4 dif;
	[unroll] for (i = 0; i < 4; i++) dif[i] = dot(-p[i], nrmW);

	dif = saturate(dif);

	// Specular lights factors
	
	if (bSpec) {

		float r2 = fRgh*fRgh;
		float4 d2 = dHN*dHN;
		float4 w = rcp(3.14*r2*d2*d2);
		float4 q = rcp(r2*d2);

		spe = (att*spt*dif) * w * exp((d2 - 1.0f) * q);
	}

	dif *= (att*spt);

	// ORO patch (z3) round 2b / (ah) step 4: every light of this block that owns a shadow map
	// loses its light behind a caster. The map index rides the light's own struct (diffuse.a =
	// cell + 1), so no slot uniform is needed; the branches are on constants - coherent, and a
	// few instructions for the common mesh no shadowed light reaches. ONLY THE FIRST TWO BLOCKS
	// (a mesh's eight strongest lights) test shadows: a ps_3_0 shader is validated against the
	// device's instruction-slot cap - 4096 on NVIDIA (tools/fxeff reads and checks it) - and the
	// tests inlined for sixteen lights put every pass over it (4758 slots: the hulls vanished at
	// 20 fps with nothing logged, 2026-09-09). A spot ranked below eight on a mesh is faint on
	// it. x is a compile-time block offset, so this folds away for blocks 8 and 12.
	if (x < ORO_LCL_SHDBLOCKS * 4) {
		const float4 sidx = float4(gLights[x].diffuse.a, gLights[x + 1].diffuse.a, gLights[x + 2].diffuse.a, gLights[x + 3].diffuse.a);
		[branch] if (gLclShd.x > 0.5f && any(sidx > 0.5f)) {
			float4 m = float4(1, 1, 1, 1);
			[unroll] for (i = 0; i < 4; i++) {
				[branch] if (sidx[i] > 0.5f) m[i] = OroLclShadow(posW, nrmW, sidx[i], dst[i], dot(-p[i], nrmW));
			}
			dif *= sqrt(m);						// this path SQUARES the diffuse at accumulation (Sq below) - land the shadow linear
			if (bSpec) spe *= m;
		}
	}

	diff_out = 0;
	spec_out = 0;

	[unroll] for (i = 0; i < 4; i++) diff_out += Sq(gLights[i + x].diffuse.rgb * dif[i]);

	if (bSpec) {
		[unroll] for (i = 0; i < 4; i++) spec_out += gLights[i + x].diffuse.rgb * spe[i];
	}
}



void LocalLightsEx(out float3 cDiffLocal, out float3 cSpecLocal, in float3 nrmW, in float3 posW, in float sp, uniform bool ubBeckman)
{
	cDiffLocal = 0;
	cSpecLocal = 0;

#if LMODE != 0
	// ORO patch (ah) 2026-09-08: PAY FOR THE LIGHTS YOU HAVE. Mesh.cpp sorts a mesh's lights
	// by illuminance and zero-fills the rest, so a black FIRST light of a block means that
	// block and every later one are empty - one coherent branch per block of four (the client
	// compiles with PREFER_FLOW_CONTROL, so these are real branches on constant registers).
	// gLightsEnabled used to be a dead guard: it zeroed two values the loops then overwrote
	// unconditionally. Measured: each block of four is ~250 instructions per pixel on the PBR
	// pass, paid by every pixel whether or not a light reached the mesh - and most meshes in
	// most scenes have no local light at all.
#if (LMODE == 2) || (LMODE == 4) || (LMODE == 6) || (LMODE == 8)		// even = Full (specular) - ORO patch (ah) step 3 (the D3DX preprocessor has no %)
	#define ORO_LSPEC true
#else
	#define ORO_LSPEC false
#endif
	[branch] if (gLightsEnabled && dot(gLights[0].diffuse.rgb, 1.0f) > 0.0f) {
		float3 dd, ss;
		if (ubBeckman) LocalLightsBeckman(dd, ss, nrmW, posW, sp, 0, ORO_LSPEC);
		else           LocalLights(dd, ss, nrmW, posW, sp, 0, ORO_LSPEC);
		cDiffLocal += dd; cSpecLocal += ss;
#if LMODE >= 3
		[branch] if (dot(gLights[4].diffuse.rgb, 1.0f) > 0.0f) {
			if (ubBeckman) LocalLightsBeckman(dd, ss, nrmW, posW, sp, 4, ORO_LSPEC);
			else           LocalLights(dd, ss, nrmW, posW, sp, 4, ORO_LSPEC);
			cDiffLocal += dd; cSpecLocal += ss;
#if LMODE >= 5		// ORO patch (ah) step 3: 12x - a third block, nested so an empty block 4 ends the work
			[branch] if (dot(gLights[8].diffuse.rgb, 1.0f) > 0.0f) {
				if (ubBeckman) LocalLightsBeckman(dd, ss, nrmW, posW, sp, 8, ORO_LSPEC);
				else           LocalLights(dd, ss, nrmW, posW, sp, 8, ORO_LSPEC);
				cDiffLocal += dd; cSpecLocal += ss;
#if LMODE >= 7		// 16x - a fourth
				[branch] if (dot(gLights[12].diffuse.rgb, 1.0f) > 0.0f) {
					if (ubBeckman) LocalLightsBeckman(dd, ss, nrmW, posW, sp, 12, ORO_LSPEC);
					else           LocalLights(dd, ss, nrmW, posW, sp, 12, ORO_LSPEC);
					cDiffLocal += dd; cSpecLocal += ss;
				}
#endif
			}
#endif
		}
#endif
	}
#endif
}







// ==========================================================================================================
// Object Self Shadows
// ==========================================================================================================
/*
float ProjectShadows(float2 sp)
{
	if (!gShadowsEnabled) return 0.0f;

	if (sp.x < 0 || sp.y < 0) return 0.0f;
	if (sp.x > 1 || sp.y > 1) return 0.0f;

	float2 dx = float2(gSHD[1], 0) * 1.5f;
	float2 dy = float2(0, gSHD[1]) * 1.5f;
	float  va = 0;
	float  pd = 1e-4;

	sp -= dy;
	if ((tex2D(ShadowS, sp - dx).r) > pd) va++;
	if ((tex2D(ShadowS, sp).r) > pd) va++;
	if ((tex2D(ShadowS, sp + dx).r) > pd) va++;
	sp += dy;
	if ((tex2D(ShadowS, sp - dx).r) > pd) va++;
	if ((tex2D(ShadowS, sp).r) > pd) va++;
	if ((tex2D(ShadowS, sp + dx).r) > pd) va++;
	sp += dy;
	if ((tex2D(ShadowS, sp - dx).r) > pd) va++;
	if ((tex2D(ShadowS, sp).r) > pd) va++;
	if ((tex2D(ShadowS, sp + dx).r) > pd) va++;

	return va / 9.0f;
}*/


// ---------------------------------------------------------------------------------------------------
//
float SampleShadows(float2 sp, float pd)
{

	float2 dx = float2(gSHD[1], 0) * 1.5f;
	float2 dy = float2(0, gSHD[1]) * 1.5f;
	float  va = 0;

	sp -= dy;
	if ((tex2D(ShadowS, sp - dx).r) > pd) va++;
	if ((tex2D(ShadowS, sp).r) > pd) va++;
	if ((tex2D(ShadowS, sp + dx).r) > pd) va++;
	sp += dy;
	if ((tex2D(ShadowS, sp - dx).r) > pd) va++;
	if ((tex2D(ShadowS, sp).r) > pd) va++;
	if ((tex2D(ShadowS, sp + dx).r) > pd) va++;
	sp += dy;
	if ((tex2D(ShadowS, sp - dx).r) > pd) va++;
	if ((tex2D(ShadowS, sp).r) > pd) va++;
	if ((tex2D(ShadowS, sp + dx).r) > pd) va++;

	return va * 0.1111111f;
}


// ---------------------------------------------------------------------------------------------------
//
float SampleShadows2(float2 sp, float pd)
{
	
	float val = 0;
	float m = KERNEL_RADIUS * gSHD[1];

	[unroll] for (int i = 0; i < KERNEL_SIZE; i++) {
		if ((tex2D(ShadowS, sp + kernel[i].xy * m).r) > pd) val += kernel[i].z;
	}

	return saturate(val * KERNEL_WEIGHT);
}


// ---------------------------------------------------------------------------------------------------
//
float SampleShadows3(float2 sp, float pd, float4 frame)
{

	float val = 0;
	frame *= KERNEL_RADIUS * gSHD[1];

	[unroll] for (int i = 0; i < KERNEL_SIZE; i++) {
		float2 ofs = frame.xy*kernel[i].x + frame.zw*kernel[i].y;
		if (tex2D(ShadowS, sp + ofs).r > pd) val += kernel[i].z;
	}

	return saturate(val * KERNEL_WEIGHT);
}


// ---------------------------------------------------------------------------------------------------
//
float SampleShadowsEx(float2 sp, float pd, float4 sc)
{
	
#if SHDMAP == 1
	return SampleShadows(sp, pd);
#elif SHDMAP == 2 || SHDMAP == 4
	return SampleShadows2(sp, pd);
#else
	float si, co;
	sc += (gSHD[2] * 2.0f);
	sincos(sc.y + sc.x*149.0f, si, co);
	return SampleShadows3(sp, pd, float4(si, co, co, -si));
#endif
}


// ---------------------------------------------------------------------------------------------------
//
float ComputeShadow(float4 shdH, float dLN, float4 sc)
{
	if (!gShadowsEnabled) return 1.0f;

	shdH.xyz /= shdH.w;
	shdH.z = 1 - shdH.z;
	float2 sp = shdH.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	sp += gSHD[1] * 0.5f;

	if (sp.x < 0 || sp.y < 0) return 1.0f;	// If a sample is outside border -> fully lit
	if (sp.x > 1 || sp.y > 1) return 1.0f;
	
	float fShadow;

	float kr = gSHD[0] * KERNEL_RADIUS;
	float dx = rsqrt(1.0 - dLN*dLN);
	float ofs = kr / (dLN * dx);
	float omx = min(0.05 + ofs, 0.5);

	float  pd = shdH.z + omx * gSHD[3];

	if (pd < 0) pd = 0;
	if (pd > 1) pd = 1;

	fShadow = SampleShadowsEx(sp, pd, sc);
	
	return 1 - fShadow;
}