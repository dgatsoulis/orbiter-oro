// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ==============================================================

struct TileVS
{
	float4 posH    : POSITION0;
	float2 tex0    : TEXCOORD0;
	float3 normalW : TEXCOORD1;
	float3 toCamW  : TEXCOORD2;  // Vector to the camera
	float3 posW    : TEXCOORD3;  // World space vertex position
	float4 aux     : TEXCOORD4;  // Specular, Diffuse, Twilight, Night Texture Intensity,
	float4 diffuse : TEXCOORD5;  // Sun light
	float4 atten   : COLOR0;     // Attennuate incoming fragment color
	float4 insca   : COLOR1;     // "Inscatter" Add to incoming fragment color
};



TileVS PlanetTechVS(TILEVERTEX vrt)
{
	// Zero output.
	TileVS outVS = (TileVS)0;

	// Apply a mesh group transformation matrix
	float3 posW = mul(float4(vrt.posL, 1.0f), gW).xyz;
	float3 nrmW = normalize(mul(float4(vrt.normalL, 0.0f), gW).xyz);

	// Convert transformed vertex position into a "screen" space using a combined (World, View and Projection) Matrix
	outVS.posH = mul(float4(posW, 1.0f), gVP);

	// A vector from the vertex to the camera
	float3 tocam  = normalize(-posW);
	float3 sundir = gSun.Dir;

	float diff    = saturate(dot(-sundir, nrmW));
	float dotr    = max(dot(reflect(sundir, nrmW), tocam), 0.0f);
	float spec    = pow(diff,0.25f) * pow(dotr, gWater.specPower);
	float nigh    = 0.0f;
	float ambi    = 0.0f;

	outVS.tex0    = float2(vrt.tex0.x*gTexOff[0] + gTexOff[1], vrt.tex0.y*gTexOff[2] + gTexOff[3]);
	outVS.toCamW  = tocam;
	outVS.normalW = nrmW;
	outVS.posW    = gCameraPos*gRadius[2] + posW*gDistScale;

	LegacySunColor(outVS.diffuse, ambi, nigh, nrmW);

	outVS.aux     = float4(spec, diff, ambi, nigh);

	AtmosphericHaze(outVS.atten, outVS.insca, outVS.posH.z, posW);

	outVS.insca *= (outVS.diffuse+ambi);

	return outVS;
}



float4 PlanetTechPS(TileVS frg) : COLOR
{
	// ORO patch (aj) 2026-09-12: THE RING'S SHADOW ON THE PLANET - absent from stock
	// entirely. Position from the normal (exact: surfmgr v1 is a sphere), march toward
	// the sun (-gSun.Dir - Dir is the direction light TRAVELS), intersect the ring
	// plane, read the profile's optical depth at that radius, attenuate the SUN term
	// only - never ambient, and only ever darkens (a shadow must not create light).
	// Carries the ring's own structure, so the Cassini Division is a bright line inside
	// the band; the obliquity does the seasons (broad at solstice, a knife-edge at
	// equinox) with no special case. No branch around tex2D: sample, then gate.
	float3 P   = normalize(frg.normalW) * gRadius[0];
	float3 S   = -gSun.Dir;
	float  dn  = dot(S, gRingShd.xyz);
	float  adn = max(abs(dn), 1e-4);
	float  t   = -dot(P, gRingShd.xyz) / (dn >= 0.0 ? adn : -adn);
	float  R   = length(P + t * S);
	float  u   = saturate((R - gRingRad.x) * gRingRad.z);
	float  a   = tex2D(RingProfS, float2(u, 0.5)).a;
	float  tau = 5.0 * a * a * gRingPrm.y;
	float  hit = step(0.0, t) * step(gRingRad.x, R) * step(R, gRingRad.y);   // toward the sun, inside the annulus
	float  ringShd = lerp(1.0, exp(-tau * hit / max(adn, 0.02)), gRingPrm.x);

	float4 diff  = frg.aux.g*(gMat.diffuse*frg.diffuse*ringShd) + (gMat.ambient*frg.aux.b);
	float4 vSpe = frg.aux.r * (gWater.specular*frg.diffuse);
	float4 vEff = tex2D(Planet1S, frg.tex0);

	if (gSpecMode==2) vSpe *= 1.0f - vEff.a;
	if (gSpecMode==0) vSpe = 0;

	float3 cTex = tex2D(Planet0S, frg.tex0).rgb;
	float3 color = diff.rgb * cTex.rgb + frg.aux.a*vEff.rgb + vSpe.rgb;

	return float4(color*frg.atten.rgb+gColor.rgb+frg.insca.rgb, 1.0f);
}



float4 CloudTechPS(TileVS frg) : COLOR
{

	float4 data  = (gMat.ambient*frg.aux.b);
	float4 color = tex2D(Planet0S, frg.tex0);
	float  alpha = color.a;

	if (dot(frg.normalW, frg.toCamW)<0) {    // Render cloud layer from below
		float4 diff = (min(1,frg.aux.g*2) * frg.diffuse) * gMat.diffuse + data;
		return float4(color.rgb*diff.rgb, alpha);
	}

	else { // Render cloud layer from above
		float4 diff = (min(1,frg.aux.g*1.5) * frg.diffuse) * gMat.diffuse + data;
		return float4(color.rgb*diff.rgb, alpha);
	}
}









// -----------------------------------------------------------------------------
// Cloud Shadow Techs
// -----------------------------------------------------------------------------


struct ShadowVS
{
	float4 posH    : POSITION0;
	float2 tex0    : TEXCOORD0;
	float4 atten   : TEXCOORD2;
};

ShadowVS CloudShadowTechVS(TILEVERTEX vrt)
{
	// Zero output.
	ShadowVS outVS = (ShadowVS)0;

	float3 posW = mul(float4(vrt.posL, 1.0f), gW).xyz;
	outVS.posH  = mul(float4(posW, 1.0f), gVP);
	outVS.tex0  = float2(vrt.tex0.x*gTexOff[0] + gTexOff[1], vrt.tex0.y*gTexOff[2] + gTexOff[3]);

	float4 none;

	AtmosphericHaze(outVS.atten, none, outVS.posH.z, posW);

	return outVS;
}

float4 CloudShadowPS(ShadowVS frg) : COLOR
{
	return float4(0,0,0, tex2D(Planet0S, frg.tex0).a * frg.atten.b);
}





// This is used for high resolution base tiles ---------------------------------
//
technique PlanetTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 PlanetTechVS();
		pixelShader  = compile ps_3_0 PlanetTechPS();

		AlphaBlendEnable = false;
		ZEnable = false;
		ZWriteEnable = false;
	}
}

technique PlanetCloudTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 PlanetTechVS();
		pixelShader  = compile ps_3_0 CloudTechPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
	}
}

technique PlanetCloudShadowTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 CloudShadowTechVS();
		pixelShader  = compile ps_3_0 CloudShadowPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZEnable = false;
		ZWriteEnable = false;
	}
}