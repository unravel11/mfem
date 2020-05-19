// Copyright (c) 2010-2020, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#include "../config/config.hpp"

#ifdef MFEM_USE_MPI
#ifdef MFEM_USE_PETSC
#ifdef MFEM_USE_SLEPC

#include "linalg.hpp"

#include "slepc.h"

// Error handling
// Prints SLEPc's stacktrace and then calls MFEM_ABORT
// We cannot use PETSc's CHKERRQ since it returns a PetscErrorCode
#define PCHKERRQ(obj,err) do {                                                   \
     if ((err))                                                                  \
     {                                                                           \
        PetscError(PetscObjectComm((PetscObject)(obj)),__LINE__,_MFEM_FUNC_NAME, \
                   __FILE__,(err),PETSC_ERROR_REPEAT,NULL);                      \
        MFEM_ABORT("Error in SLEPc. See stacktrace above.");                     \
     }                                                                           \
  } while(0);
#define CCHKERRQ(comm,err) do {                                \
     if ((err))                                                \
     {                                                         \
        PetscError(comm,__LINE__,_MFEM_FUNC_NAME,              \
                   __FILE__,(err),PETSC_ERROR_REPEAT,NULL);    \
        MFEM_ABORT("Error in SLEPc. See stacktrace above.");   \
     }                                                         \
  } while(0);


static PetscErrorCode ierr;

