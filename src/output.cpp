/*!
 * \file output.cpp
 * \brief Functions for solution restart & visualization data output
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
#include "output.hpp"

#include <iomanip>
#include <string>

// Used for making sub-directories (for MPI and 'time-stamp' files)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef _NO_MPI
#include "mpi.h"
#endif

void writeData(solver *Solver, input *params)
{
  if (params->plotType == 0) {
    writeCSV(Solver,params);
  }
  else if (params->plotType == 1) {
    writeParaview(Solver,params);
    if (params->plotSurfaces)
      writeSurfaces(Solver,params);
  }

  /* Write out mesh in Tecplot format, with IBLANK data [Overset cases only] */
  if (params->meshType==OVERSET_MESH && params->writeIBLANK) {
    writeMeshTecplot(Solver->Geo,params);
  }
}

void writeCSV(solver *Solver, input *params)
{
  ofstream dataFile;
  int iter = params->iter;

  char fileNameC[256];
  string fileName = params->dataFileName;
  sprintf(fileNameC,"%s.csv.%.09d",&fileName[0],iter);

  dataFile.precision(15);
  dataFile.setf(ios_base::fixed);

  dataFile.open(fileNameC);

  // Vector of primitive variables
  vector<double> V;
  // Location of solution point
  point pt;

  // Write header:
  // x  y  z(=0)  rho  [u  v  p]
  dataFile << "x,y,z,";
  if (params->equation == ADVECTION_DIFFUSION) {
    dataFile << "rho" << endl;
  }
  else if (params->equation == NAVIER_STOKES) {
    dataFile << "rho,u,v,p" << endl;
  }

  bool plotFpts = true;

  if (plotFpts)
    Solver->extrapolateU();

  // Solution data
  for (auto& e:Solver->eles) {
    if (params->motion != 0) {
//      e->updatePosSpts();
//      e->updatePosFpts();
//      e->setPpts();
    }
    for (uint spt=0; spt<e->getNSpts(); spt++) {
      V = e->getPrimitives(spt);
      pt = e->getPosSpt(spt);

      for (uint dim=0; dim<e->getNDims(); dim++) {
        dataFile << pt[dim] << ",";
      }
      if (e->getNDims() == 2) dataFile << "0.0,"; // output a 0 for z [2D]

      for (uint i=0; i<e->getNFields()-1; i++) {
        dataFile << V[i] << ",";
      }
      dataFile << V[e->getNFields()-1] << endl;
    }

    if (plotFpts) {
      for (uint fpt=0; fpt<e->getNFpts(); fpt++) {
        V = e->getPrimitivesFpt(fpt);
        pt = e->getPosFpt(fpt);

        for (uint dim=0; dim<e->getNDims(); dim++) {
          dataFile << pt[dim] << ",";
        }
        if (e->getNDims() == 2) dataFile << "0.0,"; // output a 0 for z [2D]

        for (uint i=0; i<e->getNFields()-1; i++) {
          dataFile << V[i] << ",";
        }
        dataFile << V[e->getNFields()-1] << endl;
      }
    }
  }

  dataFile.close();
}

