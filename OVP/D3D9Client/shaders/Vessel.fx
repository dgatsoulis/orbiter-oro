// ==============================================================
// Part of the ORBITER VISUALISATION PROJECT (OVP)
// Dual licensed under GPL v3 and LGPL v3
// ==============================================================



float3 cLuminosity = { 0.4, 0.7, 0.3 };


inline float cmax(float3 color)
{
	return max(max(color.r, color.g), color.b);
}

// Sun light brightness for diffuse and specular lighting
#include "LightBlur.hlsl"

// Incluse Light and Shadow
#include "Common.hlsl"

// Must be included here
#include "PBR.fx"

// Must be included here
#include "Metalness.fx"

// ============================================================================
// Vertex shader for physics based rendering
//
PBRData AdvancedVS(MESH_VERTEX vrt)
{
	// Zero output.
	PBRData outVS = (PBRData)0;

	float3 posW = mul(float4(vrt.posL, 1.0f), gW).xyz;
	float3 nrmW = mul(float4(vrt.nrmL, 0.0f), gW).xyz;

#if SHDMAP > 0
	outVS.shdH = mul(float4(posW, 1.0f), gLVP);
#endif

	outVS.nrmW = nrmW;
	outVS.tanW = float4(mul(float4(vrt.tanL, 0.0f), gW).xyz, vrt.tex0.z);
	outVS.posH = mul(float4(posW, 1.0f), gVP);
	outVS.camW = -posW;
	outVS.tex0 = vrt.tex0.xy;

	return outVS;
}



