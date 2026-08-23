// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012-2016 Jarmo Nikkanen
// ==============================================================


struct TileMeshVS
{
	float4 posH     : POSITION0;
	float3 CamW     : TEXCOORD0;
	float2 tex0     : TEXCOORD1;
	float3 nrmW     : TEXCOORD2;
	float4 atten    : COLOR0;			// (Atmospheric haze) Attennuate incoming fragment color
	float4 insca    : COLOR1;			// (Atmospheric haze) "Inscatter" Add to incoming fragment color
};

struct MeshVS
{
	float4 posH     : POSITION0;
	float3 CamW     : TEXCOORD0;
	float2 tex0     : TEXCOORD1;
	float3 nrmW     : TEXCOORD2;
};

struct TileMeshNMVS
{
	float4 posH     : POSITION0;
	float3 camW     : TEXCOORD0;
	float4 atten    : TEXCOORD1;
	float4 insca    : TEXCOORD2;
	float2 tex0     : TEXCOORD3;
	float3 nrmT     : TEXCOORD4;
	float3 tanT     : TEXCOORD5;

};

MeshVS TinyMeshTechVS(MESH_VERTEX vrt)
{
	// Zero output.
	MeshVS outVS = (MeshVS)0;

	float3 posW = mul(float4(vrt.posL, 1.0f), gW).xyz;	// Apply world transformation matrix
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	float3 nrmW = mul(float4(vrt.nrmL, 0.0f), gW).xyz;	// Apply world transformation matri
	outVS.nrmW  = normalize(nrmW);
	outVS.CamW  = -posW;
	outVS.tex0  = vrt.tex0.xy;

	return outVS;
}


float4 TinyMeshTechPS(MeshVS frg) : COLOR
{
	return float4(0,1,0,1);

	// Normalize input
	float3 nrmW = normalize(frg.nrmW);
	float3 CamW = normalize(frg.CamW);
	float4 cSpec = gMtrl.specular;
	float4 cTex = 1;

	if (gTextured) {
		if (gNoColor) cTex.a = tex2D(WrapS, frg.tex0.xy).a;
		else cTex = tex2D(WrapS, frg.tex0.xy);
	}

	if (gFullyLit) return float4(cTex.rgb*saturate(gMtrl.diffuse.rgb + gMtrl.emissive.rgb), cTex.a);

	cTex.a *= gMtrlAlpha;

	// Sunlight calculations. Saturate with cSpec.a to gain an ability to disable specular light
	float  d = saturate(-dot(gSun.Dir, nrmW));
	float  s = pow(saturate(dot(reflect(gSun.Dir, nrmW), CamW)), cSpec.a) * saturate(cSpec.a);

	if (d == 0) s = 0;

	float3 diff = gMtrl.diffuse.rgb * (d * saturate(gSun.Color)); // Compute total diffuse light
	diff += (gMtrl.ambient.rgb*gSun.Ambient) + (gMtrl.emissive.rgb);

	float3 cTot = cSpec.rgb * (s * gSun.Color);	// Compute total specular light

	cTex.rgb *= saturate(diff);	// Lit the diffuse texture

#if defined(_GLASS)
	cTex.a = saturate(cTex.a + max(max(cTot.r, cTot.g), cTot.b));	// Re-compute output alpha for alpha blending stage
#endif

	cTex.rgb += cTot.rgb;											// Apply reflections to output color

	return cTex;
}



// ============================================================================
// Planet Rings Technique
// ============================================================================

float4 RingTechPS(MeshVS frg) : COLOR
{
	float4 color = tex2D(RingS, frg.tex0);

	float3 pp = gCameraPos*gRadius[2] - frg.CamW*gDistScale;

	float  da = dot(normalize(pp), gSun.Dir);
	float  r  = sqrt(dot(pp,pp) * (1.0-da*da));

	float sh  = max(0.05, smoothstep(gRadius[0], gRadius[1], r));

	if (da<0) sh = 1.0f;

	if ((dot(frg.nrmW, frg.CamW)*dot(frg.nrmW, gSun.Dir))>0) return float4(color.rgb*0.35f*sh, color.a);
	return float4(color.rgb*sh, color.a);
}

