#ifndef COLOR_CONVERSION_H
#define COLOR_CONVERSION_H
half3
hue(const half H)
{
    half R = abs(H * 6 - 3) - 1;
    half G = 2 - abs(H * 6 - 2);
    half B = 2 - abs(H * 6 - 4);
    return saturate(half3(R, G, B));
}

half3
rgb_from_hsv(const half3 hsv)
{
    return half3(((hue(hsv.x) - 1) * hsv.y + 1) * hsv.z);
}

half3
color_from_uint(const uint u)
{
    // hash into a random uint
    uint v = u;
    v ^= v >> 17;
    v *= 0xed5ad4bbU;
    v ^= v >> 11;
    v *= 0xac4c1b51U;
    v ^= v >> 15;
    v *= 0x31848babU;
    v ^= v >> 14;
    // convert into hue
    half h = half(v % 256) / 256.0h;
    // compute color from hsv
    half3 p = rgb_from_hsv(half3(h, 1.0h, 1.0h));
    return p * 0.7h + 0.3h;
}

#endif
