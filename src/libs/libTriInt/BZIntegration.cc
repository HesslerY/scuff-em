/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of SCUFF-EM.
 *
 * SCUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * SCUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * BZIntegration.cc -- routines for computing numerical
 *                  -- Brillouin-zone integrals in scuff-EM
 *
 * homer reid       -- 3/2015
 *
 */

/*
 *
 */

#include "libhrutil.h"
#include "libTriInt.h"

#ifdef HAVE_DCUTRI
 void *CreateDCUTRIWorkspace(int numfun, int maxpts);
 int DCUTRI(void *opW, double **Vertices, TriIntFun func,
            void *parms, double epsabs, double epsrel,
            double *I, double *abserr);
#else

void *CreateDCUTRIWorkspace(int numfun, int maxpts)
{ (void) numfun;
  (void) maxpts;
  ErrExit("SCUFF-EM was not compiled with DCUTRI; configure --with-dcutri");
  return 0;
}

int DCUTRI(void *opW, double **Vertices, TriIntFun func,
           void *parms, double epsabs, double epsrel,
           double *I, double *abserr)
{ 
  (void)opW; 
  (void)Vertices;
  (void)func;
  (void)parms;
  (void)epsabs;
  (void)epsrel;
  (void)I;
  (void)abserr;

  CreateDCUTRIWorkspace(0,0);
  return 0;
}
#endif