float4 RingTech2PS(MeshVS frg) : COLOR
{
	float3 pp  = gCameraPos*gRadius[2] - frg.CamW*gDistScale;
	float  dpp = dot(pp,pp);
	float  len = sqrt(dpp);

	len = saturate(smoothstep(gTexOff.x, gTexOff.y, len));

	float4 color = tex2D(RingS, float2(len, 0.5));
	color.a = color.r*0.75;

	float  da = dot(normalize(pp), gSun.Dir);
	float  r  = sqrt(dpp*(1.0-da*da));

	float sh  = max(0.05, smoothstep(gRadius[0], gRadius[1], r));

	if (da<0) sh = 1.0f;

	color.rgb *= sh;

	if ((dot(frg.nrmW, frg.CamW)*dot(frg.nrmW, gSun.Dir))>0) return float4(color.rgb*0.35f, color.a);
	return float4(color.rgb, color.a);
}


// ============================================================================
// Base Tile Rendering Technique
// ============================================================================

TileMeshVS BaseTileVS(NTVERTEX vrt)
{
	// Null the output
	TileMeshVS outVS = (TileMeshVS)0;

	float3 posW  = mul(float4(vrt.posL, 1.0f), gW).xyz;
	outVS.posH   = mul(float4(posW, 1.0f), gVP);
	outVS.nrmW   = mul(float4(vrt.nrmL, 0.0f), gW).xyz;
	outVS.tex0   = vrt.tex0;
	outVS.CamW   = -posW;

	// Atmospheric haze -------------------------------------------------------

	AtmosphericHaze(outVS.atten, outVS.insca, outVS.posH.z, posW);

	float4 diffuse;
	float ambi, nigh;

	LegacySunColor(diffuse, ambi, nigh, outVS.nrmW);

	outVS.insca *= (diffuse+ambi);
	outVS.insca.a = nigh;

	return outVS;
}


