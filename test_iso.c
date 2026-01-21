#include <stdio.h>
#include <math.h>

float ae_iso9613_absorption(float f_hz, float T_celsius, float humidity_pct, float pressure_kpa) {
    float T0 = 293.15f;
    float T01 = 273.16f;
    float p0 = 101.325f;
    float T = T_celsius + 273.15f;
    float pa = pressure_kpa;
    float f = f_hz;

    float C = -6.8346f * powf(T01 / T, 1.261f) + 4.6151f;
    float psat = pa * powf(10.0f, C);
    float h = humidity_pct * psat / pa;

    float frO = (pa / p0) * (24.0f + 4.04e4f * h * (0.02f + h) / (0.391f + h));
    float frN = (pa / p0) * powf(T / T0, -0.5f) * (9.0f + 280.0f * h * expf(-4.170f * (powf(T / T0, -1.0f / 3.0f) - 1.0f)));

    float freq_sq = f * f;
    float term1 = 1.84e-11f * (p0 / pa) * sqrtf(T / T0);
    float term2 = powf(T / T0, -2.5f) * (0.01275f * expf(-2239.1f / T) / (frO + freq_sq / frO) + 0.1068f * expf(-3352.0f / T) / (frN + freq_sq / frN));

    float alpha = 8.686f * freq_sq * (term1 + term2);
    return alpha;
}

int main() {
    printf("ISO 9613-1 Test Values:\n");
    printf("  250Hz: %.6f dB/m\n", ae_iso9613_absorption(250.0f, 20.0f, 50.0f, 101.325f));
    printf("  1kHz:  %.6f dB/m\n", ae_iso9613_absorption(1000.0f, 20.0f, 50.0f, 101.325f));
    printf("  8kHz:  %.6f dB/m\n", ae_iso9613_absorption(8000.0f, 20.0f, 50.0f, 101.325f));
    printf("Expected 8kHz: 0.01-0.1 dB/m (reference: ~0.02-0.04 dB/m)\n");
    return 0;
}