/***************************************************************/
/***************************************************************/
/***************************************************************/
int BZIntegrand_PCubature(unsigned ndim, const double *u,
                          void *pArgs, unsigned fdim,
                          double *BZIntegrand)
{
  (void) ndim; // unused
  (void) fdim; // unused

  /*--------------------------------------------------------------*/
  /*- unpack fields from user data structure ---------------------*/
  /*--------------------------------------------------------------*/
  GetBZIArgStruct *Args  = (GetBZIArgStruct *)pArgs;
  cdouble Omega          = Args->Omega;
  BZIFunction *BZIFunc   = Args->BZIFunc;
  void *UserData         = Args->UserData;
  HMatrix *RLBasis       = Args->RLBasis;
  int LDim               = RLBasis->NR;

  /*--------------------------------------------------------------*/
  /*- convert (ux, uy) variable to kBloch ------------------------*/
  /*--------------------------------------------------------------*/
  double kBloch[2]={0.0, 0.0};
  kBloch[0] = u[0]*RLBasis->GetEntryD(0,0);
  kBloch[1] = u[0]*RLBasis->GetEntryD(0,1);
  if (LDim==2)
   { kBloch[0] += u[1]*RLBasis->GetEntryD(1,0);
     kBloch[1] += u[1]*RLBasis->GetEntryD(1,1);
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  BZIFunc(UserData, Omega, kBloch, BZIntegrand);

  Args->NumCalls++;
  return 0;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void GetBZIntegral_PCubature(GetBZIArgStruct *Args,
                             cdouble Omega, double *BZIntegral)
{ 
  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  HMatrix *RLBasis   = Args->RLBasis;
  int LDim           = RLBasis->NR;
  int FDim           = Args->FDim;
  int MaxPoints      = Args->MaxPoints;
  double *BZIError   = Args->BZIError;
  double RelTol      = Args->RelTol;
  double AbsTol      = Args->AbsTol;
  bool Reduced       = Args->Reduced;
  
  double Lower[2]={0.0, 0.0};
  double Upper[2]={1.0, 1.0};
  if (Reduced)
   Upper[0]=Upper[1]=0.5;
  Args->Omega = Omega;
  pcubature(FDim, BZIntegrand_PCubature, (void *)Args, LDim,
	    Lower, Upper, MaxPoints, AbsTol, RelTol,
	    ERROR_INDIVIDUAL, BZIntegral, BZIError);
 
}

/***************************************************************/
/* BZ integrand function passed to triangle cubature routines  */
/***************************************************************/
void BZIntegrand_TriCub(double *u, void *pArgs, double *BZIntegrand)
{
  /*--------------------------------------------------------------*/
  /*- unpack fields from user data structure ---------------------*/
  /*--------------------------------------------------------------*/
  GetBZIArgStruct *Args  = (GetBZIArgStruct *)pArgs;
  cdouble Omega          = Args->Omega;
  BZIFunction *BZIFunc   = Args->BZIFunc;
  void *UserData         = Args->UserData;
  HMatrix *RLBasis       = Args->RLBasis;

  /*--------------------------------------------------------------*/
  /*- convert (ux, uy) variable to kBloch ------------------------*/
  /*--------------------------------------------------------------*/
  double kBloch[2]={0.0, 0.0};
  kBloch[0] = u[0]*RLBasis->GetEntryD(0,0)
             +u[1]*RLBasis->GetEntryD(1,0);
  kBloch[1] = u[0]*RLBasis->GetEntryD(0,1) 
             +u[1]*RLBasis->GetEntryD(1,1);

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  BZIFunc(UserData, Omega, kBloch, BZIntegrand);

  Args->NumCalls++;
}

/***************************************************************/
/* triangle cubature                                           */
/* Order = {0, 1, 2, 4, 5, 7, 9, 13, 14, 16, 20, 25}           */
/***************************************************************/
void GetBZIntegral_TC(GetBZIArgStruct *Args, cdouble Omega,
                      double *BZIntegral)
{
  int Order              = Args->Order;
  bool BZSymmetric       = Args->BZSymmetric;
  bool Reduced           = Args->Reduced;
  int FDim               = Args->FDim;

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  static int FDimSave=0, MaxPointsSave=0;
  static void *Workspace=0;
  static double *Error=0;
  if (Order==0)
   {
     if (FDimSave!=FDim || MaxPointsSave!=Args->MaxPoints )
      { FDimSave=FDim;
        MaxPointsSave=Args->MaxPoints;
        if (Workspace) free(Workspace);
        Workspace=CreateDCUTRIWorkspace(FDim, Args->MaxPoints);
        Error=(double *)reallocEC(Error, FDim*sizeof(double));
      };
   };
  
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  double V1[3]={0.0, 0.0, 0.0};
  double V2[3]={1.0, 0.0, 0.0};
  double V3[3]={1.0, 1.0, 0.0};
  double *Vertices[3]={V1,V2,V3};
  if (Reduced)
   V2[0]=V3[0]=V3[1]=0.5;
  Args->Omega=Omega; 

  if (Order==0)
   Args->NumCalls = DCUTRI(Workspace, Vertices, 
                             BZIntegrand_TriCub, (void *) Args,
                             Args->AbsTol, Args->RelTol, 
                             BZIntegral, Error);
  else
   Args->NumCalls = TriIntFixed(BZIntegrand_TriCub, FDim, (void *)Args,
                                V1, V2, V3, Order, BZIntegral);
  
  if (BZSymmetric)
   { for(int nf=0; nf<FDim; nf++)
      BZIntegral[nf]*=2.0;
   }
  else
   { V2[1]=V2[0]; V2[0]=0.0;
     double *F2=new double[FDim];
     if (Order==0)
      Args->NumCalls += DCUTRI(Workspace, Vertices, 
                               BZIntegrand_TriCub, (void *) Args,
                               Args->AbsTol, Args->RelTol, 
                               F2, Error);
     else
      Args->NumCalls += TriIntFixed(BZIntegrand_TriCub, FDim, (void *)Args,
                                    V1, V2, V3, Order, F2);

     for(int nf=0; nf<FDim; nf++)
      BZIntegral[nf] += F2[nf];
     delete[] F2;
   };

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void GetBZIntegral_CC(GetBZIArgStruct *Args, cdouble Omega,
                      double *BZIntegral)
{
  BZIFunction *BZIFunc  = Args->BZIFunc;
  void *UserData        = Args->UserData;
  bool BZSymmetric      = Args->BZSymmetric;
  int FDim              = Args->FDim;
  int LDim              = Args->RLBasis->NR;
  double AbsTol         = Args->AbsTol;
  double RelTol         = Args->RelTol;
  double *BZIError      = Args->BZIError;
  bool Reduced          = Args->Reduced; 
  int Order             = Args->Order;
  HMatrix *RLBasis      = Args->RLBasis;

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  if (Order>0)
   { double *CCQR=GetCCRule(Order);
     double uMin = 0.0, uMax = Reduced ? 0.5 : 1.0;
     double uAvg = 0.5*(uMax+uMin), uDelta=0.5*(uMax-uMin);
     double kBloch[2];
     double *dBZI=BZIError;
     memset(BZIntegral, 0, FDim*sizeof(double));
     if (LDim==1)
      { for(int n=0; n<Order; n++)
         { double u  = uAvg + uDelta*CCQR[2*n + 0];
           double w  = CCQR[2*n+1];
           kBloch[0] = u*RLBasis->GetEntryD(0,0);
           kBloch[1] = u*RLBasis->GetEntryD(0,1);
           BZIFunc(UserData, Omega, kBloch, dBZI);
           Args->NumCalls++;
           for(int nf=0; nf<FDim; nf++)
            BZIntegral[nf]+=w*uDelta*dBZI[nf];
         };
      }
     else //(LDim==2)
      { for(int n1=0; n1<Order; n1++)
         for(int n2=(BZSymmetric ? n1 : 0); n2<Order; n2++)
          { double u1 = uAvg + uDelta*CCQR[2*n1 + 0];
            double u2 = uAvg + uDelta*CCQR[2*n2 + 0];
            double w  = CCQR[2*n1+1] * CCQR[2*n2+1];
            if (BZSymmetric && n2>n1) w*=2.0;
            kBloch[0] 
             = u1*RLBasis->GetEntryD(0,0)+u2*RLBasis->GetEntryD(1,0);
            kBloch[1] 
             = u1*RLBasis->GetEntryD(0,1)+u2*RLBasis->GetEntryD(1,1);
            BZIFunc(UserData, Omega, kBloch, dBZI);
            Args->NumCalls++;
            for(int nf=0; nf<FDim; nf++)
             BZIntegral[nf]+=w*uDelta*uDelta*dBZI[nf];
          };
      };
     return; 
   };

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  static int FDimSave=0;
  static int LDimSave=0;
  static double *AllValues=0, *OuterValues=0;
  if (FDimSave!=FDim || LDimSave!=LDim)
   { FDimSave=FDim;
     LDimSave=LDim;
     int NP1 = (LDim==1) ? 129 : 129*129;
     int NP2 = (LDim==1) ? 65  : 65*65;
     AllValues=(double *)realloc(AllValues, NP1*FDim*sizeof(double));
     OuterValues=(double *)realloc(OuterValues, NP2*FDim*sizeof(double));
   };

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  int pMin = 2;
  int pMax = 6;
  bool Converged = false;
  double xMin[2]={0.0, 0.0};
  double xMax[2]={1.0, 1.0};
  if (Reduced)
   xMax[0]=xMax[1]=0.5;
  for(int p=pMin; p<=pMax && !Converged; p++)
   { 
     if (LDim==1)
      ECC( p, xMin[0], xMax[0], BZIntegrand_PCubature,
           (void *)Args, FDim, AllValues,
           (p==pMin ? 0 : OuterValues), BZIntegral, BZIError);
     else
      ECC2D( p, xMin, xMax, BZIntegrand_PCubature,
             (void *)Args, FDim,
             BZSymmetric, AllValues,
             (p==pMin ? 0 : OuterValues), BZIntegral, BZIError);

     int N=(1<<(p+1)+1);
     Args->NumCalls += (LDim==1 ? N : N*N);
     int OVSize = (1<<p)+1;
     if (LDim==2) OVSize*=OVSize;
     if (p<pMax) 
      memcpy(OuterValues, AllValues, OVSize*FDim*sizeof(double));

     /*--------------------------------------------------------------*/
     /*- convergence analysis ---------------------------------------*/
     /*--------------------------------------------------------------*/
     double MaxAbsError=0.0, MaxRelError=0.0; 
     for(int nf=0; nf<FDim; nf++)
      { 
        MaxAbsError = fmax(MaxAbsError, BZIError[nf]);
        if ( BZIError[nf] > AbsTol )
         MaxRelError = fmax(MaxRelError, BZIError[nf] / fabs(BZIntegral[nf]) );
      };
     if ( (MaxAbsError < AbsTol) || (MaxRelError < RelTol) )
      Converged=true;
   };

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void GetBZIntegral(GetBZIArgStruct *Args, cdouble Omega, 
                   double *BZIntegral)
{
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  int FDim = Args->FDim;
  if (Args->BZIErrorSize!=FDim)
   { Args->BZIErrorSize=FDim;
     Args->BZIError
      =(double *)reallocEC(Args->BZIError, FDim*sizeof(double));
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  Args->NumCalls=0;
  switch(Args->BZIMethod)
   { case BZI_ADAPTIVE:
      GetBZIntegral_PCubature(Args, Omega, BZIntegral);
      break;
     case BZI_CC:
      GetBZIntegral_CC(Args, Omega, BZIntegral);
      break;
     case BZI_TC:
      GetBZIntegral_TC(Args, Omega, BZIntegral);
      break;
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  if (Args->Reduced)
   { int LDim = Args->RLBasis->NR;
     double Factor = (LDim==1) ? 2.0 : 4.0;
     for(int nf=0; nf<FDim; nf++)
     { BZIntegral[nf]*=Factor;
       Args->BZIError[nf]*=Factor;
     };
   };
} 

/***************************************************************/
/***************************************************************/
/***************************************************************/
void InitGetBZIArgs(GetBZIArgStruct *Args, HMatrix *LBasis)
{
  Args->BZIFunc      = 0;
  Args->UserData     = 0;
  Args->FDim         = 0;
  Args->BZSymmetric  = false;

  Args->BZIMethod    = BZI_ADAPTIVE;
  Args->MaxPoints    = 0;
  Args->NumCalls     = 0;
  Args->Order        = 4;
  Args->RelTol       = 1.0e-2;
  Args->AbsTol       = 0.0;
  Args->Reduced      = true;

  Args->BZIError     = 0;
  Args->BZIErrorSize = 0;

  if (!LBasis || (LBasis->NR!=3 || LBasis->NC>=4) )
   ErrExit("Invalid lattice basis in InitGetBZIArgs");

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  int LDim = LBasis->NR;
  Args->RLBasis = new HMatrix(LDim, 2);
  if (LDim==1)
   { 
     if (LBasis->GetEntryD(0,1)!=0.0)
      ErrExit("1D lattice vectors must be parallel to x axis");

     double GammaX=2.0*M_PI/(LBasis->GetEntryD(0,0));
     Args->RLBasis->SetEntry(0,0,GammaX);
     Args->BZVolume = GammaX;
   }
  else 
   { double L1X=LBasis->GetEntryD(0,0);
     double L1Y=LBasis->GetEntryD(1,0);
     double L2X=LBasis->GetEntryD(0,1);
     double L2Y=LBasis->GetEntryD(1,1);

     double Area= L1X*L2Y - L1Y*L2X;
     if (Area==0.0)
      ErrExit("%s:%i: lattice has empty unit cell",__FILE__,__LINE__);
     double Gamma1X =  2.0*M_PI*L2Y / Area;
     double Gamma1Y = -2.0*M_PI*L1Y / Area;
     double Gamma2X = -2.0*M_PI*L2X / Area;
     double Gamma2Y =  2.0*M_PI*L1X / Area;

     Args->RLBasis->SetEntry(0,0,Gamma1X);
     Args->RLBasis->SetEntry(1,0,Gamma1Y);
     Args->RLBasis->SetEntry(0,1,Gamma2X);
     Args->RLBasis->SetEntry(1,1,Gamma2Y);
  
     Args->BZVolume = Gamma1X*Gamma2Y - Gamma1Y*Gamma2X;
   };
  
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
GetBZIArgStruct *CreateGetBZIArgs(HMatrix *LBasis)
{ 
  GetBZIArgStruct *Args = (GetBZIArgStruct *)mallocEC(sizeof(*Args));
  InitGetBZIArgs(Args, LBasis);
  return Args;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
#if 0
GetBZIArgStruct *ProcessBZIOptions(int argc, char *argv[])
{
  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  GetBZIArgStruct *Args = (GetBZIArgStruct *)mallocEC(sizeof(*Args));

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  char *BZIString=0;
  int BZIOrder=-1;
  int BZIMaxPoints=1000;
  bool BZSymmetric=false;
  double BZIRelTol=1.0e-2;
  double BZIAbsTol=0.0;

  OptStruct OSArray[]=
   { 
     {"BZIMethod",     PA_STRING,  1, 1, (void *)&BZIString,    0,  "Brillouin-zone integration method [DCUTRI | adaptive]"},
     {"BZIOrder",      PA_INT,     1, 1, (void *)&BZIOrder,     0,  "Brillouin-zone integration order"},
     {"BZIRelTol",     PA_DOUBLE,  1, 1, (void *)&RelTol,       0,  "relative tolerance for Brillouin-zone integration"},
     {"BZIAbsTol",     PA_DOUBLE,  1, 1, (void *)&RelTol,       0,  "absolute tolerance for Brillouin-zone integration"},
     {"BZIMaxPoints",  PA_INT,     1, 1, (void *)&MaxEvals,     0,  "maximum number of Brillouin-zone samples"},
     {"BZSymmetric",   PA_BOOL,    0, 1, (void *)&BZSymmetric,  0,  "assume BZ integrand is xy-symmetric"},
   };
  ProcessOptions(argc, argv, OSArray);

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void SetLBasis(Get BZIArgStruct *Args, HMatrix *LBasis)
{
}
#endif
