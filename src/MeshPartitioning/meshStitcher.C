#include <meshStitcher.H>
#include <rocstarCgns.H>
#include <meshBase.H>
#include <AuxiliaryFunctions.H>

meshStitcher::meshStitcher(const std::vector<std::string>& _cgFileNames, bool surf)
  : cgFileNames(_cgFileNames), stitchedMesh(nullptr), cgObj(nullptr)
{
  if (cgFileNames.size() > 0)
  {
    if (surf)
      initSurfCgObj();
    else
      initVolCgObj();
  }
}

void meshStitcher::initVolCgObj()
{
  partitions.resize(cgFileNames.size(),nullptr);
  for (int iCg = 0; iCg < cgFileNames.size(); ++iCg)
  {
    partitions[iCg] = std::make_shared<cgnsAnalyzer>(cgFileNames[iCg]);
    partitions[iCg]->loadGrid(1);
  	// defining partition flags
    std::vector<double> slnData(partitions[iCg]->getNElement(),iCg);
    partitions[iCg]
      ->appendSolutionData("partitionOld", slnData, ELEMENTAL, partitions[iCg]->getNElement(),1);
    if (iCg)
      partitions[0]->stitchMesh(partitions[iCg].get(),true); 
  } 
  std::cout << "Meshes stitched successfully! #####################################\n";
  std::cout << "Exporting stitched mesh to VTK format #############################\n";
  std::string newname(cgFileNames[0]);
  std::size_t pos = newname.find_last_of("/");
  newname = newname.substr(pos+1);
  newname = trim_fname(newname, "stitched.vtu");
  stitchedMesh = meshBase::CreateShared(partitions[0]->getVTKMesh(),newname);
  std::cout << "Transferring physical quantities to vtk mesh ######################\n";
  // figure out what is existing on the stitched grid
  int outNData, outNDim;
  std::vector<std::string> slnNameList;
  std::vector<std::string> appSlnNameList;
  partitions[0]->getSolutionDataNames(slnNameList);  
  partitions[0]->getAppendedSolutionDataName(appSlnNameList);
  slnNameList.insert(slnNameList.end(),
                     appSlnNameList.begin(), appSlnNameList.end());
  
  // write all data into vtk file
  for (auto is=slnNameList.begin(); is<slnNameList.end(); is++)
  {
    std::vector<double> physData;
    partitions[0]->getSolutionDataStitched(*is, physData, outNData, outNDim);
    solution_type_t dt = partitions[0]->getSolutionDataObj(*is)->getDataType();
    if (dt == NODAL)  
    {      
      std::cout << "Writing nodal " << *is << std::endl; 
      stitchedMesh->setPointDataArray((*is).c_str(), physData);
    }
    else
    {
      // gs field is 'weird' in irocstar files- we don't write it back
      //if (!(*is).compare("gs"))
      //  continue;
      std::cout << "Writing cell-based " << *is << std::endl;
      stitchedMesh->setCellDataArray((*is).c_str(), physData);
    }
  }
  stitchedMesh->report();
  stitchedMesh->write();
}

void meshStitcher::initSurfCgObj()
{
  cgObj = std::make_shared<rocstarCgns>(cgFileNames);
	// different read for burn files
	if (cgFileNames[0].find("burn") != std::string::npos)
	{
		cgObj->setBurnBool(1);
	}
  cgObj->loadCgSeries();
  cgObj->dummy(); 
  std::cout << "Meshes stitched successfully! #####################################\n";
  std::cout << "Exporting stitched mesh to VTK format #############################\n";
  std::string newname(cgFileNames[0]);
  std::size_t pos = newname.find_last_of("/");
  newname = newname.substr(pos+1);
  newname = trim_fname(newname, "stitched.vtu");
  stitchedMesh = meshBase::CreateShared(cgObj->getVTKMesh(),newname);
  std::cout << "Transferring physical quantities to vtk mesh ######################\n";
  // figure out what exists on the stitched grid
  int outNData, outNDim;
  std::vector<std::string> slnNameList;
  std::vector<std::string> appSlnNameList;
  cgObj->getSolutionDataNames(slnNameList);  
  cgObj->getAppendedSolutionDataName(appSlnNameList);
  slnNameList.insert(slnNameList.end(),
                     appSlnNameList.begin(), appSlnNameList.end());

	// write all data into vtk file
  for (auto is=slnNameList.begin(); is<slnNameList.end(); is++)
  {
    std::vector<double> physData;
    cgObj->getSolutionDataStitched(*is, physData, outNData, outNDim);
    solution_type_t dt = cgObj->getSolutionDataObj(*is)->getDataType();
    if (dt == NODAL)  
    {      
      std::cout << "Writing nodal " << *is << std::endl; 
      stitchedMesh->setPointDataArray((*is).c_str(), physData);
    }
    else
    {
      // gs field is 'weird' in irocstar files- we don't write it back
      //if (!(*is).compare("gs"))
      //  continue;
      std::cout << "Writing cell-based " << *is << std::endl;
      stitchedMesh->setCellDataArray((*is).c_str(), physData);
    }
  }
  stitchedMesh->report();
  stitchedMesh->write();
}

std::shared_ptr<meshBase> meshStitcher::getStitchedMB()
{
  if (stitchedMesh)
    return stitchedMesh;
  else
  {
    std::cerr << "No stitched mesh to return!" << std::endl;
    exit(1);
  }
}

std::shared_ptr<cgnsAnalyzer> meshStitcher::getStitchedCGNS()
{
  if (partitions.size())
    return partitions[0];
  else if (cgObj)
    return cgObj;
  else
  {
    std::cerr << "No stitched mesh to return!" << std::endl;
    exit(1);
  }
}
