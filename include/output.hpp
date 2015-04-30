/*!
 * \file output.hpp
 * \brief Header file for restart & visualization data output functions
 *
 * \author - Jacob Crabill
 *           Aerospace Computing Laboratory (ACL)
 *           Aero/Astro Department. Stanford University
 *
 * \version 0.0.1
 *
 * Flux Reconstruction in C++ (Flurry++) Code
 * Copyright (C) 2014 Jacob Crabill.
 *
 */
#pragma once

#include "global.hpp"
#include "solver.hpp"
#include "ele.hpp"
#include "geo.hpp"

/*! Write solution data to a file. */
void writeData(solver *Solver, input *params);

/*! Compute the residual and print to the screen. */
void writeResidual(solver *Solver, input *params);
