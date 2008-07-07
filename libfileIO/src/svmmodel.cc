#include "svmmodel.hh"

SVMModel::SVMModel()
  : prob(NULL), model(NULL) { }

SVMModel::SVMModel(struct svm_problem *prob, struct svm_model *model) 
  : prob(prob), model(model) { }

SVMModel::SVMModel(const char *fname) 
  : prob(NULL) {
  setStructsFromFile(fname);
}

SVMModel::~SVMModel() {
  svm_destroy_model(model);
  free(prob)
}

void SVMModel::setStructs(struct svm_problem *svmp, struct svm_model *svmm) {
  prob = svmp;
  model = svmm;
}

void SVMModel::setStructsFromFile(const char *fname) {
  model = svm_load_model(fname);
  if(model == NULL)
    cerr << "SVMModel : Unable to load model from " << fname << "\n";
}
