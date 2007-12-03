#include <iostream>
#include <fstream>

#include <iostream>
#include <iterator>
#include <cstdlib>
#include <cassert>
#include <cmath>

#include <gsl/gsl_math.h>
#include <gsl/gsl_combination.h>

#include "dataset.hh"

using std::cout;
using std::cerr;
using std::ofstream;

MSZDataSet::MSZDataSet(double **m, size_t nrows, size_t ncols, size_t outputs) 
  : nrows(nrows), rfeatures(ncols - outputs), ncols(ncols), outputs(outputs),
    stdDone(false), oStdDone(false) {

  // Recomputing matrix to be made of columns instead of lines
  matrix = new double* [ncols];
  for(size_t c = 0; c < ncols; c++)
    setMColumn(c, new double [nrows]);
  
  // Copy the elements
  for(size_t r = 0; r < nrows; r++)
    for(size_t c = 0; c < ncols; c++)
      setMValue(r, c, m[r][c]);
  
  // Delete old matrix
  for(size_t r = 0; r < nrows; r++)
    delete[] m[r];
  delete[] m;
}
  
MSZDataSet::MSZDataSet(const MSZDataSet& ds) 
  : nrows(ds.nrows), rfeatures(ds.rfeatures), ncols(ds.ncols),
    outputs(ds.outputs), stdDone(ds.stdDone), oStdDone(ds.oStdDone) 
{
  
  // Now we just need to copy the matrix.
  matrix = new double* [ncols];
  for(size_t c = 0; c < ncols; c++)
    setMColumn(c, new double [nrows]);

  for(size_t r = 0; r < nrows; r++)
    for(size_t c = 0; c < ncols; c++)
      setMValue(r, c, ds.getMValue(r, c));

}

MSZDataSet::~MSZDataSet() {

  // Please God, do not let this Seg Fault!

  // Clean up matrix by deleting each line.
  for(size_t c = 0; c < ncols; c++) 
    delete[] getMColumn(c);
	 
  delete[] matrix;
  
}

void MSZDataSet::dumpPlotFiles(const vector<string> &labels, const string &prefix) const {

  if(labels.size() != ncols) {
    cerr << "Warning: Trying to dump plot files but labels vector is too small.\n";
    cerr << "Labels has size " << labels.size() << " but we have " << ncols << " columns.\n";
    return;
  }

  // Dump the files for plotting features against output
  for(size_t f1 = 0; f1 < outputs; f1++) {
    for(size_t f2 = (f1 < outputs ? outputs : f1+1); f2 < ncols; f2++) {
      ofstream file;
      file.open((prefix + "_" + labels[f1] + "_" + labels[f2] + ".dat").c_str());
      
      file << "# This file is generated by MatSATZilla\n"
	   << "# It should be used with gnuplot to plot\n"
	   << "# the feature " << labels[f2] << " against\n"
	   << "# the output " << labels[f1] << "\n"
	   << "#\n"
	// Outputting timestamp could be nice.
	//<< "# Timestamp: " << 
	   << "#\n"
	   << "# " << labels[f2] << "\t\t" << labels[f1] << "\n";
      
      // For each instance
      for(size_t i = 0; i < nrows; i++) 
	file << getMValue(i, f2) << "\t\t" << getMValue(i, f1) << "\n";

      file.close();
    }
  }
}

void MSZDataSet::printSolverStats(size_t timeout, const vector<string> &slabels) {

  if(slabels.size() != outputs) {
    cerr << "printSolverStats: You're passing in " << slabels.size() << " solver labels but there are " << outputs << " outputs in dataset.\n";
    return;
  }

  vector<size_t> nbTimeouts(outputs, 0);
  vector<size_t> nbOtherErrors(outputs, 0);

  for(size_t s = 0; s < outputs; s++) {
    for(size_t i = 0; i < nrows; i++) {
      if(getMValue(i, s) == timeout)
	nbTimeouts[s]++;
      else if(getMValue(i, s) == 2*timeout)
	nbOtherErrors[s]++;
    }
  }

  for(size_t s = 0; s < outputs; s++) {
    cout << "Solver: " << slabels[s] << "\n"
	 << "\t\tTimeouts: " << nbTimeouts[s] << " (" << (((double)(nbTimeouts[s])) / nrows)*100.0 << ")\n"
	 << "\t\tOther Errors: " << nbOtherErrors[s] << " (" << (((double)(nbOtherErrors[s])) / nrows)*100.0 << ")\n"
	 << "\t\tUsable Instances: " << nrows - nbTimeouts[s] - nbOtherErrors[s] << " (" << (((double)(nrows - nbTimeouts[s] - nbOtherErrors[s])) / nrows)*100.0 << ")\n";
  }

}

