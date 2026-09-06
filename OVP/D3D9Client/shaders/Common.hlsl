
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
// ORO patch (z3) round 2b: the LOCAL-LIGHT shadow map reaches the vessels. One perspective
// depth map rendered from the frame's strongest shadow-casting SPOT light
// (Scene::RenderLocalLightShadowMap); gLclShd.x names which of THIS MESH's light slots it
// shadows (-1 = none; every Mesh.cpp light-upload site sets it), .y = 1/mapsize.
// Same depth convention as the sun's map (ShdMapPS stores 1 - z/w); tex2Dlod because the
// call sits behind a comparison on a float uniform - dynamic flow, and ps_3_0 refuses
// divergent gradients there (the map has one mip anyway). Returns the LIT factor.
// ==========================================================================================================

uniform extern float4x4 gLclShdVP;
uniform extern float4   gLclShd = {-1.0, 0.0, 0.0, 0.0};
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

float SampleLocalShadowV(float3 posW, float3 nrmW, float dstL, float nl)
{
	// NORMAL-OFFSET (gLclShd.z = world texel per metre of light distance): push the
	// receiver point ~2 map texels out along its surface normal - the ordinary-acne
	// cure. Curved hulls always carry a grazing band, so the vessel receivers get
	// the same laws as the terrain (see NewPlanet.hlsl's twin for the full story).
	posW += nrmW * (dstL * gLclShd.z * 2.0f);

	float4 q = mul(float4(posW, 1.0f), gLclShdVP);
	if (q.w < 0.001f) return 1.0f;						// behind the light: lit
	q.xyz /= q.w;
	float2 sp = q.xy * float2(0.5f, -0.5f) + 0.5f;
	if (sp.x < 0 || sp.x > 1 || sp.y < 0 || sp.y > 1) return 1.0f;	// outside the map: lit
	if (q.z < 0 || q.z > 1) return 1.0f;

	float pd = (1.0f - q.z) * 1.012f + 0.0008f;

	// TEXEL-FOOTPRINT BIAS - the grazing cure, NewPlanet.hlsl's twin: clear
	// exactly the ray-depth span one PCF-widened texel covers on this surface.
	float grz = sqrt(saturate(1.0f - nl * nl)) / max(nl, 0.05f);
	pd += (gLclShd.z * 3.0f) * grz * gLclShd.w / max(dstL, 1.0f);

	float2 dx = float2(gLclShd.y, 0) * 1.5f;
	float2 dy = float2(0, gLclShd.y) * 1.5f;
	float  va = 0;
	if (tex2Dlod(LclShmS, float4(sp - dx, 0, 0)).r > pd) va++;
	if (tex2Dlod(LclShmS, float4(sp + dx, 0, 0)).r > pd) va++;
	if (tex2Dlod(LclShmS, float4(sp - dy, 0, 0)).r > pd) va++;
	if (tex2Dlod(LclShmS, float4(sp + dy, 0, 0)).r > pd) va++;
	return 1.0f - va * 0.25f;
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
	[unroll] for (i = 0; i < 4; i++) p[i] = posW - gLights[i + x].position;

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
		spt[i] = (dot(p[i], gLights[i + x].direction) - gLights[i + x].param[Phi]) * gLights[i + x].param[Theta];
		if (gLights[i + x].type == 0) spt[i] = 1.0f;
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

	// ORO patch (z3) round 2b: the shadow-mapped slot loses its light behind a caster.
	// x is a compile-time block offset, so the range guard folds away for the block
	// that cannot hold the slot; the select mask stands in for dynamic indexing and
	// fishes out the slot's light distance for the normal-offset.
	if (gLclShd.x > (x - 0.5f) && gLclShd.x < (x + 3.5f)) {
		float4 sel = float4(abs(gLclShd.x - x - 0.0f) < 0.5f, abs(gLclShd.x - x - 1.0f) < 0.5f,
		                    abs(gLclShd.x - x - 2.0f) < 0.5f, abs(gLclShd.x - x - 3.0f) < 0.5f);
		float3 pK = p[0]*sel.x + p[1]*sel.y + p[2]*sel.z + p[3]*sel.w;
		float shd = SampleLocalShadowV(posW, nrmW, dot(dst, sel), dot(-pK, nrmW));
		float4 m = lerp(float4(1, 1, 1, 1), shd.xxxx, sel);
		dif *= m;
		if (bSpec) spe *= m;
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
	[unroll] for (i = 0; i < 4; i++) p[i] = posW - gLights[i + x].position;

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
		spt[i] = (dot(p[i], gLights[i + x].direction) - gLights[i + x].param[Phi]) * gLights[i + x].param[Theta];
		if (gLights[i + x].type == 0) spt[i] = 1.0f;
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

	// ORO patch (z3) round 2b: the shadow-mapped slot loses its light behind a caster.
	// This path SQUARES the diffuse factor at accumulation (Sq below), so dif takes
	// sqrt(shd) to land the shadow linear - matching the plain LocalLights depth.
	if (gLclShd.x > (x - 0.5f) && gLclShd.x < (x + 3.5f)) {
		float4 sel = float4(abs(gLclShd.x - x - 0.0f) < 0.5f, abs(gLclShd.x - x - 1.0f) < 0.5f,
		                    abs(gLclShd.x - x - 2.0f) < 0.5f, abs(gLclShd.x - x - 3.0f) < 0.5f);
		float3 pK = p[0]*sel.x + p[1]*sel.y + p[2]*sel.z + p[3]*sel.w;
		float shd = SampleLocalShadowV(posW, nrmW, dot(dst, sel), dot(-pK, nrmW));
		dif *= lerp(float4(1, 1, 1, 1), sqrt(shd).xxxx, sel);
		if (bSpec) spe *= lerp(float4(1, 1, 1, 1), shd.xxxx, sel);
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

#if LMODE !=0
	if (!gLightsEnabled) {
		cDiffLocal = 0;
		cSpecLocal = 0;
	}
#endif

#if LMODE == 1
	if (ubBeckman) LocalLightsBeckman(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, false);
	else LocalLights(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, false);

#elif LMODE == 2
	if (ubBeckman) LocalLightsBeckman(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, true);
	else LocalLights(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, true);

#elif LMODE == 3
	float3 dd, ss;
	if (ubBeckman) {
		LocalLightsBeckman(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, false);
		LocalLightsBeckman(dd, ss, nrmW, posW, sp, 4, false);
	}
	else {
		LocalLights(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, false);
		LocalLights(dd, ss, nrmW, posW, sp, 4, false);
	}
	cDiffLocal += dd;
	cSpecLocal += ss;

#elif LMODE == 4
	float3 dd, ss;
	if (ubBeckman) {
		LocalLightsBeckman(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, true);
		LocalLightsBeckman(dd, ss, nrmW, posW, sp, 4, true);
	}
	else {
		LocalLights(cDiffLocal, cSpecLocal, nrmW, posW, sp, 0, true);
		LocalLights(dd, ss, nrmW, posW, sp, 4, true);
	}
	cDiffLocal += dd;
	cSpecLocal += ss;
#else
	cDiffLocal = 0;
	cSpecLocal = 0;
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