void writeParaview(solver *Solver, input *params)
{
  ofstream dataFile;
  int iter = params->iter;

  char fileNameC[256];
  string fileName = params->dataFileName;

#ifndef _NO_MPI
  /* --- All processors write their solution to their own .vtu file --- */
  if (params->overset || params->meshType == OVERSET_MESH)
    sprintf(fileNameC,"%s_%.09d/%s%d_%.09d_%d.vtu",&fileName[0],iter,
        &fileName[0],Solver->gridID,iter,Solver->gridRank);
  else
    sprintf(fileNameC,"%s_%.09d/%s_%.09d_%d.vtu",&fileName[0],iter,
        &fileName[0],iter,params->rank);
#else
  /* --- Filename to write to --- */
  sprintf(fileNameC,"%s_%.09d.vtu",&fileName[0],iter);
#endif

  char Iter[10];
  sprintf(Iter,"%.09d",iter);

  if (params->rank == 0)
    cout << "Writing ParaView file " << fileName << "_" << string(Iter) << ".vtu...  " << flush;

#ifndef _NO_MPI
  // Get # of eles on each rank to avoid printing completely-blanked ranks (if exist)
  int nEles = Solver->eles.size();
  vector<int> nEles_rank(Solver->nprocPerGrid);
  MPI_Allgather(&nEles,1,MPI_INT,nEles_rank.data(),1,MPI_INT,Solver->Geo->gridComm);

  /* --- Write 'master' .pvtu file (for each grid, if overset) --- */
  if (Solver->gridRank == 0) {
    ofstream pVTU;
    char pvtuC[256];
    if (params->overset || params->meshType == OVERSET_MESH)
      sprintf(pvtuC,"%s%d_%.09d.pvtu",&fileName[0],Solver->gridID,iter);
    else
      sprintf(pvtuC,"%s_%.09d.pvtu",&fileName[0],iter);

    pVTU.open(pvtuC);

    pVTU << "<?xml version=\"1.0\" ?>" << endl;
    pVTU << "<VTKFile type=\"PUnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\" compressor=\"vtkZLibDataCompressor\">" << endl;

    // Use XML / ParaView comments here for simulation time
    pVTU << "<!-- TIME " << params->time << " -->" << endl;
    pVTU << "<!-- ITER " << params->iter << " -->" << endl;

    pVTU << "  <PUnstructuredGrid GhostLevel=\"1\">" << endl;
    // NOTE: Must be careful with order here [particularly of vector data], or else ParaView gets confused
    pVTU << "    <PPointData Scalars=\"Density\" Vectors=\"Velocity\" >" << endl;
    pVTU << "      <PDataArray type=\"Float32\" Name=\"Density\" />" << endl;
    if (params->equation == NAVIER_STOKES) {
      pVTU << "      <PDataArray type=\"Float32\" Name=\"Velocity\" NumberOfComponents=\"3\" />" << endl;
      pVTU << "      <PDataArray type=\"Float32\" Name=\"Pressure\" />" << endl;
      if (params->calcEntropySensor) {
        pVTU << "      <PDataArray type=\"Float32\" Name=\"EntropyErr\" />" << endl;
      }
    }
    if (params->scFlag == 1) {
      pVTU << "      <PDataArray type=\"Float32\" Name=\"Sensor\" />" << endl;
    }
    if (params->motion) {
      pVTU << "      <PDataArray type=\"Float32\" Name=\"GridVelocity\" NumberOfComponents=\"3\" />" << endl;
    }
    if (params->meshType == OVERSET_MESH && params->writeIBLANK) {
      pVTU << "      <PDataArray type=\"Float32\" Name=\"IBLANK\" />" << endl;
    }
    pVTU << "    </PPointData>" << endl;
    pVTU << "    <PPoints>" << endl;
    pVTU << "      <PDataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" />" << endl;
    pVTU << "    </PPoints>" << endl;

    char filnameTmpC[256];
    for (int p=0; p<Solver->nprocPerGrid; p++) {
      if (params->overset || params->meshType == OVERSET_MESH)
        sprintf(filnameTmpC,"%s_%.09d/%s%d_%.09d_%d.vtu",&fileName[0],iter,&fileName[0],Solver->gridID,iter,p);
      else
        sprintf(filnameTmpC,"%s_%.09d/%s_%.09d_%d.vtu",&fileName[0],iter,&fileName[0],iter,p);
      if (nEles_rank[p]>0)
        pVTU << "    <Piece Source=\"" << string(filnameTmpC) << "\" />" << endl;
    }
    pVTU << "  </PUnstructuredGrid>" << endl;
    pVTU << "</VTKFile>" << endl;

    pVTU.close();

    char datadirC[256];
    char *datadir = &datadirC[0];
    sprintf(datadirC,"%s_%.09d",&fileName[0],iter);

    /* --- Master node creates a subdirectory to store .vtu files --- */
    if (params->rank == 0) {
      struct stat st = {0};
      if (stat(datadir, &st) == -1) {
        mkdir(datadir, 0755);
      }
    }
  }

  /* --- Wait for all processes to get here, otherwise there won't be a
   *     directory to put .vtus into --- */
  MPI_Barrier(Solver->myComm);
#endif

  /* --- Move onto the rank-specific data file --- */
  if (Solver->eles.size()==0) return;

  dataFile.open(fileNameC);
  dataFile.precision(16);

  // File header
  dataFile << "<?xml version=\"1.0\" ?>" << endl;
  dataFile << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\" compressor=\"vtkZLibDataCompressor\">" << endl;

  // Write simulation time and iteration number
  dataFile << "<!-- TIME " << params->time << " -->" << endl;
  dataFile << "<!-- ITER " << params->iter << " -->" << endl;

  // Write the cell iblank data for restarting purposes
  if (params->overset || params->meshType == OVERSET_MESH) {
    dataFile << "<!-- IBLANK_CELL ";
    for (int i=0; i<Solver->Geo->nEles; i++) {
      dataFile << Solver->Geo->iblankCell[i] << " ";
    }
    dataFile << " -->" << endl;
  }

  dataFile << "	<UnstructuredGrid>" << endl;

  Solver->extrapolateUPpts();

  if (params->motion)
    Solver->extrapolateGridVelPpts();

  if (params->equation == NAVIER_STOKES) {
    if (params->squeeze) {
      Solver->calcAvgSolution();
      Solver->checkEntropyPlot();
    }

    if (params->calcEntropySensor) {
      Solver->calcEntropyErr_spts();
      Solver->extrapolateSFpts();
      Solver->extrapolateSMpts();
    }
  }

  if (params->motion != 0)
    Solver->updatePosSptsFpts();

  for (auto& e:Solver->eles) {
    if ((params->overset || params->meshType == OVERSET_MESH) && Solver->Geo->iblankCell[e->ID]!=NORMAL) continue;

    // The combination of spts + fpts will be the plot points
    matrix<double> vPpts, gridVelPpts, errPpts;
    vector<point> ppts;
    e->getPrimitivesPlot(vPpts);
    if (params->motion)
      e->getGridVelPlot(gridVelPpts);
    ppts = e->getPpts();

    // Shock Capturing stuff
    double sensor;
    if(params->scFlag == 1) {
      sensor = e->getSensor();
    }

    if (params->equation == NAVIER_STOKES && params->calcEntropySensor)
      e->getEntropyErrPlot(errPpts);

    int nSubCells, nPpts;
    int nPpts1D = e->order+3;
    if (params->nDims == 2) {
      nSubCells = (e->order+2)*(e->order+2);
      nPpts = (e->order+3)*(e->order+3);
    }
    else if (params->nDims == 3) {
      nSubCells = (e->order+2)*(e->order+2)*(e->order+2);
      nPpts = (e->order+3)*(e->order+3)*(e->order+3);
    }
    else
      FatalError("Invalid dimensionality [nDims].");

    // Write cell header
    dataFile << "		<Piece NumberOfPoints=\"" << nPpts << "\" NumberOfCells=\"" << nSubCells << "\">" << endl;

    /* ==== Write out solution to file ==== */

    dataFile << "			<PointData>" << endl;

    /* --- Density --- */
    dataFile << "				<DataArray type=\"Float32\" Name=\"Density\" format=\"ascii\">" << endl;
    for(int k=0; k<nPpts; k++) {
      dataFile << vPpts(k,0) << " ";
    }
    dataFile << endl;
    dataFile << "				</DataArray>" << endl;

    if (params->equation == NAVIER_STOKES) {
      /* --- Velocity --- */
      dataFile << "				<DataArray type=\"Float32\" NumberOfComponents=\"3\" Name=\"Velocity\" format=\"ascii\">" << endl;
      for(int k=0; k<nPpts; k++) {
        dataFile << vPpts(k,1) << " " << vPpts(k,2) << " ";

        // In 2D the z-component of velocity is not stored, but Paraview needs it so write a 0.
        if(params->nDims==2) {
          dataFile << 0.0 << " ";
        }
        else {
          dataFile << vPpts(k,3) << " ";
        }
      }
      dataFile << endl;
      dataFile << "				</DataArray>" << endl;

      /* --- Pressure --- */
      dataFile << "				<DataArray type=\"Float32\" Name=\"Pressure\" format=\"ascii\">" << endl;
      for(int k=0; k<nPpts; k++) {
        dataFile << vPpts(k,params->nDims+1) << " ";
      }
      dataFile << endl;
      dataFile << "				</DataArray>" << endl;

      if (params->calcEntropySensor) {
        /* --- Entropy Error Estimate --- */
        dataFile << "				<DataArray type=\"Float32\" Name=\"EntropyErr\" format=\"ascii\">" << endl;
        for(int k=0; k<nPpts; k++) {
          dataFile << std::abs(errPpts(k)) << " ";
        }
        dataFile << endl;
        dataFile << "				</DataArray>" << endl;
      }
    }

    if(params->scFlag == 1) {
      /* --- Shock Sensor --- */
      dataFile << "				<DataArray type=\"Float32\" Name=\"Sensor\" format=\"ascii\">" << endl;
      for(int k=0; k<nPpts; k++) {
        dataFile << sensor << " ";
      }
      dataFile << endl;
      dataFile << "				</DataArray>" << endl;
    }

    if (params->motion > 0) {
      /* --- Grid Velocity --- */
      dataFile << "				<DataArray type=\"Float32\" NumberOfComponents=\"3\" Name=\"GridVelocity\" format=\"ascii\">" << endl;
      for(int k=0; k<nPpts; k++) {
        // Divide momentum components by density to obtain velocity components
        dataFile << gridVelPpts(k,0) << " " << gridVelPpts(k,1) << " ";

        // In 2D the z-component of velocity is not stored, but Paraview needs it so write a 0.
        if(params->nDims==2) {
          dataFile << 0.0 << " ";
        }
        else {
          dataFile << gridVelPpts(k,2) << " ";
        }
      }
      dataFile << endl;
      dataFile << "				</DataArray>" << endl;
    }

    if (params->meshType == OVERSET_MESH && params->writeIBLANK) {
      /* --- TIOGA iBlank value --- */
      dataFile << "				<DataArray type=\"Float32\" Name=\"IBLANK\" format=\"ascii\">" << endl;

      for(int k=0; k<nPpts; k++) {
        dataFile << Solver->Geo->iblankCell[e->ID] << " ";
      }
      dataFile << endl;
      dataFile << "				</DataArray>" << endl;
    }

    /* --- End of Cell's Solution Data --- */

    dataFile << "			</PointData>" << endl;

    /* ==== Write Out Cell Points & Connectivity==== */

    /* --- Write out the plot point coordinates --- */
    dataFile << "			<Points>" << endl;
    dataFile << "				<DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"ascii\">" << endl;

    // Loop over plot points in element
    for(int k=0; k<nPpts; k++) {
      for(int l=0;l<params->nDims;l++) {
        dataFile << e->pos_ppts(k,l) << " ";
      }

      // If 2D, write a 0 as the z-component
      if(params->nDims == 2) {
        dataFile << "0 ";
      }
    }

    dataFile << endl;
    dataFile << "				</DataArray>" << endl;
    dataFile << "			</Points>" << endl;

    /* --- Write out Cell data: connectivity, offsets, element types --- */
    dataFile << "			<Cells>" << endl;

    /* --- Write connectivity array --- */
    dataFile << "				<DataArray type=\"Int32\" Name=\"connectivity\" format=\"ascii\">" << endl;

    if (params->nDims == 2) {
      for (int j=0; j<nPpts1D-1; j++) {
        for (int i=0; i<nPpts1D-1; i++) {
          dataFile << j*nPpts1D     + i   << " ";
          dataFile << j*nPpts1D     + i+1 << " ";
          dataFile << (j+1)*nPpts1D + i+1 << " ";
          dataFile << (j+1)*nPpts1D + i   << " ";
          dataFile << endl;
        }
      }
    }
    else if (params->nDims == 3) {
      for (int k=0; k<nPpts1D-1; k++) {
        for (int j=0; j<nPpts1D-1; j++) {
          for (int i=0; i<nPpts1D-1; i++) {
            dataFile << i   + nPpts1D*(j   + nPpts1D*k) << " ";
            dataFile << i+1 + nPpts1D*(j   + nPpts1D*k) << " ";
            dataFile << i+1 + nPpts1D*(j+1 + nPpts1D*k) << " ";
            dataFile << i   + nPpts1D*(j+1 + nPpts1D*k) << " ";

            dataFile << i   + nPpts1D*(j   + nPpts1D*(k+1)) << " ";
            dataFile << i+1 + nPpts1D*(j   + nPpts1D*(k+1)) << " ";
            dataFile << i+1 + nPpts1D*(j+1 + nPpts1D*(k+1)) << " ";
            dataFile << i   + nPpts1D*(j+1 + nPpts1D*(k+1)) << " ";

            dataFile << endl;
          }
        }
      }
    }
    dataFile << "				</DataArray>" << endl;

    // Write cell-node offsets
    int nvPerCell;
    if (params->nDims == 2) nvPerCell = 4;
    else                    nvPerCell = 8;
    dataFile << "				<DataArray type=\"Int32\" Name=\"offsets\" format=\"ascii\">" << endl;
    for(int k=0; k<nSubCells; k++){
      dataFile << (k+1)*nvPerCell << " ";
    }
    dataFile << endl;
    dataFile << "				</DataArray>" << endl;

    // Write VTK element type
    // 5 = tri, 9 = quad, 10 = tet, 12 = hex
    int eType;
    if (params->nDims == 2) eType = 9;
    else                    eType = 12;
    dataFile << "				<DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">" << endl;
    for(int k=0; k<nSubCells; k++) {
      dataFile << eType << " ";
    }
    dataFile << endl;
    dataFile << "				</DataArray>" << endl;

    /* --- Write cell and piece footers --- */
    dataFile << "			</Cells>" << endl;
    dataFile << "		</Piece>" << endl;
  }

  /* --- Write footer of file & close --- */
  dataFile << "	</UnstructuredGrid>" << endl;
  dataFile << "</VTKFile>" << endl;

  dataFile.close();

  if (params->rank == 0) cout << "done." <<  endl;
}

