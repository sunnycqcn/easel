/* esl_weibull.h
 * Weibull distributions.
 * 
 * SRE, Tue Aug  9 10:48:35 2005 [St. Louis]
 * SVN $Id$
 */
#ifndef ESL_WEI_INCLUDED
#define ESL_WEI_INCLUDED

#ifdef eslAUGMENT_RANDOM
#include <esl_random.h>
#endif

extern double esl_wei_pdf    (double x, double mu, double lambda, double tau);
extern double esl_wei_logpdf (double x, double mu, double lambda, double tau);
extern double esl_wei_cdf    (double x, double mu, double lambda, double tau);
extern double esl_wei_logcdf (double x, double mu, double lambda, double tau);
extern double esl_wei_surv   (double x, double mu, double lambda, double tau);
extern double esl_wei_logsurv(double x, double mu, double lambda, double tau);

extern double esl_wei_generic_cdf(double x, void *params);

extern int esl_wei_Plot(FILE *fp, double mu, double lambda, double tau,
			double (*func)(double x, double mu, double lambda, double tau), 
			double xmin, double xmax, double xstep);


#ifdef eslAUGMENT_RANDOM
extern double esl_wei_Sample(ESL_RANDOMNESS *r, double mu, double lambda, double tau);
#endif

#ifdef eslAUGMENT_MINIMIZER
extern int esl_wei_FitComplete(double *x, int n, double mu,
			       double *ret_lambda, double *ret_tau);
#endif /*eslAUGMENT_MINIMIZER*/


#endif /*ESL_WEI_INCLUDED*/
/*****************************************************************
 * @LICENSE@
 *****************************************************************/