namespace mfem
{
void MFEMInitializeSlepc()
{
   MFEMInitializeSlepc(NULL,NULL,NULL,NULL);
}

void MFEMInitializeSlepc(int *argc,char*** argv)
{
   MFEMInitializeSlepc(argc,argv,NULL,NULL);
}

void MFEMInitializeSlepc(int *argc,char ***argv,const char rc_file[],
                         const char help[])
{
   ierr = SlepcInitialize(argc,argv,rc_file,help);
   MFEM_VERIFY(!ierr,"Unable to initialize SLEPc");
}

void MFEMFinalizeSlepc()
{
   ierr = SlepcFinalize();
   MFEM_VERIFY(!ierr,"Unable to finalize SLEPc");
}


SlepcEigenSolver::SlepcEigenSolver(MPI_Comm comm, const std::string &prefix,
                                   bool wrap)
{
   clcustom = false;
   _tol = PETSC_DEFAULT;
   _max_its = PETSC_DEFAULT;
   _wrap = wrap;
   VR = NULL;
   VC = NULL;
   operatorset = false;

   ierr = EPSCreate(comm,&eps); CCHKERRQ(comm,ierr);
   ierr = EPSSetOptionsPrefix(eps, prefix.c_str()); PCHKERRQ(eps, ierr);
}

SlepcEigenSolver::~SlepcEigenSolver()
{
   MPI_Comm comm;
   ierr = PetscObjectGetComm((PetscObject)eps,&comm); PCHKERRQ(eps,ierr);
   ierr = EPSDestroy(&eps); CCHKERRQ(comm,ierr);
}


void SlepcEigenSolver::SetOperator(const Operator &op)
{
   PetscParMatrix *pA = const_cast<PetscParMatrix *>
                        (dynamic_cast<const PetscParMatrix *>(&op));
   const HypreParMatrix *hA = dynamic_cast<const HypreParMatrix *>(&op);
   const Operator       *oA = dynamic_cast<const Operator *>(&op);
   bool delete_pA = false;

   if (!pA)
   {
      if (hA)
      {
         pA = new PetscParMatrix(hA,
                                 _wrap ? Operator::PETSC_MATSHELL : Operator::PETSC_MATAIJ);
         delete_pA = true;
      }
      else if (oA)
      {
         pA = new PetscParMatrix(PetscObjectComm((PetscObject)eps),oA,
                                 _wrap ? Operator::PETSC_MATSHELL : Operator::PETSC_MATAIJ);
         delete_pA = true;

      }
   }
   MFEM_VERIFY(pA, "Unsupported operation!");

   if (operatorset)
   {
      delete VR;
      delete VC;
      VR = VC = NULL;
   }
   ierr = EPSSetOperators(eps,*pA,NULL); PCHKERRQ(eps, ierr);
   operatorset = true;

   if (delete_pA) {delete_pA;}
}

void SlepcEigenSolver::SetOperators(const Operator &op, const Operator &opB)
{
   PetscParMatrix *pA = const_cast<PetscParMatrix *>
                        (dynamic_cast<const PetscParMatrix *>(&op));
   PetscParMatrix *pB = const_cast<PetscParMatrix *>
                        (dynamic_cast<const PetscParMatrix *>(&opB));
   const HypreParMatrix *hA = dynamic_cast<const HypreParMatrix *>(&op);
   const HypreParMatrix *hB = dynamic_cast<const HypreParMatrix *>(&opB);

   const Operator *oA = dynamic_cast<const Operator *>(&op);
   const Operator *oB = dynamic_cast<const Operator *>(&opB);
   bool delete_pA = false;
   bool delete_pB = false;
   if (!pA)
   {
      if (hA)
      {
         pA = new PetscParMatrix(hA,
                                 _wrap ? Operator::PETSC_MATSHELL : Operator::PETSC_MATAIJ);
         delete_pA = true;
      }
      else if (oA)
      {
         pA = new PetscParMatrix(PetscObjectComm((PetscObject)eps),oA,
                                 _wrap ? Operator::PETSC_MATSHELL : Operator::PETSC_MATAIJ);
         delete_pA = true;
      }
   }
   MFEM_VERIFY(pA, "Unsupported Operation!");
   if (!pB)
   {
      if (hB)
      {
         pB = new PetscParMatrix(hB,
                                 _wrap ? Operator::PETSC_MATSHELL : Operator::PETSC_MATAIJ);
         delete_pB = true;
      }
      else if (oB)
      {
         pB = new PetscParMatrix(PetscObjectComm((PetscObject)eps),oB,
                                 _wrap ? Operator::PETSC_MATSHELL : Operator::PETSC_MATAIJ);
         delete_pB = true;
      }
   }
   MFEM_VERIFY(pB, "Unsupported Operation!");

   if (operatorset)
   {
      delete VR;
      delete VC;
      VR = VC = NULL;
   }
   operatorset = true;

   ierr = EPSSetOperators(eps,*pA,*pB); PCHKERRQ(eps,ierr);
   if (delete_pA) {delete_pA;}
   if (delete_pB) {delete_pB;}
}

void SlepcEigenSolver::SetTol(double tol)
{
   _tol = tol;
   ierr = EPSSetTolerances(eps,_tol,_max_its); PCHKERRQ(eps,ierr);
}

void SlepcEigenSolver::SetMaxIter(int max_its)
{
   _max_its = max_its;
   ierr = EPSSetTolerances(eps,_tol,_max_its); PCHKERRQ(eps,ierr);
}

void SlepcEigenSolver::SetNumModes(int num_eigs)
{
   ierr = EPSSetDimensions(eps,num_eigs,PETSC_DECIDE,PETSC_DECIDE);
   PCHKERRQ(eps,ierr);
}

void SlepcEigenSolver::Solve()
{
   Customize();

   ierr = EPSSolve(eps); PCHKERRQ(eps,ierr);
}

void SlepcEigenSolver::Customize(bool customize) const
{
   if (!customize) {clcustom = true; }
   if (!clcustom)
   {
      ierr = EPSSetFromOptions(eps); PCHKERRQ(eps,ierr);
   }
   clcustom = true;
}

void SlepcEigenSolver::GetEigenvalue(unsigned int i, double & lr) const
{
   ierr = EPSGetEigenvalue(eps,i,&lr,NULL); PCHKERRQ(eps,ierr);
}

void SlepcEigenSolver::GetEigenvalue(unsigned int i, double & lr,
                                     double & lc) const
{
   ierr = EPSGetEigenvalue(eps,i,&lr,&lc); PCHKERRQ(eps,ierr);
}

void SlepcEigenSolver::GetEigenvector(unsigned int i, Vector & vr) const
{
   if (!VR)
   {
      Mat pA = NULL;
      ierr = EPSGetOperators(eps, &pA, NULL); PCHKERRQ(eps,ierr);
      VR = new PetscParVector(pA, true, false);
   }
   VR->PlaceArray(vr.GetData());
   ierr = EPSGetEigenvector(eps,i,*VR,NULL); PCHKERRQ(eps,ierr);
   VR->ResetArray();

}

void SlepcEigenSolver::GetEigenvector(unsigned int i, Vector & vr,
                                      Vector & vc) const
{
   if (!VR || !VC)
   {
      Mat pA = NULL;
      ierr = EPSGetOperators(eps, &pA, NULL); PCHKERRQ(eps,ierr);

      if (!VR)
      {
         VR = new PetscParVector(pA, true, false);
      }
      if (!VC)
      {
         VC = new PetscParVector(pA, true, false);
      }
   }
   VR->PlaceArray(vr.GetData());
   VC->PlaceArray(vc.GetData());
   ierr = EPSGetEigenvector(eps,i,*VR,*VC); PCHKERRQ(eps,ierr);
   VR->ResetArray();
   VC->ResetArray();
}

int SlepcEigenSolver::GetNumConverged()
{
   int num_conv;
   ierr = EPSGetConverged(eps,&num_conv); PCHKERRQ(eps,ierr);
   return num_conv;
}


void SlepcEigenSolver::SetWhichEigenpairs(SlepcEigenSolver::Which which)
{
   switch (which)
   {
      case SlepcEigenSolver::LARGEST_MAGNITUDE:
         ierr = EPSSetWhichEigenpairs(eps,EPS_LARGEST_MAGNITUDE); PCHKERRQ(eps,ierr);
         break;
      case SlepcEigenSolver::SMALLEST_MAGNITUDE:
         ierr = EPSSetWhichEigenpairs(eps,EPS_SMALLEST_MAGNITUDE); PCHKERRQ(eps,ierr);
         break;
      case SlepcEigenSolver::LARGEST_REAL:
         ierr = EPSSetWhichEigenpairs(eps,EPS_LARGEST_REAL); PCHKERRQ(eps,ierr);
         break;
      case SlepcEigenSolver::SMALLEST_REAL:
         ierr = EPSSetWhichEigenpairs(eps,EPS_SMALLEST_REAL); PCHKERRQ(eps,ierr);
         break;
      case SlepcEigenSolver::LARGEST_IMAGINARY:
         ierr = EPSSetWhichEigenpairs(eps,EPS_LARGEST_IMAGINARY); PCHKERRQ(eps,ierr);
         break;
      case SlepcEigenSolver::SMALLEST_IMAGINARY:
         ierr = EPSSetWhichEigenpairs(eps,EPS_SMALLEST_IMAGINARY); PCHKERRQ(eps,ierr);
         break;
      case SlepcEigenSolver::TARGET_MAGNITUDE:
         ierr = EPSSetWhichEigenpairs(eps,EPS_TARGET_MAGNITUDE); PCHKERRQ(eps,ierr);
         break;
      case SlepcEigenSolver::TARGET_REAL:
         ierr = EPSSetWhichEigenpairs(eps,EPS_TARGET_REAL); PCHKERRQ(eps,ierr);
         break;
      default:
         MFEM_ABORT("Which eigenpair not implemented!");
         break;
   }
}

void SlepcEigenSolver::SetTarget(double target)
{
   ierr = EPSSetTarget(eps,target); PCHKERRQ(eps,ierr);
}

void SlepcEigenSolver::SetSpectralTransformation(
   SlepcEigenSolver::SpectralTransformation transformation)
{
   ST st;
   ierr = EPSGetST(eps,&st); PCHKERRQ(eps,ierr);
   switch (transformation)
   {
      case SlepcEigenSolver::SHIFT:
         ierr = STSetType(st,STSHIFT); PCHKERRQ(eps,ierr);
         break;
      case SlepcEigenSolver::SHIFT_INVERT:
         ierr = STSetType(st,STSINVERT); PCHKERRQ(eps,ierr);
         break;
      default:
         MFEM_ABORT("Spectral transformation not implemented!");
         break;
   }
}

}

#endif  // MFEM_USE_SLEPC
#endif  // MFEM_USE_PETSC
#endif  // MFEM_USE_MPI