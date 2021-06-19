#ifndef WEIGHTED_RESERVIOR_H
#define WEIGHTED_RESERVIOR_H

struct Reservior
{
    float m;
    float w_sum;

    bool
    update(float w, float rnd)
    {
        w_sum = w_sum + w;
        m += 1.0f;
        return (rnd < (w / w_sum));
    }
};

Reservior
Reservior_create()
{
    Reservior reservior;
    reservior.w_sum = 0.0f;
    reservior.m     = 0.0f;
    return reservior;
}

#endif