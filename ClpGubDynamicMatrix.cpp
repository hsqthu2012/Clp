// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.


#include <cstdio>

#include "CoinPragma.hpp"
#include "CoinIndexedVector.hpp"
#include "CoinHelperFunctions.hpp"

#include "ClpSimplex.hpp"
#include "ClpFactorization.hpp"
#include "ClpQuadraticObjective.hpp"
#include "ClpNonLinearCost.hpp"
// at end to get min/max!
#include "ClpGubDynamicMatrix.hpp"
#include "ClpMessage.hpp"
//#define CLP_DEBUG
//#define CLP_DEBUG_PRINT
//#############################################################################
// Constructors / Destructor / Assignment
//#############################################################################

//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
ClpGubDynamicMatrix::ClpGubDynamicMatrix () 
  : ClpGubMatrix(),
    objectiveOffset_(0.0),
    startColumn_(NULL),
    row_(NULL),
    element_(NULL),
    cost_(NULL),
    fullStart_(NULL),
    id_(NULL),
    flagged_(NULL),
    whichBound_(NULL),
    lowerColumn_(NULL),
    upperColumn_(NULL),
    lowerSet_(NULL),
    upperSet_(NULL),
    model_(NULL),
    numberGubColumns_(0),
    firstAvailable_(0),
    firstDynamic_(0),
    lastDynamic_(0),
    numberElements_(0)
{
  setType(13);
}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
ClpGubDynamicMatrix::ClpGubDynamicMatrix (const ClpGubDynamicMatrix & rhs) 
: ClpGubMatrix(rhs)
{  
  objectiveOffset_ = rhs.objectiveOffset_;
  numberGubColumns_ = rhs.numberGubColumns_;
  firstAvailable_ = rhs.firstAvailable_;
  firstDynamic_ = rhs.firstDynamic_;
  lastDynamic_ = rhs.lastDynamic_;
  numberElements_ = rhs.numberElements_;
  startColumn_ = ClpCopyOfArray(rhs.startColumn_,numberGubColumns_+1);
  CoinBigIndex numberElements = startColumn_[numberGubColumns_];
  row_ = ClpCopyOfArray(rhs.row_,numberElements);;
  element_ = ClpCopyOfArray(rhs.element_,numberElements);;
  cost_ = ClpCopyOfArray(rhs.cost_,numberGubColumns_);
  fullStart_ = ClpCopyOfArray(rhs.fullStart_,numberSets_+1);
  id_ = ClpCopyOfArray(rhs.id_,lastDynamic_-firstDynamic_);
  lowerColumn_ = ClpCopyOfArray(rhs.lowerColumn_,numberGubColumns_);
  upperColumn_ = ClpCopyOfArray(rhs.upperColumn_,numberGubColumns_);
  lowerSet_ = ClpCopyOfArray(rhs.lowerSet_,numberSets_);
  upperSet_ = ClpCopyOfArray(rhs.upperSet_,numberSets_);
  model_ = rhs.model_;
  // size of flagged array and whichbound array
  int size = (numberGubColumns_+ COINFACTORIZATION_BITS_PER_INT-1)/
    COINFACTORIZATION_BITS_PER_INT;
  flagged_ = ClpCopyOfArray(rhs.flagged_,size);
  whichBound_ = ClpCopyOfArray(rhs.whichBound_,size);
}

