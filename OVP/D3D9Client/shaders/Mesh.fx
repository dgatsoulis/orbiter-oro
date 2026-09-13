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
// ORO patch (aj) 2026-09-12: THE ORO RING. Same carrier mesh, same per-pixel radius,
// but three things the stock shader gets wrong are put right, and the profile's
// ALPHA is a real optical depth instead of red*0.75:
//   1. LINEAR in radius. Stock samples at smoothstep(irad, orad, r) - the cubic - while
//      the shipped profile is authored linear, displacing structure by up to 6,300 km
//      (the B ring's outer edge measures at the linear prediction, gradient 18.0, and
//      not at the smoothstep one, gradient 1.6). Linear here, and the derived profiles
//      are linear, so it is right for every planet.
//   2. OPACITY is 1 - exp(-tau/mu_view): the path through a 10-30 m sheet lengthens as
//      1/mu, so the ring goes opaque near edge-on. That IS the honest "thickness".
//   3. The LIT and UNLIT faces are different phenomena. Lit = reflection, with the sun's
//      own grazing term. Unlit = TRANSMISSION, x/(e^x - 1) in x = tau/mu, which goes to 1
//      for a thin ring and to 0 for a thick one - so backlit, the B ring goes DARK and the
//      Cassini Division and C ring go BRIGHT, the inversion every Cassini image shows.
//      Stock: a flat *0.35.
// The planet's shadow on the ring is the stock term, kept verbatim - it works.
// gRingPrm.x lerps the WHOLE result against the stock computation, so 0 is stock
// arithmetically and the pill is an exact A/B. No branches around tex2D (the (aa)
// landmine: X3528) - everything is sampled, then gated.
// ============================================================================


