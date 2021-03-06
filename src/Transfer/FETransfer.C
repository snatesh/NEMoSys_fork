#include <FETransfer.H>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <AuxiliaryFunctions.H>

using namespace nemAux;

FETransfer::FETransfer(meshBase* _source, meshBase* _target)
{
  source = _source;
  srcCellLocator = source->buildLocator();
  target = _target;
  trgCellLocator = target->buildLocator();
  std::cout << "FETransfer constructed" << std::endl;
}

/* transfers point data with arrayID from source mesh to target
   The algorithm is as follows;
    1) For each point in the target mesh, find the cell of the source
       mesh in which it exists.
        - using a cell locator
        - if cell locator fails, find the nearest neighbor in the source mesh
          and all cells sharing this neighbor point. Check if the target point is 
          in any of these neighboring cells
    2) When the cell is identified, evaluate the weights for interpolation of the 
       solution to the target point and perform the interpolation.
*/
int FETransfer::transferPointData(const std::vector<int>& arrayIDs,
                                  const std::vector<std::string>& newnames)
{

  if (arrayIDs.size() == 0)
  {
    std::cerr << "no arrays selected for interpolation" << std::endl;
    exit(1);
  }
  
  vtkSmartPointer<vtkPointData> pd = source->getDataSet()->GetPointData();
  // clean target data of duplicate names if no newnames specified
  if (newnames.empty())
  {
    int numArr = pd->GetNumberOfArrays();
    for (int i = 0; i < arrayIDs.size(); ++i)
    {
      if (arrayIDs[i] >= numArr)
      {
        std::cout << "ERROR: arrayID is out of bounds" << std::endl;
        std::cout << "There are " << numArr << " point data arrays" << std::endl;
        exit(1);
      }
      target->unsetPointDataArray(pd->GetArrayName(arrayIDs[i]));
    }
  }
  std::vector<vtkSmartPointer<vtkDoubleArray>> dasSource(arrayIDs.size());
  std::vector<vtkSmartPointer<vtkDoubleArray>> dasTarget(arrayIDs.size());

  // initializing arrays storing interpolated data
  for (int id = 0; id < arrayIDs.size(); ++id)
  {
    // get desired point data array from source to be transferred to target
    vtkSmartPointer<vtkDoubleArray> daSource 
      = vtkDoubleArray::SafeDownCast(pd->GetArray(arrayIDs[id]));
    // get tuple length of given data
    int numComponent = daSource->GetNumberOfComponents();
    // declare data array to be populated with values at target points
    vtkSmartPointer<vtkDoubleArray> daTarget = vtkSmartPointer<vtkDoubleArray>::New();
    // names and sizing
    if (newnames.empty())
      daTarget->SetName(pd->GetArrayName(arrayIDs[id]));
    else
      daTarget->SetName(&(newnames[id])[0u]); 
    daTarget->SetNumberOfComponents(numComponent);
    daTarget->SetNumberOfTuples(target->getNumberOfPoints());
    dasSource[id] = daSource;
    dasTarget[id] = daTarget;
  }
  // genCell used by locator
  vtkSmartPointer<vtkGenericCell> genCell = vtkSmartPointer<vtkGenericCell>::New();       
  for (int i = 0; i < target->getNumberOfPoints(); ++i)
  {
    transferPointData(i, genCell, dasSource,dasTarget,0);
  }
  for (int id = 0; id < arrayIDs.size(); ++id)
  { 
    target->getDataSet()->GetPointData()->AddArray(dasTarget[id]);
  }
  if (checkQual)
  {
    std::vector<vtkSmartPointer<vtkDoubleArray>> newDasSource(arrayIDs.size());
    for (int id = 0; id < arrayIDs.size(); ++id) 
    {
      int numComponent = dasTarget[id]->GetNumberOfComponents();
      // declare data array to be populated with values at target points
      vtkSmartPointer<vtkDoubleArray> newDaSource = vtkSmartPointer<vtkDoubleArray>::New();
      newDaSource->SetNumberOfComponents(numComponent);
      newDaSource->SetNumberOfTuples(target->getNumberOfPoints());
      newDasSource[id] = newDaSource;
    }

    for (int i = 0; i < source->getNumberOfPoints(); ++i)
    {
      transferPointData(i, genCell, dasTarget, newDasSource,1);
    }

    for (int id = 0; id < arrayIDs.size(); ++id)
    {
      int numComponent = newDasSource[id]->GetNumberOfComponents();
      double diffsum = 0.0;
      for (int i = 0; i < source->getNumberOfPoints(); ++i)
      {
        double comps_old[numComponent];
        double comps_new[numComponent];
        dasSource[id]->GetTuple(i,comps_old);
        newDasSource[id]->GetTuple(i,comps_new); 
        for (int j = 0; j < numComponent; ++j)
        {
          double diff = std::fabs((comps_new[j]-comps_old[j])/comps_old[j]);
          diffsum += std::isnan(diff) ? 0.0 : diff*diff;
        }
      }
      double rmse = std::sqrt(diffsum/(numComponent*source->getNumberOfPoints()));
      std::cout << "RMS Error in Nodal Transfer: " 
                << (!(std::isnan(rmse) || std::isinf(rmse)) ? rmse : 0)
                << std::endl;
    }
  }
  return 0;
}