/* This is the real constructor*/
ClpGubDynamicMatrix::ClpGubDynamicMatrix(ClpSimplex * model, int numberSets,
					 int numberGubColumns, const int * starts,
					 const double * lower, const double * upper,
					 const CoinBigIndex * startColumn, const int * row,
					 const double * element, const double * cost,
					 const double * lowerColumn, const double * upperColumn,
					 const unsigned char * status)
  : ClpGubMatrix()
{
  objectiveOffset_ = model->objectiveOffset();
  model_ = model;
  numberSets_ = numberSets;
  numberGubColumns_ = numberGubColumns;
  fullStart_ = ClpCopyOfArray(starts,numberSets_+1);
  lower_ = ClpCopyOfArray(lower,numberSets_);
  upper_ = ClpCopyOfArray(upper,numberSets_);
  int numberColumns = model->numberColumns();
  int numberRows = model->numberRows();
  // Number of columns needed
  int numberGubInSmall = numberSets_+numberRows + 2*model->factorizationFrequency()+2;
  // for small problems this could be too big
  //numberGubInSmall = min(numberGubInSmall,numberGubColumns_);
  int numberNeeded = numberGubInSmall + numberColumns;
  firstAvailable_ = numberColumns;
  firstDynamic_ = numberColumns;
  lastDynamic_ = numberNeeded;
  startColumn_ = ClpCopyOfArray(startColumn,numberGubColumns_+1);
  CoinBigIndex numberElements = startColumn_[numberGubColumns_];
  row_ = ClpCopyOfArray(row,numberElements);
  element_ = new float[numberElements];
  CoinBigIndex i;
  for (i=0;i<numberElements;i++)
    element_[i]=element[i];
  cost_ = new float[numberGubColumns_];
  for (i=0;i<numberGubColumns_;i++) {
    cost_[i]=cost[i];
    // need sorted
    CoinSort_2(row_+startColumn_[i],row_+startColumn_[i+1],element_+startColumn_[i]);
  }
  if (lowerColumn) {
    lowerColumn_ = new float[numberGubColumns_];
    for (i=0;i<numberGubColumns_;i++) 
      lowerColumn_[i]=lowerColumn[i];
  } else {
    lowerColumn_=NULL;
  }
  if (upperColumn) {
    upperColumn_ = new float[numberGubColumns_];
    for (i=0;i<numberGubColumns_;i++) 
      upperColumn_[i]=upperColumn[i];
  } else {
    upperColumn_=NULL;
  }
  if (upperColumn||lowerColumn) {
    int size = (numberGubColumns_+ COINFACTORIZATION_BITS_PER_INT-1)/
      COINFACTORIZATION_BITS_PER_INT;
    whichBound_ = new unsigned int [size];
    memset(whichBound_,0,size*sizeof(unsigned int));
    lowerSet_ = new float[numberSets_];
    for (i=0;i<numberSets_;i++) {
      if (lower[i]>-1.0e20)
	lowerSet_[i]=lower[i];
      else
	lowerSet_[i] = -1.0e30;
    }
    upperSet_ = new float[numberSets_];
    for (i=0;i<numberSets_;i++) {
      if (upper[i]<1.0e20)
	upperSet_[i]=upper[i];
      else
	upperSet_[i] = 1.0e30;
    }
  } else {
    whichBound_=NULL;
    lowerSet_=NULL;
    upperSet_=NULL;
  }
  start_=NULL;
  end_=NULL;
  flagged_=NULL;
  id_ = new int[numberGubInSmall]; 
  for (i=0;i<numberGubInSmall;i++)
    id_[i]=-1;
  ClpPackedMatrix* originalMatrixA =
    dynamic_cast< ClpPackedMatrix*>(model->clpMatrix());
  assert (originalMatrixA);
  CoinPackedMatrix * originalMatrix = originalMatrixA->getPackedMatrix();
  // guess how much space needed
  double guess = numberElements;
  guess /= (double) numberColumns;
  guess *= 2*numberGubColumns_;
  numberElements_ = (int) guess;
  numberElements_ = min(numberElements_,numberElements)+originalMatrix->getNumElements();
  matrix_ = originalMatrix;
  zeroElements_ = false;
  // resize model (matrix stays same)
  model->resize(numberRows,numberNeeded);
  // resize matrix
  // extra 1 is so can keep number of elements handy
  originalMatrix->reserve(numberNeeded,numberElements_,true);
  originalMatrix->reserve(numberNeeded+1,numberElements_,false);
  originalMatrix->getMutableVectorStarts()[numberColumns]=originalMatrix->getNumElements();
  // redo number of columns
  numberColumns = matrix_->getNumCols();
  backward_ = new int[numberNeeded];
  backToPivotRow_ = new int[numberNeeded];
  changeCost_ = new double [numberRows];
  keyVariable_ = new int[numberSets_];
  // signal to need new ordering
  next_ = NULL;
  for (int iColumn=0;iColumn<numberNeeded;iColumn++) 
    backward_[iColumn]=-1;

  firstGub_=firstDynamic_;
  lastGub_=lastDynamic_;
  if (!whichBound_)
    gubType_=8;
  if (status) {
    status_ = ClpCopyOfArray(status,numberSets_);
  } else {
    status_= new unsigned char [numberSets_];
    memset(status_,0,numberSets_);
    int i;
    for (i=0;i<numberSets_;i++) {
      // make slack key
      setStatus(i,ClpSimplex::basic);
    }
  }
  saveStatus_= new unsigned char [numberSets_];
  memset(saveStatus_,0,numberSets_);
}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
ClpGubDynamicMatrix::~ClpGubDynamicMatrix ()
{
  delete [] startColumn_;
  delete [] row_;
  delete [] element_;
  delete [] cost_;
  delete [] fullStart_;
  delete [] id_;
  delete [] flagged_;
  delete [] whichBound_;
  delete [] lowerColumn_;
  delete [] upperColumn_;
  delete [] lowerSet_;
  delete [] upperSet_;
}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
ClpGubDynamicMatrix &
ClpGubDynamicMatrix::operator=(const ClpGubDynamicMatrix& rhs)
{
  if (this != &rhs) {
    ClpGubMatrix::operator=(rhs);
    delete [] startColumn_;
    delete [] row_;
    delete [] element_;
    delete [] cost_;
    delete [] fullStart_;
    delete [] id_;
    delete [] flagged_;
    delete [] whichBound_;
    delete [] lowerColumn_;
    delete [] upperColumn_;
    delete [] lowerSet_;
    delete [] upperSet_;
    objectiveOffset_ = rhs.objectiveOffset_;
    numberGubColumns_ = rhs.numberGubColumns_;
    firstAvailable_ = rhs.firstAvailable_;
    firstDynamic_ = rhs.firstDynamic_;
    lastDynamic_ = rhs.lastDynamic_;
    numberElements_ = rhs.numberElements_;
    startColumn_ = ClpCopyOfArray(rhs.startColumn_,numberGubColumns_+1);
    int numberElements = startColumn_[numberGubColumns_];
    row_ = ClpCopyOfArray(rhs.row_,numberElements);;
    element_ = ClpCopyOfArray(rhs.element_,numberElements);;
    cost_ = ClpCopyOfArray(rhs.cost_,numberGubColumns_);
    fullStart_ = ClpCopyOfArray(rhs.fullStart_,numberSets_+1);
    id_ = ClpCopyOfArray(rhs.id_,lastDynamic_-firstDynamic_);
    lowerColumn_ = ClpCopyOfArray(rhs.lowerColumn_,numberGubColumns_);
    upperColumn_ = ClpCopyOfArray(rhs.upperColumn_,numberGubColumns_);
    lowerSet_ = ClpCopyOfArray(rhs.lowerSet_,numberSets_);
    upperSet_ = ClpCopyOfArray(rhs.upperSet_,numberSets_);
    model_ = rhs.model_;
    // size of flagged and whichbound array
    int size = (numberGubColumns_+ COINFACTORIZATION_BITS_PER_INT-1)/
      COINFACTORIZATION_BITS_PER_INT;
    flagged_ = ClpCopyOfArray(rhs.flagged_,size);
    whichBound_ = ClpCopyOfArray(rhs.whichBound_,size);
  }
  return *this;
}
//-------------------------------------------------------------------
// Clone
//-------------------------------------------------------------------
ClpMatrixBase * ClpGubDynamicMatrix::clone() const
{
  return new ClpGubDynamicMatrix(*this);
}
// Partial pricing 
void 
ClpGubDynamicMatrix::partialPricing(ClpSimplex * model, int start, int end,
			      int & bestSequence, int & numberWanted)
{
  assert(!model->rowScale());
  if (numberSets_) {
    // Do packed part before gub
    // always???
    ClpPackedMatrix::partialPricing(model,0,firstDynamic_,bestSequence,numberWanted);
  } else {
    // no gub
    ClpPackedMatrix::partialPricing(model,start,end,bestSequence,numberWanted);
  }
  if (numberWanted>0) {
    // and do some proportion of full set
    double ratio = ((double) numberSets_)/((double) lastDynamic_-firstDynamic_);
    int startG = max (start,firstDynamic_);
    int endG = min(lastDynamic_,end);
    int startG2 = (int) (ratio* (startG-firstDynamic_));
    int endG2 = ((int) (startG2 + ratio *(endG-startG)))+1;
    endG2 = min(endG2,numberSets_);
    double tolerance=model->currentDualTolerance();
    double * reducedCost = model->djRegion();
    const double * duals = model->dualRowSolution();
    const double * cost = model->costRegion();
    double bestDj;
    int numberRows = model->numberRows();
    int numberColumns = lastDynamic_;
    int saveWanted=numberWanted;
    if (bestSequence>=0)
      bestDj = fabs(reducedCost[bestSequence]);
    else
      bestDj=tolerance;
    int saveSequence = bestSequence;
    double djMod=0.0;
    double infeasibilityCost = model->infeasibilityCost();
    double bestDjMod=0.0;
    //printf("iteration %d start %d end %d - wanted %d\n",model->numberIterations(),
    //     startG2,endG2,numberWanted);
    int bestType=-1;
    int bestSet=-1;
    const double * element =matrix_->getElements();
    const int * row = matrix_->getIndices();
    const CoinBigIndex * startColumn = matrix_->getVectorStarts();
    const int * length = matrix_->getVectorLengths();
    for (int iSet = startG2;iSet<endG2;iSet++) {
      if (numberWanted+5<saveWanted&&iSet>startG2+5) {
	// give up
	numberWanted=0;
	break;
      }
      CoinBigIndex j;
      int iBasic = keyVariable_[iSet];
      if (iBasic>=numberColumns) {
	djMod = - weight(iSet)*infeasibilityCost;
      } else {
	// get dj without 
	assert (model->getStatus(iBasic)==ClpSimplex::basic);
	djMod=0.0;
	
	for (j=startColumn[iBasic];
	     j<startColumn[iBasic]+length[iBasic];j++) {
	  int jRow=row[j];
	  djMod -= duals[jRow]*element[j];
	}
	djMod += cost[iBasic];
	// See if gub slack possible - dj is djMod
	if (getStatus(iSet)==ClpSimplex::atLowerBound) {
	  double value = -djMod;
	  if (value>tolerance) {
	    numberWanted--;
	    if (value>bestDj) {
	      // check flagged variable and correct dj
	      if (!flagged(iSet)) {
		bestDj=value;
		bestSequence = numberRows+numberColumns+iSet;
		bestDjMod = djMod;
		bestType=0;
		bestSet = iSet;
	      } else {
		// just to make sure we don't exit before got something
		numberWanted++;
		abort();
	      }
	    }
	  }
	} else if (getStatus(iSet)==ClpSimplex::atUpperBound) {
	  double value = djMod;
	  if (value>tolerance) {
	    numberWanted--;
	    if (value>bestDj) {
	      // check flagged variable and correct dj
	      if (!flagged(iSet)) {
		bestDj=value;
		bestSequence = numberRows+numberColumns+iSet;
		bestDjMod = djMod;
		bestType=0;
		bestSet = iSet;
	      } else {
		// just to make sure we don't exit before got something
		numberWanted++;
		abort();
	      }
	    }
	  }
	}
      }
      for (int iSequence=fullStart_[iSet];iSequence<fullStart_[iSet+1];iSequence++) {
	double value=cost_[iSequence]-djMod;
	for (j=startColumn_[iSequence];
	     j<startColumn_[iSequence+1];j++) {
	  int jRow=row_[j];
	  value -= duals[jRow]*element_[j];
	}
	// change sign if at lower bound
	if (!whichBound_||atLowerBound(iSequence))
	  value = -value;
	if (value>tolerance) {
	  numberWanted--;
	  if (value>bestDj) {
	    // check flagged variable and correct dj
	    if (!flagged(iSequence)) {
	      bestDj=value;
	      bestSequence = iSequence;
	      bestDjMod = djMod;
	      bestType=1;
	      bestSet = iSet;
	    } else {
	      // just to make sure we don't exit before got something
	      numberWanted++;
	    }
	  }
	}
      }
      if (numberWanted<=0) {
	numberWanted=0;
	break;
      }
    }
    if (bestSequence!=saveSequence||bestType>=0) {
      if (bestType>0) {
	// recompute dj and create
	double value=cost_[bestSequence]-bestDjMod;
	for (CoinBigIndex j=startColumn_[bestSequence];
	     j<startColumn_[bestSequence+1];j++) {
	  int jRow=row_[j];
	  value -= duals[jRow]*element_[j];
	}
	double * element =  matrix_->getMutableElements();
	int * row = matrix_->getMutableIndices();
	CoinBigIndex * startColumn = matrix_->getMutableVectorStarts();
	int * length = matrix_->getMutableVectorLengths();
	CoinBigIndex numberElements = startColumn[firstAvailable_];
	int numberThis = startColumn_[bestSequence+1]-startColumn_[bestSequence];
	if (numberElements+numberThis>numberElements_) {
	  // need to redo
	  numberElements_ = (3*numberElements_/2);
	  matrix_->reserve(numberColumns,numberElements_);
	  element =  matrix_->getMutableElements();
	  row = matrix_->getMutableIndices();
	  // these probably okay but be safe
	  startColumn = matrix_->getMutableVectorStarts();
	  length = matrix_->getMutableVectorLengths();
	}
	// already set startColumn[firstAvailable_]=numberElements;
	length[firstAvailable_]=numberThis;
	model->costRegion()[firstAvailable_]=cost_[bestSequence];
	CoinBigIndex base = startColumn_[bestSequence];
	for (int j=0;j<numberThis;j++) {
	  row[numberElements]=row_[base+j];
	  element[numberElements++]=element_[base+j];
	}
	id_[firstAvailable_-firstDynamic_]=bestSequence;
	backward_[firstAvailable_]=bestSet;
	model->nonLinearCost()->setOne(firstAvailable_,0.0,0.0,COIN_DBL_MAX,cost_[bestSequence]);
	model->solutionRegion()[firstAvailable_]=0.0;
	if (!whichBound_) {
	  model->setStatus(firstAvailable_,ClpSimplex::atLowerBound);
	}  else {
	  double * solution = model->solutionRegion();
	  if (lowerColumn_) 
	    model->lowerRegion()[firstAvailable_] = lowerColumn_[bestSequence];
	  if (upperColumn_)
	    model->upperRegion()[firstAvailable_] = upperColumn_[bestSequence];
	  if (atLowerBound(bestSequence)) {
	    solution[firstAvailable_]=model->lowerRegion()[firstAvailable_];
	    model->setStatus(firstAvailable_,ClpSimplex::atLowerBound);
	  } else {
	    solution[firstAvailable_]=model->upperRegion()[firstAvailable_];
	    model->setStatus(firstAvailable_,ClpSimplex::atUpperBound);
	  }
	  if (lowerSet_[bestSet]>-1.0e20)
	    lower_[bestSet] += solution[firstAvailable_];
	  if (upperSet_[bestSet]<1.0e20)
	    upper_[bestSet] += solution[firstAvailable_];
	  model->setObjectiveOffset(model->objectiveOffset()+
				    solution[firstAvailable_]*cost_[bestSequence]);
	}
	bestSequence=firstAvailable_;
	// firstAvaialble_ only updated if good pivot (in updatePivot)
	startColumn[firstAvailable_+1]=numberElements;
	//printf("price struct %d - dj %g gubpi %g\n",bestSequence,value,bestDjMod);
	reducedCost[bestSequence] = value;
	gubSlackIn_=-1;
      } else {
	// slack - make last column
	gubSlackIn_= bestSequence - numberRows - numberColumns;
	bestSequence = numberColumns + 2*numberRows;
	reducedCost[bestSequence] = bestDjMod;
	//printf("price slack %d - gubpi %g\n",gubSlackIn_,bestDjMod);
	model->setStatus(bestSequence,getStatus(gubSlackIn_));
	if (getStatus(gubSlackIn_)==ClpSimplex::atUpperBound)
	  model->solutionRegion()[bestSequence] = upper_[gubSlackIn_];
	else
	  model->solutionRegion()[bestSequence] = lower_[gubSlackIn_];
	model->lowerRegion()[bestSequence] = lower_[gubSlackIn_];
	model->upperRegion()[bestSequence] = upper_[gubSlackIn_];
	model->costRegion()[bestSequence] = 0.0;
      }
    }
  }
}
// This is local to Gub to allow synchronization when status is good
int 
ClpGubDynamicMatrix::synchronize(ClpSimplex * model,int mode)
{
  int returnNumber=0;
  switch (mode) {
  case 0:
    {
#ifdef CLP_DEBUG
      {
	for (int i=0;i<numberSets_;i++)
	  assert(toIndex_[i]==-1);
      }
#endif
      // lookup array
      int * lookup = new int[lastDynamic_];
      int iColumn;
      int numberColumns = model->numberColumns();
      double * element =  matrix_->getMutableElements();
      int * row = matrix_->getMutableIndices();
      CoinBigIndex * startColumn = matrix_->getMutableVectorStarts();
      int * length = matrix_->getMutableVectorLengths();
      double * cost = model->costRegion();
      double * lowerColumn = model->lowerRegion();
      double * upperColumn = model->upperRegion();
      int * pivotVariable = model->pivotVariable();
      CoinBigIndex numberElements=startColumn[firstDynamic_];
      // first just do lookup and basic stuff
      int currentNumber = firstAvailable_;
      firstAvailable_ = firstDynamic_;
      int numberToDo=0;
      double objectiveChange=0.0;
      double * solution = model->solutionRegion();
      for (iColumn=firstDynamic_;iColumn<currentNumber;iColumn++) {
	int iSet = backward_[iColumn];
	if (toIndex_[iSet]<0) {
	  toIndex_[iSet]=0;
	  fromIndex_[numberToDo++]=iSet;
	}
	if (model->getStatus(iColumn)==ClpSimplex::basic||iColumn==keyVariable_[iSet]) {
	  lookup[iColumn]=firstAvailable_;
	  if (iColumn!=keyVariable_[iSet]) {
	    int iPivot = backToPivotRow_[iColumn];
	    backToPivotRow_[firstAvailable_]=iPivot;
	    pivotVariable[iPivot]=firstAvailable_;
	  }
	  firstAvailable_++;
	} else {
	  if (whichBound_) {
	    objectiveChange += cost[iColumn]*solution[iColumn];
	    // redo lower and upper on sets
	    double shift=solution[iColumn];
	    if (lowerSet_[iSet]>-1.0e20)
	      lower_[iSet] = lowerSet_[iSet]-shift;
	    if (upperSet_[iSet]<1.0e20)
	      upper_[iSet] = upperSet_[iSet]-shift;
	  }
	  lookup[iColumn]=-1;
	}
      }
      model->setObjectiveOffset(model->objectiveOffset()-objectiveChange);
      firstAvailable_ = firstDynamic_;
      for (iColumn=firstDynamic_;iColumn<currentNumber;iColumn++) {
	if (lookup[iColumn]>=0) {
	  // move
	  int jColumn = id_[iColumn-firstDynamic_];
	  id_[firstAvailable_-firstDynamic_] = jColumn;
	  int numberThis = startColumn_[jColumn+1]-startColumn_[jColumn];
	  length[firstAvailable_]=numberThis;
	  cost[firstAvailable_]=cost[iColumn];
	  lowerColumn[firstAvailable_]=lowerColumn[iColumn];
	  upperColumn[firstAvailable_]=upperColumn[iColumn];
	  model->nonLinearCost()->setOne(firstAvailable_,solution[iColumn],0.0,COIN_DBL_MAX,
					 cost_[jColumn]);
	  CoinBigIndex base = startColumn_[jColumn];
	  for (int j=0;j<numberThis;j++) {
	    row[numberElements]=row_[base+j];
	    element[numberElements++]=element_[base+j];
	  }
	  model->setStatus(firstAvailable_,model->getStatus(iColumn));
	  backward_[firstAvailable_] = backward_[iColumn];
	  solution[firstAvailable_] = solution[iColumn];
	  firstAvailable_++;
	  startColumn[firstAvailable_]=numberElements;
	}
      }
      // clean up next_
      int * temp = new int [firstAvailable_];
      for (int jSet=0;jSet<numberToDo;jSet++) {
	int iSet = fromIndex_[jSet];
	toIndex_[iSet]=-1;
	int last = keyVariable_[iSet];
	int j = next_[last];
	bool setTemp=true;
	if (last<lastDynamic_) {
	  last = lookup[last];
	  assert (last>=0);
	  keyVariable_[iSet]=last;
	} else if (j>=0) {
	  int newJ = lookup[j];
	  assert (newJ>=0);
	  j=next_[j];
	  next_[last]=newJ;
	  last = newJ;
	} else {
	  next_[last] = -(iSet+numberColumns+1);
	  setTemp=false;
	}
	while (j>=0) {
	  int newJ = lookup[j];
	  assert (newJ>=0);
	  temp[last]=newJ;
	  last = newJ;
	  j=next_[j];
	}
	if (setTemp)
	  temp[last]=-(keyVariable_[iSet]+1);
      }
      // move to next_
      memcpy(next_+firstDynamic_,temp+firstDynamic_,(firstAvailable_-firstDynamic_)*sizeof(int));
      // zero solution
      CoinZeroN(solution+firstAvailable_,currentNumber-firstAvailable_);
      // zero cost
      CoinZeroN(cost+firstAvailable_,currentNumber-firstAvailable_);
      // zero lengths
      CoinZeroN(length+firstAvailable_,currentNumber-firstAvailable_);
      for ( iColumn=firstAvailable_;iColumn<currentNumber;iColumn++) {
	model->nonLinearCost()->setOne(iColumn,0.0,0.0,COIN_DBL_MAX,0.0);
	model->setStatus(iColumn,ClpSimplex::atLowerBound);
      }
      delete [] lookup;
      delete [] temp;
      // make sure fromIndex clean
      fromIndex_[0]=-1;
      //#define CLP_DEBUG
#ifdef CLP_DEBUG
      // debug
      {
	int i;
	int numberRows=model->numberRows();
	char * xxxx = new char[numberColumns];
	memset(xxxx,0,numberColumns);
	for (i=0;i<numberRows;i++) {
	  int iPivot = pivotVariable[i];
	  assert (model->getStatus(iPivot)==ClpSimplex::basic);
	  if (iPivot<numberColumns&&backward_[iPivot]>=0)
	    xxxx[iPivot]=1;
	}
	for (i=0;i<numberSets_;i++) {
	  int key=keyVariable_[i];
	  int iColumn =next_[key];
	  int k=0;
	  while(iColumn>=0) {
	    k++;
	    assert (k<100);
	    assert (backward_[iColumn]==i);
	    iColumn=next_[iColumn];
	  }
	  int stop = -(key+1);
	  while (iColumn!=stop) {
	    assert (iColumn<0);
	    iColumn = -iColumn-1;
	    k++;
	    assert (k<100);
	    assert (backward_[iColumn]==i);
	    iColumn=next_[iColumn];
	  }
	  iColumn =next_[key];
	  while (iColumn>=0) {
	    assert (xxxx[iColumn]);
	    xxxx[iColumn]=0;
	    iColumn=next_[iColumn];
	  }
	}
	for (i=0;i<numberColumns;i++) {
	  if (i<numberColumns&&backward_[i]>=0) {
	    assert (!xxxx[i]||i==keyVariable_[backward_[i]]);
	  }
	}
	delete [] xxxx;
      }
      {
	for (int i=0;i<numberSets_;i++)
	  assert(toIndex_[i]==-1);
      }
#endif
    }
    break;
    // flag a variable
  case 1:
    {
      // id will be sitting at firstAvailable
      int sequence = id_[firstAvailable_-firstDynamic_];
      setFlagged(sequence);
      model->clearFlagged(firstAvailable_);
    }
    break;
    // unflag all variables
  case 2:
    {
      for (int i=0;i<numberGubColumns_;i++) {
	if (flagged(i)) {
	  unsetFlagged(i);
	  returnNumber++;
	}
      }
    }
    break;
    //  just reset costs (primal)
  case 3:
    {
      double * cost = model->costRegion();
      double * solution = model->solutionRegion();
      for (int i=firstDynamic_;i<firstAvailable_;i++) {
	int jColumn = id_[i-firstDynamic_];
	cost[i]=cost_[jColumn];
	model->nonLinearCost()->setOne(i,solution[i],0.0,COIN_DBL_MAX,cost_[jColumn]);
      }
    }
    break;
    // and get statistics for column generation
  case 4:
    {
      // In theory we should subtract out ones we have done but ....
      // If key slack then dual 0.0
      // If not then slack could be dual infeasible
      // dj for key is zero so that defines dual on set
      int i;
      int numberColumns = model->numberColumns();
      double * dual = model->dualRowSolution();
      double infeasibilityCost = model->infeasibilityCost();
      double dualTolerance = model->dualTolerance();
      double relaxedTolerance=dualTolerance;
      // we can't really trust infeasibilities if there is dual error
      double error = min(1.0e-3,model->largestDualError());
      // allow tolerance at least slightly bigger than standard
      relaxedTolerance = relaxedTolerance +  error;
      // but we will be using difference
      relaxedTolerance -= dualTolerance;
      for (i=0;i<numberSets_;i++) {
	int kColumn = keyVariable_[i];
	double value=0.0;
	if (kColumn<numberColumns) {
	  // dj without set
	  value = cost_[kColumn];
	  for (CoinBigIndex j=startColumn_[kColumn];
	       j<startColumn_[kColumn+1];j++) {
	    int iRow = row_[j];
	    value -= dual[iRow]*element_[j];
	  }
	} else {
	  // slack key - may not be feasible
	  assert (getStatus(i)==ClpSimplex::basic);
	  // negative as -1.0 for slack
	  value=-weight(i)*infeasibilityCost;
	}
	// Now subtract out from all 
	for (int k= fullStart_[i];k<fullStart_[i+1];k++) {
	  double djValue = cost_[k]-value;
	  for (CoinBigIndex j=startColumn_[kColumn];
	       j<startColumn_[kColumn+1];j++) {
	    int iRow = row_[j];
	    djValue -= dual[iRow]*element_[j];
	  }
	  double infeasibility=0.0;
	  if (!whichBound_||atLowerBound(k)) {
	    if (value<-dualTolerance) 
	      infeasibility=-value-dualTolerance;
	  } else {
	    // at upper bound
	    if (value>dualTolerance) 
	      infeasibility=value-dualTolerance;
	  }
	  if (infeasibility>0.0) {
	    sumDualInfeasibilities_ += infeasibility;
	    if (infeasibility>relaxedTolerance) 
	      sumOfRelaxedDualInfeasibilities_ += infeasibility;
	    numberDualInfeasibilities_ ++;
	  }
	}
      }
    }
    break;
    // see if time to re-factorize
  case 5:
    {
      if (firstAvailable_>numberSets_+model->numberRows()+ model->factorizationFrequency())
	returnNumber=4;
    }
    break;
  }
  return returnNumber;
}
// Add a new variable to a set
void 
ClpGubDynamicMatrix::insertNonBasic(int sequence, int iSet)
{
  int last = keyVariable_[iSet];
  int j=next_[last];
  while (j>=0) {
    last=j;
    j=next_[j];
  }
  next_[last]=-(sequence+1);
  next_[sequence]=j;
}
// Sets up an effective RHS and does gub crash if needed
void 
ClpGubDynamicMatrix::useEffectiveRhs(ClpSimplex * model, bool cheapest)
{
  // size of flagged array
  int size = (numberGubColumns_+ COINFACTORIZATION_BITS_PER_INT-1)/
    COINFACTORIZATION_BITS_PER_INT;
  delete [] flagged_;
  flagged_ = new unsigned int [size];
  memset(flagged_,0,size*sizeof(unsigned int));
  // Do basis - cheapest or slack if feasible (unless cheapest set)
  int longestSet=0;
  int iSet;
  for (iSet=0;iSet<numberSets_;iSet++) 
    longestSet = max(longestSet,fullStart_[iSet+1]-fullStart_[iSet]);
    
  double * upper = new double[longestSet+1];
  double * cost = new double[longestSet+1];
  double * lower = new double[longestSet+1];
  double * solution = new double[longestSet+1];
  assert (!next_);
  delete [] next_;
  int numberColumns = model->numberColumns();
  next_ = new int[numberColumns+numberSets_+max(2*longestSet,lastDynamic_-firstDynamic_)];
  char * mark = new char[numberColumns];
  memset(mark,0,numberColumns);
  for (int iColumn=0;iColumn<numberColumns;iColumn++) 
    next_[iColumn]=INT_MAX;
  int i;
  int * keys = new int[numberSets_];
  int * back = new int[numberGubColumns_];
  CoinFillN(back,numberGubColumns_,-1);
  for (i=0;i<numberSets_;i++) 
    keys[i]=INT_MAX;
  // set up chains
  for (i=firstDynamic_;i<lastDynamic_;i++){
    if (id_[i-firstDynamic_]>=0) {
      if (model->getStatus(i)==ClpSimplex::basic) 
	mark[i]=1;
      int iSet = backward_[i];
      assert (iSet>=0);
      int iNext = keys[iSet];
      next_[i]=iNext;
      keys[iSet]=i;
      back[id_[i-firstDynamic_]]=i;
    } else {
      model->setStatus(i,ClpSimplex::atLowerBound); 
      backward_[i]=-1;
    }
  }
  double * columnSolution = model->solutionRegion();
  int numberRows = getNumRows();
  toIndex_ = new int[numberSets_];
  for (iSet=0;iSet<numberSets_;iSet++) 
    toIndex_[iSet]=-1;
  fromIndex_ = new int [max(getNumRows()+1,numberSets_)];
  double tolerance = model->primalTolerance();
  double * element =  matrix_->getMutableElements();
  int * row = matrix_->getMutableIndices();
  CoinBigIndex * startColumn = matrix_->getMutableVectorStarts();
  int * length = matrix_->getMutableVectorLengths();
  double objectiveOffset = 0.0;
  for (iSet=0;iSet<numberSets_;iSet++) {
    int j;
    int numberBasic=0;
    int iBasic=-1;
    int iStart = fullStart_[iSet];
    int iEnd=fullStart_[iSet+1];
    // find one with smallest length
    int smallest=numberRows+1;
    double value=0.0;
    j=keys[iSet];
    while (j!=INT_MAX) {
      if (model->getStatus(j)== ClpSimplex::basic) {
	if (length[j]<smallest) {
	  smallest=length[j];
	  iBasic=j;
	}
	numberBasic++;
      }
      value += columnSolution[j];
      j = next_[j];
    }
    bool done=false;
    if (numberBasic>1||(numberBasic==1&&getStatus(iSet)==ClpSimplex::basic)) {
      if (getStatus(iSet)==ClpSimplex::basic) 
	iBasic = iSet+numberColumns;// slack key - use
      done=true;
    } else if (numberBasic==1) {
      // see if can be key
      double thisSolution = columnSolution[iBasic];
      if (thisSolution<0.0) {
	value -= thisSolution;
	thisSolution = 0.0;
	columnSolution[iBasic]=thisSolution;
      }
      // try setting slack to a bound
      assert (upper_[iSet]<1.0e20||lower_[iSet]>-1.0e20);
      double cost1 = COIN_DBL_MAX;
      int whichBound=-1;
      if (upper_[iSet]<1.0e20) {
	// try slack at ub
	double newBasic = thisSolution +upper_[iSet]-value;
	if (newBasic>=-tolerance) {
	  // can go
	  whichBound=1;
	  cost1 = newBasic*cost_[iBasic];
	  // But if exact then may be good solution
	  if (fabs(upper_[iSet]-value)<tolerance)
	    cost1=-COIN_DBL_MAX;
	}
      }
      if (lower_[iSet]>-1.0e20) {
	// try slack at lb
	double newBasic = thisSolution +lower_[iSet]-value;
	if (newBasic>=-tolerance) {
	  // can go but is it cheaper
	  double cost2 = newBasic*cost_[iBasic];
	  // But if exact then may be good solution
	  if (fabs(lower_[iSet]-value)<tolerance)
	    cost2=-COIN_DBL_MAX;
	  if (cost2<cost1)
	    whichBound=0;
	}
      }
      if (whichBound!=-1) {
	// key
	done=true;
	if (whichBound) {
	  // slack to upper
	  columnSolution[iBasic]=thisSolution + upper_[iSet]-value;
	  setStatus(iSet,ClpSimplex::atUpperBound);
	} else {
	  // slack to lower
	  columnSolution[iBasic]=thisSolution + lower_[iSet]-value;
	  setStatus(iSet,ClpSimplex::atLowerBound);
	}
      }
    }
    if (!done) {
      if (!cheapest) {
	// see if slack can be key
	if (value>=lower_[iSet]-tolerance&&value<=upper_[iSet]+tolerance) {
	  done=true;
	  setStatus(iSet,ClpSimplex::basic);
	  iBasic=iSet+numberColumns;
	}
      }
      if (!done) {
	// set non basic if there was one
	if (iBasic>=0)
	  model->setStatus(iBasic,ClpSimplex::atLowerBound);
	// find cheapest
	int numberInSet = iEnd-iStart;
	if (!lowerColumn_) {
	  CoinZeroN(lower,numberInSet);
	} else {
	  for (int j=0;j<numberInSet;j++)
	    lower[j]= lowerColumn_[j+iStart];
	}
	if (!upperColumn_) {
	  CoinFillN(upper,numberInSet,COIN_DBL_MAX);
	} else {
	  for (int j=0;j<numberInSet;j++)
	    upper[j]= upperColumn_[j+iStart];
	}
	CoinFillN(solution,numberInSet,0.0);
	// and slack
	iBasic=numberInSet;
	solution[iBasic]=-value;
	lower[iBasic]=-upper_[iSet];
	upper[iBasic]=-lower_[iSet];
	int kphase;
	if (value>=lower_[iSet]-tolerance&&value<=upper_[iSet]+tolerance) {
	  // feasible
	  kphase=1;
	  cost[iBasic]=0.0;
	  for (int j=0;j<numberInSet;j++)
	    cost[j]= cost_[j+iStart];
	} else {
	  // infeasible
	  kphase=0;
	  // remember bounds are flipped so opposite to natural
	  if (value<lower_[iSet]-tolerance)
	    cost[iBasic]=1.0;
	  else
	    cost[iBasic]=-1.0;
	  CoinZeroN(cost,numberInSet);
	}
	double dualTolerance =model->dualTolerance();
	for (int iphase =kphase;iphase<2;iphase++) {
	  if (iphase) {
	    cost[numberInSet]=0.0;
	    for (int j=0;j<numberInSet;j++)
	      cost[j]= cost_[j+iStart];
	  }
	  // now do one row lp
	  bool improve=true;
	  while (improve) {
	    improve=false;
	    double dual = cost[iBasic];
	    int chosen =-1;
	    double best=dualTolerance;
	    int way=0;
	    for (int i=0;i<=numberInSet;i++) {
	      double dj = cost[i]-dual;
	      double improvement =0.0;
	      double distance=0.0;
	      if (iphase||i<numberInSet)
		assert (solution[i]>=lower[i]&&solution[i]<=upper[i]);
	      if (dj>dualTolerance)
		improvement = dj*(solution[i]-lower[i]);
	      else if (dj<-dualTolerance)
		improvement = dj*(solution[i]-upper[i]);
	      if (improvement>best) {
		best=improvement;
		chosen=i;
		if (dj<0.0) {
		  way = 1;
		  distance = upper[i]-solution[i];
		} else {
		  way = -1;
		  distance = solution[i]-lower[i];
		}
	      }
	    }
	    if (chosen>=0) {
	      improve=true;
	      // now see how far
	      if (way>0) {
		// incoming increasing so basic decreasing
		// if phase 0 then go to nearest bound
		double distance=upper[chosen]-solution[chosen];
		double basicDistance;
		if (!iphase) {
		  assert (iBasic==numberInSet);
		  assert (solution[iBasic]>upper[iBasic]);
		  basicDistance = solution[iBasic]-upper[iBasic];
		} else {
		  basicDistance = solution[iBasic]-lower[iBasic];
		}
		// need extra coding for unbounded
		assert (min(distance,basicDistance)<1.0e20);
		if (distance>basicDistance) {
		  // incoming becomes basic
		  solution[chosen] += basicDistance;
		  if (!iphase) 
		    solution[iBasic]=upper[iBasic];
		  else 
		    solution[iBasic]=lower[iBasic];
		  iBasic = chosen;
		} else {
		  // flip
		  solution[chosen]=upper[chosen];
		  solution[iBasic] -= distance;
		}
	      } else {
		// incoming decreasing so basic increasing
		// if phase 0 then go to nearest bound
		double distance=solution[chosen]-lower[chosen];
		double basicDistance;
		if (!iphase) {
		  assert (iBasic==numberInSet);
		  assert (solution[iBasic]<lower[iBasic]);
		  basicDistance = lower[iBasic]-solution[iBasic];
		} else {
		  basicDistance = upper[iBasic]-solution[iBasic];
		}
		// need extra coding for unbounded - for now just exit
		if (min(distance,basicDistance)>1.0e20) {
		  printf("unbounded on set %d\n",iSet);
		  iphase=1;
		  iBasic=numberInSet;
		  break;
		}
		if (distance>basicDistance) {
		  // incoming becomes basic
		  solution[chosen] -= basicDistance;
		  if (!iphase) 
		    solution[iBasic]=lower[iBasic];
		  else 
		    solution[iBasic]=upper[iBasic];
		  iBasic = chosen;
		} else {
		  // flip
		  solution[chosen]=lower[chosen];
		  solution[iBasic] += distance;
		}
	      }
	      if (!iphase) {
		if(iBasic<numberInSet)
		  break; // feasible
		else if (solution[iBasic]>=lower[iBasic]&&
			 solution[iBasic]<=upper[iBasic])
		  break; // feasible (on flip)
	      }
	    }
	  }
	}
	// do solution i.e. bounds
	if (whichBound_) {
	  for (int j=0;j<numberInSet;j++) {
	    if (j!=iBasic) {
	      objectiveOffset += solution[j]*cost[j];
	      if (lowerColumn_&&upperColumn_) {
		if (fabs(solution[j]-lowerColumn_[j+iStart])>
		    fabs(solution[j]-upperColumn_[j+iStart]))
		  setAtUpperBound(j+iStart);
	      } else if (upperColumn_&&solution[j]>0.0) {
		setAtUpperBound(j+iStart);
	      }
	    }
	  }
	}
	// convert iBasic back and do bounds
	if (iBasic==numberInSet) {
	  // slack basic
	  setStatus(iSet,ClpSimplex::basic);
	  iBasic=iSet+numberColumns;
	} else {
	  iBasic += fullStart_[iSet];
	  if (back[iBasic]>=0) {
	    // exists
	    iBasic = back[iBasic];
	  } else {
	    // create
	    CoinBigIndex numberElements = startColumn[firstAvailable_];
	    int numberThis = startColumn_[iBasic+1]-startColumn_[iBasic];
	    if (numberElements+numberThis>numberElements_) {
	      // need to redo
	      abort();
	      element =  matrix_->getMutableElements();
	      row = matrix_->getMutableIndices();
	      // these probably okay but be safe
	      startColumn = matrix_->getMutableVectorStarts();
	      length = matrix_->getMutableVectorLengths();
	    }
	    length[firstAvailable_]=numberThis;
	    model->costRegion()[firstAvailable_]=cost_[iBasic];
	    if (lowerColumn_)
	      model->lowerRegion()[firstAvailable_] = lowerColumn_[iBasic];
	    else
	      model->lowerRegion()[firstAvailable_] = 0.0;
	    if (upperColumn_)
	      model->upperRegion()[firstAvailable_] = upperColumn_[iBasic];
	    else
	      model->upperRegion()[firstAvailable_] = COIN_DBL_MAX;
	    columnSolution[firstAvailable_]=solution[iBasic-fullStart_[iSet]];
	    CoinBigIndex base = startColumn_[iBasic];
	    for (int j=0;j<numberThis;j++) {
	      row[numberElements]=row_[base+j];
	      element[numberElements++]=element_[base+j];
	    }
	    // already set startColumn[firstAvailable_]=numberElements;
	    id_[firstAvailable_-firstDynamic_]=iBasic;
	    backward_[firstAvailable_]=iSet;
	    iBasic=firstAvailable_;
	    firstAvailable_++;
	    startColumn[firstAvailable_]=numberElements;
	  }
	  model->setStatus(iBasic,ClpSimplex::basic);
	  // remember bounds flipped
	  if (upper[numberInSet]==lower[numberInSet]) 
	    setStatus(iSet,ClpSimplex::isFixed);
	  else if (solution[numberInSet]==upper[numberInSet])
	    setStatus(iSet,ClpSimplex::atLowerBound);
	  else if (solution[numberInSet]==lower[numberInSet])
	    setStatus(iSet,ClpSimplex::atUpperBound);
	  else 
	    abort();
	}
	for (j=iStart;j<iEnd;j++) {
	  int iBack = back[j];
	  if (iBack>=0) {
	    if (model->getStatus(iBack)!=ClpSimplex::basic) {
	      int inSet=j-iStart;
	      columnSolution[iBack]=solution[inSet];
	      if (upper[inSet]==lower[inSet]) 
		model->setStatus(iBack,ClpSimplex::isFixed);
	      else if (solution[inSet]==upper[inSet])
		model->setStatus(iBack,ClpSimplex::atUpperBound);
	      else if (solution[inSet]==lower[inSet])
		model->setStatus(iBack,ClpSimplex::atLowerBound);
	    }
	  }
	}
      }
    } 
    keyVariable_[iSet]=iBasic;
  }
  model->setObjectiveOffset(objectiveOffset_-objectiveOffset);
  delete [] lower;
  delete [] solution;
  delete [] upper;
  delete [] cost;
  // make sure matrix is in good shape
  matrix_->orderMatrix();
  // create effective rhs
  delete [] rhsOffset_;
  rhsOffset_ = new double[numberRows];
  // and redo chains
  memset(mark,0,numberColumns);
  for (int iColumn=0;iColumn<firstAvailable_;iColumn++) 
    next_[iColumn]=INT_MAX;
  for (i=0;i<numberSets_;i++) {
    keys[i]=INT_MAX;
    int iKey = keyVariable_[i];
    if (iKey<numberColumns)
      model->setStatus(iKey,ClpSimplex::basic);
  }
  // set up chains
  for (i=0;i<firstAvailable_;i++){
    if (model->getStatus(i)==ClpSimplex::basic) 
      mark[i]=1;
    int iSet = backward_[i];
    if (iSet>=0) {
      int iNext = keys[iSet];
      next_[i]=iNext;
      keys[iSet]=i;
    }
  }
  for (i=0;i<numberSets_;i++) {
    if (keys[i]!=INT_MAX) {
      // something in set
      int j;
      if (getStatus(i)!=ClpSimplex::basic) {
	// make sure fixed if it is
	if (upper_[i]==lower_[i])
	  setStatus(i,ClpSimplex::isFixed);
	// slack not key - choose one with smallest length
	int smallest=numberRows+1;
	int key=-1;
	j = keys[i];
	while (1) {
	  if (mark[j]&&length[j]<smallest) {
	    key=j;
	    smallest=length[j];
	  }
	  if (next_[j]!=INT_MAX) {
	    j = next_[j];
	  } else {
	    // correct end
	    next_[j]=-(keys[i]+1);
	    break;
	  }
	}
	if (key>=0) {
	  keyVariable_[i]=key;
	} else {
	  // nothing basic - make slack key
	  //((ClpGubMatrix *)this)->setStatus(i,ClpSimplex::basic);
	  // fudge to avoid const problem
	  status_[i]=1;
	}
      } else {
	// slack key
	keyVariable_[i]=numberColumns+i;
	int j;
	double sol=0.0;
	j = keys[i];
	while (1) {
	  sol += columnSolution[j];
	  if (next_[j]!=INT_MAX) {
	    j = next_[j];
	  } else {
	    // correct end
	    next_[j]=-(keys[i]+1);
	    break;
	  }
	}
	if (sol>upper_[i]+tolerance) {
	  setAbove(i);
	} else if (sol<lower_[i]-tolerance) {
	  setBelow(i);
	} else {
	  setFeasible(i);
	}
      }
      // Create next_
      int key = keyVariable_[i];
      redoSet(model,key,keys[i],i);
    } else {
      // nothing in set!
      next_[i+numberColumns]=-(i+numberColumns+1);
      keyVariable_[i]=numberColumns+i;
      double sol=0.0;
      if (sol>upper_[i]+tolerance) {
	setAbove(i);
      } else if (sol<lower_[i]-tolerance) {
	setBelow(i);
      } else {
	setFeasible(i);
      }
    }
  }
  delete [] keys;
  delete [] mark;
  delete [] back;
  rhsOffset(model,true);
}
/* Returns effective RHS if it is being used.  This is used for long problems
   or big gub or anywhere where going through full columns is
   expensive.  This may re-compute */
