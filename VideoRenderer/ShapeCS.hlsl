#define PI 3.14159265359


cbuffer CSConstants : register(b0)
{
	float2 Resolution;
	float Time;
	float Scale;
	float2 rSize;
	float2 rSizeInitial;
};

Texture2D<float4> BackgroundImg : register(t0);
Texture2D<float4> BlurredImg : register(t1);
RWTexture2D<float4> Output : register(u0);


float sdRoundRect(float2 p, float2 center, float2 size, float r)
{
	float2 d = abs(p - center) - (size * 0.5f - r);
	return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f) - r;
}

float sdCircle(float2 p, float2 center, float r)
{
	return length(p - center) - r;
}

[numthreads(32, 32, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
	if (tid.x >= (uint) Resolution.x || tid.y >= (uint) Resolution.y)
		return;

	float2 p = float2(tid.xy);
	float2 uv = (p + 0.5f) / Resolution;

	float4 outp = BackgroundImg.Load(uint3(tid.xy, 0));

	float2 rcenter = Resolution * 0.5f;
	float cornerRadius = 100.0f * Scale;
	
	float dRect = sdRoundRect(p, rcenter, rSize, cornerRadius);
	if (dRect <= 0.0f)
		outp = BlurredImg.Load(uint3(tid.xy, 0));

	float2 topLeft = rcenter - rSize * 0.5f + cornerRadius;

	float circleRadius = 25.0f * Scale;
	float d = sdCircle(p, topLeft, circleRadius);
	if (d <= 0.0f)
		outp = float4(1.0f, 0.3686f, 0.3412f, 1.0f);
	
	float2 center2 = topLeft;
	center2.x += 75.0f * Scale;
	d = sdCircle(p, center2, circleRadius);
	if (d <= 0.0f)
		outp = float4(1.0f, 0.7294f, 0.1804f, 1.0f);

	float2 center3 = topLeft;
	center3.x += 150.0f * Scale;
	d = sdCircle(p, center3, circleRadius);
	if (d <= 0.0f)
		outp = float4(0.1569f, 0.7882f, 0.2549f, 1.0f);

	Output[tid.xy] = outp;
}
