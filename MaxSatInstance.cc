/*******************************************************************************[MaxSatInstance.cc]
Magpie -- Florian Letombe, Joao Marques-Silva, Paulo Matos, Jordi Planes (2007)

Parts of the code in this file have been extracted from SATzilla.
**************************************************************************************************/

#include <algorithm>

#include "MaxSatInstance.hh"
#include "ubcsat/ubcsat.h"

bool MaxSatInstance::isTautologicalClause( int lits[ MAX_NUM_LITERALS ], int& numLits, const int clauseNum ) {
  // sort the clause and remove redundant literals
  //!! For large clauses better use sort()
  for (int i=0; i<numLits-1; i++) {
    for (int j=i+1; j<numLits; j++) {
      if (abs(lits[i]) > abs(lits[j])) { // Bubble sort
        swap( lits[i], lits[j] );
      } else if (lits[i] == lits[j]) {
	lits[j--] = lits[--numLits];
	printf("c literal %d is redundant in clause %d\n", lits[i], clauseNum);
      } else if (abs(lits[i]) == abs(lits[j])) {
	printf("c Clause %d is tautological.\n", clauseNum);
	return true;
      }
    }
  }              
  return false;
}

MaxSatInstance::MaxSatInstance( const char* filename ) 
{
  ifstream infile(filename);
  if (!infile) {
    fprintf(stderr, "c Error: could not read from %s.\n", filename);
    exit(1);
  }
  inputFileName = new char[ strlen( filename ) + 1 ];
  strcpy( inputFileName, filename );

  while (infile.get() != 'p') {
    infile.ignore(MAX_LINE_LENGTH, '\n');
  }

  char strbuf[MAX_LINE_LENGTH];
  infile >> strbuf;

  if( strcmp(strbuf, "cnf")==0 ) {
    format = CNF;
  } else if( strcmp(strbuf, "wcnf")==0 ) {
    format = WEIGHTED;
  } else if( strcmp(strbuf, "pcnf")==0 ) {
    format = PARTIAL;
  } else if( strcmp(strbuf, "wpcnf")==0 ) {
    format = WEIGHTED_PARTIAL;
  } else {
    fprintf(stderr, "c Error: Can only understand cnf format!\n");
    exit(1);
  }

  infile >> numVars >> numClauses;

  negClausesWithVar = new int[ numVars+1 ];
  posClausesWithVar = new int[ numVars+1 ];
  fill( negClausesWithVar, &negClausesWithVar[numVars+1], 0 );
  fill( posClausesWithVar, &posClausesWithVar[numVars+1], 0 );

  unitClauses = binaryClauses = ternaryClauses = 0;

  int lits[ MAX_NUM_LITERALS ];
  for (int clauseNum=0; clauseNum<numClauses; clauseNum++) {
    
    int numLits = 0;
    
    infile >> lits[numLits];
    while (lits[numLits] != 0) 
      infile >> lits[++numLits];

    if ( numLits == 1 or !isTautologicalClause( lits, numLits, clauseNum )) {
      switch( numLits ) {
      case 1: unitClauses++; break;
      case 2: binaryClauses++; break;
      case 3: ternaryClauses++; break;
      }
 
      for (int litNum = 0; litNum < numLits; litNum++) 
	if (lits[litNum] < 0)
	  negClausesWithVar[abs(lits[litNum])]++;
	else
	  posClausesWithVar[lits[litNum]]++;
      
    } else {
      clauseNum--;
      numClauses--;
    }
  }
}

MaxSatInstance::~MaxSatInstance() {
  delete negClausesWithVar;
  delete posClausesWithVar;
}

namespace ubcsat { int main(int, char**); }

void MaxSatInstance::computeLocalSearchProperties() {
  printf("local search probe...\n");

  char sTimeout[64];
  char sRuns[64];

  sprintf(sTimeout, "%d", UBCSAT_TIME_LIMIT);
  sprintf(sRuns, "%d", UBCSAT_NUM_RUNS);


  char* vlineFilename = new char[512];
  sprintf(vlineFilename, "%s", P_tmpdir);
  strcat(vlineFilename, "/XXXXXX");
  vlineFilename = mktemp(vlineFilename);


  char strseed[64];
  sprintf(strseed, "%d", UBCSAT_SEED );

  char* argv[] = {"ubcsat", 
		  "-alg", NULL, "-noimprove", NULL,  "-i", inputFileName, 
		  "-runs", sRuns, "-timeout", sTimeout,
		  "-r", "satcomp", vlineFilename,
		  "-seed", strseed,
		  "-satzilla", "-solve"};
  
  int argc = 17;
  
  // -- do saps
  argv[2]="saps";
  argv[4]="0.1";
  
  if ( ubcsat::main(argc, argv) == 10 ) printf("Instance satisfiable\n");
  
  // -- do gsat
  argv[2]="gsat";
  argv[4]="0.5";

  if ( ubcsat::main(argc, argv) == 10 ) printf("Instance satisfiable\n");

  delete[] vlineFilename;
}

void MaxSatInstance::printInfo(ostream& os) {
  os << "Number of Variables: " << numVars << endl;
  os << "Number of Clauses: " <<  numClauses << endl;
  os << "Ratio Clauses/Variables: " << (float)numClauses/numVars << endl;
  int negClauses = 0, posClauses = 0;
  for (int varNum=1; varNum<=numVars; varNum++) {
    negClauses += negClausesWithVar[ varNum ];
    posClauses += posClausesWithVar[ varNum ];
  }
  os << "Ratio Negative Clauses: " << (float)negClauses/numClauses << endl;
  os << "Ratio Positive Clauses: " << (float)posClauses/numClauses << endl;
  os << "Ratio Unit Clauses: " << (float)unitClauses/numClauses << endl;
  os << "Ratio Binary Clauses: " << (float)binaryClauses/numClauses << endl;
  os << "Ratio Ternary Clauses: " << (float)ternaryClauses/numClauses << endl;
}
