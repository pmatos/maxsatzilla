#!/usr/bin/env python

import sys

solver_times_path = sys.argv[1]
set_list = sys.argv[2:]

solvers = [ 'maxsatz-a-e', 'mspbo-a-e', 'msuncore-a1-ei' ]
times = { 'maxsatz-a-e' : 0.0, 'mspbo-a-e' : 0.0, 'msuncore-a1-ei' : 0.0 }

def sort_solvers( times ):
    from operator import itemgetter
    return sorted(times.items(), key=itemgetter(1))

for set in set_list:
    for solver in solvers:        
        times[ solver ] = 0
        for line in open( solver_times_path + '/' + solver + '-' + set + '.log' ):
            instance, solution, time = line.split()
            times[ solver ] += float( time )
        print 'Total time for solver ' + solver + ' in ' + set + ' ' + str( times[ solver ] )
    print sort_solvers( times )