void MSZDataSet::dumpPlotFiles(char **labels, size_t len, char *prefix) const {

  vector<string> vec(len);

  for(size_t i = 0; i < len; i++)
    vec[i] = string(labels[i]);

  dumpPlotFiles(vec, string(prefix));
}

double MSZDataSet::getOutputValue(size_t row, size_t col) const {
  assert(row < nrows);
  assert(col < outputs);

  return getMValue(row, col);
}

double MSZDataSet::getFeatureValue(size_t row, size_t col) const {
  assert(row < nrows);
  assert(col < ncols - outputs);

  return getMValue(row, col+outputs);
}

void MSZDataSet::standardize() {
  
  cout << "Standardizing features ...";

  if(!stdDone) {
    for(size_t c = outputs; c < ncols; c++) {

      // Compute column mean and compute column variance.
      double mean = 0.0;
      for(size_t r = 0; r < nrows; r++) 
	mean += getMValue(r, c);
      mean /= nrows;

      // Compute column standard deviation. 
      double sdv = 0.0;
      for(size_t r = 0; r < nrows; r++) 
	sdv += gsl_pow_2(getMValue(r, c) - mean);
      sdv /= nrows;

      for(size_t r = 0; r < nrows; r++) 
	setMValue(r, c, (getMValue(r, c) - mean) / sdv);
	
    }

  }
  stdDone = true;

  cout << "DONE\n";

}

void MSZDataSet::removeFeatures(const vector<size_t> &keepVec) {
  // We need to remove the feature indexes in vec from the current data.
  
  cout << "Keeping features in dataset: ";

  // Create the indices to keep
  // by: 
  // - incrementing all indices by output
  // - sorting
  // - adding output indices to the beginning of vector
  vector<size_t> vec(keepVec);
  for(vector<size_t>::iterator it = vec.begin(); 
      it != vec.end();
      it++)
    *it += outputs;
  sort(vec.begin(), vec.end());        // sorts 
  vec.insert(vec.begin(), outputs, 0); // adds positions
  for(size_t o = 0; o < outputs; o++)   
    vec[o] += o;                       // sets positions

  if(vec.size() == ncols) { 
    cout << "ALL\n";
    return;
  } else {
    copy(vec.begin(), vec.end(), std::ostream_iterator<size_t>(cout, " "));
    cout << std::endl;
  }

  // Count the number of raw features about to be removed
  size_t rawToRemove = 0;
  for(vector<size_t>::const_iterator it = vec.begin(); 
      it != vec.end();
      it++)
    if(*it >= outputs && *it < outputs + rfeatures) 
      rawToRemove++;

  rfeatures -= rawToRemove;
  const size_t oldNCols = ncols;
  ncols = vec.size();
  
  // Now, we just need to delete the columns referenced in the vector
  // and resize the main column array (delete + new, no realloc!).
  double **newMatrix = new double* [ncols];
  // Note that the vector indices denote the destination column
  // in the new matrix and the vector contents the origin column on the
  // old matrix. This means that we need to copy the column v[i] of old matrix
  // to column i of the new matrix.
  for(size_t c = 0; c < ncols; c++) {
    newMatrix[c] = getMColumn(vec[c]);
    setMColumn(vec[c], 0); // Those which are not 0 are the columns 
                        // that should be deleted afterwards
  }
  for(size_t c = 0; c < oldNCols; c++)
    if(getMColumn(c) != 0) delete[] getMColumn(c);
  
  delete[] matrix;
  matrix = newMatrix;
}

void MSZDataSet::standardizeOutputs() {

  cout << "Standardizing outputs... ";

  if(!stdDone) {
    for(size_t c = 0; c < outputs; c++)
      for(size_t r = 0; r < nrows; r++)
	setMValue(r, c, log(getMValue(r, c)));
  }

  oStdDone = true;

  cout <<  "DONE\n";

}