// ORO patch (s), round 12: the pool grain's value noise. Periodic (the wrap) so the
// tile seam law holds; STATIC in the world - no clock, his call after the migrating
// flecks read as state-jumping dots.
float OroGrainHash(float2 c, float wrap)
{
	c = fmod(c, wrap);
	return frac(sin(dot(c, float2(127.1f, 311.7f))) * 43758.5453f);
}
float OroGrainNoise(float2 p, float wrap)
{
	float2 i = floor(p), f = frac(p);
	f = f * f * (3.0f - 2.0f * f);
	float a = OroGrainHash(i, wrap),                b = OroGrainHash(i + float2(1, 0), wrap);
	float c = OroGrainHash(i + float2(0, 1), wrap), d = OroGrainHash(i + float2(1, 1), wrap);
	return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float4 BaseTilePS(float4 sc : VPOS, TileMeshVS frg) : COLOR
{
	// Normalize input
	float3 nrmW = normalize(frg.nrmW);
	float3 CamW = normalize(frg.CamW);

	float4 cTex = tex2D(ClampS, frg.tex0);

	float3 r = reflect(gSun.Dir, nrmW);
	float  s = pow(saturate(dot(r, CamW)), 20.0f) * (1.0f-cTex.a);
	float  d = saturate(dot(-gSun.Dir, nrmW));

	if (d<=0) s = 0;

	// ORO patch (s) part 2: overcast. The lifted ambient (ambE) is shared with the wet
	// block below, so the puddles reflect the same sky the tile is lit by.
	float  dstB = length(frg.CamW);           // frg.CamW is the raw camera-relative vector
	                                          // (the normalized copy is the local CamW), so
	                                          // its length is the pixel-camera distance
	float3 ambE = gSun.Ambient * (1.0f + gStorm * 1.8f);
	float3 clr = cTex.rgb * saturate((d + s) * gSun.Color * (1.0f - gStorm) + ambE);

	if (gNight) clr += tex2D(Tex1S, frg.tex0).rgb;

	// ------------------------------------------------------------------------
	// ORO patch (s): WET GROUND. Everything below is exactly zero at gSurfWet 0, so
	// stock content cannot move.
	//
	// Three things make a surface read as wet, and only one of them is "shiny":
	//  1. IT GOES DARK. Water fills the pores and light that would have scattered
	//     straight back out gets trapped, so wet concrete loses about half its albedo.
	//     This is the single biggest cue and it costs one multiply.
	//  2. IT REFLECTS THE SKY AT GRAZING ANGLES. Fresnel: a wet road is nearly black
	//     underfoot and a mirror toward the horizon. `frg.insca` is the atmospheric
	//     in-scatter the client already computed for this pixel - which IS the colour
	//     of the sky in that direction - so the reflection is free and correctly
	//     coloured, without a reflection pass or a probe.
	//  3. PUDDLES. Water pools where the surface dips, and a puddle is not "more wet",
	//     it is a different material: darker still, and glassy rather than damp.
	//     The mask is procedural in tile UV - no texture, and low frequency enough that
	//     a base tile does not turn into a chequerboard.
	// ------------------------------------------------------------------------
	if (gSurfWet > 0.001f)
	{
		// THREE SCALES OF POOLING (part 7 round 2: "more variation in size"). The fine
		// lattice gives blobs, the mid one gathers them into areas, and the COARSE one
		// shifts the fine threshold regionally - whole stretches of merged sheets next
		// to stretches of bare speckle, which is where real aprons get their size
		// variety. All three ride the Pool size slider (gWetSwimPrm.z, 1 = designed).
		// SEAMLESS LATTICE (round 3: he screenshotted the seams). Adjacent tiles'
		// UVs are only guaranteed to agree MODULO 1 - that is all a wrapping texture
		// needs - so the old fractional-cycle sinusoids jumped phase at every tile
		// boundary. Every sinusoid now completes an INTEGER number of cycles per UV
		// unit (TAU x QK), which makes the whole lattice 1-periodic: a mod-1 jump
		// lands on the same value and the seam cannot exist. Consequences accepted:
		// the Pool size slider moves in integer-cycle steps (invisible on the fine
		// lattice, coarse on p3), and frac() bounds the sin arguments for precision -
		// legal only BECAUSE the lattice is 1-periodic. p3 carries phase offsets to
		// decorrelate it from p2 where the slider drives both to 1 cycle.
		// ROUND 3 ("too squarish"): interference SUM of angled plane waves instead of
		// the axis-aligned product - rounded quasi-random blobs. Seam law intact:
		// integer wave-vector components (signed quantizer QS), free phases.
		float  poolK = 1.0f / max(0.35f, gWetSwimPrm.z);
	#define QS(k) (((k) < 0.0f) ? -max(1.0f, floor(-(k) * poolK + 0.5f)) : max(1.0f, floor((k) * poolK + 0.5f)))
		float2 uvT = frac(frg.tex0) * 6.2831853f;
		float  p1 = (sin(uvT.x * QS(8.0f)  + uvT.y * QS(3.0f)  + 1.3f)
		           + sin(uvT.x * QS(4.0f)  + uvT.y * QS(7.0f)  + 4.1f)
		           + sin(uvT.x * QS(-6.0f) + uvT.y * QS(6.0f)  + 2.6f)
		           + sin(uvT.x * QS(7.0f)  + uvT.y * QS(-4.0f) + 5.5f)) * 0.42f;
		float  p2 = (sin(uvT.x * QS(2.0f)  + uvT.y * QS(1.0f)  + 0.7f)
		           + sin(uvT.x * QS(-1.0f) + uvT.y * QS(2.0f)  + 3.9f)
		           + sin(uvT.x * QS(2.0f)  + uvT.y * QS(-2.0f) + 2.2f)) * 0.55f;
		float  p3 = (sin(uvT.x * QS(1.0f)  + uvT.y * QS(1.0f)  + 1.9f)
		           + sin(uvT.x * QS(-1.0f) + uvT.y * QS(1.0f)  + 5.0f)
		           + sin(uvT.x * QS(1.0f)  + uvT.y * QS(-1.0f) + 0.4f)) * 0.55f;
		// GRAIN IN THE REFLECTION (round 12 - the cell flecks were CIRCULAR DOTS that
		// jumped from state to state; his circled reference areas are fine IRREGULAR
		// broken-water texture). Two octaves of continuous value noise, THRESHOLDED
		// so only its upper patches dig into the reflection: irregular connected
		// blotches at two scales, sparse because the threshold is high, STATIC in
		// the world (no migration, his call), periodic with the tile (seam law).
		// the two user knobs: opacity scales the knockout depth; size rescales the
		// lattice - QUANTIZED to whole cells per period so the seam law survives it
		float  GNq = max(8.0f, floor(96.0f / max(0.35f, gWetGrainPrm.y) + 0.5f));
		float2 guv = frac(frg.tex0) * GNq;
		float  gn  = 0.62f * OroGrainNoise(guv, GNq)
		           + 0.38f * OroGrainNoise(guv * 2.0f, GNq * 2.0f);
		float  gran = 1.0f - saturate(0.85f * gWetGrainPrm.x) * smoothstep(0.52f, 0.78f, gn);
	#undef QS
		// POOLS MERGE AS THE GROUND SOAKS: thresholds slide with gSurfWet so puddles
		// widen and join into sheets. HIS SPEC (round 2): pools only START at 70% rain -
		// the ground soaks first, water stands later - so the trailing wet factor is a
		// 0.70..1.00 ramp, not gSurfWet itself. And the FAR FIELD FADES (Pool reach,
		// gWetSwimPrm.w): pools to the horizon read as a pattern, so an exponential
		// e-fold (~900 m at slider 1) blends them out with distance while the damp
		// sheen carries on - cut and blend, exactly as asked.
		float  pud = saturate(p1 * 1.30f - 0.16f + 0.30f * gSurfWet + p3 * 0.35f)
		           * saturate(p2 * 1.10f + 0.35f + 0.25f * gSurfWet);
		pud = saturate(pud * 1.6f) * saturate((gSurfWet - 0.70f) / 0.30f);
		pud *= exp(-dstB / (90.0f + 780.0f * gWetSwimPrm.w * gWetSwimPrm.w));

		float wet = gSurfWet;
		// gWetDark is the user's darkening gain: 0 = wet with no darkening at all, 2 =
		// fully black ground. RECALIBRATED TWICE 2026-08-22 and this is the settled map:
		// the doubled coefficients saturated by ~1.3 on the slider, so the top 0.7 of the
		// track did nothing. Coefficients x0.65 stretch the whole USEFUL range (old
		// 0..1.3) across the full track (0..2) - the maximum is the same black, and every
		// position below it now moves something.
		clr *= saturate(lerp(1.0f, 1.0f - 0.702f * gWetDark, wet));   // damp
		clr *= saturate(lerp(1.0f, 1.0f - 0.988f * gWetDark, pud));   // standing water, darker still

		// ⚠️ THE SKY IS NOT frg.insca. The first build reflected the in-scatter, on the
		// reasoning that it is the colour of the air in that direction - which is true and
		// useless CLOSE UP, because in-scatter accumulates with distance and is ~0 twenty
		// metres away. So the "reflection" was a lerp toward black and the wet ground near
		// the camera got darker instead of shinier, which is exactly what he reported.
		// A wet surface reflects the SKY DOME, which is bright even under overcast, and
		// the nearest thing to its luminance that this shader holds is the ambient term.
		// insca is still added, so distant wet ground still picks up aerial perspective.
		// Under storm light ambE is already lifted, so the puddles automatically reflect
		// the same darker sky the scene is lit by - the two cannot disagree.
		float3 cSky = ambE * 2.6f + gSun.Color * (1.0f - gStorm) * 0.30f + frg.insca.rgb * 1.2f;

		// Fresnel: near-black underfoot, mirror toward the horizon. That gradient IS the
		// look of a wet road, and it is why this cannot be a uniform gloss term.
		float fres = pow(1.0f - saturate(dot(nrmW, CamW)), 3.0f);
		// REPARTITIONED (look round 2): the damp film stays angle-dependent, but the
		// POOLS reflect the sky strongly at ANY viewing angle - which is what standing
		// water does, and why the reference's puddles read as light-grey sky patches
		// against the dark ground instead of staying dark themselves.
		float mirror = saturate((0.25f + 0.75f * fres) * 0.42f * wet
		                      + (0.60f + 0.40f * fres) * pud * gran);
		clr = lerp(clr, cSky, mirror);

		// THE VESSEL IMAGE IN THE WATER (ORO patch (s) part 6). The half-res target
		// holds this frame's vessels rendered through a ground-mirrored camera, so the
		// lookup is simply this pixel's own screen position - the whole point of the
		// planar trick. Alpha is coverage (the RT clears to 0).
		// LOOK ROUND vs the reference (2026-08-22): THREE taps smeared VERTICALLY - the
		// wet-road signature, micro-roughness stretching the image down-screen while it
		// stays horizontally coherent - a stronger base presence (the old blend was
		// throttled to near-nothing by Fresnel at ordinary camera heights), and a
		// heavier ripple so the image breaks into patches over the pools.
		if (gWetReflPrm.w > 0.5f) {
			float2 ruv = (sc.xy + 0.5f) * gWetReflPrm.xy;
			ruv.x = 1.0f - ruv.x;      // undo the clip-space X flip (see the mirror pass)
			// ⚠️ THE RIPPLE SHIMMERS FAST NOW (look round 2) - "dances too slowly,
			// like the aurora" is the third time that exact critique has hit this
			// project, and the cure is always the clock, never the geometry: rain-
			// pocked water flickers at a few Hz. gWetTime is the shared rain clock,
			// so it pauses with the sim like everything else.
			// LOOK ROUND 3: the STATIC pool-shape offset (p1/p2 - what breaks the
			// image into patches) and the ANIMATED swim are now scaled separately.
			// "The reflection swings too far": the swim is a rain-pocked surface
			// jittering, not waves. The pool patchiness must not shrink with it, so
			// p1/p2 keep the old scale. LOOK ROUND 4: amplitude and cadence are HIS
			// now - gWetSwimPrm (x = amp scale, y = rate scale, both 0..2, 1 = this
			// baseline) rides SetWetReflection, the Swim size / Swim rate sliders.
			ruv += float2(p1, p2) * (0.0015f + 0.0055f * pud);
			ruv += float2(sin(gWetTime * 12.0f * gWetSwimPrm.y + uvT.y * 4.0f),
			              cos(gWetTime *  9.7f * gWetSwimPrm.y + uvT.x * 4.0f))
			     * (0.0005f + 0.0016f * pud) * gWetSwimPrm.x;
			float  smr = gWetReflPrm.y * 3.2f;                   // shorter smear
			float4 cVes = tex2D(WetReflS, ruv) * 0.5f
			            + tex2D(WetReflS, ruv + float2(0, smr)) * 0.3f
			            + tex2D(WetReflS, ruv + float2(0, smr * 2.5f)) * 0.2f;
			float  rStr = cVes.a * saturate(0.35f + 0.90f * mirror)
			            * saturate(0.55f * wet + 1.10f * pud) * gWetReflPrm.z;
			clr = lerp(clr, cVes.rgb, saturate(rStr));
		}

		// A tight sun glint on top - the specular the dry path already computes, but
		// sharper and stronger, because water is smooth where concrete is not.
		// Round 3: now storm-collapsed like every directional term (it previously rode
		// the RAW diffuse dot and survived full overcast) - the broad glare below is
		// what replaces it under the deck.
		float gl = pow(saturate(dot(r, CamW)), 90.0f) * d * (1.0f - gStorm)
		         * (0.7f * wet + 2.4f * pud);
		clr += gSun.Color * gl;
		// POOL SKY GLARE (part 7 round 3, his ask): a BROAD view-dependent lobe - the
		// bright cloud deck around the hidden sun - growing as the storm does, so the
		// pools flash as the camera moves even under full overcast. cSky already
		// carries the daylight factor, so this goes dark at night on its own.
		float glb = pow(saturate(dot(r, CamW)), 7.0f) * (0.30f + 0.70f * gStorm);
		clr += cSky * glb * (0.30f * wet + 1.10f * pud);
	}

	// ORO patch (s) part 2: STORM FOG. Rain scatters light, so visibility collapses and
	// the crisp bright horizon goes with it. Exponential in the pixel distance, toward the
	// lifted-ambient grey, so the fog is the same colour as the light. Zero at gStorm 0.
	if (gStorm > 0.001f) {
		float  ff = (1.0f - exp(-dstB * 0.00045f)) * saturate(gStorm * 1.4f);
		float3 cFog = saturate(ambE * 2.2f + gSun.Color * 0.06f);
		clr = lerp(clr, cFog, ff);
	}

	return float4(clr.rgb*frg.atten.rgb+frg.insca.rgb, cTex.a);
	//return float4(clr.rgb*frg.atten.rgb+frg.insca.rgb, cTex.a*(1-frg.insca.a));	// Make basetiles transparent during night
}


// ============================================================================
// Vessel Axis vector technique
// ============================================================================

MeshVS AxisTechVS(MESH_VERTEX vrt)
{
	// Zero output.
	MeshVS outVS = (MeshVS)0;
	float  stretch = vrt.tex0.x * gMix;
	float3 posX = vrt.posL + float3(0.0, stretch, 0.0);
	float3 posW = mul(float4(posX, 1.0f), gW).xyz;			// Apply world transformation matrix
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	float3 nrmW = mul(float4(vrt.nrmL, 0.0f), gW).xyz;		// Apply world transformation matrix

	outVS.nrmW  = normalize(nrmW);
	outVS.CamW  = -posW;

	return outVS;
}


float4 AxisTechPS(MeshVS frg) : COLOR
{
	float3 nrmW = normalize(frg.nrmW);
	float  d = saturate(dot(-gSun.Dir, nrmW));
	float3 clr = gColor.rgb * saturate(max(d,0) + 0.5);
	return float4(clr, gColor.a);
}

technique AxisTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 AxisTechVS();
		pixelShader  = compile ps_3_0 AxisTechPS();

		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = true;
		ZWriteEnable = true;
	}
}



