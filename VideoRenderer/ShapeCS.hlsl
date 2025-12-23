// ShapeCS.hlsl
// Writes premultiplied RGBA into an RGBA16F UAV.

cbuffer CSConstants : register(b0)
{
	float2 Resolution; // (width, height)
	float Time; // seconds
	float Progress; // 0..1
};

RWTexture2D<float4> Output : register(u0);

static float sdCircle(float2 p, float2 c, float r)
{
	return length(p - c) - r;
}

static float smoothAA(float dist, float aa)
{
    // dist < 0 => inside; dist > 0 => outside
	return saturate(0.5 - dist / aa);
}

[numthreads(32, 32, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
	if (tid.x >= (uint) Resolution.x || tid.y >= (uint) Resolution.y)
		return;

	float2 p = float2(tid.xy);
	float2 uv = (p + 0.5) / Resolution;

    // Background gradient (linear-light)
	float3 bgA = float3(0.05, 0.06, 0.10);
	float3 bgB = float3(0.30, 0.45, 0.90);
	float3 col = lerp(bgA, bgB, uv.x);

    // Add a second subtle vertical gradient to stress-test banding
	col *= lerp(0.85, 1.05, uv.y);

    // Animated circle
	float2 center = float2(Resolution.x * (0.5 + 0.25 * sin(Time * 1.3)),
                           Resolution.y * (0.5 + 0.20 * cos(Time * 1.1)));
	float radius = min(Resolution.x, Resolution.y) * (0.12 + 0.02 * sin(Time * 2.0));

	float d = sdCircle(p, center, radius);
	float alpha = smoothAA(d, 1.25); // ~1 px AA width

	float3 circleColor = float3(0.95, 0.35 + 0.25 * sin(Time * 0.7), 0.20);
	col = lerp(col, circleColor, alpha);

    // Premultiplied alpha output (opaque frame here, alpha=1)
	float4 outp = float4(col, 1.0);
	outp.rgb *= outp.a;

	Output[tid.xy] = outp;
}
