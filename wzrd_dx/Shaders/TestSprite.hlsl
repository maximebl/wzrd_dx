#include "LightingUtil.hlsl"

Texture2D gDiffuseMap : register(t0);

cbuffer cbMaterial : register(b2)
{
	float4   gDiffuseAlbedo;
    float3   gFresnelR0;
    float    gRoughness;
	float4x4 gMatTransform;
};

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per material.
cbuffer cbPass : register(b1)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
	float4 gAmbientLight;

	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 cbPerObjectPad2;

	Light gLights[MaxLights];
}

struct vertexIn  
{
	float3 PosW : POSITION;
	float2 SizeW : SIZE;
};

struct vertexOut 
{
	float3 CenterW : POSITION;
	float2 SizeW : SIZE;
};

struct GeoOut 
{
	float4 PosH	: SV_POSITION;
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC : TEXCOORD;
	uint PrimID : SV_PrimitiveID;
};

vertexOut VS(vertexIn vIn) 
{
	vertexOut vOut;

	vOut.CenterW = vIn.PosW;
	vOut.SizeW = vIn.SizeW;

	return vOut;
}

[maxvertexcount(4)]
void GS(point vertexOut gin[1], uint primID: SV_PrimitiveID, inout TriangleStream<GeoOut> triStream) 
{

	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 look = gEyePosW - gin[0].CenterW;
	look.y = 0.0f;
	look = normalize(look);
	float3 right = cross(up, look);

	float halfWidth = 0.5f * gin[0].SizeW.x;
	float halfHeight = 0.5f * gin[0].SizeW.y;

	float4 v[4];
	v[0] = float4(gin[0].CenterW + halfWidth * right - halfHeight * up, 1.0f);
	v[1] = float4(gin[0].CenterW + halfWidth * right + halfHeight * up, 1.0f);
	v[2] = float4(gin[0].CenterW - halfWidth * right - halfHeight * up, 1.0f);
	v[3] = float4(gin[0].CenterW - halfWidth * right + halfHeight * up, 1.0f);

	float2 texC[4] =
	{
		float2(0.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 1.0f),
		float2(1.0f, 0.0f)
	};

	GeoOut gout;
	[unroll]
	for (int i = 0; i < 4; ++i)
	{
		gout.PosH = mul(v[i], gViewProj);
		gout.PosW = v[i].xyz;
		gout.NormalW = look;
		gout.TexC = texC[i];
		gout.PrimID = primID;

		triStream.Append(gout);
	}
}

float4 PS(GeoOut pin) : SV_Target 
{
	float3 uvw = float3(pin.TexC, pin.PrimID % 3);
	float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, uvw) * gDiffuseAlbedo;

#ifdef ALPHA_TEST
	//clip(diffuseAlbedo = gDiffuseMap.a - 0.1f);
#endif

	pin.NormalW = normalize(pin.NormalW);

	float3 toEyeW = gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye;

	float4 ambient = gAmbientLight * diffuseAlbedo;

	const float shininess = 1.0f - gRoughness;
	Material mat = { diffuseAlbedo, gFresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);
	float4 litColor = ambient + directLight;
	litColor.a = diffuseAlbedo.a;

	return litColor;
}