void writeSurfaces(solver *Solver, input *params)
{
  ofstream dataFile;
  int iter = params->iter;

  char Iter[12];
  sprintf(Iter,"%.09d",iter);

  string fileName = params->dataFileName;

  geo *Geo = Solver->Geo;

#ifndef _NO_MPI
  /* --- Master rank creates a subdirectory to store .vtu files --- */

  if (params->rank == 0) {
    char datadirC[256];
    char *datadir = &datadirC[0];
    sprintf(datadirC,"%s_%.09d",&fileName[0],iter);

    if (params->rank == 0) {
      struct stat st = {0};
      if (stat(datadir, &st) == -1) {
        mkdir(datadir, 0755);
      }
    }
  }

  /* --- Wait for all processes to get here, otherwise there won't be a
   *     directory to put .vtus into --- */
  MPI_Barrier(Solver->myComm);
#endif

  /* --- Write out each boundary surface into separate .vtu file --- */

  for (int bnd = 0; bnd < Geo->nBounds; bnd++) {
    char fileNameC[256];
    string bndName = Geo->bcNames[bnd];

#ifndef _NO_MPI
    /* --- All processors write their solution to their own .vtu file --- */
    if (params->meshType == OVERSET_MESH || params->overset)
      sprintf(fileNameC,"%s_%.09d/surf_%s_%d_%d.vtu",&fileName[0],iter,&bndName[0],Solver->gridID,Solver->gridRank);
    else
      sprintf(fileNameC,"%s_%.09d/surf_%s_%d.vtu",&fileName[0],iter,&bndName[0],params->rank);
#else
    /* --- Filename to write to --- */
    sprintf(fileNameC,"%s_surf_%s_%.09d.vtu",&fileName[0],&bndName[0],iter);
#endif

    if (params->rank == 0)
      cout << "Writing ParaView surface file " << fileName << "_surf_" << bndName << "_" << string(Iter) << ".vtu...  " << flush;

    /* --- Move onto the rank-specific data file --- */
    if (Solver->eles.size()==0) return;

    dataFile.open(fileNameC);
    dataFile.precision(16);

    // File header
    dataFile << "<?xml version=\"1.0\" ?>" << endl;
    dataFile << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\" compressor=\"vtkZLibDataCompressor\">" << endl;

    // Write simulation time and iteration number
    dataFile << "<!-- TIME " << params->time << " -->" << endl;
    dataFile << "<!-- ITER " << params->iter << " -->" << endl;

    dataFile << "	<UnstructuredGrid>" << endl;

    int nFields = params->nFields;
    int nDims = params->nDims;
    int nPts1D = Solver->order+3;
    int nPtsFace = nPts1D;
    if (nDims==3) nPtsFace *= nPts1D;
    int nSubCells = nPts1D - 1;
    if (nDims==3) nSubCells *= nSubCells;

    matrix<double> vPpts, vFace(nPtsFace,nFields);
    matrix<double> gridVelPpts, gridVelFace;
    matrix<double> errPpts, errFace;
    matrix<double> posPpts(nPtsFace,nDims);

    if (params->motion)
      gridVelFace.setup(nPtsFace,nDims);

    bool EntErrFlag = (params->equation == NAVIER_STOKES && params->calcEntropySensor);
    if (EntErrFlag)
      errFace.setup(nPtsFace,1);

    int nFaces = 0;
    for (int i = 0; i < Geo->bndFaces.size(); i++) {
      if (Geo->bcID[i] != bnd) continue;

      int ff = Geo->bndFaces[i];
      int ic = Geo->f2c(ff,0);
      auto cellFaces = Geo->c2f.getRow(ic);
      int fid = findFirst(cellFaces,ff);

      int ie = Geo->eleMap[ic];
      if (ie < 0) continue;

      nFaces++;

      auto &e = Solver->eles[ie];

      e->getPrimitivesPlot(vPpts);
      if (params->motion)
        e->getGridVelPlot(gridVelPpts);

      // Shock Capturing stuff
      double sensor;
      if(params->scFlag == 1)
        sensor = e->getSensor();

      if (EntErrFlag)
        e->getEntropyErrPlot(errPpts);

      int start = 0;
      int stride = 0;
      if (fid < 4)
      {
        if (nDims == 2)
        {
          switch (fid)
          {
            case 0:
              start = 0;
              stride = 1;
              break;

            case 1:
              start = nPts1D-1;
              stride = nPts1D;
              break;

            case 2:
              start = nPts1D*nPts1D - 1;
              stride = -1;
              break;

            case 3:
              start = 0;
              stride = nPts1D;
              break;
          }
        }
        else
        {
          switch (fid)
          {
            case 0:
              // Zmin
              start = 0;
              stride = 1;
              break;

            case 1:
              // Zmax
              start = nPts1D * nPtsFace - 1;
              stride = -1;
              break;

            case 2:
              // Xmin / Left
              start = 0;
              stride = nPts1D;
              break;

            case 3:
              // Xmax / Right
              start = nPts1D - 1;
              stride = nPts1D;
              break;
          }
        }

        int j2 = start;
        for (int j = 0; j < nPtsFace; j++) {
          for (int k = 0; k < nFields; k++) {
            vFace(j,k) = vPpts(j2,k);
          }
          for (int k = 0; k < nDims; k++) {
            posPpts(j,k) = e->pos_ppts(j2,k);
            if (params->motion)
              gridVelFace(j,k) = gridVelPpts(j2,k);
          }
          if (EntErrFlag)
            errFace(j) = errPpts(j2);
          j2 += stride;
        }
      }
      else
      {
        if (fid == 4)
        {
          // Ymin / Front
          for (int j1 = 0; j1 < nPts1D; j1++) {
            for (int j2 = 0; j2 < nPts1D; j2++) {
              int J  = j2 + j1*nPts1D;
              int J2 = j2 + j1*nPtsFace;
              for (int k = 0; k < nFields; k++) {
                vFace(J,k) = vPpts(J2,k);
              }
              for (int k = 0; k < 3; k++) {
                posPpts(J,k) = e->pos_ppts(J2,k);
              }
              if (EntErrFlag)
                errFace(J) = errPpts(J2);
            }
          }
        }
        else if (fid == 5)
        {
          // Ymax / Back
          for (int j1 = 0; j1 < nPts1D; j1++) {
            for (int j2 = 0; j2 < nPts1D; j2++) {
              int J  = j2 + j1*nPts1D;
              int J2 = j2 + (j1+1)*nPtsFace - nPts1D;
              for (int k = 0; k < nFields; k++) {
                vFace(J,k) = vPpts(J2,k);
              }
              for (int k = 0; k < 3; k++) {
                posPpts(J,k) = e->pos_ppts(J2,k);
                if (params->motion)
                  gridVelFace(J,k) = gridVelPpts(J2,k);
              }
              if (EntErrFlag)
                errFace(J) = errPpts(J2);
            }
          }
        }
        else
          FatalError("Invalid cell-local face ID found.");
      }


      // Write cell header
      dataFile << "		<Piece NumberOfPoints=\"" << nPtsFace << "\" NumberOfCells=\"" << nSubCells << "\">" << endl;

      /* ==== Write out solution to file ==== */

      dataFile << "			<PointData>" << endl;

      /* --- Density --- */
      dataFile << "				<DataArray type=\"Float32\" Name=\"Density\" format=\"ascii\">" << endl;
      for(int k=0; k<nPtsFace; k++) {
        dataFile << vFace(k,0) << " ";
      }
      dataFile << endl;
      dataFile << "				</DataArray>" << endl;

      if (params->equation == NAVIER_STOKES) {
        /* --- Velocity --- */
        dataFile << "				<DataArray type=\"Float32\" NumberOfComponents=\"3\" Name=\"Velocity\" format=\"ascii\">" << endl;
        for(int k=0; k<nPtsFace; k++) {
          dataFile << vFace(k,1) << " " << vFace(k,2) << " ";

          // In 2D the z-component of velocity is not stored, but Paraview needs it so write a 0.
          if(params->nDims==2) {
            dataFile << 0.0 << " ";
          }
          else {
            dataFile << vFace(k,3) << " ";
          }
        }
        dataFile << endl;
        dataFile << "				</DataArray>" << endl;

        /* --- Pressure --- */
        dataFile << "				<DataArray type=\"Float32\" Name=\"Pressure\" format=\"ascii\">" << endl;
        for(int k=0; k<nPtsFace; k++) {
          dataFile << vFace(k,params->nDims+1) << " ";
        }
        dataFile << endl;
        dataFile << "				</DataArray>" << endl;

        if (params->calcEntropySensor) {
          /* --- Entropy Error Estimate --- */
          dataFile << "				<DataArray type=\"Float32\" Name=\"EntropyErr\" format=\"ascii\">" << endl;
          for(int k=0; k<nPtsFace; k++) {
            dataFile << std::abs(errPpts(k)) << " ";
          }
          dataFile << endl;
          dataFile << "				</DataArray>" << endl;
        }
      }

      if(params->scFlag == 1) {
        /* --- Shock Sensor --- */
        dataFile << "				<DataArray type=\"Float32\" Name=\"Sensor\" format=\"ascii\">" << endl;
        for(int k=0; k<nPtsFace; k++) {
          dataFile << sensor << " ";
        }
        dataFile << endl;
        dataFile << "				</DataArray>" << endl;
      }

      if (params->motion > 0) {
        /* --- Grid Velocity --- */
        dataFile << "				<DataArray type=\"Float32\" NumberOfComponents=\"3\" Name=\"GridVelocity\" format=\"ascii\">" << endl;
        for(int k=0; k<nPtsFace; k++) {
          // Divide momentum components by density to obtain velocity components
          dataFile << gridVelFace(k,0) << " " << gridVelFace(k,1) << " ";

          // In 2D the z-component of velocity is not stored, but Paraview needs it so write a 0.
          if(params->nDims==2) {
            dataFile << 0.0 << " ";
          }
          else {
            dataFile << gridVelFace(k,2) << " ";
          }
        }
        dataFile << endl;
        dataFile << "				</DataArray>" << endl;
      }

      if (params->plotPolarCoords) {
        /* --- Polar/Spherical Coordinates (Useful for plotting) --- */
        dataFile << "				<DataArray type=\"Float32\" NumberOfComponents=\"3\" Name=\"PolarCoords\" format=\"ascii\">" << endl;
        for(int k=0; k<nPtsFace; k++) {
          double x = posPpts(k,0);
          double y = posPpts(k,1);
          double z = 0;
          if (nDims==3) z = posPpts(k,2);
          double r = sqrt(x*x+y*y+z*z);
          double theta = std::atan2(y,x);
          double psi = 0;
          if (nDims==3) psi = std::acos(z/r);
          // Divide momentum components by density to obtain velocity components
          dataFile << r << " " << theta << " " << psi << " ";
        }

        dataFile << endl;
        dataFile << "				</DataArray>" << endl;
      }

      /* --- End of Cell's Solution Data --- */

      dataFile << "			</PointData>" << endl;

      /* ==== Write Out Cell Points & Connectivity==== */

      /* --- Write out the plot point coordinates --- */
      dataFile << "			<Points>" << endl;
      dataFile << "				<DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"ascii\">" << endl;

      // Loop over plot points in element
      for(int k=0; k<nPtsFace; k++) {
        for(int l=0; l<params->nDims; l++) {
          dataFile << posPpts(k,l) << " ";
        }

        // If 2D, write a 0 as the z-component
        if(params->nDims == 2) {
          dataFile << "0 ";
        }
      }

      dataFile << endl;
      dataFile << "				</DataArray>" << endl;
      dataFile << "			</Points>" << endl;

      /* --- Write out Cell data: connectivity, offsets, element types --- */
      dataFile << "			<Cells>" << endl;

      /* --- Write connectivity array --- */
      dataFile << "				<DataArray type=\"Int32\" Name=\"connectivity\" format=\"ascii\">" << endl;

      if (params->nDims == 2) {
        for (int j=0; j<nPts1D-1; j++) {
          dataFile << j << " ";
          dataFile << j + 1 << " ";
          dataFile << endl;
        }

      }
      else if (params->nDims == 3) {
        for (int j=0; j<nPts1D-1; j++) {
          for (int i=0; i<nPts1D-1; i++) {
            dataFile << j*nPts1D     + i   << " ";
            dataFile << j*nPts1D     + i+1 << " ";
            dataFile << (j+1)*nPts1D + i+1 << " ";
            dataFile << (j+1)*nPts1D + i   << " ";
            dataFile << endl;
          }
        }
      }
      dataFile << "				</DataArray>" << endl;

      // Write cell-node offsets
      int nvPerCell;
      if (params->nDims == 2) nvPerCell = 2;
      else                    nvPerCell = 4;
      dataFile << "				<DataArray type=\"Int32\" Name=\"offsets\" format=\"ascii\">" << endl;
      for(int k=0; k<nSubCells; k++){
        dataFile << (k+1)*nvPerCell << " ";
      }
      dataFile << endl;
      dataFile << "				</DataArray>" << endl;

      // Write VTK element type
      // 5 = tri, 9 = quad, 10 = tet, 12 = hex
      int eType;
      if (params->nDims == 2) eType = 3;
      else                    eType = 9;
      dataFile << "				<DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">" << endl;
      for(int k=0; k<nSubCells; k++) {
        dataFile << eType << " ";
      }
      dataFile << endl;
      dataFile << "				</DataArray>" << endl;

      /* --- Write cell and piece footers --- */
      dataFile << "			</Cells>" << endl;
      dataFile << "		</Piece>" << endl;
    }

    /* --- Write footer of file & close --- */
    dataFile << "	</UnstructuredGrid>" << endl;
    dataFile << "</VTKFile>" << endl;

    dataFile.close();

    if (params->rank == 0) cout << "done." <<  endl;

#ifndef _NO_MPI
    /* --- Write 'master' .pvtu file (for each grid, if overset) --- */

    // Get # of faces on each rank to avoid printing completely-blanked ranks (if exist)
    vector<int> nFaces_rank(Solver->nprocPerGrid);
    MPI_Allgather(&nFaces,1,MPI_INT,nFaces_rank.data(),1,MPI_INT,Geo->gridComm);

    if (Solver->gridRank == 0) {
      ofstream pVTU;
      char pvtuC[256];
      if (params->meshType == OVERSET_MESH || params->overset)
        sprintf(pvtuC,"%s%d_surf_%s_%.09d.pvtu",&fileName[0],Solver->gridID,&bndName[0],iter);
      else
        sprintf(pvtuC,"%s_surf_%s_%.09d.pvtu",&fileName[0],&bndName[0],iter);

      pVTU.open(pvtuC);

      pVTU << "<?xml version=\"1.0\" ?>" << endl;
      pVTU << "<VTKFile type=\"PUnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\" compressor=\"vtkZLibDataCompressor\">" << endl;

      // Use XML / ParaView comments here for simulation time
      pVTU << "<!-- TIME " << params->time << " -->" << endl;
      pVTU << "<!-- ITER " << params->iter << " -->" << endl;

      pVTU << "  <PUnstructuredGrid GhostLevel=\"1\">" << endl;
      // NOTE: Must be careful with order here [particularly of vector data], or else ParaView gets confused
      pVTU << "    <PPointData Scalars=\"Density\" Vectors=\"Velocity\" >" << endl;
      pVTU << "      <PDataArray type=\"Float32\" Name=\"Density\" />" << endl;
      if (params->equation == NAVIER_STOKES) {
        pVTU << "      <PDataArray type=\"Float32\" Name=\"Velocity\" NumberOfComponents=\"3\" />" << endl;
        pVTU << "      <PDataArray type=\"Float32\" Name=\"Pressure\" />" << endl;
        if (params->calcEntropySensor) {
          pVTU << "      <PDataArray type=\"Float32\" Name=\"EntropyErr\" />" << endl;
        }
      }
      if (params->scFlag == 1) {
        pVTU << "      <PDataArray type=\"Float32\" Name=\"Sensor\" />" << endl;
      }
      if (params->motion) {
        pVTU << "      <PDataArray type=\"Float32\" Name=\"GridVelocity\" NumberOfComponents=\"3\" />" << endl;
      }
      if (params->plotPolarCoords) {
        pVTU << "      <PDataArray type=\"Float32\" Name=\"PolarCoords\" NumberOfComponents=\"3\" />" << endl;
      }
      if (params->meshType == OVERSET_MESH && params->writeIBLANK) {
        pVTU << "      <PDataArray type=\"Float32\" Name=\"IBLANK\" />" << endl;
      }
      pVTU << "    </PPointData>" << endl;
      pVTU << "    <PPoints>" << endl;
      pVTU << "      <PDataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" />" << endl;
      pVTU << "    </PPoints>" << endl;

      char filnameTmpC[256];
      for (int p=0; p<Solver->nprocPerGrid; p++) {
        if (nFaces_rank[p] == 0) continue;
        if (params->meshType == OVERSET_MESH || params->overset)
          sprintf(filnameTmpC,"%s_%.09d/surf_%s_%d_%d.vtu",&fileName[0],iter,&bndName[0],Solver->gridID,p);
        else
          sprintf(filnameTmpC,"%s_%.09d/surf_%s_%d.vtu",&fileName[0],iter,&bndName[0],p);
        if (nFaces_rank[p]>0)
          pVTU << "    <Piece Source=\"" << string(filnameTmpC) << "\" />" << endl;
      }
      pVTU << "  </PUnstructuredGrid>" << endl;
      pVTU << "</VTKFile>" << endl;

      pVTU.close();
    }
#endif
  }
}


