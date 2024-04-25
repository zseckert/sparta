/* ----------------------------------------------------------------------
   SPARTA - Stochastic PArallel Rarefied-gas Time-accurate Analyzer
   http://sparta.sandia.gov
   Steve Plimpton, sjplimp@gmail.com, Michael Gallis, magalli@sandia.gov
   Sandia National Laboratories

   Copyright (2014) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level SPARTA directory.
------------------------------------------------------------------------- */

#ifdef COMPUTE_CLASS

ComputeStyle(telec/grid,ComputeTelecGrid)

#else

#ifndef SPARTA_COMPUTE_TELEC_GRID_H
#define SPARTA_COMPUTE_TELEC_GRID_H

#include "compute.h"

namespace SPARTA_NS {

class ComputeTelecGrid : public Compute {
 public:
  ComputeTelecGrid(class SPARTA *, int, char **);
  virtual ~ComputeTelecGrid();
  virtual void init();
  virtual void compute_per_grid();
  virtual int query_tally_grid(int, double **&, int *&);
  virtual void post_process_grid(int, int, double **, int *, double *, int);
  virtual void reallocate();
  bigint memory_usage();

  double elec_energy(int, double);

 protected:
  int groupbit,imix,ngroup,mixspecies,nspecies;
  int maxstate;               // max elec state for any species

  int ntally;                // total # of columns in tally array
  int nglocal;               // # of owned grid cells
  double **tally;            // array of tally quantities, ncells by ntally

  int *nmap;               // nmap[i] = # of tally values for Ith output column
                           // size = # of outputs (Ngroup)

  int **map;               // map[i][j] = tally columns for Ith output column
                           // size = # of outputs (Ngroup)
                           //   by # of tally columns (2*nmax)
                           // nmax = max # of species in any group

  double *tspecies;          // per-species electronic temps
                             // size = Nspecies

  int *s2t;                  // s2t[i] = first tally column for species I
                             // size = Nspecies

  int index_eelec;         // index into extra particle values for elecmodes
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running SPARTA to see the offending line.

E: Compute grid mixture ID does not exist

Self-explanatory.

E: Number of groups in compute telec/grid mixture has changed

This mixture property cannot be changed after this compute command is
issued.

E: Number of species in compute telec/grid mixture has changed

This mixture property cannot be changed after this compute command is
issued.

*/
