/*!
 * \file polynomials.cpp
 * \brief Polynomial definitions for all Flux Reconstruction operations
 *
 * \author - Jacob Crabill
 *           Aerospace Computing Laboratory (ACL)
 *           Aero/Astro Department. Stanford University
 *
 * \version 1.0.0
 *
 * Flux Reconstruction in C++ (Flurry++) Code
 * Copyright (C) 2015 Jacob Crabill
 *
 * Flurry++ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Flurry++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Flurry++; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA..
 *
 */

#include "polynomials.hpp"

#include <cmath>

#include "funcs.hpp"

using namespace std;


double Lagrange(vector<double> &x_lag, double y, uint mode)
{
  double lag = 1.0;

  for (uint i = 0; i < x_lag.size(); i++) {
    if (i != mode) {
      lag = lag*((y-x_lag[i])/(x_lag[mode]-x_lag[i]));
    }
  }

  return lag;
}


double dLagrange(vector<double> &x_lag, double y, uint mode)
{
  uint i, j;
  double dLag, dLag_num, dLag_den;

  dLag = 0.0;

  for (i=0; i<x_lag.size(); i++) {
    if (i!=mode) {
      dLag_num = 1.0;
      dLag_den = 1.0;

      for (j=0; j<x_lag.size(); j++) {
        if (j!=mode && j!=i) {
          dLag_num = dLag_num*(y-x_lag[j]);
        }

        if (j!=mode) {
          dLag_den = dLag_den*(x_lag[mode]-x_lag[j]);
        }
      }

      dLag = dLag+(dLag_num/dLag_den);
    }
  }

  return dLag;
}

double ddLagrange(vector<double> &x_lag, double y, uint mode)
{
  uint i, j, k;
  double ddLag, ddLag_num, ddLag_den;

  ddLag = 0.0;

  for (i=0; i<x_lag.size(); i++) {
    if (i!=mode) {

      for (j=0; j<x_lag.size(); j++) {
        if (j!=mode) {
          ddLag_num = 1.0;
          ddLag_den = 1.0;

          for (k=0; k<x_lag.size(); k++) {
            if(k!=mode) {
              if(k!=i && k!=j) {
                ddLag_num *= (y-x_lag[k]);
              }

              ddLag_den *= (x_lag[mode]-x_lag[k]);
            }
          }

          if(j!=i) {
            ddLag = ddLag + (ddLag_num/ddLag_den);
          }
        }
      }

    }
  }

  return ddLag;
}

double Legendre(double in_r, int in_mode)
{
  if (in_mode < 0)
    return 0.;
  else if (in_mode==0)
    return 1.0;
  else if (in_mode==1)
    return in_r;
  else
    return ((2*in_mode-1)*in_r*Legendre(in_r,in_mode-1)-(in_mode-1)*Legendre(in_r,in_mode-2)) / in_mode;
}

double dLegendre(double in_r, int in_mode)
{
  double dLeg = 0.;

  if (in_mode == 0) {
    dLeg = 0;
  } else {
    if (in_r > -1.0 && in_r < 1.0) {
      dLeg = in_mode*((in_r*Legendre(in_r,in_mode)) - Legendre(in_r,in_mode-1)) / (in_r*in_r-1.0);
    } else {
      if (in_r == -1.0) {
        dLeg = pow(-1.0,in_mode-1.0)*0.5*in_mode*(in_mode+1.0);
      }
      if (in_r == 1.0) {
        dLeg = 0.5*in_mode*(in_mode + 1.0);
      }
    }
  }

  return dLeg;
}

double Legendre2D_hierarchical(int in_mode, vector<double> in_loc, int in_basis_order)
{
  double leg_basis = 0;
  int n_dof = (in_basis_order+1)*(in_basis_order+1);

  if ( !(in_mode<n_dof) )
    FatalError("Invalid mode when evaluating Legendre basis.");

  int mode = 0;
  for (int k=0; k<in_basis_order*in_basis_order+1; k++) {
    for (int j=0; j<k+1; j++) {
      int i = k-j;
      if(i<=in_basis_order && j<=in_basis_order){

        if(mode == in_mode) // found the correct mode
          leg_basis = Legendre(in_loc[0],i)*Legendre(in_loc[1],j);

        mode++;
      }
    }
  }

  return leg_basis;
}

