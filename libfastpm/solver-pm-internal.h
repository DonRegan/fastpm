struct FastPMModel {
    FastPMModelType type;
    FastPMSolver * fastpm;
    FastPMPainter painter[1];
    PM * pm;
    double Pexpect;
    int factor;
    struct {
        FastPMStore * po;
        double a_x;
        double a_x1;
        double a_v;
        double a_v1;
    } ev;
    void * priv;
    void (*build)(FastPMModel * model, double ainit, double afinal);
    void (*evolve)(FastPMModel * model, double af);
    void (*destroy)(FastPMModel * model);
};

double
fastpm_model_find_correction(FastPMModel * model,
    double a_x, double a_x1, double a_v, double a_v1);

void fastpm_model_init(FastPMModel * model, FastPMSolver * fastpm, FastPMModelType type);
void fastpm_model_pt_init(FastPMModel * model);
void fastpm_model_pm_init(FastPMModel * model);
void fastpm_model_linear_init(FastPMModel * model);

double fastpm_model_measure_large_scale_power(FastPMModel * model, FastPMStore * p);
void fastpm_model_destroy(FastPMModel * model);
void fastpm_model_build(FastPMModel * model, double ainit, double afinal);
void fastpm_model_evolve(FastPMModel * model, double af);

void fastpm_calculate_forces(FastPMSolver * fastpm, FastPMFloat * delta_k);

void 
fastpm_kick_store(FastPMSolver * fastpm, 
              FastPMStore * pi, FastPMStore * po,
              double af);

void 
fastpm_drift_store(FastPMSolver * fastpm,
               FastPMStore * pi, FastPMStore * po,
               double af);

void 
fastpm_set_snapshot(FastPMSolver * fastpm,
                FastPMStore * p, FastPMStore * po,
                double aout);