// ============================================================================
// ORO patch (aj) ROUND 2 (2026-09-12): THE CLOSE-UP - grooves and grain that appear in the
// sheet's OWN texture as the camera approaches, octave by octave down to a 4 m cell. His
// rules: from far, identical to round 1; closer, texture; and after five flights his cut -
// "lose the particles and the halo and just keep the grooved texture, but we must make it
// as good resolution as we can" (bright specks read as HOLES when backlit; a dust halo the
// addon drew was only ever the reverted swarm's atmosphere).
//
// WHY TEXTURE AND NOT GEOMETRY: the reference frames are the sheet gaining detail, and a
// detail term in the sheet's shader gives every rule for free - it is a per-pixel fade by
// camera distance, the way terrain detail textures work, so the far ring is round 1 bit for
// bit. And GROOVES ARE AXISYMMETRIC: stationary for every observer whatever its orbit, so
// none of the orbital-motion problems that killed a boulder swarm can touch them.
//
// THE COORDINATES ARE CAMERA-RELATIVE. The round-1 terms work from the planet-relative
// position, 1.2e8 m in float32 = 8 m of quantisation that jumps with the camera's own
// float rounding. The detail instead takes the vector from the EYE to the fragment (full
// precision where the detail is visible) dotted with the camera's radial and along-track
// directions in the ring plane (gRingAxR/AxT, set by the ring bracket), plus two offsets the
// addon reduces on the CPU each frame: the camera's radial cell coordinate, and the grain's
// along-track cell coordinate for a pattern that ROTATES RIGIDLY at the orbital rate of the
// camera's own radius (J2-aware, addon-side). A co-orbiting camera sees its neighbourhood
// stand still; material at other radii drifts slowly, the right way; a camera on some other
// orbit sees the grain stream, as it would real material. The hash period divides the cell
// count round the ring, so the phase wrap can never pop.
//
// THE NOISE is value noise on an integer lattice wrapped every RING_NP cells so the hash's
// input stays small however far a coordinate runs (the classic sin hash falls apart past
// ~1e4; this is Hoskins's, which does not). Octaves are integer multiples of the base cell,
// so the period still tiles - the integer-cycle lattice law again.
// ============================================================================
#define RING_NP 4096.0
float RingHash1(float i)
{
	i = fmod(i + RING_NP, RING_NP);
	float p = frac(i * 0.1031);
	p *= p + 33.33;
	p *= p + p;
	return frac(p);
}
float RingHash2(float i, float j)
{
	i = fmod(i + RING_NP, RING_NP);
	j = fmod(j + RING_NP, RING_NP);
	float3 p3 = frac(float3(i, j, i) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return frac((p3.x + p3.y) * p3.z);
}
// smooth value noise WITH ITS DERIVATIVES (Quilez's form): .x the value, -1..1; .y (and .z)
// d/dx (d/dy) per cell. The derivatives are the RELIEF's slope (flight 9, "some type of 3d
// feeling"), and they cost a few multiplies over the value alone - no second sample.
float2 RingNoise1(float x)
{
	float i = floor(x), f = x - i;
	float u = f * f * (3.0 - 2.0 * f), du = 6.0 * f * (1.0 - f);
	float a = RingHash1(i), b = RingHash1(i + 1.0);
	return float2((a + (b - a) * u) * 2.0 - 1.0, (b - a) * du * 2.0);
}
float3 RingNoise2(float x, float y)
{
	float i = floor(x), j = floor(y);
	float fx = x - i, fy = y - j;
	float u = fx * fx * (3.0 - 2.0 * fx), du = 6.0 * fx * (1.0 - fx);
	float v = fy * fy * (3.0 - 2.0 * fy), dv = 6.0 * fy * (1.0 - fy);
	float a = RingHash2(i, j),       b = RingHash2(i + 1.0, j);
	float c = RingHash2(i, j + 1.0), d = RingHash2(i + 1.0, j + 1.0);
	float k1 = b - a, k2 = c - a, k3 = a - b - c + d;
	return float3((a + k1 * u + k2 * v + k3 * u * v) * 2.0 - 1.0,
	              (k1 + k3 * v) * du * 2.0,
	              (k2 + k3 * u) * dv * 2.0);
}
// A feature of physical size L [m] seen from d [m] is gone once it subtends under ~4 px on a
// 1080-line 50-degree view (L/d * 1158 px), i.e. at d ~ 290 L - at Detail 1. fk is that
// reach, 290 x the Detail slider: 2 admits features half the size at any distance (a 2 px
// rule), 0 admits nothing. Fades in over the last three quarters of the reach, so successive
// octaves arrive one after another as the camera closes. (Flight 7: he expected Detail to
// mean FINER, not stronger - it did neither; it was the amplitude, which is Contrast now.)
float RingFade(float L, float d, float fk)
{
	return 1.0 - smoothstep(0.25 * L * fk, L * fk, d);
}
// grooves (1D, radial) + grain (2D): a density modulation, -1..1 at full fade. TEN groove
// octaves and EIGHT grain octaves, each doubling the frequency, down to a 4 m cell - flight
// 6's "as good resolution as we can". Amplitudes fall ~0.75 an octave onto a floor, so the
// fine structure stays visible once the fade lets it in; the fade (4 px) decides which
// octaves exist at a distance, so the far ring costs nothing extra to LOOK at.
// !! THE OCTAVE COORDINATE IS fmod(I k, NP) + F k, with I the INTEGER part of the CPU's
// pattern offset and F its FRACTION plus the eye-to-fragment metres in base cells. A single
// (I + F) k is not exact: 4096 x 512 leaves float32 a quarter of a cell, and the 4 m octave
// would step. Split, I k stays an exact integer below 2^24 and F k keeps its bits.
// Amplitudes fall ~0.83 an octave - a near-1/f spectrum, flatter than the first cut's
// 0.75 (flight 7: at 120 km the 2 km cells read as "pixels" because they carried half the
// weight; real ring structure has power at every scale down to the resolution limit).
static const float RING_GA[10] = { 0.40, 0.30, 0.24, 0.19, 0.16, 0.14, 0.12, 0.10, 0.09, 0.08 };
static const float RING_NA[8]  = { 0.26, 0.20, 0.16, 0.13, 0.11, 0.09, 0.08, 0.07 };
// Returns .x the density modulation and .yz its SLOPE (d/d rho, d/d psi) - the relief's
// input. Each octave's derivative is taken in its OWN cell, so the summed slope is that of
// a self-similar surface (an octave's height falls with its cell size): scale-free, and
// it fades with the octaves, so the far ring is flat to the bit.
float3 RingDetail(float Ir, float fr, float Ip, float fp, float d, float Lr, float Lt, float fk)
{
	float3 r = 0.0;
	float  k = 1.0;
	[unroll] for (int i = 0; i < 10; i++) {
		float2 g = RingNoise1(fmod(Ir * k, RING_NP) + fr * k);
		float  w = RING_GA[i] * RingFade(Lr / k, d, fk);
		r.x += w * g.x; r.y += w * g.y;
		k *= 2.0;
	}
	k = 1.0;
	[unroll] for (int j = 0; j < 8; j++) {
		float3 g = RingNoise2(fmod(Ir * k, RING_NP) + fr * k, fmod(Ip * k, RING_NP) + fp * k);
		float  w = RING_NA[j] * RingFade(Lt / k, d, fk);
		r += w * g;
		k *= 2.0;
	}
	return r;
}

float4 RingTechOROPS(MeshVS frg) : COLOR
{
	float3 pp  = gCameraPos*gRadius[2] - frg.CamW*gDistScale;      // fragment rel. planet centre [m]
	float  dpp = dot(pp, pp);
	float  len = sqrt(dpp);

	// --- the stock result, for the blend --------------------------------------------
	float  lenS  = saturate(smoothstep(gRingRad.x, gRingRad.y, len));
	float4 stock = tex2D(RingProfS, float2(lenS, 0.5));
	stock.a = stock.r * 0.75;

	// --- ORO: linear radius, real tau -----------------------------------------------
	float  u    = saturate((len - gRingRad.x) * gRingRad.z);
	float4 prof = tex2D(RingProfS, float2(u, 0.5));
	float  tau  = 5.0 * prof.a * prof.a * gRingPrm.y;
	float  inRing = step(gRingRad.x, len) * step(len, gRingRad.y);

	// --- ROUND 2: THE CLOSE-UP (see the block above RingTechOROPS) -----------------------
	// eye -> fragment in metres (CamW points the other way), then the camera-relative ring
	// coordinates: the CPU's offsets as integer + fraction (gRingPrm2.yz = psi/rho integer,
	// gRingPrm2.w / gRingPrm3.z = their fractions), the fraction carrying the metres in base
	// cells. The grooves and grain modulate the DENSITY - brightness and tau together, which
	// is what a ringlet is. Multiplying by exactly 1.0 at zero amplitude is the A/B; the
	// max() guards a zeroed look (no anchor pushed) from a 1/0.
	// THE PLANET PASS INSIDE THE NEAR-FIELD SEAM is alpha 0 by the crossfade below, so the
	// whole evaluation from here on would be thrown away: out early. The near-field disc
	// draws those pixels (flight 11: the seam moved out to 600 km, most of the visible sheet).
	if (gRingCut.w > 0.0 && dot(-frg.CamW * gDistScale, gRingCut.xyz) < gRingCut.w) return float4(0, 0, 0, 0);
	float2 bumpS = 0.0;                                                   // the relief's slope (radial, along-track)
	{
		float3 cw   = -frg.CamW * gDistScale;
		float  dcam = length(cw);
		float  Lt   = 1.0 / max(gRingPrm3.x, 1e-9);                          // along-track base cell [m]
		float  Lr   = 1.0 / max(gRingPrm3.y, 1e-9);                          // radial base cell [m]
		float  fr   = gRingPrm3.z + dot(cw, gRingAxR.xyz) * gRingPrm3.y;     // radial fraction + metres in cells
		float  fp   = gRingPrm2.w + dot(cw, gRingAxT.xyz) * gRingPrm3.x;     // along track, likewise
		float  fk   = 290.0 * gRingPrm3.w;                                   // the Detail slider: the fade's reach
		float3 r    = RingDetail(gRingPrm2.z, fr, gRingPrm2.y, fp, dcam, Lr, Lt, fk);
		float  det  = r.x * gRingPrm2.x;                                     // Contrast
		float  dm   = max(1.0 + det, 0.05);
		prof.rgb *= dm;
		tau      *= dm;
		bumpS     = r.yz * (0.5 * gRingPrm4.x);                              // Relief: the density read as height
	}

	float3 N   = normalize(frg.nrmW);
	float  muV = max(abs(dot(N, normalize(frg.CamW))), 0.02);
	float  muS = max(abs(dot(N, gSun.Dir)), 0.02);
	float  A   = (1.0 - exp(-tau / muV)) * inRing;                 // opacity

	// the planet's shadow on the ring: stock, verbatim
	float  da = dot(normalize(pp), gSun.Dir);
	float  r  = sqrt(max(0.0, dpp * (1.0 - da*da)));
	float  sh = max(0.05, smoothstep(gRadius[0], gRadius[1], r));
	if (da < 0) sh = 1.0;
	stock.rgb *= sh;

	// lit face: reflection, dimming as the sun goes grazing relative to the view
	float  illum = min(1.0, 2.0 * muS / (muV + muS));
	// ROUND 2, RELIEF (his flight-9 ask, "some type of 3d feeling"): the density field read
	// as HEIGHT. Its slope tilts the sheet's sun-facing normal, and the lit face takes the
	// tilted normal's Lambert term over the flat one - every ridge gets a lit side and a
	// shade side, which is what makes a flat texture read as relief, and Saturn's sun is
	// never more than 27 deg above the plane, the ideal grazing light for it. 1.0 exactly
	// where the slope is zero (the far ring, or Relief 0), a floor so a ridge's back is
	// shaded and not black, a cap so a steep face cannot blow out.
	float3 toSunV = -gSun.Dir;
	float3 Ns     = (dot(N, toSunV) >= 0.0) ? N : -N;                // the sun-facing side
	float3 Nb     = normalize(Ns - bumpS.x * gRingAxR.xyz - bumpS.y * gRingAxT.xyz);
	float  nl0    = max(dot(Ns, toSunV), 0.05);
	float  nl1    = saturate(dot(Nb, toSunV));
	float  relief = min(0.15 + 0.85 * nl1 / nl0, 3.0);
	float3 lit    = prof.rgb * gRingPrm.z * illum * relief;

	// unlit face: transmission. x/(e^x - 1) with x = tau * (1/muS) ... the single-scatter
	// slab's transmitted term normalised by the opacity, in closed form for muS == muV and
	// the general (exp(-tau/muS) - exp(-tau/muV)) * muS / (muS - muV) otherwise.
	float  dmu = muS - muV;
	float  x   = tau / muS;
	float  Gs  = x / max(exp(x) - 1.0, 1e-5);                       // muS == muV form
	float  Gg  = (exp(-tau/muS) - exp(-tau/muV)) * muS / (abs(dmu) > 1e-3 ? dmu : 1e-3)
	             / max(1.0 - exp(-tau/muV), 1e-4);                  // general form / opacity
	float  G   = (abs(dmu) > 1e-3) ? saturate(Gg) : saturate(Gs);
	// ... PLUS what single scattering leaves out, and what made the unlit face "a negative of
	// the top side, with large black bands" (his flight 7): at tau 5 the single-scatter
	// transmission is e^-5, and no glow slider multiplies that into anything. Two more
	// routes light takes through a real ring, both visible in every Cassini unlit-side
	// mosaic, where the B ring is dark but THERE and the whole face brightens toward the
	// planet: (1) MULTIPLE SCATTERING - the two-stream slab's total transmission
	// 1/(1 + 3 tau/4) less the direct beam, halved for an icy albedo - a dense ring diffuses
	// a tenth of the sunlight out of its far face however thick it is; (2) PLANETSHINE,
	// (R/r)^2 off the planet's lit crescent. Both are absolute radiances, so they divide by
	// the opacity like G does (the blend multiplies by A again).
	float  Td  = 0.5 * (1.0 / (1.0 + 0.75 * tau / muS) - exp(-tau / muS));
	float  Ps  = 0.05 * gRadius[2] * gRadius[2] / max(dpp, 1.0);
	float3 unlit = prof.rgb * gRingPrm.w * (G + (Td + Ps) / max(A, 1e-3));

	bool   backlit = (dot(N, frg.CamW) * dot(N, gSun.Dir)) > 0;     // camera and sun on opposite faces
	float3 oro = (backlit ? unlit : lit) * sh;

	// ROUND 2 (his ask after flight 8): THE WORLD'S SHADOWS ON THE SHEET - a hull, a base -
	// from patch (ae)'s cascade atlas, exactly the lookup every hull and terrain pixel makes
	// (Cascaded (ORO) mode; the function returns 1 in any other). The receiver normal is the
	// sheet's sun-facing side, and the shadow darkens BOTH faces - the transmitted light of
	// the unlit face is the same sunlight, blocked by the same hull. Near-field draw only:
	// the planet pass's positions are distance-scaled, and no hull's shadow reaches past the
	// disc (the focus hull's atlas box carries a kilometre of reach, the shadow of a ship
	// 20 m off the plane at Saturn's sun falls ~40 m from it).
	if (gRingCut.w < 0.0) {
		oro *= OroCascadeShadow(-frg.CamW * gDistScale, Ns, toSunV);
	}

	float4 res = float4(lerp(stock.rgb * (backlit ? 0.35 : 1.0), oro, gRingPrm.x),
	                    lerp(stock.a, A, gRingPrm.x));

	// ROUND 2: THE SEAM. In the scene's z-clear mode the planet pass clips the sheet on its
	// near plane (a straight line at |gRingCut.w| of real depth) and a near-field draw after
	// the hulls fills what it cut. Butted, the two draws met on a hairline that carried the
	// planet pass's float error (flight 5's "visible line"). So they OVERLAP over the band
	// [zc, 1.2 zc] and crossfade: the near draw goes a(1-s), and the planet pass - drawn
	// FIRST - takes the EXACT complement a s / (1 - a + a s), so that composited in that
	// order the two total exactly a everywhere in the band (1-(1-af)(1-an) = a) and the
	// planet pass's hard clip edge sits at af = 0, invisible whatever its float error.
	if (gRingCut.w != 0.0) {
		float  zc = abs(gRingCut.w);
		float  dz = dot(-frg.CamW * gDistScale, gRingCut.xyz);         // view depth [m]
		float  s  = smoothstep(zc, zc * 1.2, dz);
		float  a  = res.a;
		res.a = (gRingCut.w > 0.0) ? a * s / max(1.0 - a + a * s, 1e-4) : a * (1.0 - s);
	}
	return res;
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
	// ORO patch (aa): the fog, up front - it attenuates the sun and lifts the ambient.
	float  fogT = 1.0f, fogSun = 1.0f; float3 cFog = 0;
	OroFog(-frg.CamW, -gSun.Dir, fogT, fogSun, cFog);
	// ORO patch (aa): SNOW COVER - dormant at gSnow.x 0
	[branch] if (gSnow.x > 0.0f)
		cTex.rgb = lerp(cTex.rgb, ORO_SNOW_ALBEDO, OroSnowMask(nrmW, gFogCam.xyz, 1e9f, frg.tex0 * 48.0f));
	float3 ambE = gSun.Ambient * (1.0f + gStorm * 1.8f) * (1.0f + gFogLift * (1.0f - fogSun));
	float  fCasc = 1.0f;
#if defined(_CASCADE)
	fCasc = OroCascadeShadow(-frg.CamW, nrmW, -gSun.Dir);   // ORO patch (ae): the world's shadows on the apron
#endif
	float3 clr = cTex.rgb * saturate((d + s) * gSun.Color * (1.0f - gStorm) * fogSun * fCasc + ambE);

	if (gNight) clr += tex2D(Tex1S, frg.tex0).rgb * gBaseGlow;   // ORO patch (ac): the runway markings' night layer

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
		// POOLS OFF AT THE BOTTOM OF THE SLIDER - see the matching note in NewPlanet.hlsl.
		// Both ground paths, together this time: the runway is a base tile and the apron
		// is terrain, and patch (s) has now been caught half-wired twice for exactly that.
		pud *= saturate(gWetSwimPrm.z * 10.0f + 1.0f);
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
			// REFLECTION BLUR (2026-08-24, gWetGrainPrm.z - see the note in Scene.cpp;
			// it rides the grain vector only because gWetReflPrm has no spare channel).
			// Wet ground is not a mirror: it scatters, and the reflection should be
			// slightly diffuse. The knob stretches the existing vertical smear and, past
			// zero, opens a lateral pair as well. AT 0 THE THREE TAPS AND THEIR WEIGHTS
			// ARE EXACTLY AS SHIPPED, so the approved crisp look is bit-identical; the
			// widening branch is uniform-driven, so every pixel takes the same path and
			// it costs nothing while the slider is down.
			float  blur = gWetGrainPrm.z;
			float  smr = gWetReflPrm.y * 3.2f * (1.0f + 7.0f * blur);   // shorter smear
			float4 cVes = tex2D(WetReflS, ruv) * 0.5f
			            + tex2D(WetReflS, ruv + float2(0, smr)) * 0.3f
			            + tex2D(WetReflS, ruv + float2(0, smr * 2.5f)) * 0.2f;
			if (blur > 0.001f) {
				float  hor = gWetReflPrm.x * 9.0f * blur;
				float4 wide = tex2D(WetReflS, ruv + float2( hor, 0))
				            + tex2D(WetReflS, ruv + float2(-hor, 0))
				            + tex2D(WetReflS, ruv + float2(0, -smr * 0.6f));
				cVes = lerp(cVes, cVes * 0.4f + wide * 0.2f, saturate(blur));
			}
			float  rStr = cVes.a * saturate(0.35f + 0.90f * mirror)
			            * saturate(0.55f * wet + 1.10f * pud) * gWetReflPrm.z;
			clr = lerp(clr, cVes.rgb, saturate(rStr));
		}

		// A tight sun glint on top - the specular the dry path already computes, but
		// sharper and stronger, because water is smooth where concrete is not.
		// Round 3: now storm-collapsed like every directional term (it previously rode
		// the RAW diffuse dot and survived full overcast) - the broad glare below is
		// what replaces it under the deck.
		float gl = pow(saturate(dot(r, CamW)), 90.0f) * d * (1.0f - gStorm) * fogSun   // ORO patch (aa)
		         * (0.7f * wet + 2.4f * pud);
		clr += gSun.Color * gl;
		// POOL SKY GLARE (part 7 round 3, his ask): a BROAD view-dependent lobe - the
		// bright cloud deck around the hidden sun - growing as the storm does, so the
		// pools flash as the camera moves even under full overcast. cSky already
		// carries the daylight factor, so this goes dark at night on its own.
		float glb = pow(saturate(dot(r, CamW)), 7.0f) * (0.30f + 0.70f * gStorm);
		clr += cSky * glb * (0.30f * wet + 1.10f * pud);
	}

	// ORO patch (aa): the storm fog that lived here (patch (s) part 2) is the unified
	// fog's layer 1 now, driven by the addon from the storm; the fog goes on LAST, on
	// the final colour, the same display-space grey the terrain and the hulls use.
	return float4(lerp(clr.rgb*frg.atten.rgb+frg.insca.rgb, cFog, 1.0f - fogT), cTex.a);
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
	outVS.dist  = length(posW);	// ORO patch (ab)
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
	outVS.dist  = length(posW);	// ORO patch (ab)
	return outVS;
}

