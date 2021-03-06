#include <string.h>
#include <alloca.h>
#include <math.h>
#include <mpi.h>

#include <fastpm/libfastpm.h>

#include <fastpm/prof.h>
#include <fastpm/logging.h>
#include <fastpm/transfer.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_roots.h>

#include "pmpfft.h"
#include "pmghosts.h"
#include "pm2lpt.h"
#include "solver-pm-internal.h"

void fastpm_model_init(FastPMModel * model, FastPMSolver * fastpm, FastPMModelType type)
{
    model->type = type;

    PMInit pminit = {
        .Nmesh = fastpm->nc / 2,
        .BoxSize = fastpm->boxsize,
        .NprocY = 0,
        .transposed = 1,
        .use_fftw = 0,
    };
    model->factor = 4;

    model->pm = malloc(sizeof(PM));
    model->fastpm = fastpm;
    model->build = NULL;
    model->evolve = NULL;
    model->destroy = NULL;
    pm_init(model->pm, &pminit, fastpm->comm);

    fastpm_painter_init(model->painter, model->pm, FASTPM_PAINTER_CIC, 0);

    switch(type) {
        case FASTPM_MODEL_NONE:
            break;
        case FASTPM_MODEL_LINEAR:
            fastpm_model_linear_init(model);
            break;
        case FASTPM_MODEL_ZA:
        case FASTPM_MODEL_2LPT:
            fastpm_model_pt_init(model);
            break;
        case FASTPM_MODEL_PM:
            fastpm_model_pm_init(model);
            break;
    }

    if (type != FASTPM_MODEL_NONE && fastpm->nc <= 256) {
        fastpm_info("The mesh resolution is too low to to calibrate the correction model. The growth rate will be biased. \n");
    }
}

void fastpm_model_destroy(FastPMModel * model)
{
    if(model->destroy) 
        model->destroy(model);
    pm_destroy(model->pm);
    free(model->pm);
}

void fastpm_model_build(FastPMModel * model, double ainit, double afinal)
{
    if(!model->build) return;
    model->build(model, ainit, afinal);
}

void fastpm_model_evolve(FastPMModel * model, double af)
{
    if(!model->evolve) return;
    model->evolve(model, af);
}

static void
scale_acc(FastPMStore * po, double correction, double fudge)
{
    ptrdiff_t i;
    correction = pow(correction, fudge);

#pragma omp parallel for
    for(i = 0; i < po->np; i ++) {
        int d;
        for(d = 0; d < 3; d ++) {
            po->acc[i][d] *= correction;
        }
    }
}


static double 
find_correction_eval(double correction, void * data)
{
    FastPMModel * model = (FastPMModel*) data;
    FastPMSolver * fastpm = model->fastpm;
    FastPMStore * po = model->ev.po;

    scale_acc(po, correction, 1.0);

    fastpm_kick_store(fastpm, po, po, model->ev.a_v1);
    fastpm_drift_store(fastpm, po, po, model->ev.a_x1);

    double Plarge = fastpm_model_measure_large_scale_power(model, po);

    fastpm_drift_store(fastpm, po, po, model->ev.a_x);
    fastpm_kick_store(fastpm, po, po, model->ev.a_v);
    scale_acc(po, 1.0 / correction, 1.0);

    double res = Plarge / model->Pexpect;
    return res - 1.0;
}

double
fastpm_model_find_correction(FastPMModel * model,
        double a_x, double a_x1, double a_v, double a_v1)
{
    FastPMStore * p = model->fastpm->p;

    if(model->type == FASTPM_MODEL_NONE) {
        return 1.0;
    }

    if(a_x == a_x1) {
        /* no correction is needed in this case*/
        /* FIXME: when step size is sufficiently small, use 1.0 too */
        return 1.0;
    }

    FastPMStore po[1];
    fastpm_store_init(po);

    /* FIXME: get rid of DX1 DX2, since we do not need a model for COLA */
    fastpm_store_alloc(po, 1.0 * p->np_upper / model->factor, p->attributes);
    fastpm_store_create_subsample(po, p, model->factor, model->fastpm->nc);

    gsl_root_fsolver * s;
    gsl_function F;

    F.function = find_correction_eval;
    F.params = (void*) model;
    model->ev.po = po;
    model->ev.a_x = a_x;
    model->ev.a_x1 = a_x1;
    model->ev.a_v = a_v;
    model->ev.a_v1 = a_v1;

    int iter = 0;
    double r_hi = 0, r_lo = 0;
    double x = 0;
    double x_lo = 0.9;
    double x_hi = 1.1;
    while((r_hi = find_correction_eval(x_hi, model)) < 0) {
        if(-r_hi < 1e-7) break;
        iter ++;
        fastpm_info("iter = %d x_hi = %g r = %g\n", iter, x_hi, r_hi);
        x_hi *= 1.2;
    }
    while((r_lo = find_correction_eval(x_lo, model)) > 0) {
        if(r_lo < 1e-7) break;
        iter ++;
        fastpm_info("iter = %d x_lo = %g r= %g\n", iter, x_lo, r_lo);
        x_lo /= 1.2;
    }

    if(fabs(r_hi) < 1e-7 && fabs(r_lo) < 1e-7) {
        fastpm_store_destroy(po);
        return 1.0;
    }

    s = gsl_root_fsolver_alloc(gsl_root_fsolver_brent);
    int status;
    gsl_root_fsolver_set(s, &F, x_lo, x_hi);

    do {
        iter++;
        status = gsl_root_fsolver_iterate (s);
        x = gsl_root_fsolver_root (s);
        x_lo = gsl_root_fsolver_x_lower (s);
        x_hi = gsl_root_fsolver_x_upper (s);
        status = gsl_root_test_interval (x_lo, x_hi,
                0, 1e-5);
        double r = find_correction_eval(x, model);
        fastpm_info("iter = %d x = %g r = %g\n", iter, x, r);
    }
    while (status == GSL_CONTINUE && iter < 10);
    gsl_root_fsolver_free(s);

    fastpm_store_destroy(po);

    return x;
}

double
fastpm_model_measure_large_scale_power(FastPMModel * model, FastPMStore * p)
{
    PM * pm = model->pm;

    fastpm_store_wrap(p, pm->BoxSize);

    FastPMFloat * canvas = pm_alloc(pm);
    FastPMFloat * delta_k = pm_alloc(pm);

    PMGhostData * pgd = pm_ghosts_create(pm, p, PACK_POS, NULL);

    /* Note that power will divide by the 0-th mode
     * thus we do not need to scale the density correctly */
    fastpm_paint_local(model->painter, canvas, p, p->np + pgd->nghosts, NULL, 0);

    pm_r2c(pm, canvas, delta_k);

    pm_ghosts_free(pgd);

    fastpm_apply_normalize_transfer(pm, delta_k, delta_k);

    FastPMPowerSpectrum ps;
    /* calculate the power spectrum */
    fastpm_powerspectrum_init_from_delta(&ps, pm, delta_k, delta_k);

    double Plin = fastpm_powerspectrum_large_scale(&ps, model->fastpm->K_LINEAR);

    pm_free(pm, delta_k);
    pm_free(pm, canvas);
    return Plin;
}
