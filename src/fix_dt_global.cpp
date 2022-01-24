/* ----------------------------------------------------------------------
   SPARTA - Stochastic PArallel Rarefied-gas Time-accurate Analyzer
   http://sparta.sandia.gov
   Steve Plimpton, sjplimp@sandia.gov, Michael Gallis, magalli@sandia.gov
   Sandia National Laboratories

   Copyright (2014) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level SPARTA directory.
------------------------------------------------------------------------- */

#include "stdlib.h"
#include "string.h"
#include "fix_dt_global.h"
#include "update.h"
#include "input.h"
#include "grid.h"
#include "modify.h"
#include "variable.h"
#include "memory.h"
#include "error.h"
#include "domain.h"
#include "particle.h"
#include "mixture.h"
#include <iostream>

using namespace SPARTA_NS;

enum{NONE,COMPUTE,FIX};

#define INVOKED_PER_GRID 16
#define BIG 1.0e20

/* ---------------------------------------------------------------------- */

FixDtGlobal::FixDtGlobal(SPARTA *sparta, int narg, char **arg) :
  Fix(sparta, narg, arg)
{
  if (narg < 9) error->all(FLERR,"Illegal fix dt global command");

  nevery = atoi(arg[2]);
  if (nevery <= 0) error->all(FLERR,"Illegal fix dt global command");

  imix = particle->find_mixture(arg[3]);
  if (imix < 0) error->all(FLERR,"fix dg_global mixture ID does not exist");

  if (strncmp(arg[4],"c_",2) != 0 && strncmp(arg[4],"f_",2) != 0)
    error->all(FLERR,"Illegal fix dt global command");

  if ( !((strncmp(arg[5],"f_",2) == 0) &&
         (strncmp(arg[6],"f_",2) == 0) &&
         (strncmp(arg[7],"f_",2) == 0) &&
         (strncmp(arg[8],"f_",2) == 0)) )
    error->all(FLERR,"Illegal fix dt global command");

  id_lambda = NULL;
  id_temp = NULL;
  id_usq = NULL;
  id_vsq = NULL;
  id_wsq = NULL;

  // find minimum species mass
  Particle::Species *species = particle->species;
  min_species_mass = BIG;
  for (int s = 0; s < particle->nspecies; ++s)
    min_species_mass = MIN(species[s].mass,min_species_mass);

  // lambda compute or fix
  int n = strlen(arg[4]);
  id_lambda = new char[n];
  strcpy(id_lambda,&arg[4][2]);
  char *ptr = strchr(id_lambda,'[');
  if (ptr) {
    if (id_lambda[strlen(id_lambda)-1] != ']')
      error->all(FLERR,"Invalid lambda in fix dt_global command");
    lambdaindex = atoi(ptr+1);
    *ptr = '\0';
  } else lambdaindex = 0;

  if (strncmp(arg[4],"c_",2) == 0) lambdawhich = COMPUTE;
  else lambdawhich = FIX;

  if (lambdawhich == COMPUTE) { // lambda compute
    n = modify->find_compute(id_lambda);
    if (n < 0)
      error->all(FLERR,"Could not find fix dt_global compute ID");
    if (modify->compute[n]->per_grid_flag == 0)
      error->all(FLERR,"Fix dt_global compute does not "
                 "compute per-grid info");
    if (lambdaindex == 0 && modify->compute[n]->size_per_grid_cols > 0)
      error->all(FLERR,
                 "Fix dt_global compute does not "
                 "compute per-grid vector");
    if (lambdaindex > 0 && modify->compute[n]->size_per_grid_cols == 0)
      error->all(FLERR,
                 "Fix dt_global compute does not "
                 "compute per-grid array");
    if (lambdaindex > 0 && lambdaindex > modify->compute[n]->size_per_grid_cols)
      error->all(FLERR,"Fix dt_global compute vector is "
                 "accessed out-of-range");
  } else { // lambda fix
    n = modify->find_fix(id_lambda);
    if (n < 0) error->all(FLERR,"Could not find fix dt_global fix ID");
    if (modify->fix[n]->per_grid_flag == 0)
      error->all(FLERR,"Fix dt_global fix does not "
                 "compute per-grid info");
    if (lambdaindex == 0 && modify->fix[n]->size_per_grid_cols > 0)
      error->all(FLERR,"Fix dt_global fix does not "
                 "compute per-grid vector");
    if (lambdaindex > 0 && modify->fix[n]->size_per_grid_cols == 0)
      error->all(FLERR,"Fix dt_global fix does not "
                 "compute per-grid array");
    if (lambdaindex > 0 && lambdaindex > modify->fix[n]->size_per_grid_cols)
      error->all(FLERR,"Fix dt_global fix array is "
                 "accessed out-of-range");
  }

  // temperature fix
  n = strlen(arg[5]);
  id_temp = new char[n];
  strcpy(id_temp,&arg[5][2]);

  ptr = strchr(id_temp,'[');
  if (ptr) {
    if (id_temp[strlen(id_temp)-1] != ']')
      error->all(FLERR,"Invalid temperature in fix dt_global command");
    tempindex = atoi(ptr+1);
    *ptr = '\0';
  } else tempindex = 0;

  n = modify->find_fix(id_temp);
  if (n < 0) error->all(FLERR,"Could not find fix dt_global tempeature fix ID");
  if (modify->fix[n]->per_grid_flag == 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid info");
  if (tempindex == 0 && modify->fix[n]->size_per_grid_cols > 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid vector");
  if (tempindex > 0 && modify->fix[n]->size_per_grid_cols == 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid array");
  if (tempindex > 0 && tempindex > modify->fix[n]->size_per_grid_cols)
    error->all(FLERR,"Fix dt_global fix array is "
               "accessed out-of-range");

  // squared-velocity fixes

  // usq----------------------------
  n = strlen(arg[6]);
  id_usq = new char[n];
  strcpy(id_usq,&arg[6][2]);

  ptr = strchr(id_usq,'[');
  if (ptr) {
    if (id_usq[strlen(id_usq)-1] != ']')
      error->all(FLERR,"Invalid usq in fix dt_global command");
    usqindex = atoi(ptr+1);
    *ptr = '\0';
  } else usqindex = 0;

  n = modify->find_fix(id_usq);
  if (n < 0) error->all(FLERR,"Could not find fix dt_global usq fix ID");
  if (modify->fix[n]->per_grid_flag == 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid info");
  if (usqindex == 0 && modify->fix[n]->size_per_grid_cols > 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid vector");
  if (usqindex > 0 && modify->fix[n]->size_per_grid_cols == 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid array");
  if (usqindex > 0 && usqindex > modify->fix[n]->size_per_grid_cols)
    error->all(FLERR,"Fix dt_global fix array is "
               "accessed out-of-range");

  // vsq----------------------------
  n = strlen(arg[7]);
  id_vsq = new char[n];
  strcpy(id_vsq,&arg[7][2]);

  ptr = strchr(id_vsq,'[');
  if (ptr) {
    if (id_vsq[strlen(id_vsq)-1] != ']')
      error->all(FLERR,"Invalid vsq in fix dt_global command");
    vsqindex = atoi(ptr+1);
    *ptr = '\0';
  } else vsqindex = 0;

  n = modify->find_fix(id_vsq);
  if (n < 0) error->all(FLERR,"Could not find fix dt_global vsq fix ID");
  if (modify->fix[n]->per_grid_flag == 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid info");
  if (vsqindex == 0 && modify->fix[n]->size_per_grid_cols > 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid vector");
  if (vsqindex > 0 && modify->fix[n]->size_per_grid_cols == 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid array");
  if (vsqindex > 0 && vsqindex > modify->fix[n]->size_per_grid_cols)
    error->all(FLERR,"Fix dt_global fix array is "
               "accessed out-of-range");

  // wsq----------------------------
  n = strlen(arg[8]);
  id_wsq = new char[n];
  strcpy(id_wsq,&arg[8][2]);

  ptr = strchr(id_wsq,'[');
  if (ptr) {
    if (id_wsq[strlen(id_wsq)-1] != ']')
      error->all(FLERR,"Invalid wsq in fix dt_global command");
    wsqindex = atoi(ptr+1);
    *ptr = '\0';
  } else wsqindex = 0;

  n = modify->find_fix(id_wsq);
  if (n < 0) error->all(FLERR,"Could not find fix dt_global wsq fix ID");
  if (modify->fix[n]->per_grid_flag == 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid info");
  if (wsqindex == 0 && modify->fix[n]->size_per_grid_cols > 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid vector");
  if (wsqindex > 0 && modify->fix[n]->size_per_grid_cols == 0)
    error->all(FLERR,"Fix dt_global fix does not "
               "compute per-grid array");
  if (wsqindex > 0 && wsqindex > modify->fix[n]->size_per_grid_cols)
    error->all(FLERR,"Fix dt_global fix array is "
               "accessed out-of-range");

  // optional args (prescribed temperature/velocity profiles for initial timestep calculation)
  int iarg = 9;
  while (iarg < narg) {
    if (strcmp(arg[iarg],"temperature") == 0) {
      if (iarg+5 > narg) error->all(FLERR,"Illegal fix dt_global command");
      tempflag = 1;
      tstr = arg[iarg+1];
      if (strcmp(arg[iarg+2],"NULL") == 0) txstr = NULL;
      else txstr = arg[iarg+2];
      if (strcmp(arg[iarg+3],"NULL") == 0) tystr = NULL;
      else tystr = arg[iarg+3];
      if (strcmp(arg[iarg+4],"NULL") == 0) tzstr = NULL;
      else tzstr = arg[iarg+4];
      iarg += 5;
    } else if (strcmp(arg[iarg],"velocity") == 0) {
      if (iarg+7 > narg) error->all(FLERR,"Illegal fix dt_global command");
      velflag = 1;
      if (strcmp(arg[iarg+1],"NULL") == 0) vxstr = NULL;
      else vxstr = arg[iarg+1];
      if (strcmp(arg[iarg+2],"NULL") == 0) vystr = NULL;
      else vystr = arg[iarg+2];
      if (strcmp(arg[iarg+3],"NULL") == 0) vzstr = NULL;
      else vzstr = arg[iarg+3];
      if (strcmp(arg[iarg+4],"NULL") == 0) vstrx = NULL;
      else vstrx = arg[iarg+4];
      if (strcmp(arg[iarg+5],"NULL") == 0) vstry = NULL;
      else vstry = arg[iarg+5];
      if (strcmp(arg[iarg+6],"NULL") == 0) vstrz = NULL;
      else vstrz = arg[iarg+6];
      iarg += 7;
    }
  }

  if (tempflag) {
    tvar = input->variable->find(tstr);
    if (tvar < 0)
      error->all(FLERR,"Variable name for fix dt_global does not exist");
    if (!input->variable->equal_style(tvar))
      error->all(FLERR,"Variable for fix dt_global is invalid style");
    if (txstr) {
      txvar = input->variable->find(txstr);
      if (txvar < 0)
        error->all(FLERR,"Variable name for fix dt_global does not exist");
      if (!input->variable->internal_style(txvar))
        error->all(FLERR,"Variable for fix dt_global is invalid style");
    }
    if (tystr) {
      tyvar = input->variable->find(tystr);
      if (tyvar < 0)
        error->all(FLERR,"Variable name for fix dt_global does not exist");
      if (!input->variable->internal_style(tyvar))
        error->all(FLERR,"Variable for fix dt_global is invalid style");
    }
    if (tzstr) {
      tzvar = input->variable->find(tzstr);
      if (tzvar < 0)
        error->all(FLERR,"Variable name for fix dt_global does not exist");
      if (!input->variable->internal_style(tzvar))
        error->all(FLERR,"Variable for fix dt_global is invalid style");
    }
  }

  if (velflag) {
    if (vxstr) {
      vxvar = input->variable->find(vxstr);
      if (vxvar < 0)
        error->all(FLERR,"Variable name for create_particles does not exist");
      if (!input->variable->equal_style(vxvar))
        error->all(FLERR,"Variable for create_particles is invalid style");
    }
    if (vystr) {
      vyvar = input->variable->find(vystr);
      if (vyvar < 0)
        error->all(FLERR,"Variable name for create_particles does not exist");
      if (!input->variable->equal_style(vyvar))
        error->all(FLERR,"Variable for create_particles is invalid style");
    }
    if (vzstr) {
      vzvar = input->variable->find(vzstr);
      if (vzvar < 0)
        error->all(FLERR,"Variable name for create_particles does not exist");
      if (!input->variable->equal_style(vzvar))
        error->all(FLERR,"Variable for create_particles is invalid style");
    }
    if (vstrx) {
      vvarx = input->variable->find(vstrx);
      if (vvarx < 0)
        error->all(FLERR,"Variable name for create_particles does not exist");
      if (!input->variable->internal_style(vvarx))
        error->all(FLERR,"Variable for create_particles is invalid style");
    }
    if (vstry) {
      vvary = input->variable->find(vstry);
      if (vvary < 0)
        error->all(FLERR,"Variable name for create_particles does not exist");
      if (!input->variable->internal_style(vvary))
        error->all(FLERR,"Variable for create_particles is invalid style");
    }
    if (vstrz) {
      vvarz = input->variable->find(vstrz);
      if (vvarz < 0)
        error->all(FLERR,"Variable name for create_particles does not exist");
      if (!input->variable->internal_style(vvarz))
        error->all(FLERR,"Variable for create_particles is invalid style");
    }
  }

  MPI_Comm_rank(world,&me);
  nglocal = 0;
  lambda = temp = usq = vsq = wsq = NULL;
}

/* ---------------------------------------------------------------------- */

FixDtGlobal::~FixDtGlobal()
{
  if (copymode) return;

  delete [] id_lambda;
  delete [] id_temp;
  delete [] id_usq;
  delete [] id_vsq;
  delete [] id_wsq;
  memory->destroy(lambda);
  memory->destroy(temp);
  memory->destroy(usq);
  memory->destroy(vsq);
  memory->destroy(wsq);
}

/* ---------------------------------------------------------------------- */

void FixDtGlobal::init()
{
  reallocate();

  // set up initial timestep
  double x[3];
  Grid::ChildCell *cells = grid->cells;
  double mixture_temperature_thermal = particle->mixture[imix]->temp_thermal;
  double *vstream = particle->mixture[imix]->vstream;
  double vstream_variable[3];
  double v[3];
  double temperature;

  for (int i = 0; i < nglocal; ++i) {
    x[0] = 0.5*(cells[i].lo[0] + cells[i].hi[0]);
    x[1] = 0.5*(cells[i].lo[1] + cells[i].hi[1]);
    x[2] = 0.5*(cells[i].lo[2] + cells[i].hi[2]);

    if (tempflag)
      temperature = temperature_variable(x);
    else
      temperature = mixture_temperature_thermal;

    if (velflag) {
      velocity_variable(x,vstream,vstream_variable);
      v[0] = vstream_variable[0];
      v[1] = vstream_variable[1];
      v[2] = vstream_variable[2];
    }
    else {
      v[0] = vstream[0];
      v[1] = vstream[1];
      v[2] = vstream[2];
    }

    double vrm_max = sqrt(2.0*update->boltz * temperature / min_species_mass);
  }

  // setup computes and fixes

  // lambda
  if (lambdawhich == COMPUTE) {
    int icompute = modify->find_compute(id_lambda);
    if (icompute < 0)
      error->all(FLERR,"Could not find fix dt_global compute ID");
    clambda = modify->compute[icompute];
  } else if (lambdawhich == FIX) {
    int ifix = modify->find_fix(id_lambda);
    if (ifix < 0)
      error->all(FLERR,"Could not find fix dt_global fix ID");
    flambda = modify->fix[ifix];
  }

  // temperature fix
  int ifix = modify->find_fix(id_temp);
  if (ifix < 0)
    error->all(FLERR,"Could not find temperature fix ID");
  ftemp = modify->fix[ifix];

  // usq fix
  ifix = modify->find_fix(id_usq);
  if (ifix < 0)
    error->all(FLERR,"Could not find usq fix ID");
  fusq = modify->fix[ifix];

  // vsq fix
  ifix = modify->find_fix(id_vsq);
  if (ifix < 0)
    error->all(FLERR,"Could not find vsq fix ID");
  fvsq = modify->fix[ifix];

  // wsq fix
  ifix = modify->find_fix(id_wsq);
  if (ifix < 0)
    error->all(FLERR,"Could not find wsq fix ID");
  fwsq = modify->fix[ifix];
}

/* ---------------------------------------------------------------------- */

int FixDtGlobal::setmask()
{
  int mask = 0;
  mask |= END_OF_STEP;
  return mask;
}

/* ---------------------------------------------------------------------- */

void FixDtGlobal::end_of_step()
{
  // check that fixes are computed at compatible times
  if (lambdawhich == FIX && update->ntimestep % flambda->per_grid_freq)
    error->all(FLERR,"Fix dt_global lambda fix not computed at compatible time");
  if (update->ntimestep % fusq->per_grid_freq)
    error->all(FLERR,"Fix dt_global usq fix not computed at compatible time");
  if (update->ntimestep % fvsq->per_grid_freq)
    error->all(FLERR,"Fix dt_global vsq fix not computed at compatible time");
  if (update->ntimestep % fwsq->per_grid_freq)
    error->all(FLERR,"Fix dt_global wsq fix not computed at compatible time");

  // get lambda from fix or by invoking compute
  if (lambdawhich == COMPUTE) {
    if (!(clambda->invoked_flag & INVOKED_PER_GRID)) {
      clambda->compute_per_grid();
      clambda->invoked_flag |= INVOKED_PER_GRID;
    }
    if (clambda->post_process_grid_flag)
      clambda->post_process_grid(lambdaindex,1,NULL,NULL,NULL,1);

    if (lambdaindex == 0 || clambda->post_process_grid_flag)
      memcpy(lambda,clambda->vector_grid,nglocal*sizeof(double));
    else {
      int index = lambdaindex-1;
      double **array = clambda->array_grid;
      for (int i = 0; i < nglocal; i++)
        lambda[i] = array[i][index];
    }
  } else if (lambdawhich == FIX) {
    if (lambdaindex == 0) {
      memcpy(lambda,flambda->vector_grid,nglocal*sizeof(double));
    } else {
      double **array = flambda->array_grid;
      int index = lambdaindex-1;
      for (int i = 0; i < nglocal; i++)
        lambda[i] = array[i][index];
    }
  }

  // get temperature from fix
  if (tempindex == 0) {
    memcpy(temp,ftemp->vector_grid,nglocal*sizeof(double));
  } else {
    double **array = ftemp->array_grid;
    int index = tempindex-1;
    for (int i = 0; i < nglocal; i++)
      temp[i] = array[i][index];
  }

  // get usq from fix
  if (usqindex == 0) {
    memcpy(usq,fusq->vector_grid,nglocal*sizeof(double));
  } else {
    double **array = fusq->array_grid;
    int index = usqindex-1;
    for (int i = 0; i < nglocal; i++)
      usq[i] = array[i][index];
  }

  // get vsq from fix
  if (vsqindex == 0) {
    memcpy(vsq,fvsq->vector_grid,nglocal*sizeof(double));
  } else {
    double **array = fvsq->array_grid;
    int index = vsqindex-1;
    for (int i = 0; i < nglocal; i++)
      vsq[i] = array[i][index];
  }

  // get wsq from fix
  if (wsqindex == 0) {
    memcpy(wsq,fwsq->vector_grid,nglocal*sizeof(double));
  } else {
    double **array = fwsq->array_grid;
    int index = wsqindex-1;
    for (int i = 0; i < nglocal; i++)
      wsq[i] = array[i][index];
  }

  // compute cell desired timestep ---------------------------------
  Grid::ChildCell *cells = grid->cells;
  Grid::ChildInfo *cinfo = grid->cinfo;
  Particle::OnePart *particles = particle->particles;

  // allow dt growth and reduce below if necessary
  grid->dt_global *= 2;

  double transit_fraction = 0.25;
  double collision_fraction = 0.1;
  double dt_sum = 0.;

  for (int i = 0; i < nglocal; ++i) {

    // cell dt based on mean collision time
    double mean_collision_time = lambda[i]/sqrt(usq[i] + vsq[i] + wsq[i]);
    if (mean_collision_time > 0.) {
      cells[i].dt_desired = collision_fraction*mean_collision_time;
    }

    // cell size
    double dx = cells[i].hi[0] - cells[i].lo[0];
    double dy = cells[i].hi[1] - cells[i].lo[1];
    double dz = 0.;

    // cell dt based on transit time using average velocities
    double dt_candidate = transit_fraction*dx/sqrt(usq[i]);
    cells[i].dt_desired = MIN(dt_candidate,cells[i].dt_desired);

    dt_candidate = transit_fraction*dy/sqrt(vsq[i]);
    cells[i].dt_desired = MIN(dt_candidate,cells[i].dt_desired);

    if (domain->dimension == 3) {
      dz = cells[i].hi[2] - cells[i].lo[2];
      dt_candidate = transit_fraction*dz/sqrt(wsq[i]);
      cells[i].dt_desired = MIN(dt_candidate,cells[i].dt_desired);
    }

    // cell dt based on transit time using maximum most probable speed
    double vrm_max = sqrt(2.0*update->boltz * temp[i] / min_species_mass);

    dt_candidate = transit_fraction*dx/vrm_max;
    cells[i].dt_desired = MIN(dt_candidate,cells[i].dt_desired);

    dt_candidate = transit_fraction*dy/vrm_max;
    cells[i].dt_desired = MIN(dt_candidate,cells[i].dt_desired);

    if (domain->dimension == 3) {
      dt_candidate = transit_fraction*dz/vrm_max;
      cells[i].dt_desired = MIN(dt_candidate,cells[i].dt_desired);
    }

    dt_sum += cells[i].dt_desired;
  }

  double avg_cell_dt = dt_sum/nglocal;
  double dt_factor = 1000.;
  double dtmin_this_pe = avg_cell_dt/dt_factor;
  double dtmax_this_pe = avg_cell_dt*dt_factor;
  double dtmins_this_pe[2] = {dtmin_this_pe, 1./dtmax_this_pe};

  double dtmins[2];
  MPI_Allreduce(&dtmins_this_pe,&dtmins,2,MPI_DOUBLE,MPI_MIN,world);
  double dtmin = dtmins[0];
  double dtmax = 1.0/dtmins[1];
  grid->dt_global = 2.*dtmin;

  for (int i = 0; i < nglocal; ++i) {
    if (cells[i].dt_desired < dtmin)
      cells[i].dt_desired = dtmin;
    if (cells[i].dt_desired > dtmax)
      cells[i].dt_desired = dtmax;
    if (cells[i].dt_desired < 0.51*grid->dt_global) // AKS look into this logic
      cells[i].dt_desired = 0.51*grid->dt_global;
  }

  // also what happens if dt everywhere should be the average, then are we forcing
  // a very small global dt???   Probably should track a minimum and maximum cell dt
  // and compare to the min and max dt computed with dt_factor.

  // also what happens if a large number of cells have no particles? (ex. inlet flow around a circle)
}

/* ----------------------------------------------------------------------
   reallocate arrays if nglocal has changed
   called by init() and load balancer
------------------------------------------------------------------------- */

void FixDtGlobal::reallocate()
{
  if (grid->nlocal == nglocal) return;

  nglocal = grid->nlocal;

  memory->destroy(lambda);
  memory->destroy(temp);
  memory->destroy(usq);
  memory->destroy(vsq);
  memory->destroy(wsq);
  memory->create(lambda,nglocal,"dt_global:lambda");
  memory->create(temp,nglocal,"dt_global:temp");
  memory->create(usq,nglocal,"dt_global:usq");
  memory->create(vsq,nglocal,"dt_global:vsq");
  memory->create(wsq,nglocal,"dt_global:wsq");
}

/* ----------------------------------------------------------------------
   use grid cell center in tvar variable to generate temperature scale factor
   first plug in cell x,y,z centroid values into txvar,tyvar,tzvar
------------------------------------------------------------------------- */

double FixDtGlobal::temperature_variable(double *x)
{

  if (txstr) input->variable->internal_set(txvar,x[0]);
  if (tystr) input->variable->internal_set(tyvar,x[1]);
  if (tzstr) input->variable->internal_set(tzvar,x[2]);

  double temperature = input->variable->compute_equal(tvar);
  return temperature;
}

/* ----------------------------------------------------------------------
   use cell position in vxvar,vyvar,vzvar variables to generate vel stream
   first plug in cell x,y,z centroid values into vvarx,vvary,vvarz
------------------------------------------------------------------------- */

void FixDtGlobal::velocity_variable(double *x, double *vstream,
                                    double *vstream_variable)
{
  if (vstrx) input->variable->internal_set(vvarx,x[0]);
  if (vstry) input->variable->internal_set(vvary,x[1]);
  if (vstrz) input->variable->internal_set(vvarz,x[2]);

  if (vxstr) vstream_variable[0] = input->variable->compute_equal(vxvar);
  else vstream_variable[0] = vstream[0];
  if (vystr) vstream_variable[1] = input->variable->compute_equal(vyvar);
  else vstream_variable[1] = vstream[1];
  if (vzstr) vstream_variable[2] = input->variable->compute_equal(vzvar);
  else vstream_variable[2] = vstream[2];
}