// ============================================================================
// Mesh Shadow Technique
// ============================================================================

ShadowTexVS ShadowMeshTechVS(POSTEX vrt)
{
	// Zero output.
	ShadowTexVS outVS = (ShadowTexVS)0;
	float3 posW = mul(float4(vrt.posL.xyz, 1.0f), gW).xyz;
	float alpha = dot(vrt.posL.xyz, gInScatter.xyz) + gInScatter.w;
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	outVS.tex0  = float3(vrt.tex0.xy, alpha);
	outVS.dstW  = outVS.posH.zw;
	return outVS;
}

ShadowTexVS ShadowMeshTechExVS(POSTEX vrt)
{
	// Zero output.
	ShadowTexVS outVS = (ShadowTexVS)0;
	float alpha = dot(vrt.posL.xyz, gColor.xyz) + gColor.w;
	float3 posX = mul(float4(vrt.posL.xyz, 1.0f), gGrpT).xyz;
	float3 posW = mul(float4(posX, 1.0f), gW).xyz;
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	outVS.tex0  = float3(vrt.tex0.xy, alpha);
	outVS.dstW  = outVS.posH.zw;
	return outVS;
}

float4 ShadowTechPS(ShadowTexVS frg) : COLOR
{
	if (frg.tex0.b < 0) clip(-1);
	if (gOITEnable) {
		float4 alpha = tex2D(WrapS, frg.tex0.xy);
		if (alpha.a < 0.5f) clip(-1);
	}
	return float4(0.0f, 0.0f, 0.0f, gMix);
}


