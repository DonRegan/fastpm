FASTPM_BEGIN_DECLS

typedef struct {
    size_t size;
    double *k;
    double *p;
    double *edges;

    PM * pm;
    double k0;
    double Volume;
    double *Nmodes;
} FastPMPowerSpectrum;

void
fastpm_powerspectrum_init(FastPMPowerSpectrum * ps, size_t size);

void
fastpm_powerspectrum_init_from(FastPMPowerSpectrum * ps, const FastPMPowerSpectrum * other);

void
fastpm_powerspectrum_init_from_delta(FastPMPowerSpectrum * ps, PM * pm, const FastPMFloat * delta1_k, const FastPMFloat * delta2_k);

void
fastpm_transferfunction_init(FastPMPowerSpectrum * ps, PM * pm, FastPMFloat * src_k, FastPMFloat * dest_k);

int
fastpm_powerspectrum_init_from_string(FastPMPowerSpectrum * ps, const char * string);

void
fastpm_powerspectrum_destroy(FastPMPowerSpectrum * ps);

void
fastpm_powerspectrum_write(FastPMPowerSpectrum * ps, char * filename, double N);

double
fastpm_powerspectrum_large_scale(FastPMPowerSpectrum * ps, int Nmax);

double
fastpm_powerspectrum_eval(FastPMPowerSpectrum * ps, double k);

double
fastpm_powerspectrum_eval2(double k, FastPMPowerSpectrum * ps);

double
fastpm_powerspectrum_get(FastPMPowerSpectrum * ps, double k);

double
fastpm_powerspectrum_get2(double k, FastPMPowerSpectrum * ps);

double
fastpm_powerspectrum_sigma(FastPMPowerSpectrum * ps, double R);

void
fastpm_powerspectrum_scale(FastPMPowerSpectrum * ps, double factor);

void
fastpm_powerspectrum_rebin(FastPMPowerSpectrum * ps, size_t newsize);

FASTPM_END_DECLS
