#include <iostream>
#include <fstream>

#include <cstdlib>
#include <cassert>

#include "dataset.hh"

using std::cerr;
using std::ofstream;

MSZDataSet::MSZDataSet(double **matrix, size_t nrows, size_t ncols, size_t outputs) 
  : matrix(matrix), nrows(nrows), ncols(ncols), outputs(outputs) {}
  
MSZDataSet::~MSZDataSet() {

  // Please God, do not let this Seg Fault!

  // Clean up matrix by deleting each line.
  for(size_t row = 0; row < nrows; row++) 
    free(matrix[row]);
	 
  free(matrix);
  
}

void MSZDataSet::dumpPlotFiles(const vector<string> &labels, const string &prefix) {

  if(labels.size() != ncols) {
    cerr << "Warning: Trying to dump plot files but labels vector is too small.\n";
    return;
  }

  // Dump the files 
  for(size_t out = 0; out < outputs; out++) {
    for(size_t feature = outputs; feature < ncols; feature++) {
      ofstream file;
      file.open((prefix + "_" + labels[out] + "_" + labels[feature] + ".dat").c_str());
      
      file << "# This file is generated by MatSATZilla\n"
	   << "# It should be used with gnuplot to plot\n"
	   << "# the feature " << labels[feature] << " against\n"
	   << "# the output " << labels[out] << "\n"
	   << "#\n"
	// Outputting timestamp could be nice.
	//<< "# Timestamp: " << 
	   << "#\n"
	   << "# " << labels[out] << "\t\t" << labels[feature] << "\n";
      
      // For each instance
      for(size_t i = 0; i < nrows; i++) 
	file << matrix[i][out] << "\t\t" << matrix[i][feature] << "\n";

      file.close();
    }
  }
}

void MSZDataSet::dumpPlotFiles(char **labels, size_t len, char *prefix) {

  vector<string> vec(len);

  for(size_t i = 0; i < len; i++)
    vec[i] = string(labels[i]);

  dumpPlotFiles(vec, string(prefix));
}

double MSZDataSet::getOutputValue(size_t row, size_t col) const {
  assert(row < nrows);
  assert(col < outputs);

  return matrix[row][col];
}

double MSZDataSet::getFeatureValue(size_t row, size_t col) const {
  assert(row < nrows);
  assert(col < ncols - outputs);

  return matrix[row][col+outputs];
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