// -----------------------------------------------------------------------------------
// Shadow Map rendering with plain geometry (without texture) 
//
BShadowVS ShadowMapVS(SHADOW_VERTEX vrt)
{
	// Zero output.
	BShadowVS outVS = (BShadowVS)0;
	float3 posW = mul(float4(vrt.posL.xyz, 1.0f), gW).xyz;
	outVS.posH = mul(float4(posW, 1.0f), gLVP);
	outVS.dstW = outVS.posH.zw;
	return outVS;
}

float4 ShadowMapPS(BShadowVS frg) : COLOR
{
	return 1 - (frg.dstW.x / frg.dstW.y);
}


// -----------------------------------------------------------------------------------
// Shadow Map rendering with texture alpha included
//
ShadowTexVS ShadowMapOIT_VS(POSTEX vrt)
{
	// Zero output.
	ShadowTexVS outVS = (ShadowTexVS)0;
	float3 posW = mul(float4(vrt.posL.xyz, 1.0f), gW).xyz;
	outVS.posH = mul(float4(posW, 1.0f), gLVP);
	outVS.tex0 = float3(vrt.tex0.xy, 0);
	outVS.dstW = outVS.posH.zw;
	return outVS;
}

float4 ShadowMapOIT_PS(ShadowTexVS frg) : COLOR
{
	if (gOITEnable) {
		float alpha = tex2D(WrapS, frg.tex0.xy).a;
		if (alpha < 0.5f) return 1.0f;
	}
	return 1 - (frg.dstW.x / frg.dstW.y);
}