// ============================================================================
//
float4 AdvancedPS(float4 sc : VPOS, PBRData frg) : COLOR
{
	float3 bitW;
	float3 nrmT;
	float3 cRefl;
	float3 cEmis;
	float4 cSpec;
	float4 cTex;

	float3 cDiffLocal;
	float3 cSpecLocal;

	if (gTextured) cTex = tex2D(WrapS, frg.tex0.xy);
	else		   cTex = 1;

	if (gOITEnable) if (cTex.a < 0.5f) clip(-1);

	if (gCfg.Norm) nrmT  = tex2D(Nrm0S, frg.tex0.xy).rgb;

	if (gCfg.Spec) cSpec = tex2D(SpecS, frg.tex0.xy);
	else		   cSpec = gMtrl.specular;

	if (gCfg.Refl) cRefl = tex2D(ReflS, frg.tex0.xy).rgb;
	else		   cRefl = gMtrl.reflect.rgb;

	// Sample emission map. (Note: Emissive materials and textures need to go different stages, material is added to light)
	if (gCfg.Emis) cEmis = tex2D(EmisS, frg.tex0.xy).rgb * gBaseGlow;   // ORO patch (ac): 1 on vessels
	else		   cEmis = 0;


	float3 nrmW = frg.nrmW;
	float3 tanW = frg.tanW.xyz;
	// ORO patch (aa): the fog, up front - the direct sun through the column above the pixel.
	float  fogT = 1.0f, fogSun = 1.0f; float3 cFog = 0;
	OroFog(-frg.camW, -gSun.Dir, fogT, fogSun, cFog);
	float3 cSun = saturate(gSun.Color) * (1.0f - gStorm) * fogSun;   // ORO patch (s) part 2 + (aa)
	float3 CamD = normalize(frg.camW);
	float3 Base = (gMtrl.ambient.rgb*gSun.Ambient*(1.0f + gStorm * 1.8f)*(1.0f + gFogLift * (1.0f - fogSun))) + (gMtrl.emissive.rgb);


	// Compute World space normal -------------------------------------------
	//
	if (gCfg.Norm) {
		nrmT = nrmT * 2.0 - 1.0;
		bitW = cross(tanW, nrmW) * frg.tanW.w;
		nrmW = nrmW*nrmT.z + tanW*nrmT.x + bitW*nrmT.y;
	}

	nrmW  = normalize(nrmW);

	float3 TnrmW = -nrmW;
	float3 RflW  = reflect(-CamD, nrmW);
	float  dLN   = saturate(-dot(gSun.Dir, nrmW));

	if (gCfg.Spec) cSpec.a *= 255.0f;

	// ORO patch (s): A WET HULL, legacy path - same sweep as the fast path.
	if (gSurfWet > 0.001f) {
		cSpec.rgb = lerp(cSpec.rgb, cSpec.rgb + 0.55f, gSurfWet * 0.8f);
		cSpec.a   = lerp(cSpec.a, max(cSpec.a, 1.0f) * 7.0f, gSurfWet * 0.75f);
		// ORO 2026-09-11: base ground takes the terrain's darkening - see gBaseGround.
		cTex.rgb *= lerp(1.0f, lerp(0.66f, 1.0f - 0.494f * gWetDark, gBaseGround), gSurfWet);
	}

	// Approximate roughness
	float fRghn = log2(cSpec.a) * 0.1f;

	// Sunlight calculation
	float fSun = pow(saturate(-dot(RflW, gSun.Dir)), cSpec.a) * saturate(cSpec.a);

	if (dLN == 0) fSun = 0;

	// Special alpha only texture in use
	if (gNoColor) cTex.rgb = 1;


	// ----------------------------------------------------------------------
	// Add vessel self-shadows
	// ----------------------------------------------------------------------

	{
		float fShd = 1.0f;        // ORO patch (ae): the cascade term outside the SHDMAP block (see PBR.fx)
#if SHDMAP > 0
		fShd = ComputeShadow(frg.shdH, dLN, sc);
#endif
#if defined(_CASCADE)
		fShd = min(fShd, OroCascadeShadow(-frg.camW, nrmW, -gSun.Dir));   // ORO patch (ae): the world's shadows on the hull
#endif
		cSun.rgb *= fShd;
		// ORO patch (p): let the shadow eat the AMBIENT share of Base too. Base is
		// (ambient + emissive), so subtracting at most the ambient part can never take
		// Base below emissive and never goes negative.
		Base -= (gMtrl.ambient.rgb * gSun.Ambient) * ((1.0f - fShd) * gVCShdDepth);
	}



	// ----------------------------------------------------------------------
	// Compute Local Light Sources
	// ----------------------------------------------------------------------

	LocalLightsEx(cDiffLocal, cSpecLocal, nrmW, -frg.camW, cSpec.a, false);


	// Lit the diffuse texture
	// ORO patch (r): same emissive-overdrive fix as PBR.fx - see the long note there. This
	// is the LEGACY path (SHADER_LEGACY); a plain mesh defaults to SHADER_PBR, so this copy
	// exists so the behaviour does not depend on which shader a given mesh happens to take.
	// ORO patch (aa): SNOW COVER on the up-facing hull - dormant at gSnow.x 0
	[branch] if (gSnow.x > 0.0f)
		cTex.rgb = lerp(cTex.rgb, ORO_SNOW_ALBEDO, OroSnowMask(nrmW, gFogCam.xyz, 1e9f, frg.tex0.xy * 24.0f));
	float3 cAlbedo = cTex.rgb;			// texture colour before lighting
	cTex.rgb *= saturate(Base + gMtrl.diffuse.rgb * Light_fx(cDiffLocal + cSun * dLN));
	cTex.rgb += cAlbedo * max(gMtrl.emissive.rgb - 1.0f, 0.0f);
	// ORO patch (s): the drop glint - SKY light, after the bake (see PBR_PS note)
	cTex.rgb += WetSparkle(frg.tex0.xy, nrmW, frg.camW)
	          * gSun.Ambient * (1.0f + gStorm * 1.8f) * 6.5f;

	// Lit the specular surface
	cSpec.rgb *= saturate(cSpecLocal + fSun * cSun);


	// Compute Transluciency effect --------------------------------------------------------------
	//

	if (gCfg.Transm || gCfg.Transl) {

		float4 cTransm = float4(cTex.rgb, 1.0f);

		if (gCfg.Transm) {
			cTransm = tex2D(TransmS, frg.tex0.xy);
			cTransm.a *= 1024.0f;
		}

		float3 cTransl = cTex.rgb;

		if (gCfg.Transl) {
			cTransl = tex2D(TranslS, frg.tex0.xy).rgb;
		}

		// Texture Tuning -------------------------------------------------------
		//
		if (gTuneEnabled) {
			cTransm *= gTune.Transm.rgba;
			cTransl *= gTune.Transl.rgb;
		}

		float sunLightFromBehind = saturate(dot(gSun.Dir, nrmW));
		float sunSpotFromBehind = pow(saturate(dot(gSun.Dir, CamD)), cTransm.a);
		sunSpotFromBehind *= saturate(sunLightFromBehind * 3.0f);// Causes the transmittance (sun spot) effect to fall off at very shallow angles

		cTransl.rgb *= saturate(cSun * sunLightFromBehind);

		cTex.rgb += (1 - cTex.rgb) * cTransl.rgb;
		cTex.rgb += cTransm.rgb * (sunSpotFromBehind * cSun);
	}


	float fFrsl = 1.0f;
	float fInt = 0.0f;

	// Compute reflectivity
	float fRefl = cmax(cRefl);


#if defined(_ENVMAP)


	// Compute environment map/fresnel effects --------------------------------
	//
	if (gEnvMapEnable) {

		// Do we need fresnel code for this render pass ?

		if (gFresnel) {
		
			fFrsl = gMtrl.fresnel.y;

			// Get mirror reflection for fresnel
			float3 cEnvFres = texCUBElod(EnvMapAS, float4(RflW, 0)).rgb;

			float  dCN = saturate(dot(CamD, nrmW));

			// Compute a fresnel term with compensations included
			fFrsl *= pow(1.0f - dCN, gMtrl.fresnel.x) * (1.0 - fRefl) * any(cRefl);

			// Sunlight reflection for fresnel material
			cSpec.rgb = saturate(cSpec.rgb + fSun * fFrsl * cSun);

			// Compute total reflected light with fresnel reflection
			// and accummulate in cSpec
			cSpec.rgb = saturate(cSpec.rgb + fFrsl * cEnvFres);

			// Compute intensity
			fInt = saturate(dot(cSpec.rgb, cLuminosity));

			// Attennuate diffuse surface
			cTex.rgb *= (1.0f - fInt);
		}

		// Compute LOD level for blur effect
		float fLOD = (1.0f - fRghn) * 10.0f;

		float3 cEnv = texCUBElod(EnvMapAS, float4(RflW, fLOD)).rgb;

		// Compute total reflected light, accummulate in cSpec
		cSpec.rgb += cRefl.rgb * cEnv;
	}

#endif

	// Attennuate diffuse surface
	cTex.rgb *= (1.0f - fRefl);

	// Re-compute output alpha for alpha blending stage
	// NOTE: Without fresnel fInt remains zero
	cTex.a = saturate(cTex.a + fInt);

	// Add reflections to output
	cTex.rgb += cSpec.rgb;

	// Add emissive textures to output
	cTex.rgb += cEmis;

#if defined(_DEBUG)
	//if (gDebugHL) cTex = cTex*0.5f + gColor;
	cTex = cTex * (1 - gColor*0.5f) + gColor;
#endif

	cTex.rgb *= gSun.Transmission;
	cTex.rgb += gSun.Inscatter;
	cTex.rgb = lerp(cTex.rgb, cFog, 1.0f - fogT);   // ORO patch (aa): the fog goes on last

	return cTex;
}





// ============================================================================
// This is the default mesh rendering technique
//
technique VesselTech
{
	pass P0
	{
		vertexShader = compile vs_3_0 PBR_VS();
		pixelShader = compile ps_3_0 PBR_PS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}

	pass P1
	{
		vertexShader = compile vs_3_0 AdvancedVS();
		pixelShader = compile ps_3_0 AdvancedPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}

	pass P2
	{
		vertexShader = compile vs_3_0 FAST_VS();
		pixelShader = compile ps_3_0 FAST_PS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}

	pass P3	// XR2 HUD PASS
	{
		vertexShader = compile vs_3_0 FAST_VS();
		pixelShader = compile ps_3_0 XRHUD_PS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}
	pass P4
	{
		vertexShader = compile vs_3_0 MetalnessVS();
		pixelShader = compile ps_3_0 MetalnessPS();

		AlphaBlendEnable = true;
		BlendOp = Add;
		ZEnable = true;
		SrcBlend = SrcAlpha;
		DestBlend = InvSrcAlpha;
		ZWriteEnable = true;
	}
}
