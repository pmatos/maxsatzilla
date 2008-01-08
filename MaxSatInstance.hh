
#include <fstream>
#include <vector>

#define MAX_LINE_LENGTH 2048
#define MAX_NUM_LITERALS 1000000

// UBCSAT parameters
#define UBCSAT_TIME_LIMIT 1
#define UBCSAT_NUM_RUNS 1000
#define UBCSAT_SEED 12345

using namespace std;

class MaxSatInstance {
  char* inputFileName;
  int numVars, numClauses;
  int *negClausesWithVar, *posClausesWithVar;
  int unitClauses, binaryClauses, ternaryClauses;
  bool isTautologicalClause(int[MAX_NUM_LITERALS], int&, const int);
 public:
  enum {CNF, PARTIAL, WEIGHTED, WEIGHTED_PARTIAL} format;
  MaxSatInstance( const char* filename );
  ~MaxSatInstance();

  void computeLocalSearchProperties();
  void printInfo( ostream& os );
};