// -----------------------------------------------------------------------------------

technique GeometryTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 ShadowMapVS();
		pixelShader = compile ps_3_0 ShadowMapPS();

		AlphaBlendEnable = false;
		ZEnable = true;
		ZWriteEnable = true;
		StencilEnable = false;
	}

	pass P1
	{
		vertexShader = compile vs_3_0 ShadowMapOIT_VS();
		pixelShader = compile ps_3_0 ShadowMapOIT_PS();

		AlphaBlendEnable = false;
		ZEnable = true;
		ZWriteEnable = true;
		StencilEnable = false;
	}
}

technique ShadowTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 ShadowMeshTechVS();
		pixelShader  = compile ps_3_0 ShadowTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;

		StencilEnable = true;
		StencilRef    = 1;
		StencilMask   = 1;
		StencilFunc   = NotEqual;
		StencilPass   = Replace;
	}

	pass P1
	{
		vertexShader = compile vs_3_0 ShadowMeshTechExVS();
		pixelShader  = compile ps_3_0 ShadowTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;

		StencilEnable = true;
		StencilRef    = 1;
		StencilMask   = 1;
		StencilFunc   = NotEqual;
		StencilPass   = Replace;
	}
}



// =============================================================================
// Mesh Bounding Box Technique
// =============================================================================

