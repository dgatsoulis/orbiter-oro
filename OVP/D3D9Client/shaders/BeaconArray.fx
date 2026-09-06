// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
// ==============================================================

struct BAVERTEX {
	float3 posL     : POSITION0;
	float3 dirL     : NORMAL0;
	float4 data		: TEXCOORD0;
	float2 dataB	: TEXCOORD1;
	float4 colr		: COLOR0;
};


struct BeaconVS
{
	float4 posH     : POSITION0;
	float4 colr     : COLOR0;
	float2 tex0     : TEXCOORD0;
	float  size     : PSIZE;
	float  haze	    : COLOR1;
};


BeaconVS BeaconArrayVS(BAVERTEX vrt)
{
	BeaconVS outVS = (BeaconVS)0;

	//float3 posX   = mul(float4(vrt.posL, 1.0f), gW).xyz;
	//float dist    = length(posX);	// distance to a beacon
	//float3 offL   = float3(0, dist*2e-3, 0);
	//float3 posW   = mul(float4(vrt.posL+offL, 1.0f), gW).xyz;

	float3 posW   = mul(float4(vrt.posL, 1.0f), gW).xyz;
	float3 dirW   = mul(float4(vrt.dirL, 0.0f), gW).xyz;
	outVS.posH    = mul(float4(posW, 1.0f), gVP);

	//posW = posW * gDistScale;

	float fog     =  exp(-abs(outVS.posH.z * gFogDensity * 20.0f)); // Haze effect scale factor

	float dist    = length(posW);	// distance to a beacon

	// Viewing angle dependency
	float dota    = -dot(normalize(posW), dirW);
	float angl    = saturate((dota - vrt.data[1])/(1.0f-vrt.data[1])); // beacon visibility from a current point 1.0 to 0.0
	float disp    = pow(angl, vrt.dataB[1]); // apply a proper curve into a visibility

	// Distance attennuation
	float att0    = vrt.dataB[0] / (1.0f+dist*2e-4);
	float att1    = saturate(pow(att0, 2.0f));

	if (gTime<vrt.data[2] || gTime>vrt.data[3]) disp = 0.0f;

	// ORO patch (aa): a runway light dims through the fog between it and the eye.
	float fogT = 1.0f, fogSun = 1.0f; float3 cFog = 0;
	OroFog(posW, -gSun.Dir, fogT, fogSun, cFog);
	// ORO patch (ac) part 2: THE HALO (his reference: runway lights in fog). The direct
	// beam dims by the transmittance T, but the air round the lamp scatters its strong
	// near field toward the eye: a soft aureole that GROWS with the optical depth. Size
	// and softness ride (1 - T) x the Lights halo gain; the alpha fades as sqrt(T)
	// rather than T while a halo is wanted, because what you see is the aureole, lit by
	// the lamp's near field, not the beam. Halo gain 0 = the plain (aa) attenuation.
	float haloK = saturate((1.0f - fogT) * gBaseHalo);
	float aT    = lerp(fogT, sqrt(fogT), saturate(gBaseHalo));
	outVS.colr    = float4(vrt.colr.rgb * gBaseGlow, min(1, vrt.colr.a * disp * att1 * 1.2f) * aT);   // ORO patch (ac): the glow
	outVS.size    = (5.5f + vrt.data[0] * gPointScale / dist) * disp * (1.0f + 5.0f * haloK);
	outVS.haze    = lerp(clamp(fog*1.8f, 0.3f, 1.8f), 0.30f, haloK);   // lower exponent = softer, wider disc

	return outVS;
}

float4 BeaconArrayPS(BeaconVS frg) : COLOR
{
	float4 cTex = tex2D(ClampS, frg.tex0);
	//return float4(frg.colr.rgb*saturate(pow(abs(cTex.a),0.3f)*gMix), pow(abs(cTex.a), frg.haze) * frg.colr.a);
	return float4(frg.colr.rgb, pow(abs(cTex.a), frg.haze) * frg.colr.a);
}


technique BeaconArrayTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 BeaconArrayVS();
		pixelShader  = compile ps_3_0 BeaconArrayPS();

		PointSpriteEnable = true;
		AlphaBlendEnable = true;
		BlendOp = Add;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		// ORO patch (z): runway lights obey the depth buffer - the other half
		// of the Mesh.fx BaseTileTech fix. Depth-blind beacons shone through a
		// mountain standing between camera and runway. ZWrite stays off: a
		// light is a sprite, not an occluder.
		ZEnable = true;
		ZWriteEnable = false;
	}
}