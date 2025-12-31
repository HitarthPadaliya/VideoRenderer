#include "Easing.h"

#define PI 3.1415927


float EaseInSine(float x)
{
	return 1 - cos((x * PI) / 2);
}

float EaseOutSine(float x)
{
	return sin((x * PI) / 2);
}

float EaseInOutSine(float x)
{
	return -(cos(PI * x) - 1) / 2;
}


float EaseInQuad(float x)
{
	return x * x;
}

float EaseOutQuad(float x)
{
	return 1 - (1 - x) * (1 - x);
}

float EaseInOutQuad(float x)
{
	return x < 0.5f ? 2 * x * x : 1 - pow(-2 * x + 2, 2) / 2;
}


float EaseInCubic(float x)
{
	return x * x * x;
}

float EaseOutCubic(float x)
{
	return 1 - pow(1 - x, 3);
}

float EaseInOutCubic(float x)
{
	return x < 0.5f ? 4 * x * x * x : 1 - pow(-2 * x + 2, 3) / 2;
}


float EaseInQuart(float x)
{
	return x * x * x * x;
}

float EaseOutQuart(float x)
{
	return 1 - pow(1 - x, 4);
}

float EaseInOutQuart(float x)
{
	return x < 0.5f ? 8 * x * x * x * x : 1 - pow(-2 * x + 2, 4) / 2;
}


float EaseInQuint(float x)
{
	return x * x * x * x * x;
}

float EaseOutQuint(float x)
{
	return 1 - pow(1 - x, 5);
}

float EaseInOutQuint(float x)
{
	return x < 0.5f ? 16 * x * x * x * x * x : 1 - pow(-2 * x + 2, 5) / 2;
}


float EaseInExpo(float x)
{
	return x == 0 ? 0 : pow(2, 10 * x - 10);
}

float EaseOutExpo(float x)
{
	return x == 1 ? 1 : 1 - pow(2, -10 * x);
}

float EaseInOutExpo(float x)
{
	return x == 0
		? 0
		: x == 1
			? 1
			: x < 0.5
				? pow(2, 20 * x - 10) / 2
				: (2 - pow(2, -20 * x + 10)) / 2;
}


float EaseInCirc(float x)
{
	return 1 - sqrt(1 - pow(x, 2));
}

float EaseOutCirc(float x)
{
	return sqrt(1 - pow(x - 1, 2));
}

float EaseInOutCirc(float x)
{
	return x < 0.5f
		? (1 - sqrt(1 - pow(2 * x, 2))) / 2
		: (sqrt(1 - pow(-2 * x + 2, 2)) + 1) / 2;
}


float EaseInBack(float x)
{
	const float c1 = 1.70158f;
	const float c3 = c1 + 1;

	return c3 * x * x * x - c1 * x * x;
}

float EaseOutBack(float x)
{
	const float c1 = 1.70158f;
	const float c3 = c1 + 1;
	
	return 1 + c3 * pow(x - 1, 3) + c1 * pow(x - 1, 2);
}

float EaseInOutBack(float x)
{
	const float c1 = 1.70158f;
	const float c2 = c1 * 1.525;
	
	return x < 0.5f
		? (pow(2 * x, 2) * ((c2 + 1) * 2 * x - c2)) / 2
		: (pow(2 * x - 2, 2) * ((c2 + 1) * (x * 2 - 2) + c2) + 2) / 2;
}


float EaseInElastic(float x)
{
	const float c4 = (2 * PI) / 3;

	return x == 0
		? 0
		: x == 1
			? 1
			: -pow(2, 10 * x - 10) * sin((x * 10 - 10.75f) * c4);
}

float EaseOutElastic(float x)
{
	const float c4 = (2 * PI) / 3;
	
	return x == 0
		? 0
		: x == 1
			? 1
			: pow(2, -10 * x) * sin((x * 10 - 0.75f) * c4) + 1;
}

float EaseInOutElastic(float x)
{
	const float c5 = (2 * PI) / 4.5;
	
	return x == 0
		? 0
		: x == 1
			? 1
			: x < 0.5f
				? -(pow(2, 20 * x - 10) * sin((20 * x - 11.125f) * c5)) / 2
				: (pow(2, -20 * x + 10) * sin((20 * x - 11.125f) * c5)) / 2 + 1;
}


float EaseInBounce(float x)
{
	return 1 - EaseOutBounce(1 - x);
}

float EaseOutBounce(float x)
{
	const float n1 = 7.5625f;
	const float d1 = 2.75f;
	
	if (x < 1 / d1)
		return n1 * x * x;
	if (x < 2 / d1)
		return n1 * (x -= 1.5f / d1) * x + 0.75f;
	if (x < 2.5 / d1)
		return n1 * (x -= 2.25f / d1) * x + 0.9375f;

	return n1 * (x -= 2.625f / d1) * x + 0.984375f;
}

float EaseInOutBounce(float x)
{
	return x < 0.5f
		? (1 - EaseOutBounce(1 - 2 * x)) / 2
		: (1 + EaseOutBounce(2 * x - 1)) / 2;
}


float AEDoubleBackLerp(float x)
{
	float k0 = 0.0, v0 = 0.0;
	float k1 = 0.3333, v1 = 1.04;
	float k2 = 0.4167, v2 = 0.98;
	float k3 = 0.5, v3 = 1.0;

	if (x <= k0)
		return v0;
	if (x >= k3)
		return v3;

	if (x < k1)
	{
		float s = (x - k0) / (k1 - k0);
		return std::lerp(v0, v1, EaseInOutSine(s));
	}
	else if (x < k2)
	{
		float s = (x - k1) / (k2 - k1);
		return std::lerp(v1, v2, EaseInOutSine(s));
	}
	else
	{
		float s = (x - k2) / (k3 - k2);
		return std::lerp(v2, v3, EaseInOutSine(s));
	}
}
