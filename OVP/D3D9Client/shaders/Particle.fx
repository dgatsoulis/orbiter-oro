// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ==============================================================

struct EPVERTEX {
	float3 posL     : POSITION0;
	float2 tex0     : TEXCOORD0;
};

struct ParticleVS
{
	float4 posH     : POSITION0;
	float2 tex0     : TEXCOORD0;
	float3 light    : TEXCOORD1;	// ORO patch (x): RGB (was a scalar) - the sun share carries a hue
	float4 fog      : TEXCOORD2;	// ORO patch (aa): rgb = the fog colour, a = transmittance
};

ParticleVS ParticleDiffuseVS(NTVERTEX vrt)
{
	ParticleVS outVS = (ParticleVS)0;
	outVS.tex0    = vrt.tex0;
	// ORO patch (x): per-particle lighting, computed on the CPU and carried in the
	// normal channel's x component (D3D9ParticleStream::RenderDiffuse). Stock
	// hardcoded 1.0 here with the original N.L term commented out - normal-based
	// lighting misbehaves on camera-facing billboards, so the replacement is
	// POSITION-based: sun visibility at the particle (night side, terminator,
	// twilight, altitude-depressed horizon) + the user's ambient floor + the engine
	// flame lighting nearby smoke, with the sun share tinted by the client's own
	// atmospheric sun colour (sunset reddening). A fully sunlit particle carries
	// exactly (1,1,1), so the daytime look is bit-identical to stock.
	outVS.light   = vrt.nrmL.xyz; // stock: 1.0f; // saturate(dot(-gSun.Dir, vrt.nrmL) * 2.0f);
	outVS.posH    = mul(float4(vrt.posL, 1.0f), gVP);
	// ORO patch (aa): the fog, per particle corner (posL is camera-relative world space)
	{ float T, sA; float3 cF; OroFog(vrt.posL, -gSun.Dir, T, sA, cF); outVS.fog = float4(cF, T); }
	return outVS;
}

ParticleVS ParticleEmissiveVS(EPVERTEX vrt)
{
	ParticleVS outVS = (ParticleVS)0;
	outVS.tex0   = vrt.tex0;
	outVS.posH   = mul(float4(vrt.posL, 1.0f), gVP);
	{ float T, sA; float3 cF; OroFog(vrt.posL, -gSun.Dir, T, sA, cF); outVS.fog = float4(cF, T); }   // ORO patch (aa)
	return outVS;
}



// ----------------------------------------------------------------------------
// gMix is the particle opacity computed from time and halflife
// gColor is hardcoded to [1,1,1] in exhaust streams and [1, 0.7, 0.5] in reentry streams
// frg.light is a sun light intensity level illuminating a particles. Light color is [1,1,1]
// ----------------------------------------------------------------------------


float4 ParticleDiffusePS(ParticleVS frg) : COLOR
{
	float4 color = tex2D(WrapS, frg.tex0);
	return float4(lerp(color.rgb*frg.light, frg.fog.rgb, 1.0f - frg.fog.a), color.a*gMix);   // ORO patch (aa)
}

float4 ParticleEmissivePS(ParticleVS frg) : COLOR
{
	float4 color = tex2D(WrapS, frg.tex0);
	return float4(lerp(color.rgb*gColor.rgb, frg.fog.rgb, 1.0f - frg.fog.a), color.a*gMix);   // ORO patch (aa)
}

float4 ParticleShadowPS(ParticleVS frg) : COLOR
{
	float4 color = tex2D(WrapS, frg.tex0);
	return float4(0,0,0,color.a*gMix*2.0*frg.fog.a);   // ORO patch (aa): a shadow through fog is fainter
}



technique ParticleDiffuseTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 ParticleDiffuseVS();
		pixelShader  = compile ps_3_0 ParticleDiffusePS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = false;
	}
}


technique ParticleEmissiveTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 ParticleEmissiveVS();
		pixelShader  = compile ps_3_0 ParticleEmissivePS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = false;
	}

	// ------------------------------------------------------------------------
	// Ground shadows are rendered only for DIFFUSE particles
	// Shadow rendering is defined here because of identical vertex declarations [EPVERTEX]
	// ------------------------------------------------------------------------
	pass P1
	{
		vertexShader = compile vs_3_0 ParticleEmissiveVS();
		pixelShader  = compile ps_3_0 ParticleShadowPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = false;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = false;
	}
}