int FETransfer::transferPointData(int i, vtkSmartPointer<vtkGenericCell> genCell, 
                                  std::vector<vtkSmartPointer<vtkDoubleArray>> dasSource,
                                  std::vector<vtkSmartPointer<vtkDoubleArray>> dasTarget,
                                  const bool flip)
{
  // getting point from target and setting as query
  double x[3];
  // id of the cell containing source/target mesh point
  vtkIdType id;
  int subId;
  double minDist2; 
  double closestPoint[3];
  if (!flip)
  { 
    target->getDataSet()->GetPoint(i,x);
    // find closest point and closest cell to x
    srcCellLocator->FindClosestPoint(x, closestPoint, genCell,id,subId,minDist2);
  }
  else
  {
    source->getDataSet()->GetPoint(i,x);
    trgCellLocator->FindClosestPoint(x, closestPoint, genCell, id,subId,minDist2);
  }
  if (id >= 0)
  {
    double pcoords[3];
    double tmp[3];
    double weights[genCell->GetNumberOfPoints()];
    int result = genCell->EvaluatePosition(x,tmp,subId,pcoords,minDist2,weights); 
    if (result > 0 || minDist2 < 1e-9)
    {
      for (int id = 0; id < dasSource.size(); ++id)
      {
        int numComponent = dasSource[id]->GetNumberOfComponents();
        double comps[numComponent];
        std::vector<double> interps(numComponent,0.0);
        for (int m = 0; m < genCell->GetNumberOfPoints(); ++m)
        {
          int pntId = genCell->GetPointId(m);
          dasSource[id]->GetTuple(pntId, comps);
          for (int h = 0; h < numComponent; ++h)
          {
            interps[h] += comps[h]*weights[m]; 
          }
        }
        // adding interpolated value to data of cell
        dasTarget[id]->SetTuple(i,interps.data());
      }
    }
    else if (result == 0)
    {
      std::cout << "Could not locate point from target mesh in any cells sharing"
                << " its nearest neighbor in the source mesh" << std::endl;
      exit(1);
    }
    else   
    {
      std::cout << "problem encountered evaluating position of point from target"
                << " mesh with respect to cell in source mesh" << std::endl;
      exit(1);
    }
  }
  else
  {
    std::cout << "Could not locate point from target in source mesh" << std::endl;
    exit(1);
  }
}

