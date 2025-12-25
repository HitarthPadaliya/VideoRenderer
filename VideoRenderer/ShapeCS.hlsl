#define PI 3.14159265359

float easeOutElastic(float x)
{
	float c4 = (2 * PI) / 3;

	return (x == 0)
		? 0
		: (x == 1
			? 1
			: (pow(2, -10 * x) * sin((x * 10 - 0.75) * c4) + 1));
}

float easeOutBounce(float x)
{
	float n1 = 7.5625;
	float d1 = 2.75;

	if (x < 1 / d1)
		return n1 * x * x;
	else if (x < 2 / d1)
		return n1 * (x -= 1.5 / d1) * x + 0.75;
	else if (x < 2.5 / d1)
		return n1 * (x -= 2.25 / d1) * x + 0.9375;
	
	return n1 * (x -= 2.625 / d1) * x + 0.984375;
}

float easeOutBack(float x)
{
	float c1 = 1.70158;
	float c3 = c1 + 1;

	return 1 + c3 * pow(x - 1, 3) + c1 * pow(x - 1, 2);
}

float easeInOutQuad(float x)
{
	return x < 0.5 ? 2 * x * x : 1 - pow(-2 * x + 2, 2) / 2;
}

float easeInOutQuart(float x)
{
	return x < 0.5 ? 8 * x * x * x * x : 1 - pow(-2 * x + 2, 4) / 2;
}

float easeInOutSine(float x)
{
	return -(cos(PI * x) - 1) / 2;
}








float smoothstep01(float s)
{
	s = clamp(s, 0, 1);
	return s * s * (3 - 2 * s);
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
		// return lerp(v0, v1, smoothstep01(s));
		return lerp(v0, v1, easeInOutSine(s));
	}
	else if (t < k2)
	{
		float s = (t - k1) / (k2 - k1);
		// return lerp(v1, v2, smoothstep01(s));
		return lerp(v1, v2, easeInOutSine(s));
	}
	else
	{
		float s = (t - k2) / (k3 - k2);
		// return lerp(v2, v3, smoothstep01(s));
		return lerp(v2, v3, easeInOutSine(s));
	}
}

















cbuffer CSConstants : register(b0)
{
	float2 Resolution;
	float Time;			// seconds
	float Progress;		// 0..1
};

struct RoundedRect
{
	float2 center;
	float2 size;
	float radius;
	float feather;
	float4 color;
};

Texture2D<float4> BackgroundImg : register(t0);

RWTexture2D<float4> Output : register(u0);


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
		float4 blur = float4(0, 0, 0, 0);
		float blurRad = 100;
		
		float sum = 0;
		float sigma = blurRad / 3;
		float sigma2 = 2 * sigma * sigma;
		
		float wt = 0;
		
		for (int y = -blurRad; y <= blurRad; ++y)
		{
			for (int x = -blurRad; x <= blurRad; ++x)
			{
				float distance = x * x + y * y;
				wt = exp(-distance / sigma2) / (PI * sigma2);
				
				blur += BackgroundImg.Load(int3(pix + float2(x, y), 0)) * wt;
				sum += wt;
			}
		}
		
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
	
	//float3 bgA = float3(0.9294, 0.8314, 0.8314);
	//float3 bgB = float3(0.6824, 0.5294, 0.3373);
	//float3 col = lerp(bgB, bgA, (1 - uv.x + 0.1) * (uv.y - 0.1));
	//float4 outp = float4(col, 1);
	
	float4 outp = BackgroundImg.Load(uint3(tid.xy, 0));
	
	float2 size1 = float2(1920, 1080);
	float2 size2 = float2(2800, 1600);
	
	float2 rcenter = Resolution / 2;
	
	float2 rsize = Time < 1 ? lerp(size1, size2, AEDoubleBackLerp(Time)) : size2;
	// float2 rsize = size1;
	
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
