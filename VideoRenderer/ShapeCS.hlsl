cbuffer CSConstants : register(b0)
{
	float2 Resolution; // (width, height)
	float Time; // seconds
	float Progress; // 0..1
};

RWTexture2D<float4> Output : register(u0);


[numthreads(32, 32, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
	if (tid.x >= (uint) Resolution.x || tid.y >= (uint) Resolution.y)
		return;

	float2 p = float2(tid.xy);
	float2 uv = (p + 0.5) / Resolution;
	
	float3 bgA = float3(0.9294, 0.8314, 0.8314);
	float3 bgB = float3(0.6824, 0.5294, 0.3373);
	// float bgA = float3(0, 0, 0);
	// float bgB = float3(1, 1, 1);
	float3 col = lerp(bgB, bgA, (1 - uv.x + 0.1) * (uv.y - 0.1));
	
	float4 outp = float4(col, 1);
	outp.rgb *= outp.a;
	
	Output[tid.xy] = outp;
}