void writeResidual(solver *Solver, input *params)
{
  vector<double> res(params->nFields);
  int iter = params->iter;

  if (params->dt < 1e-13)
    FatalError("Instability detected - dt approaching zero!");

  if (params->resType == 3) {
    // Infinity Norm
    for (auto &e:Solver->eles) {
      if ((params->overset || params->meshType == OVERSET_MESH) && Solver->Geo->iblankCell[e->ID]!=NORMAL) continue;
      auto resTmp = e->getNormResidual(params->resType);
      if(checkNaN(resTmp)) {
        cout << "Iter " << params->iter << ", rank " << params->rank << ", ele " << e->ID << ": ";
        auto box = e->getBoundingBox();
        cout << " minPt = " << box[0] << "," << box[1] << "," << box[2] << ", maxPt = " << box[3] << "," << box[4] << "," << box[5] << endl;
        FatalError("NaN Encountered in Solution Residual!");
      }

      for (int i=0; i<params->nFields; i++)
        res[i] = max(res[i],resTmp[i]);
    }
  }
  else if (params->resType == 1 || params->resType == 2) {
    // 1-Norm or 2-Norm
    for (auto &e:Solver->eles) {
      if ((params->overset || params->meshType == OVERSET_MESH) && Solver->Geo->iblankCell[e->ID]!=NORMAL) continue;
      auto resTmp = e->getNormResidual(params->resType);
      if(checkNaN(resTmp)) {
        cout << "Iter " << params->iter << ", rank " << params->rank << ", ele " << e->ID << ": " << flush;
        auto box = e->getBoundingBox();
        cout << " minPt = " << box[0] << "," << box[1] << "," << box[2] << ", maxPt = " << box[3] << "," << box[4] << "," << box[5] << endl;
        FatalError("NaN Encountered in Solution Residual!");
      }

      for (int i=0; i<params->nFields; i++)
        res[i] += resTmp[i];
    }
  }

  vector<double> force(6);
  if (params->equation == NAVIER_STOKES) {
    auto fTmp = Solver->computeWallForce();
#ifndef _NO_MPI
    MPI_Reduce(fTmp.data(), force.data(), 6, MPI_DOUBLE, MPI_SUM, 0, Solver->myComm);
#else
    force = fTmp;
#endif
    for (auto &f:force) f /= (0.5*params->rhoBound*params->Uinf*params->Uinf);

    double alpha = std::atan2(params->vBound,params->uBound);
    fTmp = force;
    force[0] = fTmp[0]*cos(alpha) + fTmp[1]*sin(alpha);  // Rotate to align with freestream
    force[1] = fTmp[1]*cos(alpha) - fTmp[0]*sin(alpha);
    if (params->viscous) {
      force[3] = fTmp[3]*cos(alpha) + fTmp[4]*sin(alpha);
      force[4] = fTmp[4]*cos(alpha) - fTmp[3]*sin(alpha);
    }
  }

#ifndef _NO_MPI
  if (params->nproc > 1) {
    if (params->resType == 3) {
      vector<double> resTmp = res;
      MPI_Reduce(resTmp.data(), res.data(), params->nFields, MPI_DOUBLE, MPI_MAX, 0,Solver->myComm);
    }
    else if (params->resType == 1 || params->resType == 2) {
      vector<double> resTmp = res;
      MPI_Reduce(resTmp.data(), res.data(), params->nFields, MPI_DOUBLE, MPI_SUM, 0,Solver->myComm);
    }
  }
#endif

  if (params->rank == 0) {
    // If taking 2-norm, res is sum squared; take sqrt to complete
    if (params->resType == 2) {
      for (auto& R:res) R = sqrt(R);
    }

    /* --- Print the residual and force coefficients in the terminal --- */

    int colW = 16;
    cout.precision(6);
    cout.setf(ios::scientific, ios::floatfield);
    if (iter==params->initIter+1 || (iter/params->monitorResFreq)%25==0) {
      cout << endl;
      cout << setw(8) << left << "Iter" << "Var  ";
      if (params->equation == ADVECTION_DIFFUSION) {
        cout << setw(colW) << "Residual";
        if (params->dtType != 0)
          cout << setw(colW) << left << "DeltaT";
        cout << endl;
      }else if (params->equation == NAVIER_STOKES) {
        cout << setw(colW) << left << "rho";
        cout << setw(colW) << left << "rhoU";
        cout << setw(colW) << left << "rhoV";
        if (params->nDims == 3)
          cout << setw(colW) << left << "rhoW";
        cout << setw(colW) << left << "rhoE";
        if (params->dtType != 0)
          cout << setw(colW) << left << "deltaT";
        cout << setw(colW) << left << "CD";
        cout << setw(colW) << left << "CL";
        if (params->nDims == 3)
          cout << setw(colW) << left << "CN";
      }
      cout << endl;
    }

    // Print residuals
    cout << setw(8) << left << iter << "Res  ";
    for (int i=0; i<params->nFields; i++) {
      cout << setw(colW) << left << res[i];
    }

    // Print time step (for CFL time-stepping)
    if (params->dtType != 0)
      cout << setw(colW) << left << params->dt;

    // Print wall force coefficients
    if (params->equation == NAVIER_STOKES) {
      for (int dim=0; dim<params->nDims; dim++) {
        cout << setw(colW) << left << force[dim]+force[3+dim];
      }
    }

    cout << endl;

    /* --- Write the residual and force coefficients to the history file --- */

    ofstream histFile;
    string fileName = params->dataFileName + ".hist";
    histFile.open(fileName.c_str(),ofstream::app);

    histFile.precision(5);
    histFile.setf(ios::scientific, ios::floatfield);
    if (iter==params->initIter+1) {
      histFile << setw(8) << left << "Iter";
      histFile << setw(colW) << left << "Flow Time";
      histFile << setw(colW) << left << "Wall Time";
      if (params->equation == ADVECTION_DIFFUSION) {
        histFile << setw(colW) << left << "Residual";
        if (params->timeType == 1) histFile << setw(colW) << left << "DeltaT";
        histFile << endl;
      }else if (params->equation == NAVIER_STOKES) {
        histFile << setw(colW) << left << "rho";
        histFile << setw(colW) << left << "rhoU";
        histFile << setw(colW) << left << "rhoV";
        if (params->nDims == 3)
          histFile << setw(colW) << left << "rhoW";
        histFile << setw(colW) << left << "rhoE";
        if (params->dtType != 0)
          histFile << setw(colW) << left << "deltaT";
        histFile << setw(colW) << left << "CDinv";
        histFile << setw(colW) << left << "CLinv";
        if (params->nDims == 3)
          histFile << setw(colW) << left << "CNinv";
        if (params->viscous) {
          histFile << setw(colW) << left << "CDvis";
          histFile << setw(colW) << left << "CLvis";
          if (params->nDims == 3)
            histFile << setw(colW) << left << "CNvis";
          histFile << setw(colW) << left << "CDtot";
          histFile << setw(colW) << left << "CLtot";
          if (params->nDims == 3)
            histFile << setw(colW) << left << "CNtot";
        }
      }
      histFile << endl;
    }

    // Write residuals
    histFile << setw(8) << left << iter;
    histFile << setw(colW) << left << params->time;
    histFile << setw(colW) << left << params->timer.getElapsedTime();
    for (int i=0; i<params->nFields; i++) {
      histFile << setw(colW) << left << res[i];
    }

    // Write time step (for CFL time-stepping)
    if (params->dtType != 0)
      histFile << setw(colW) << left << params->dt;

    // Write inviscid wall force coefficients
    if (params->equation == NAVIER_STOKES) {
      for (int dim=0; dim<params->nDims; dim++)
        histFile << setw(colW) << left << force[dim];              // Convective force coeffs.
      if (params->viscous) {
        for (int dim=0; dim<params->nDims; dim++)
          histFile << setw(colW) << left << force[3+dim];          // Viscous force coeffs.
        for (int dim=0; dim<params->nDims; dim++)
          histFile << setw(colW) << left << force[dim]+force[3+dim]; // Total force coeffs.
      }
    }

    histFile << endl;
    histFile.close();
  }
}

