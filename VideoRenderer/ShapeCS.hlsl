#define PI 3.14159265359


cbuffer CSConstants : register(b0)
{
	float2 Resolution;
	float Time;
	float Progress;
};

Texture2D<float4> BackgroundImg : register(t0);
RWTexture2D<float4> Output : register(u0);


float easeInOutSine(float x)
{
	return -(cos(PI * x) - 1) / 2;
}

float AEDoubleBackLerp(float t)
{
	float k0 = 0.0, v0 = 0.0;
	float k1 = 0.3333, v1 = 1.04;
	float k2 = 0.4167, v2 = 0.98;
	float k3 = 0.5, v3 = 1.0;

	if (t <= k0)
		return v0;
	if (t >= k3)
		return v3;

	if (t < k1)
	{
		float s = (t - k0) / (k1 - k0);
		return lerp(v0, v1, easeInOutSine(s));
	}
	else if (t < k2)
	{
		float s = (t - k1) / (k2 - k1);
		return lerp(v1, v2, easeInOutSine(s));
	}
	else
	{
		float s = (t - k2) / (k3 - k2);
		return lerp(v2, v3, easeInOutSine(s));
	}
}


float4 FastGaussianBlur(uint2 pix, float radius)
{
	float sigma = radius / 3.0;
	float sigma2 = 2.0 * sigma * sigma;
	
	float4 color = float4(0, 0, 0, 0);
	float sum = 0.0;
	
	for (int y = -radius; y <= radius; y += 4)
	{
		for (int x = -radius; x <= radius; x += 4)
		{
			float distance = x * x + y * y;
			float weight = exp(-distance / sigma2);
			
			int2 samplePos = int2(pix) + int2(x, y);
			samplePos = clamp(samplePos, int2(0, 0), int2(Resolution.x - 1, Resolution.y - 1));
			
			color += BackgroundImg.Load(int3(samplePos, 0)) * weight;
			sum += weight;
		}
	}
	
	return color / sum;
}


float RoundedRectMask(uint2 pix, float2 center, float2 size, float r)
{
	float2 p = float2(pix);
	
	bool inRect =
		pix.x >= (center.x - (size.x / 2)) && pix.x <= (center.x + (size.x / 2))
		&& pix.y >= (center.y - (size.y / 2)) && pix.y <= (center.y + (size.y / 2));
	
	if (!inRect)
		return 0;
	
	bool inRect1 =
		pix.x >= (center.x - (size.x / 2) + r) && pix.x <= (center.x + (size.x / 2) - r)
		&& pix.y >= (center.y - (size.y / 2)) && pix.y <= (center.y + (size.y / 2));
	
	bool inRect2 =
		pix.x >= (center.x - (size.x / 2)) && pix.x <= (center.x + (size.x / 2))
		&& pix.y >= (center.y - (size.y / 2) + r) && pix.y <= (center.y + (size.y / 2) - r);
	
	if (inRect1 || inRect2)
		return 1;
	
	float2 p1 = float2(center.x - (size.x / 2) + r, center.y - (size.y / 2) + r);
	float d = length(p - p1);
	float t = 1.0 - saturate(d - r);
	if (t > 0)
		return t;
	
	float2 p2 = float2(center.x + (size.x / 2) - r, center.y - (size.y / 2) + r);
	d = length(p - p2);
	t = 1.0 - saturate(d - r);
	if (t > 0)
		return t;
	
	float2 p3 = float2(center.x - (size.x / 2) + r, center.y + (size.y / 2) - r);
	d = length(p - p3);
	t = 1.0 - saturate(d - r);
	if (t > 0)
		return t;
	
	float2 p4 = float2(center.x + (size.x / 2) - r, center.y + (size.y / 2) - r);
	d = length(p - p4);
	t = 1.0 - saturate(d - r);
	
	return t;
}

float CircleMask(uint2 pix, float2 center, float r)
{
	float2 p = float2(pix);
	float d = length(p - center);
	float t = 1.0 - saturate(d - r);
	return t;
}

float4 AlphaBlend(float4 dst, float4 src)
{
	float a = src.a + dst.a * (1.0 - src.a);
	float3 c = (src.rgb * src.a + dst.rgb * dst.a * (1.0 - src.a)) / max(a, 1e-5);
	return float4(c, a);
}

float4 DrawRoundedRect(uint2 pix, float2 center, float2 size, float r, float4 color, float4 outp)
{
	float mask = RoundedRectMask(pix, center, size, r);
	if (mask > 0.0)
	{
		// Apply fast blur
		float4 blur = FastGaussianBlur(pix, 200.0);
		
		color.a *= mask;
		outp = AlphaBlend(blur, color);
	}
	
	return outp;
}

float4 DrawCircle(uint2 pix, float2 center, float r, float4 color, float4 outp)
{
	float mask = CircleMask(pix, center, r);
	if (mask > 0.0)
	{
		color.a *= mask;
		outp = AlphaBlend(outp, color);
	}
	
	return outp;
}

[numthreads(32, 32, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
	if (tid.x >= (uint) Resolution.x || tid.y >= (uint) Resolution.y)
		return;

	float2 p = float2(tid.xy);
	float2 uv = (p + 0.5) / Resolution;
	
	float4 outp = BackgroundImg.Load(uint3(tid.xy, 0));
	
	float2 size1 = float2(1920, 1080);
	float2 size2 = float2(2800, 1600);
	
	float2 rcenter = Resolution / 2;
	float2 rsize = Time < 1 ? lerp(size1, size2, AEDoubleBackLerp(Time)) : size2;
	
	outp = DrawRoundedRect(tid.xy, rcenter, rsize, 100, float4(0.0863, 0.0863, 0.0863, 0.75), outp);
	
	float2 topLeft = rcenter - rsize / 2;
	outp = DrawCircle(tid.xy, topLeft + 100, 25, float4(1, 0.3686, 0.3412, 1), outp);
	topLeft.x += 75;
	outp = DrawCircle(tid.xy, topLeft + 100, 25, float4(1, 0.7294, 0.1804, 1), outp);
	topLeft.x += 75;
	outp = DrawCircle(tid.xy, topLeft + 100, 25, float4(0.1569, 0.7882, 0.2549, 1), outp);
	
	outp.rgb *= outp.a;
	Output[tid.xy] = outp;
}