/* Transfer cell data from source mesh to target
   The algorithm is as follows:
    1)  Convert the cell data on the source mesh by inverse-distance 
        weighted averaging of data at cells sharing given point
          - cell data is assumed to be perscribed at cell centers
    2)  Compute the centers of cell in the target mesh
    3)  Transfer the converted cell-point data from the source mesh
        to the cell centers of the target mesh using the runPD methods
*/
int FETransfer::transferCellData(const std::vector<int>& arrayIDs,
                                 const std::vector<std::string>& newnames)
{

  if (arrayIDs.size() == 0)
  {
    std::cerr << "no arrays selected for interpolation" << std::endl;
    exit(1);
  }
  
  vtkSmartPointer<vtkCellData> cd = source->getDataSet()->GetCellData();
  // clean target data of duplicate names if no newnames specified
  if (newnames.empty())
  {
    int numArr = cd->GetNumberOfArrays();
    for (int i = 0; i < arrayIDs.size(); ++i)
    {
      if (arrayIDs[i] >= numArr)
      {
        std::cout << "ERROR: arrayID is out of bounds" << std::endl;
        std::cout << "There are " << numArr << " cell data arrays" << std::endl;
        exit(1);
      }
      target->unsetCellDataArray(cd->GetArrayName(arrayIDs[i]));
    }
  }
  std::vector<vtkSmartPointer<vtkDataArray>> dasSource(arrayIDs.size());
  std::vector<vtkSmartPointer<vtkDoubleArray>> dasTarget(arrayIDs.size());
  for (int id = 0; id < arrayIDs.size(); ++id)
  {
    // get desired cell data array from source to be transferred to target
    vtkSmartPointer<vtkDataArray> daSource = cd->GetArray(arrayIDs[id]);
    // get tuple length of given data
    int numComponent = daSource->GetNumberOfComponents();
    // declare data array to be populated with values at target points
    vtkSmartPointer<vtkDoubleArray> daTarget = vtkSmartPointer<vtkDoubleArray>::New();
    // names and sizing
    if (newnames.empty())
      daTarget->SetName(cd->GetArrayName(arrayIDs[id]));
    else
      daTarget->SetName(&(newnames[id])[0u]); 
    daTarget->SetNumberOfComponents(numComponent);
    daTarget->SetNumberOfTuples(target->getNumberOfCells());
    dasSource[id] = daSource;
    dasTarget[id] = daTarget;
  }
  // straightforwrad transfer without weighted averaging by locating target cell in source mesh
  // and assigning cell data
  if (!continuous) 
  {
    vtkSmartPointer<vtkGenericCell> genCell = vtkSmartPointer<vtkGenericCell>::New();
    for (int i = 0; i < target->getNumberOfCells(); ++i)
    {
      std::vector<double> targetCenter = target->getCellCenter(i);
      // id of the cell containing source mesh point
      vtkIdType id;
      int subId;
      double minDist2;
      // find closest point and closest cell to x
      double closestPoint[3];
      double* x = targetCenter.data();
      srcCellLocator->FindClosestPoint(x, closestPoint, genCell,id,subId,minDist2);
      if (id >= 0)
      {
        for (int j = 0; j < dasSource.size(); ++j)
        {
          int numComponent = dasSource[j]->GetNumberOfComponents();
          double comps[numComponent];
          dasSource[j]->GetTuple(id,comps);
          dasTarget[j]->SetTuple(i,comps); 
        }
      }
      else
      {
        std::cout << "Could not locate center of cell " 
                  << i << " from target in source mesh" << std::endl;
        exit(1);
      }
    }
  }

  else // transfer with weighted averaging
  {
    // ---------------------- Convert source cell data to point data -------- // 
    std::vector<vtkSmartPointer<vtkDoubleArray>> dasSourceToPoint(arrayIDs.size());
    
    // cellId container for cells sharing a point
    vtkSmartPointer<vtkIdList> cellIds = vtkSmartPointer<vtkIdList>::New(); 

    // initializing arrays storing interpolated data
    for (int id = 0; id < arrayIDs.size(); ++id)
    {
      // get desired cell data array from source to be transferred to target
      vtkSmartPointer<vtkDataArray> daSource = cd->GetArray(arrayIDs[id]);
      // get tuple length of given data
      int numComponent = daSource->GetNumberOfComponents();
      // initialize cellToPoint source array
      vtkSmartPointer<vtkDoubleArray> daSourceToPoint
        = vtkSmartPointer<vtkDoubleArray>::New(); 
      daSourceToPoint->SetNumberOfComponents(numComponent);
      daSourceToPoint->SetNumberOfTuples(source->getNumberOfPoints());
    
      for (int i = 0; i < source->getNumberOfPoints(); ++i)
      {
        // find cells sharing point i
        source->getDataSet()->GetPointCells(i, cellIds);
        int numSharedCells = cellIds->GetNumberOfIds(); 
        double totW = 0;
        double W;
        // contains averaged/weighted data
        std::vector<double> interps(numComponent,0.0);
        for (int j = 0; j < numSharedCells; ++j)
        {
          int cellId = cellIds->GetId(j);
          double comps[numComponent];
          daSource->GetTuple(cellId, comps);
          // compute distance from point to cell center
          W = 1./l2_Norm(source->getCellCenter(cellId)
                         - source->getPoint(i));
          // average over shared cells, weighted by inverse distance to center
          for (int k = 0; k < numComponent; ++k)
          {
            interps[k] += W*comps[k];
          }
          totW += W; 
        } 
        interps = (1.0/totW)*interps;
        daSourceToPoint->SetTuple(i, interps.data());
      }
      
      dasSourceToPoint[id] = daSourceToPoint;
    }
    // genCell used by locator
    vtkSmartPointer<vtkGenericCell> genCell = vtkSmartPointer<vtkGenericCell>::New();       
    for (int i = 0; i < target->getNumberOfCells(); ++i)
    {
      transferCellData(i, genCell, dasSourceToPoint, dasTarget);
    }
  }
  for (int id = 0; id < arrayIDs.size(); ++id)
  { 
    target->getDataSet()->GetCellData()->AddArray(dasTarget[id]);
  }
  return 0;
}