void writeAllError(solver *Solver, input *params)
{
  if (params->testCase == 1) {
    params->errorNorm = 0;
    if (params->rank == 0)
      cout << "Integrated conservation error:" << endl;
    writeError(Solver,params);

    params->errorNorm = 1;
    if (params->rank == 0)
      cout << "Integral L1 error:" << endl;
    writeError(Solver,params);

    params->errorNorm = 2;
    if (params->rank == 0)
      cout << "Integral L2 error:" << endl;
    writeError(Solver,params);
  }
  else if (params->testCase == 2) {
    /* Calculate mass-flux error (integrate inlet/outlet boudnary fluxes) */

    // Get the latest flux data; need to go through a lot of calcRes, so keep it simple
    Solver->calcResidual(0);

    if (params->rank == 0)
      cout << "Net Mass Flux Through Domain:" << endl;
    writeError(Solver,params);
  }
  else if (params->testCase == 3) {
    /* Calculate total amount of conserved quantities in domain */
    params->errorNorm = 0;
    if (params->rank == 0)
      cout << "Integrated conservative variables:" << endl;
    writeError(Solver,params);
  }
}

void writeError(solver *Solver, input *params)
{
  if (params->testCase == 0) return;

  // For implemented test cases, calculcate the L1/L2 error over the overset domain
  vector<double> err;

  if (params->testCase == 1)
  {
    /* --- Standard error calculation wrt analytical solution --- */

    if (params->meshType == OVERSET_MESH && params->projection)
      err = Solver->integrateErrorOverset();
    else
      err = Solver->integrateError();
  }
  else if (params->testCase == 2)
  {
    /* --- Internal-Flow Test Cases: Calculate Net Mass-Flux Error --- */

    err = Solver->computeMassFlux();
  }

  if (params->rank == 0)
  {
    /* --- Write the error out to the terminal --- */

    int colw = 16;
    cout.precision(6);
    cout.setf(ios::scientific, ios::floatfield);

    cout << setw(8) << left << params->iter << "Err  ";
    for (int i=0; i<err.size(); i++)
      cout << setw(colw) << left << std::abs(err[i]);
    cout << endl;

    /* --- Write the error out to the history file --- */

    ofstream errFile;
    string fileName = params->dataFileName + ".err";
    errFile.open(fileName.c_str(),ofstream::app);

    errFile.precision(5);
    errFile.setf(ios::scientific, ios::floatfield);

    if (params->iter==params->initIter+1) {
      errFile << setw(8) << left << "Iter";
      errFile << setw(colw) << left << "Flow Time";
      errFile << setw(colw) << left << "Wall Time";
      if (params->equation == ADVECTION_DIFFUSION) {
        errFile << setw(colw) << left << "Error" << endl;
      } else if (params->equation == NAVIER_STOKES) {
        errFile << setw(colw) << left << "rho";
        errFile << setw(colw) << left << "rhoU";
        errFile << setw(colw) << left << "rhoV";
        if (params->nDims == 3)
          errFile << setw(colw) << left << "rhoW";
        errFile << setw(colw) << left << "rhoE";
      }
      errFile << endl;
    }

    errFile << setw(8) << left << params->iter;
    errFile << setw(colw) << left << params->time;
    errFile << setw(colw) << left << params->timer.getElapsedTime();
    for (int i=0; i<params->nFields; i++) {
      errFile << setw(colw) << left << std::abs(err[i]);
    }
    errFile << endl;
    errFile.close();
  }
}

