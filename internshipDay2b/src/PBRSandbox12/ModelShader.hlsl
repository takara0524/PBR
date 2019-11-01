#define PI 3.14159265359

struct VSInput
{
	float4 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float3 position_ws : POSITION_WS;
	float3 normal_ws : NORMAL;
	float2 uv : TEXCOORD;
};

cbuffer cbVS0 : register(b0)
{
	float4x4 mWVP;
	float4x4 mWorld;
};

cbuffer cbPS0 : register(b0)
{
	float4 baseColor;
	float metallic;
	float roughness;
	float reflectance;
	float pad1;
	float3 view;
	float pad2;
	float3 ambient;
};

cbuffer cbPS1 : register(b1)
{
	float3 direction;
	float intensity;
	float3 lightColor;
};

Texture2D g_texture : register(t0);
Texture2D g_metalroughness : register(t1);

TextureCube PBR_RadianceTexture : register(t10);
TextureCube PBR_IrradianceTexture : register(t11);
SamplerState g_sampler : register(s0);

PSInput VSMain(VSInput v)
{
	PSInput result;

	result.position = mul( v.position, mWVP);
	result.position_ws = mul(v.position, mWorld).xyz;
	result.normal_ws = normalize(mul(v.normal, (float3x3)mWorld));
	result.uv = v.uv;

	return result;
}

float Fd_Lambert() {
	return 1.0 / PI;
}

float F_Schlick(float VoH, float f0, float f90) {
	return f0 + (f90 - f0) * pow(1.0 - VoH, 5.0);
}

float3 F_Schlick(float VoH, float3 f0) {
	return f0 + (float3(1, 1, 1) - f0) * pow(1.0 - VoH, 5.0f);
}

float Fd_Burley(float NoV, float NoL, float LoH, float linearRoughness) {
	float f90 = 0.5 + 2.0 * linearRoughness * LoH * LoH;
	float lightScatter = F_Schlick(NoL, 1.0, f90);
	float viewScatter = F_Schlick(NoV, 1.0, f90);
	return lightScatter * viewScatter * (1.0 / PI);
}

float D_GGX(float NoH, float a) {
	float a2 = a * a;
	float f = (NoH * a2 - NoH) * NoH + 1.0;
	return a2 / (PI * f * f);
}


float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
	float a2 = a * a;
	float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
	float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
	return 0.5 / (GGXV + GGXL);
}

// Diffuse irradiance
float3 Diffuse_IBL(in float3 N)
{
	return PBR_IrradianceTexture.Sample(g_sampler, N);
}

// Approximate specular image based lighting by sampling radiance map at lower mips 
// according to roughness, then modulating by Fresnel term. 
float3 Specular_IBL(in float3 N, in float3 V, in float lodBias)
{
	//	float mip = lodBias * PBR_NumRadianceMipLevels;
	float mip = lodBias * 9;
	float3 dir = reflect(-V, N);
	return PBR_RadianceTexture.SampleLevel(g_sampler, dir, mip);
}

float3 BRDF(float3 diffuseColor, float3 f0, float3 v, float3 n, float linearRoughness)
{
	float3 l = normalize(-direction);
	float3 h = normalize(l + v);

	float NoV = abs(dot(n, v)) + 1e-6;
	float NoL = clamp(dot(n, l), 0.0, 1.0);
	float NoH = clamp(dot(n, h), 0.0, 1.0);
	float LoH = clamp(dot(l, h), 0.0, 1.0);

	float a = linearRoughness * linearRoughness;

	float D = D_GGX(NoH, a);
	float3  F = F_Schlick(LoH, f0);
	float V = V_SmithGGXCorrelated(NoV, NoL, a);

	float3 Fr = (D*V) * F;
//	float3 Fd = diffuseColor.xyz * Fd_Lambert();
	float3 Fd = diffuseColor.xyz * Fd_Burley(NoV,NoL,LoH,a);


	Fr *= Specular_IBL(n, v, linearRoughness);
	Fd *= max(Diffuse_IBL(n), 0.0f) * Fd_Lambert();

	float3 col = Fd + Fr;
		
	float3 illuminance = intensity * NoL * lightColor;

	return col * illuminance;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float4 color = float4(0,0,0,1);

	float4 textureBaseColor = g_texture.Sample(g_sampler, input.uv);
	float4 metallicRoughness = g_metalroughness.Sample(g_sampler, input.uv);

	float textureMetallic = metallicRoughness.b;
	float textureRoughness = metallicRoughness.g;

	float3 v = normalize( view- input.position_ws.xyz);

	float3 diffuseColor = (1.0 - textureMetallic) * textureBaseColor.xyz;
	float3 f0 = 0.16 * reflectance * reflectance * (1.0 - textureMetallic) + textureBaseColor.xyz * textureMetallic;

	color.xyz =  BRDF(diffuseColor, f0, v, input.normal_ws, textureRoughness)* intensity + (ambient);
	
	return color;
}