double exponential_filter(int in_mode, int inBasisOrder, double exponent)
{
  double sigma = 0;
  double eta;

  int n_dof=(inBasisOrder+1)*(inBasisOrder+1);

  if( !(in_mode<n_dof) )
    FatalError("Invalid mode when evaluating exponential filter.");

  int mode = 0;
  for (int k=0; k<inBasisOrder*inBasisOrder+1; k++) {
    for (int j=0; j<k+1; j++) {
      int i = k-j;
      if(i<=inBasisOrder && j<=inBasisOrder) {

        if (mode == in_mode) {
          // found the correct mode
          eta = (double)(i+j)/n_dof;
          sigma = exp(-1*pow(eta,exponent));
          //cout<<"sigma values are "<<sigma<<endl;
        }
        mode++;
      }
    }
  }

  return sigma;
}

// helper method to evaluate a normalized jacobi polynomial
double eval_jacobi(double in_r, int in_alpha, int in_beta, int in_mode)
{
  double jacobi = 0.;

  if(in_mode==0) {
    double dtemp_0, dtemp_1, dtemp_2;

    dtemp_0=pow(2.0,(-in_alpha-in_beta-1));
    dtemp_1=eval_gamma(in_alpha+in_beta+2);
    dtemp_2=eval_gamma(in_alpha+1)*eval_gamma(in_beta+1);

    jacobi=sqrt(dtemp_0*(dtemp_1/dtemp_2));
  }
  else if (in_mode==1) {
    double dtemp_0, dtemp_1, dtemp_2, dtemp_3, dtemp_4, dtemp_5;

    dtemp_0=pow(2.0,(-in_alpha-in_beta-1));
    dtemp_1=eval_gamma(in_alpha+in_beta+2);
    dtemp_2=eval_gamma(in_alpha+1)*eval_gamma(in_beta+1);
    dtemp_3=in_alpha+in_beta+3;
    dtemp_4=(in_alpha+1)*(in_beta+1);
    dtemp_5=(in_r*(in_alpha+in_beta+2)+(in_alpha-in_beta));

    jacobi=0.5*sqrt(dtemp_0*(dtemp_1/dtemp_2))*sqrt(dtemp_3/dtemp_4)*dtemp_5;
  }
  else {
    double dtemp_0, dtemp_1, dtemp_3, dtemp_4, dtemp_5, dtemp_6, dtemp_7, dtemp_8, dtemp_9, dtemp_10, dtemp_11, dtemp_12, dtemp_13, dtemp_14;

    dtemp_0=in_mode*(in_mode+in_alpha+in_beta)*(in_mode+in_alpha)*(in_mode+in_beta);
    dtemp_1=((2*in_mode)+in_alpha+in_beta-1)*((2*in_mode)+in_alpha+in_beta+1);
    dtemp_3=(2*in_mode)+in_alpha+in_beta;

    dtemp_4=(in_mode-1)*((in_mode-1)+in_alpha+in_beta)*((in_mode-1)+in_alpha)*((in_mode-1)+in_beta);
    dtemp_5=((2*(in_mode-1))+in_alpha+in_beta-1)*((2*(in_mode-1))+in_alpha+in_beta+1);
    dtemp_6=(2*(in_mode-1))+in_alpha+in_beta;

    dtemp_7=-((in_alpha*in_alpha)-(in_beta*in_beta));
    dtemp_8=((2*(in_mode-1))+in_alpha+in_beta)*((2*(in_mode-1))+in_alpha+in_beta+2);

    dtemp_9=(2.0/dtemp_3)*sqrt(dtemp_0/dtemp_1);
    dtemp_10=(2.0/dtemp_6)*sqrt(dtemp_4/dtemp_5);
    dtemp_11=dtemp_7/dtemp_8;

    dtemp_12=in_r*eval_jacobi(in_r,in_alpha,in_beta,in_mode-1);
    dtemp_13=dtemp_10*eval_jacobi(in_r,in_alpha,in_beta,in_mode-2);
    dtemp_14=dtemp_11*eval_jacobi(in_r,in_alpha,in_beta,in_mode-1);

    jacobi=(1.0/dtemp_9)*(dtemp_12-dtemp_13-dtemp_14);
  }

  return jacobi;
}

