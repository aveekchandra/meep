/* Copyright (C) 2003 Massachusetts Institute of Technology
%
%  This program is free software; you can redistribute it and/or modify
%  it under the terms of the GNU General Public License as published by
%  the Free Software Foundation; either version 2, or (at your option)
%  any later version.
%
%  This program is distributed in the hope that it will be useful,
%  but WITHOUT ANY WARRANTY; without even the implied warranty of
%  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
%  GNU General Public License for more details.
%
%  You should have received a copy of the GNU General Public License
%  along with this program; if not, write to the Free Software Foundation,
%  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "dactyl.h"
#include "dactyl_internals.h"

/* Below are the monitor point routines. */

monitor_point::monitor_point() {
  next = NULL;
}

monitor_point::~monitor_point() {
  if (next) delete next;
}

void fields::output_point(FILE *o, const vec &loc, const char *name) {
  monitor_point tmp;
  get_point(&tmp, loc);
  fprintf(o, "%s\t%8lg", name, t*inva);
  for (int c=0;c<10;c++)
    if (v.has_field((component)c))
      fprintf(o, "\t%8lg\t%8lg", real(tmp.f[c]), imag(tmp.f[c]));
  fprintf(o, "\n");
}

inline complex<double> getcm(double *f[2], int i) {
  return complex<double>(f[0][i],f[1][i]);
}

void fields::get_point(monitor_point *pt, const vec &loc) {
  if (pt == NULL) {
    printf("Error:  get_point passed a null pointer!\n");
    exit(1);
  }
  pt->loc = loc;
  pt->t = t*inva*c;
  for (int c=0;c<10;c++)
    if (v.has_field((component)c)) {
      int ind[8];
      double w[8];
      v.interpolate((component)c,loc,ind,w);
      pt->f[c] = 0;
      for (int i=0;i<8&&w[i];i++)
        pt->f[c] += getcm(f[c],ind[i]);
    }
}

monitor_point *fields::get_new_point(const vec &loc, monitor_point *the_list) {
  monitor_point *p = new monitor_point();
  get_point(p, loc);
  p->next = the_list;
  return p;
}

complex<double> monitor_point::get_component(component w) {
  return f[w];
}

#include <fftw.h>

void monitor_point::fourier_transform(component w,
                                      complex<double> **a, complex<double> **f,
                                      int *numout, double fmin, double fmax,
                                      int maxbands) {
  int n = 1;
  monitor_point *p = next;
  double tmax = t, tmin = t;
  while (p) {
    n++;
    if (p->t > tmax) tmax = p->t;
    if (p->t < tmin) tmin = p->t;
    p = p->next;
  }
  p = this;
  complex<double> *d = new complex<double>[n];
  for (int i=0;i<n;i++,p=p->next) {
    d[i] = p->get_component(w);
  }
  if (fmin > 0.0) { // Get rid of any static fields!
    complex<double> mean = 0.0;
    for (int i=0;i<n;i++) mean += d[i];
    mean /= n;
    for (int i=0;i<n;i++) d[i] -= mean;
  }
  if ((fmin > 0.0 || fmax > 0.0) && maxbands > 0) {
    *a = new complex<double>[maxbands];
    *f = new complex<double>[maxbands];
    *numout = maxbands;
    delete[] d;
    for (int i = 0;i<maxbands;i++) {
      double df = (maxbands == 1) ? 0.0 : (fmax-fmin)/(maxbands-1);
      (*f)[i] = fmin + i*df;
      (*a)[i] = 0.0;
      p = this;
      while (p) {
        double inside = 2*pi*real((*f)[i])*p->t;
        (*a)[i] += p->get_component(w)*complex<double>(cos(inside),sin(inside));
        p = p->next;
      }
      (*a)[i] /= (tmax-tmin);
    }
  } else {
    *numout = n;
    *a = new complex<double>[n];
    *f = d;
    fftw_complex *in = (fftw_complex *) d, *out = (fftw_complex *) *a;
    fftw_plan p;
    p = fftw_create_plan(n, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_one(p, in, out);
    fftw_destroy_plan(p);
    for (int i=0;i<n;i++) {
      (*f)[i] = i*(1.0/(tmax-tmin));
      if (real((*f)[i]) > 0.5*n/(tmax-tmin)) (*f)[i] -= n/(tmax-tmin);
      (*a)[i] *= (tmax-tmin)/n;
    }
  }
}

void monitor_point::harminv(component w,
                            complex<double> **a, double **f_re, double **f_im,
                            int *numout, double fmin, double fmax,
                            int maxbands) {
  int n = 1;
  monitor_point *p = next;
  double tmax = t, tmin = t;
  while (p) {
    n++;
    if (p->t > tmax) tmax = p->t;
    if (p->t < tmin) tmin = p->t;
    p = p->next;
  }
  p = this;
  complex<double> *d = new complex<double>[n];
  for (int i=0;i<n;i++,p=p->next) {
    d[i] = p->get_component(w);
  }
  *a = new complex<double>[n];
  *f_re = new double[n];
  *f_im = new double[n];
  printf("using an a of %lg\n", (n-1)/(tmax-tmin)*c);
  *numout = do_harminv(d, n, 1, (n-1)/(tmax-tmin)*c, fmin, fmax, maxbands,
                       *a, *f_re, *f_im, NULL);
  delete[] d;
}