int FETransfer::transferCellData(int i, vtkSmartPointer<vtkGenericCell> genCell, 
                                 std::vector<vtkSmartPointer<vtkDoubleArray>> dasSourceToPoint,
                                 std::vector<vtkSmartPointer<vtkDoubleArray>> dasTarget)
{
  // getting point from target and setting as query
  std::vector<double> targetCenter = target->getCellCenter(i);
  // id of the cell containing source mesh point
  vtkIdType id;
  int subId;
  double minDist2;
  // find closest point and closest cell to x
  double closestPoint[3];
  double* x = targetCenter.data();
  srcCellLocator->FindClosestPoint(x, closestPoint, genCell,id,subId,minDist2);
  if (id >= 0)
  {
    // passed to evaulate position if called
    double pcoords[3];
    // parameters for interpolation
    int pntId;
    int result;
    double weights[genCell->GetNumberOfPoints()];
    result = genCell->EvaluatePosition(x,NULL,subId,pcoords,minDist2,weights); 
    if (result > 0)
    {
      for (int id = 0; id < dasSourceToPoint.size(); ++id)
      {
        int numComponent = dasSourceToPoint[id]->GetNumberOfComponents();
        double comps[numComponent];
        std::vector<double> interps(numComponent,0.0);
        for (int m = 0; m < genCell->GetNumberOfPoints(); ++m)
        {
          int pntId = genCell->GetPointId(m);
          dasSourceToPoint[id]->GetTuple(pntId, comps);
          for (int h = 0; h < numComponent; ++h)
          {
            interps[h] += comps[h]*weights[m]; 
          }
        }
        // adding interpolated value to data of cell
        dasTarget[id]->SetTuple(i,interps.data());
      }
    }
    else if (result == 0)
    {
      std::cout << "Could not locate point from target mesh in any cells sharing"
                << " its nearest neighbor in the source mesh" << std::endl;
      exit(1);
    }
    else   
    {
      std::cout << "problem encountered evaluating position of point from target"
                << " mesh with respect to cell in source mesh" << std::endl;
      exit(1);
    }
  }
  else
  {
    std::cout << "Could not locate center of cell " 
              << i << " from target in source mesh" << std::endl;
    exit(1);
  }
  return 0;
}

int FETransfer::run(const std::vector<std::string>& newnames)
{
  if (!(source && target))
  {
    std::cout << "source and target meshes must be initialized" << std::endl;
    exit(1);
  }

  // transferring point data
  int numArr = source->getDataSet()->GetPointData()->GetNumberOfArrays();
  if (numArr > 0)
  {
    std::vector<int> arrayIDs(numArr);
    std::cout << "Transferring point arrays: \n";
    for (int i = 0; i < numArr; ++i)
    {
      arrayIDs[i] = i;
      std::cout << "\t" << source->getDataSet()->GetPointData()->GetArrayName(i)
                << std::endl;
    }
    transferPointData(arrayIDs,newnames);
  }
  else
  {
    std::cout << "no point data found" << std::endl;
  }
  

  // transferring cell data
  numArr = source->getDataSet()->GetCellData()->GetNumberOfArrays();
  if (numArr > 0)
  {
    std::vector<int> arrayIDs(numArr);
    std::cout << "Transferring cell arrays: \n";
    for (int i = 0; i < numArr; ++i)
    {
      arrayIDs[i] = i;
      std::cout << "\t" << source->getDataSet()->GetCellData()->GetArrayName(i)
                << std::endl;
    }
    transferCellData(arrayIDs, newnames);
  }
  else
  {
    std::cout << "no cell data found" << std::endl;
  }

  return 0;
}