float4 ShadowTechPS(float4 sc : VPOS, ShadowTexVS frg) : COLOR
{
	if (frg.tex0.b < 0) clip(-1);
	if (gOITEnable) {
		float4 alpha = tex2D(WrapS, frg.tex0.xy);
		if (alpha.a < 0.5f) clip(-1);
	}
	// ORO patch (ab): THE SOFT DEPTH TEST. The sheet is the mesh projected onto ONE flat
	// plane, so it sits metres under every bump and floats over every dip - a hardware
	// z-test clips it at every bump (flown 2026-09-01, reverted). Now that the terrain
	// writes GBUF_DEPTH, this asks HOW FAR behind the scene the sheet is: a bump under
	// it is metres and passes; a hill (or a hull, or a hangar) between it and the eye
	// is tens to hundreds and hides it. Tolerance = base + k * distance, pushed by
	// Scene.cpp after the depth pass and zero while the buffer is stale (probes).
	[branch] if (gSceneDepthPrm.z > 0.0f) {
		float2 duv = (sc.xy + 0.5f) * gSceneDepthPrm.xy;
		float  sd  = tex2Dlod(SceneDepthS, float4(duv, 0, 0)).a;   // explicit LOD: a fetch inside [branch] may not use gradients
		bool   hit = (sd > 0.1f && frg.dist > sd + gSceneDepthPrm.z + gSceneDepthPrm.w * sd);
		// ORO patch (ab) INSTRUMENT (2026-09-06, flight one of the shadow rewrite): with
		// ShadowDebug set, the sheet is COLOURED by the verdict instead of clipped, so a
		// screenshot shows which branch a blinking shadow took. Mode 1: green = no depth
		// under it, red = clipped, blue = passed. Mode 2: red = the scene depth is NEARER
		// than the sheet, blue = farther, brightness = |difference| / 4 m.
		[branch] if (gOroDbg > 0.5f) {
			if (sd <= 0.1f) return float4(0.0f, 1.0f, 0.0f, 0.75f);
			if (gOroDbg < 1.5f) return hit ? float4(1.0f, 0.0f, 0.0f, 0.75f) : float4(0.0f, 0.3f, 1.0f, 0.75f);
			float df = frg.dist - sd;                     // + = the sheet lies behind the scene
			float m  = 0.15f + 0.85f * saturate(abs(df) / 4.0f);
			return (df > 0.0f) ? float4(m, 0.0f, 0.0f, 0.75f) : float4(0.0f, 0.0f, m, 0.75f);
		}
		if (hit) clip(-1);
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
		// ORO patch (ab): the hardware z-test stays OFF - a depth-tested variant was
		// flown 2026-09-01 and reverted (the flat sheet clipped under every bump).
		// The occlusion is a SOFT test in ShadowTechPS against GBUF_DEPTH instead,
		// possible now that the terrain writes that buffer.
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
		// ORO patch (ab): the hardware z-test stays OFF - a depth-tested variant was
		// flown 2026-09-01 and reverted (the flat sheet clipped under every bump).
		// The occlusion is a SOFT test in ShadowTechPS against GBUF_DEPTH instead,
		// possible now that the terrain writes that buffer.
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


// ============================================================================
// ORO 2026-09-10: THE WET-GROUND LOOK ON RUNWAYS, PADS AND TAXIWAYS (his report: "a vessel
// landed on a textured runway doesn't get reflected"). Those are BELOW-SHADOW BASE
// STRUCTURES, drawn through the VESSEL shader path (RENDER_BASEBS -> PBR/FAST), which gives
// them a hull's wet look - darkening, drop glint - and none of the ground's: no pools, no
// sky film, no mirror. FAST_PS sits at the ps_3_0 temp ceiling, so the tile's wet block
// cannot move in there. Instead it is drawn OVER each such mesh, straight after its own
// draw and inside the same depth-bias bracket (vBase::RenderSurface), as two cheap passes
// carrying BaseTilePS's sky film and mirrored vessel in ONE alpha blend: the tile's two
// sequential lerps collapse to a = 1-(1-m)(1-r), src = (cSky m (1-r) + cVes r)/a - exact,
// not an approximation. Haze and fog are affine, so a fogged overlay over the runway's
// already-fogged colour composes correctly.
//
// ⚠️⚠️ NO POOLS, AND THE FIRST BUILD'S TWO MISTAKES ARE WHY (his screenshots, 2026-09-10:
// "the pools look awful"). It lifted the coefficients from BaseTilePS, whose pool darkening
// is 1 - 0.988 gWetDark (essentially black) against the TERRAIN's 1 - 0.85, and its lattice
// period was 48 m - three to five times the apron's - so the runway carried huge blobs of
// near-black ground under a full-weight sky film: white plates. AND IT IS THE RIGHT THING TO
// DROP RATHER THAN RETUNE: a runway is crowned and transversely grooved precisely to shed
// water, so standing water on one is a defect, not weather. Pads do puddle in reality, but
// they are indistinguishable from runways in the below-shadow list and a uniform rule beats
// a wrong half. The lattice and the grain go with them (grain is a POOL texture - his call,
// same day, for the water case), which makes this a fraction of the first build's cost.
//
// ⚠️ AND THE DARKENING PASS IS GONE, because it was the black-silhouette bug. It MODULATED,
// and it multiplied a colour that already contained fog and aerial perspective - so at
// distance, where the pixel is mostly fog, it darkened the fog itself and the runways stayed
// as hard shapes that never blended (his zoomed-out Mojave shots). A multiply cannot be
// fixed by tuning: the runway keeps the vessel path's own lerp(1, 0.66, wet) darkening,
// which is applied BEFORE fog inside the shader where it belongs. The remaining difference
// from the apron beside it is modest and one-directional; if it ever reads wrong the honest
// fix is a base-ground flag in the vessel shaders, not a pass that multiplies the sky.
// Exactly nothing at gSurfWet 0, and the draw is skipped entirely.
// ============================================================================
struct WetOvVS
{
	float4 posH  : POSITION0;
	float3 CamW  : TEXCOORD0;
	float2 uvL   : TEXCOORD1;   // base-local metres (x, z): the pool lattice coordinate
	float3 nrmW  : TEXCOORD2;
	float4 atten : COLOR0;
	float4 insca : COLOR1;
};

WetOvVS WetOverlayVS(NTVERTEX vrt)
{
	WetOvVS outVS = (WetOvVS)0;
	float3 posW  = mul(float4(vrt.posL, 1.0f), gW).xyz;
	outVS.posH   = mul(float4(posW, 1.0f), gVP);
	outVS.nrmW   = mul(float4(vrt.nrmL, 0.0f), gW).xyz;
	outVS.uvL    = vrt.posL.xz;
	outVS.CamW   = -posW;
	AtmosphericHaze(outVS.atten, outVS.insca, outVS.posH.z, posW);
	float4 diffuse; float ambi, nigh;
	LegacySunColor(diffuse, ambi, nigh, outVS.nrmW);
	outVS.insca *= (diffuse + ambi);
	outVS.insca.a = nigh;
	return outVS;
}

// The sky film and the mirrored vessel over a damp paved surface, in one alpha blend.
// Every constant in here is the TERRAIN's, so the runway matches the apron it sits in -
// see the sky-colour note below for why copying the base tile's was wrong.
void WetOvCommon(float4 sc, WetOvVS frg, out float mirror, out float3 cSky,
                 out float4 cVes, out float rStr)
{
	float3 nrmW = normalize(frg.nrmW);
	float3 CamW = normalize(frg.CamW);
	float  fogT = 1.0f, fogSun = 1.0f; float3 cFog = 0;
	OroFog(-frg.CamW, -gSun.Dir, fogT, fogSun, cFog);
	float3 ambE = gSun.Ambient * (1.0f + gStorm * 1.8f) * (1.0f + gFogLift * (1.0f - fogSun));
	float  wet  = gSurfWet;

	// ⚠️⚠️ THE SKY COLOUR IS THE TERRAIN'S, NOT THE BASE TILE'S, AND THAT IS THE WHOLE
	// POINT OF THIS BLOCK (his screenshots, 2026-09-11: a wet runway LIGHTER than the dry
	// one, and far lighter than the correctly-darkened terrain beside it). The tile's own
	// film colour is `ambE * 2.6`, and ambE is the Launchpad ambient times (1 + storm*1.8)
	// times the fog lift - UNBOUNDED, and under a storm with mist it goes past 1.0, so the
	// overlay was painting a blown-out white sky onto black asphalt. The TERRAIN uses a
	// bounded grey that tops out near 0.66, which is why the apron has always looked right.
	// The base-tile version was never judged because the ground a vessel parks on at KSC is
	// terrain - the seventh-path trap, one more time. Matching the apron is the whole job
	// here, so the apron's numbers are the ones to copy: same colour, same daylight scaling,
	// same storm lift, same pow-5 Fresnel, same 0.30/0.70 film weights.
	float dayK = saturate(dot(-gSun.Dir, nrmW) * 1.3f + 0.12f);   // the terrain's cAmb.a, for a ground plane
	cSky = float3(0.42f, 0.45f, 0.49f) * dayK * (1.0f + gStorm * 0.35f);
	float fres = pow(1.0f - saturate(dot(nrmW, CamW)), 5.0f);
	mirror = saturate(saturate((0.30f + 0.70f * fres) * wet) * 0.60f);

	cVes = 0; rStr = 0;
	if (gWetReflPrm.w > 0.5f) {
		float2 ruv = (sc.xy + 0.5f) * gWetReflPrm.xy;
		ruv.x = 1.0f - ruv.x;      // undo the mirror pass's clip-space X flip
		// the animated jitter only - the static pool-shape offset went with the pools.
		// Phase in base-local metres, so it is fixed in the world like the tile's.
		ruv += float2(sin(gWetTime * 12.0f * gWetSwimPrm.y + frg.uvL.y * 0.4f),
		              cos(gWetTime *  9.7f * gWetSwimPrm.y + frg.uvL.x * 0.4f))
		     * 0.0005f * gWetSwimPrm.x;
		float  blur = gWetGrainPrm.z;
		float  smr = gWetReflPrm.y * 3.2f * (1.0f + 7.0f * blur);
		cVes = tex2D(WetReflS, ruv) * 0.5f
		     + tex2D(WetReflS, ruv + float2(0, smr)) * 0.3f
		     + tex2D(WetReflS, ruv + float2(0, smr * 2.5f)) * 0.2f;
		if (blur > 0.001f) {
			float  hor = gWetReflPrm.x * 9.0f * blur;
			float4 wide = tex2D(WetReflS, ruv + float2( hor, 0))
			            + tex2D(WetReflS, ruv + float2(-hor, 0))
			            + tex2D(WetReflS, ruv + float2(0, -smr * 0.6f));
			cVes = lerp(cVes, cVes * 0.4f + wide * 0.2f, saturate(blur));
		}
		// The terrain's own reflection shape too, with ONE compensation: its wetness term is
		// `0.70*wet + 1.25*pud`, and with no pools here the 1.05 stands in for what the
		// pools were carrying, so a damp runway mirrors as strongly as a pooled apron does.
		rStr = saturate(cVes.a * saturate(0.30f + 2.2f * fres)
		     * saturate(1.05f * wet) * 0.85f * gWetReflPrm.z);
	}
}

float4 WetOverlayPS(float4 sc : VPOS, WetOvVS frg) : COLOR
{
	float mirror, rStr; float3 cSky; float4 cVes;
	WetOvCommon(sc, frg, mirror, cSky, cVes, rStr);
	float  a   = 1.0f - (1.0f - mirror) * (1.0f - rStr);
	float3 src = (cSky * mirror * (1.0f - rStr) + cVes.rgb * rStr) / max(a, 1e-4f);
	float  fogT = 1.0f, fogSun = 1.0f; float3 cFog = 0;
	OroFog(-frg.CamW, -gSun.Dir, fogT, fogSun, cFog);
	src = lerp(src * frg.atten.rgb + frg.insca.rgb, cFog, 1.0f - fogT);
	return float4(src, a);
}

technique WetOverlayTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 WetOverlayVS();
		pixelShader  = compile ps_3_0 WetOverlayPS();
		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = true;
		ZWriteEnable = false;
		CullMode = CCW;
	}
}

// ============================================================================
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
		// ORO patch (z): base tiles obey the depth buffer. ZEnable=false is a
		// fossil of the flat-planet era (nothing could stand between camera and
		// runway, and depth-off dodged z-fighting with the coplanar ground);
		// with terrain elevation a RUNWAY painted through a mountain is the
		// visible symptom - a stock bug, reproducible with no addon loaded.
		// The terrain writes real depth at close range, so the test is valid;
		// ZWrite stays off, exactly as before.
		ZEnable = true;
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

// ORO patch (aj): same states as RingTech2 - the painter's split in vPlanet::Render
// (far half, planet, near half) is what makes ZEnable=false correct, and ZWrite lets
// vessels test against the sheet.
technique RingTechORO
{
	pass P0
	{
		vertexShader = compile vs_3_0 TinyMeshTechVS();
		pixelShader  = compile ps_3_0 RingTechOROPS();

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