double * 
ClpGubDynamicMatrix::rhsOffset(ClpSimplex * model,bool forceRefresh,
		      bool check)
{
  if (rhsOffset_) {
#ifdef CLP_DEBUG
    if (check) {
      // no need - but check anyway
      int numberRows = model->numberRows();
      double * rhs = new double[numberRows];
      int numberColumns = model->numberColumns();
      int iRow;
      CoinZeroN(rhs,numberRows);
      if (whichBound_) {
	double * solution = new double [numberGubColumns_];
	int iColumn;
	for (iColumn=0;iColumn<numberGubColumns_;iColumn++) {
	  double value=0.0;
	  if(atUpperBound(iColumn) )
	    value = upperColumn_[iColumn];
	  else if (lowerColumn_)
	    value = lowerColumn_[iColumn];
	  solution[iColumn]=value;
	}
	// zero all basic in small model
	int * pivotVariable = model->pivotVariable();
	for (iRow=0;iRow<numberRows;iRow++) {
	  int iColumn = pivotVariable[iRow];
	  if (iColumn>=firstDynamic_&&iColumn<lastDynamic_) {
	    int iSequence = id_[iColumn-firstDynamic_];
	    solution[iSequence]=0.0;
	  }
	}
	// and now compute value to use for key
	ClpSimplex::Status iStatus;
	for (int iSet=0;iSet<numberSets_;iSet++) {
	  iColumn = keyVariable_[iSet];
	  if (iColumn<numberColumns) {
	    int iSequence = id_[iColumn-firstDynamic_];
	    solution[iSequence]=0.0;
	    double b=0.0;
	    // key is structural - where is slack
	    iStatus = getStatus(iSet);
	    assert (iStatus!=ClpSimplex::basic);
	    if (iStatus==ClpSimplex::atLowerBound)
	      b=lowerSet_[iSet];
	    else
	      b=upperSet_[iSet];
	    // subtract out others at bounds
	    for (int j=fullStart_[iSet];j<fullStart_[iSet+1];j++) 
	      b -= solution[j];
	    solution[iSequence]=b;
	  }
	}
	for (iColumn=0;iColumn<numberGubColumns_;iColumn++) {
	  double value = solution[iColumn];
	  if (value) {
	    for (CoinBigIndex j= startColumn_[iColumn];j<startColumn_[iColumn+1];j++) {
	      int iRow = row_[j];
	      rhs[iRow] -= element_[j]*value;
	    }
	  }
	}
	// now do lower and upper bounds on sets
	// so we take out ones in small problem
	for (iColumn=firstDynamic_;iColumn<firstAvailable_;iColumn++) {
	  int iSequence = id_[iColumn-firstDynamic_];
	  solution[iSequence]=0.0;
	}
	for (int iSet=0;iSet<numberSets_;iSet++) {
	  iColumn = keyVariable_[iSet];
	  if (iColumn<numberColumns) {
	    int iSequence = id_[iColumn-firstDynamic_];
	    solution[iSequence]=0.0;
	  }
	  double shift=0.0;
	  for (int j=fullStart_[iSet];j<fullStart_[iSet+1];j++) 
	    shift += solution[j];
	  if (lowerSet_[iSet]>-1.0e20) 
	    assert(fabs(lower_[iSet] - (lowerSet_[iSet]-shift))<1.0e-3);
	  if (upperSet_[iSet]<1.0e20)
	    assert(fabs(upper_[iSet] -( upperSet_[iSet]-shift))<1.0e-3);
	}
	delete [] solution;
      } else {
	// no bounds
	ClpSimplex::Status iStatus;
	for (int iSet=0;iSet<numberSets_;iSet++) {
	  int iColumn = keyVariable_[iSet];
	  if (iColumn<numberColumns) {
	    int iSequence = id_[iColumn-firstDynamic_];
	    double b=0.0;
	    // key is structural - where is slack
	    iStatus = getStatus(iSet);
	    assert (iStatus!=ClpSimplex::basic);
	    if (iStatus==ClpSimplex::atLowerBound)
	      b=lower_[iSet];
	    else
	      b=upper_[iSet];
	    if (b) {
	      for (CoinBigIndex j= startColumn_[iSequence];j<startColumn_[iSequence+1];j++) {
		int iRow = row_[j];
		rhs[iRow] -= element_[j]*b;
	      }
	    }
	  }
	}
      }
      for (iRow=0;iRow<numberRows;iRow++) {
	if (fabs(rhs[iRow]-rhsOffset_[iRow])>1.0e-3)
	  printf("** bad effective %d - true %g old %g\n",iRow,rhs[iRow],rhsOffset_[iRow]);
      }
      delete [] rhs;
    }
#endif
    if (forceRefresh||(refreshFrequency_&&model->numberIterations()>=
		       lastRefresh_+refreshFrequency_)) {
      int numberRows = model->numberRows();
      int numberColumns = model->numberColumns();
      int iRow;
      CoinZeroN(rhsOffset_,numberRows);
      if (whichBound_) {
	double * solution = new double [numberGubColumns_];
	int iColumn;
	for (iColumn=0;iColumn<numberGubColumns_;iColumn++) {
	  double value=0.0;
	  if(atUpperBound(iColumn) )
	    value = upperColumn_[iColumn];
	  else if (lowerColumn_)
	    value = lowerColumn_[iColumn];
	  solution[iColumn]=value;
	}
	// zero all basic in small model
	int * pivotVariable = model->pivotVariable();
	for (iRow=0;iRow<numberRows;iRow++) {
	  int iColumn = pivotVariable[iRow];
	  if (iColumn>=firstDynamic_&&iColumn<lastDynamic_) {
	    int iSequence = id_[iColumn-firstDynamic_];
	    solution[iSequence]=0.0;
	  }
	}
	// and now compute value to use for key
	ClpSimplex::Status iStatus;
	for (int iSet=0;iSet<numberSets_;iSet++) {
	  iColumn = keyVariable_[iSet];
	  if (iColumn<numberColumns) {
	    int iSequence = id_[iColumn-firstDynamic_];
	    solution[iSequence]=0.0;
	    double b=0.0;
	    // key is structural - where is slack
	    iStatus = getStatus(iSet);
	    assert (iStatus!=ClpSimplex::basic);
	    if (iStatus==ClpSimplex::atLowerBound)
	      b=lowerSet_[iSet];
	    else
	      b=upperSet_[iSet];
	    // subtract out others at bounds
	    for (int j=fullStart_[iSet];j<fullStart_[iSet+1];j++) 
	      b -= solution[j];
	    solution[iSequence]=b;
	  }
	}
	for (iColumn=0;iColumn<numberGubColumns_;iColumn++) {
	  double value = solution[iColumn];
	  if (value) {
	    for (CoinBigIndex j= startColumn_[iColumn];j<startColumn_[iColumn+1];j++) {
	      int iRow = row_[j];
	      rhsOffset_[iRow] -= element_[j]*value;
	    }
	  }
	}
	// now do lower and upper bounds on sets
	// and offset
	// so we take out ones in small problem
	for (iColumn=firstDynamic_;iColumn<firstAvailable_;iColumn++) {
	  int iSequence = id_[iColumn-firstDynamic_];
	  solution[iSequence]=0.0;
	}
	double objectiveOffset = 0.0;
	for (int iSet=0;iSet<numberSets_;iSet++) {
	  iColumn = keyVariable_[iSet];
	  if (iColumn<numberColumns) {
	    int iSequence = id_[iColumn-firstDynamic_];
	    solution[iSequence]=0.0;
	  }
	  double shift=0.0;
	  for (int j=fullStart_[iSet];j<fullStart_[iSet+1];j++) {
	    objectiveOffset += solution[j]*cost_[j];
	    shift += solution[j];
	  }
	  if (lowerSet_[iSet]>-1.0e20)
	    lower_[iSet] = lowerSet_[iSet]-shift;
	  if (upperSet_[iSet]<1.0e20)
	    upper_[iSet] = upperSet_[iSet]-shift;
	}
	delete [] solution;
	model->setObjectiveOffset(objectiveOffset_-objectiveOffset);
      } else {
	// no bounds
	ClpSimplex::Status iStatus;
	for (int iSet=0;iSet<numberSets_;iSet++) {
	  int iColumn = keyVariable_[iSet];
	  if (iColumn<numberColumns) {
	    int iSequence = id_[iColumn-firstDynamic_];
	    double b=0.0;
	    // key is structural - where is slack
	    iStatus = getStatus(iSet);
	    assert (iStatus!=ClpSimplex::basic);
	    if (iStatus==ClpSimplex::atLowerBound)
	      b=lower_[iSet];
	    else
	      b=upper_[iSet];
	    if (b) {
	      for (CoinBigIndex j= startColumn_[iSequence];j<startColumn_[iSequence+1];j++) {
		int iRow = row_[j];
		rhsOffset_[iRow] -= element_[j]*b;
	      }
	    }
	  }
	}
      }
      lastRefresh_ = model->numberIterations();
    }
  }
  return rhsOffset_;
}
/*
  update information for a pivot (and effective rhs)
*/
int 
ClpGubDynamicMatrix::updatePivot(ClpSimplex * model,double oldInValue, double oldOutValue)
{
  
  // now update working model
  int sequenceIn = model->sequenceIn();
  if (sequenceIn==firstAvailable_) {
    insertNonBasic(firstAvailable_,backward_[firstAvailable_]);
    firstAvailable_++;
  }
  int sequenceOut = model->sequenceOut();
  if (sequenceOut>=firstDynamic_&&sequenceOut<firstAvailable_) {
    int jColumn = id_[sequenceOut-firstDynamic_];
    if (model->getStatus(sequenceOut)==ClpSimplex::atUpperBound)
      setAtUpperBound(jColumn);
    else if (whichBound_)
      setAtLowerBound(jColumn);
  }
  ClpGubMatrix::updatePivot(model,oldInValue,oldOutValue);
  return 0;
}
void 
ClpGubDynamicMatrix::times(double scalar,
			   const double * x, double * y) const
{
  if (model_->specialOptions()!=16) {
    ClpPackedMatrix::times(scalar,x,y);
  } else {
    int iRow;
    int numberColumns = model_->numberColumns();
    int numberRows = model_->numberRows();
    const double * element =  matrix_->getElements();
    const int * row = matrix_->getIndices();
    const CoinBigIndex * startColumn = matrix_->getVectorStarts();
    const int * length = matrix_->getVectorLengths();
    int * pivotVariable = model_->pivotVariable();
    int numberToDo=0;
    for (iRow=0;iRow<numberRows;iRow++) {
      y[iRow] -= scalar*rhsOffset_[iRow];
      int iColumn = pivotVariable[iRow];
      if (iColumn<numberColumns) {
	int iSet = backward_[iColumn];
	if (iSet>=0&&toIndex_[iSet]<0) {
	  toIndex_[iSet]=0;
	  fromIndex_[numberToDo++]=iSet;
	}
	CoinBigIndex j;
	double value = scalar*x[iColumn];
	if (value) {
	  for (j=startColumn[iColumn];
	       j<startColumn[iColumn]+length[iColumn];j++) {
	    int jRow=row[j];
	    y[jRow] += value*element[j];
	  }
	}
      }
    }
    // and gubs which are interacting
    for (int jSet=0;jSet<numberToDo;jSet++) {
      int iSet = fromIndex_[jSet];
      toIndex_[iSet]=-1;
      int iKey=keyVariable_[iSet];
      if (iKey<numberColumns) {
	double valueKey;
	if (getStatus(iSet)==ClpSimplex::atLowerBound) 
	  valueKey = lower_[iSet];
	else
	  valueKey = upper_[iSet];
	double value = scalar*(x[iKey]-valueKey);
	if (value) {
	  for (CoinBigIndex j=startColumn[iKey];
	       j<startColumn[iKey]+length[iKey];j++) {
	    int jRow=row[j];
	    y[jRow] += value*element[j];
	  }
	}
      }
    }
  }
}