double eval_grad_jacobi(double in_r, int in_alpha, int in_beta, int in_mode)
{
  double grad_jacobi = 0.;

  if (in_mode==0){
    grad_jacobi = 0.;
  } else {
    grad_jacobi=sqrt(1.0*in_mode*(in_mode+in_alpha+in_beta+1))*eval_jacobi(in_r,in_alpha+1,in_beta+1,in_mode-1);
  }

  return grad_jacobi;
}

double eval_dubiner_basis_2d(point &in_rs, int in_mode, int in_basis_order)
{
  double dubiner_basis_2d = 0.;

  int n_dof = ((in_basis_order+1)*(in_basis_order+2))/2;

  if(in_mode<n_dof) {
    int i,j,k;
    int mode;
    double jacobi_0, jacobi_1;
    point ab;

    ab = rs_to_ab(in_rs);

    mode = 0;
    for (k=0;k<in_basis_order+1;k++) {
      for (j=0;j<k+1;j++) {
        i = k-j;

        if(mode==in_mode) {
          // found the correct mode
          jacobi_0=eval_jacobi(ab.x,0,0,i);
          jacobi_1=eval_jacobi(ab.y,(2*i)+1,0,j);
          dubiner_basis_2d=sqrt(2.0)*jacobi_0*jacobi_1*pow(1.0-ab.y,i);
        }

        mode++;
      }
    }
  } else {
    FatalError("Invalid mode when evaluating Dubiner basis.");
  }

  return dubiner_basis_2d;
}

double eval_dr_dubiner_basis_2d(point &in_rs, int in_mode, int in_basis_order)
{
  double dr_dubiner_basis_2d = 0.;

  int n_dof=((in_basis_order+1)*(in_basis_order+2))/2;

  if(in_mode<n_dof) {
    int i,j,k;
    int mode;
    double jacobi_0, jacobi_1;
    point ab;

    ab = rs_to_ab(in_rs);

    mode = 0;
    for (k=0;k<in_basis_order+1;k++) {
      for (j=0;j<k+1;j++) {
        i = k-j;

        if(mode==in_mode) {
          // found the correct mode
          jacobi_0=eval_grad_jacobi(ab.x,0,0,i);
          jacobi_1=eval_jacobi(ab.y,(2*i)+1,0,j);

          if(i==0) {
            // to avoid singularity
            //dr_dubiner_basis_2d=sqrt(2.0)*jacobi_0*jacobi_1;
            dr_dubiner_basis_2d=0.;
          } else {
            dr_dubiner_basis_2d=2.0*sqrt(2.0)*jacobi_0*jacobi_1*pow(1.0-ab.y,i-1);
          }
        }

        mode++;
      }
    }
  } else {
    FatalError("Invalid mode when evaluating basis.");
  }

  return dr_dubiner_basis_2d;
}

// helper method to evaluate d/ds of scalar dubiner basis

double eval_ds_dubiner_basis_2d(point &in_rs, int in_mode, int in_basis_order)
{
  double ds_dubiner_basis_2d = 0.;

  int n_dof=((in_basis_order+1)*(in_basis_order+2))/2;

  if(in_mode<n_dof)
  {
    int i,j,k;
    int mode;
    double jacobi_0, jacobi_1, jacobi_2, jacobi_3, jacobi_4;
    point ab;

    ab = rs_to_ab(in_rs);


    mode = 0;
    for (k=0;k<in_basis_order+1;k++) {
      for (j=0;j<k+1;j++) {
        i = k-j;

        if(mode==in_mode)  {
          // found the correct mode
          jacobi_0 = eval_grad_jacobi(ab.x,0,0,i);
          jacobi_1 = eval_jacobi(ab.y,(2*i)+1,0,j);

          jacobi_2 = eval_jacobi(ab.x,0,0,i);
          jacobi_3 = eval_grad_jacobi(ab.y,(2*i)+1,0,j)*pow(1.0-ab.y,i);
          jacobi_4 = eval_jacobi(ab.y,(2*i)+1,0,j)*i*pow(1.0-ab.y,i-1);

          if (i==0) {
            // to avoid singularity
            ds_dubiner_basis_2d = sqrt(2.0)*(jacobi_2*jacobi_3);
          } else {
            ds_dubiner_basis_2d = sqrt(2.0)*((jacobi_0*jacobi_1*pow(1.0-ab.y,i-1)*(1.0+ab.x))+(jacobi_2*(jacobi_3-jacobi_4)));
          }
        }

        mode++;
      }
    }
  }
  else
  {
    FatalError("ERROR: Invalid mode when evaluating basis.");
  }

  return ds_dubiner_basis_2d;
}