void MSZDataSet::expand(size_t n) { 
  vector<size_t> pvec(ncols - outputs); 
  for(size_t i = 0; i < pvec.size(); i++)
    pvec[i] = i;

  expandOnPartition(n, pvec);
}

void MSZDataSet::expand(size_t n, const vector<vector<size_t> > &pvec) {
  for(size_t i = 0; i < pvec.size(); i++)
    expandOnPartition(n, pvec[i]);
}

void MSZDataSet::expandOnPartition(size_t k, const vector<size_t> &pvec) {
  
  // expansion is quadratic at the minimum and 
  // and be bigger than partition size.
  assert(k >= 2 && k <= pvec.size());

  // Standardize features
  standardize();

  // Output information
  cout << "Expanding feature set by order " << k << " on partition:\n";
  copy(pvec.begin(), pvec.end(), std::ostream_iterator<size_t>(cout, " "));
  cout << "\n";

  // Reallocate the data.
  // If we have N features, then, to expand to a order-K polynomial
  // we are adding 
  // n + (n !) / (k! (n-k!))
  size_t num = 1;
  for(size_t i = 0; i < k; i++) num *= rfeatures - i; // for big k hell breaks loose
  size_t den = 1;
  for(size_t i = k; i > 0; i--) den *= i; 
  size_t nNewF = rfeatures;
  nNewF += num / den;
  ncols += nNewF; // Update number of columns

  cout << "Expansion increases feature set by " << nNewF << " columns " << " summing a total of " << ncols << " columns.\n";
  
  // Reallocating main vector and copying all the others to their initial places.
  double **newMatrix = new double* [ncols];
  for(size_t c = 0; c < ncols - nNewF; c++)
    newMatrix[c] = getMColumn(c);
  delete[] matrix;
  matrix = newMatrix;

  for(size_t nc = ncols - nNewF; nc < ncols; nc++)
    setMColumn(nc, new double [nrows]);

  // Compute the cross products
  // To do this we compute all the indices combinations and use them
  // to compute the cross product on columns defined in pvec.
  size_t currColumn = rfeatures + outputs;
  const size_t n = pvec.size();
  gsl_combination * c;
  c = gsl_combination_calloc (n, k);
  size_t *ind = new size_t [k];
  
  // Compute cross product for initial configuration
  // Now using the very nice combination structure from GSL.
  // Up to rev. 121, we used custom combination generator.
  do {
    for(size_t i = 0; i < k; i++)
      ind[i] = gsl_combination_get(c, i);

    // Compute cross product in current configuration
    for(size_t r = 0; r < nrows; r++) {
      const double cprod = computeCrossProduct(r, ind, k, pvec);
      setMValue(r, currColumn, cprod);
    }
    currColumn++;
  } while(gsl_combination_next (c) == GSL_SUCCESS);
  gsl_combination_free (c);
  delete[] ind;

  // Compute Powers
  for(size_t i = 0; i < pvec.size(); i++) {
    for(size_t r = 0; r < nrows; r++) {
      // Compute powers for pvec[i] feature
      setMValue(r, currColumn, gsl_pow_int(getMValue(r, pvec[i]), k));
    }
  }

}

double MSZDataSet::computeCrossProduct(size_t r, size_t *ind, size_t k, const vector<size_t> &vec) {
  double cp = 1.0; // Cross-product
  for(size_t i = 0; i < k; i++)
    cp *= getMValue(r, vec[ind[i]]);
  return cp;
}

/////////////////////////////////////////
/////////////////////////////////////////
//
// API Entrace Function for Data Set creation
//
/////////////////////////////////////////
/////////////////////////////////////////

MSZDataSet *createDataSet(double** matrix, size_t nrows, size_t ncols, size_t outputs) {
  assert(nrows > 0);
  assert(ncols > 0);
  assert(matrix != 0);
  assert(outputs > 0);
  assert(nrows >= ncols - outputs);
  
#ifndef NDEBUG
  for(size_t r = 0; r < nrows; r++)
    assert(matrix[r] != 0);
#endif // NDEBUG

  // The call!
  MSZDataSet *ds = new MSZDataSet(matrix, nrows, ncols, outputs);
  if(!ds) {
    cerr << "Error: Allocation of Dataset\n";
    exit(EXIT_FAILURE);
  }
  
  return ds;
}