BShadowVS BoundingBoxVS(float3 posL : POSITION0)
{
	// Zero output.
	BShadowVS outVS = (BShadowVS)0;
	float3 pos;
	pos.x = gAttennuate.x * posL.x + gInScatter.x * (1-posL.x);
	pos.y = gAttennuate.y * posL.y + gInScatter.y * (1-posL.y);
	pos.z = gAttennuate.z * posL.z + gInScatter.z * (1-posL.z);

	float3 posX = mul(float4(pos, 1.0f), gGrpT).xyz;		// Apply meshgroup specific transformation
	float3 posW = mul(float4(posX, 1.0f), gW).xyz;			// Apply world transformation matrix
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	return outVS;
}

BShadowVS BoundingSphereVS(float3 posL : POSITION0)
{
	// Zero output.
	BShadowVS outVS = (BShadowVS)0;
	float3 posW = mul(float4(posL, 1.0f), gW).xyz;			// Apply world transformation matrix
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	return outVS;
}

float4 BoundingBoxPS(BShadowVS frg) : COLOR
{
	return gColor;
}

technique TileBoxTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BoundingSphereVS();
		pixelShader  = compile ps_3_0 BoundingBoxPS();

		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = true;
		ZWriteEnable = true;
	}
}

technique BoundingBoxTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BoundingBoxVS();
		pixelShader  = compile ps_3_0 BoundingBoxPS();

		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = true;
		ZWriteEnable = true;
	}
}

technique BoundingSphereTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BoundingSphereVS();
		pixelShader  = compile ps_3_0 BoundingBoxPS();

		AlphaBlendEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = true;
		ZWriteEnable = true;
	}
}


technique BaseTileTech
{
	/*pass P0
	{
		vertexShader = compile VS_MOD BaseTileNMVS();
		pixelShader  = compile PS_MOD BaseTileNMPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
		CullMode = CCW;
	}*/

	pass P0
	{
		vertexShader = compile vs_3_0 BaseTileVS();
		pixelShader  = compile ps_3_0 BaseTilePS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
		CullMode = CCW;
	}
}

technique RingTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 TinyMeshTechVS();
		pixelShader  = compile ps_3_0 RingTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
		ZEnable = false;
		CullMode = NONE;
	}
}

technique RingTech2
{
	pass P0
	{
		vertexShader = compile vs_3_0 TinyMeshTechVS();
		pixelShader  = compile ps_3_0 RingTech2PS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
		ZEnable = false;
		CullMode = NONE;
	}
}

technique SimplifiedTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 TinyMeshTechVS();
		pixelShader = compile ps_3_0 TinyMeshTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
		ZEnable = true;
	}
}