double eval_gamma(int in_n)
{
  int i;
  double gamma_val;

  if(in_n==1) {
    gamma_val=1;
  } else {
    gamma_val=in_n-1;
    for(i=0; i<in_n-2; i++) {
      gamma_val=gamma_val*(in_n-2-i);
    }
  }

  return gamma_val;
}

double compute_eta(int vcjh_scheme, int order)
{
  double eta;
  // Check for P=0 compatibility
  if(order == 0 && vcjh_scheme != DG)
    FatalError("ERROR: P=0 only compatible with DG. Set VCJH scheme type to 0!");

  if(vcjh_scheme == DG) {
    eta=0.0;
  }
  else if(vcjh_scheme == SD) {
    eta=(1.0*(order))/(1.0*(order+1));
  }
  else if(vcjh_scheme == HU) {
    eta=(1.0*(order+1))/(1.0*order);
  }
  else if (vcjh_scheme == CPLUS){
    double c_1d;
    if (order==2)
      c_1d = 0.206;
    else if (order==3)
      c_1d = 3.80e-3;
    else if (order==4)
      c_1d = 4.67e-5;
    else if (order==5)
      c_1d = 4.28e-7;
    else
      FatalError("C_plus scheme not implemented for this order");

    double ap = 1./pow(2.0,order)*factorial(2*order)/ (factorial(order)*factorial(order));
    eta = c_1d*(2*order+1)/2*(factorial(order)*ap)*(factorial(order)*ap);

  } else {
    FatalError("Invalid VCJH scheme.");
  }

  return eta;
}

double VCJH_1d(double xi, int mode, int order, double eta)
{
  double val;

  if (mode == 0) // Left
    val = pow(-1,order)/2.*(Legendre(xi,order) - (eta*Legendre(xi,order-1) + Legendre(xi,order+1))/(1.+eta));
  else
    val = 0.5*(Legendre(xi,order) + (eta*Legendre(xi,order-1) + Legendre(xi,order+1)))/(1+eta);

  return val;
}

double dVCJH_1d(double in_r, int in_mode, int in_order, double in_eta)
{
  double dtemp_0 = 0.;

  if(in_mode==0) { // left correction function
    if(in_order == 0) {
      dtemp_0=0.5*pow(-1.0,in_order)*(dLegendre(in_r,in_order)-((dLegendre(in_r,in_order+1))/(1.0+in_eta)));
    } else {
      dtemp_0=0.5*pow(-1.0,in_order)*(dLegendre(in_r,in_order)-(((in_eta*dLegendre(in_r,in_order-1))+dLegendre(in_r,in_order+1))/(1.0+in_eta)));
    }
  } else if(in_mode==1) { // right correction function
    if (in_order == 0) {
      dtemp_0=0.5*(dLegendre(in_r,in_order)+((dLegendre(in_r,in_order+1))/(1.0+in_eta)));
    } else {
      dtemp_0=0.5*(dLegendre(in_r,in_order)+(((in_eta*dLegendre(in_r,in_order-1))+dLegendre(in_r,in_order+1))/(1.0+in_eta)));
    }
  }

  return dtemp_0;
}

point rs_to_ab(point &rs)
{
  point ab;

  if (rs.x == 1.0) { // to avoid singularity
    ab.x = -1.0;
  } else {
    ab.y = (2.0*((1.0+rs.x)/(1.0-rs.y))) - 1.0;
  }

  ab.y = rs.y;

  return ab;
}