void writeMeshTecplot(geo *Geo, input* params)
{
  ofstream dataFile;

  char fileNameC[100];
  string fileName = params->dataFileName;

#ifndef _NO_MPI
  /* --- All processors write their solution to their own .vtu file --- */
  sprintf(fileNameC,"%s/%s_%d_%d.plt",&fileName[0],&fileName[0],params->iter,params->rank);
#else
  /* --- Filename to write to --- */
  sprintf(fileNameC,"%s.plt",&fileName[0]);
#endif

  if (params->rank == 0)
    cout << "Writing Tecplot mesh file " << string(fileNameC) << "...  " << flush;

#ifndef _NO_MPI
  /* --- Write folder for output mesh files --- */
  if (params->rank == 0) {

    char datadirC[100];
    char *datadir = &datadirC[0];
    sprintf(datadirC,"%s",&fileName[0]);

    /* --- Master node creates a subdirectory to store .vtu files --- */
    if (params->rank == 0) {
      struct stat st = {0};
      if (stat(datadir, &st) == -1) {
        mkdir(datadir, 0755);
      }
    }
  }

  /* --- Wait for all processes to get here, otherwise there won't be a
   *     directory to put .vtus into --- */
  MPI_Barrier(MPI_COMM_WORLD);
#endif

  dataFile.open(fileNameC);
  dataFile.precision(16);

  // Count wall-boundary nodes
  int nNodesWall = Geo->iwall.size();
  // Count overset-boundary nodes
  int nNodesOver = Geo->iover.size();

  int gridID = Geo->gridID;

  int nPrism = 0;
  int nNodes = Geo->nVerts;
  int nCells = Geo->nEles;
  int nHex = nCells;
  int nv = (params->nDims==2) ? 4 : 8; // Ignoring edge/inner nodes for high-order elements

  dataFile << "# " << nPrism << " " << nHex << " " << nNodes << " " << nCells << " " << nNodesWall << " " << nNodesOver << endl;
  dataFile << "TITLE = \"" << fileName << "\"" << endl;
  dataFile << "VARIABLES = \"X\", \"Y\", \"Z\", \"bodyTag\", \"IBLANK\", \"IBLANKCELL\"" << endl;
  string ET;
  if (params->nDims==2)
    ET = "QUADRILATERAL";
  else
    ET = "BRICK";
  dataFile << "ZONE T = \"VOL_MIXED\", N=" << nCells*nv << ", E=" << nCells << ", ET=" << ET << ", F=FEPOINT" << endl;

  for (int ic=0; ic<nCells; ic++) {
    for (int j=0; j<nv; j++) {
      dataFile << Geo->xv(Geo->c2v(ic,j),0) << " " << Geo->xv(Geo->c2v(ic,j),1) << " ";
      if (params->nDims == 2)
        dataFile << 0.0;
      else
        dataFile << Geo->xv(Geo->c2v(ic,j),2);

      if (params->meshType == OVERSET_MESH)
        dataFile << " " << gridID << " " << Geo->iblank[Geo->c2v(ic,j)] << " " << Geo->iblankCell[ic] << endl;
      else
        dataFile << " " << gridID << " " << 1 << " " << 1 << endl;
    }
  }

  for (int ic=0; ic<nCells; ic++) {
    for (int j=0; j<nv; j++) {
      dataFile << ic*nv + j + 1 << " ";
    }
    dataFile << endl;
  }

//  // output wall-boundary node IDs
//  for (auto& iv:Geo->iwall)
//    dataFile << iv+1 << endl;

//  // output overset-boundary node IDs
//  for (auto& iv:Geo->iover)
//    dataFile << iv+1 << endl;

  dataFile.close();

  if (params->rank == 0) cout << "done." << endl;
}
