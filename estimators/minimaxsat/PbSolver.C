#include "MiniSat.h"
#include "Sort.h"
#include "Debug.h"

extern int verbosity;


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


//=================================================================================================
// Interface required by parser:


int PbSolver::getVar(cchar* name)
{
    int ret;
    if (!name2index.peek(name, ret)){
        // Create new variable:
        Var x = index2name.size();
        index2name.push(xstrdup(name));
        n_occurs  .push(0);
        n_occurs  .push(0);
        //assigns   .push(toInt(l_Undef));
        sat_solver.newVar();        // (reserve one SAT variable for each PB variable)
        ret = name2index.set(index2name.last(), x);
    }
    return ret;
}


void PbSolver::allocConstrs(int n_vars, int n_constrs)
{
    declared_n_vars    = n_vars;
    declared_n_constrs = n_constrs;
}


void PbSolver::addGoal(const vec<Lit>& ps, const vec<Int>& Cs)
{
    /**/debug_names = &index2name;
    //**/reportf("MIN: "); dump(ps, Cs); reportf("\n");

    goal = new (xmalloc<char>(sizeof(Linear) + ps.size()*(sizeof(Lit) + sizeof(Int)))) Linear(ps, Cs, Int_MIN, Int_MAX);
}


bool PbSolver::addConstr(const vec<Lit>& ps, const vec<Int>& Cs, Int rhs, int ineq)
{
    //**/debug_names = &index2name;
    //**/static cchar* ineq_name[5] = { "<", "<=" ,"==", ">=", ">" };
    //**/reportf("CONSTR: "); dump(ps, Cs, assigns); reportf(" %s ", ineq_name[ineq+2]); dump(rhs); reportf("\n");

    vec<Lit>    norm_ps;
    vec<Int>    norm_Cs;
    Int         norm_rhs;

    #define Copy    do{ norm_ps.clear(); norm_Cs.clear(); for (int i = 0; i < ps.size(); i++) norm_ps.push(ps[i]), norm_Cs.push( Cs[i]); norm_rhs =  rhs; }while(0)
    #define CopyInv do{ norm_ps.clear(); norm_Cs.clear(); for (int i = 0; i < ps.size(); i++) norm_ps.push(ps[i]), norm_Cs.push(-Cs[i]); norm_rhs = -rhs; }while(0)

    if (ineq == 0){
        Copy;
        if (normalizePb(norm_ps, norm_Cs, norm_rhs))
            storePb(norm_ps, norm_Cs, norm_rhs, Int_MAX); //**/reportf("STORED: "), dump(constrs.last()), reportf("\n");

        CopyInv;
        if (normalizePb(norm_ps, norm_Cs, norm_rhs))
            storePb(norm_ps, norm_Cs, norm_rhs, Int_MAX); //**/reportf("STORED: "), dump(constrs.last()), reportf("\n");

    }else{
        if (ineq > 0)
            Copy;
        else{
            CopyInv;
            ineq = -ineq;
        }
        if (ineq == 2)
            ++norm_rhs;

        if (normalizePb(norm_ps, norm_Cs, norm_rhs))
            storePb(norm_ps, norm_Cs, norm_rhs, Int_MAX); //**/reportf("STORED: "), dump(constrs.last()), reportf("\n");
    }

    return ok;
}


//=================================================================================================


static Int gcd(Int small, Int big) {
    return (small == 0) ? big: gcd(big % small, small); }


// Normalize a PB constraint to only positive constants. Depends on:
//
//   bool    ok            -- Will be set to FALSE if constraint is unsatisfiable.
//   lbool   value(Lit)    -- Returns the value of a literal (however, it is sound to always return 'l_Undef', but produces less efficient results)
//   bool    addUnit(Lit)  -- Enqueue unit fact for propagation (returns FALSE if conflict detected).
//
// The two vectors 'ps' and 'Cs' (which are modififed by this method) should be interpreted as:
//
//   'p[0]*C[0] + p[1]*C[1] + ... + p[N-1]*C[N-1] >= C[N]'
//
// The method returns TRUE if constraint was normalized, FALSE if the constraint was already
// satisfied or determined contradictory. The vectors 'ps' and 'Cs' should ONLY be used if
// TRUE is returned.
//
bool PbSolver::normalizePb(vec<Lit>& ps, vec<Int>& Cs, Int& C)
{
    assert(ps.size() == Cs.size());
    if (!ok) return false;

    // Remove assigned literals and literals with zero coefficients:
    int new_sz = 0;
    for (int i = 0; i < ps.size(); i++){
        if (value(ps[i]) != l_Undef){
            if (value(ps[i]) == l_True)
                C -= Cs[i];
        }else if (Cs[i] != 0){
            ps[new_sz] = ps[i];
            Cs[new_sz] = Cs[i];
            new_sz++;
        }
    }
    ps.shrink(ps.size() - new_sz);
    Cs.shrink(Cs.size() - new_sz);
    //**/reportf("No zero, no assigned :"); for (int i = 0; i < ps.size(); i++) reportf(" %d*%sx%d", Cs[i], sign(ps[i])?"~":"", var(ps[i])); reportf(" >= %d\n", C);

    // Group all x/~x pairs
    //
    Map<Var, Pair<Int,Int> >    var2consts(Pair_new(0,0));     // Variable -> negative/positive polarity constant
    for (int i = 0; i < ps.size(); i++){
        Var             x      = var(ps[i]);
        Pair<Int,Int>   consts = var2consts.at(x);
        if (sign(ps[i]))
            consts.fst += Cs[i];
        else
            consts.snd += Cs[i];
        var2consts.set(x, consts);
    }

    // Normalize constants to positive values only:
    //
    vec<Pair<Var, Pair<Int,Int> > > all;
    var2consts.pairs(all);
    vec<Pair<Int,Lit> > Csps(all.size());
    for (int i = 0; i < all.size(); i++){
        if (all[i].snd.fst < all[i].snd.snd){
            // Negative polarity will vanish
            C -= all[i].snd.fst;
            Csps[i] = Pair_new(all[i].snd.snd - all[i].snd.fst, Lit(all[i].fst));
        }else{
            // Positive polarity will vanish
            C -= all[i].snd.snd;
            Csps[i] = Pair_new(all[i].snd.fst - all[i].snd.snd, ~Lit(all[i].fst));
        }
    }

    // Sort literals on growing constant values:
    //
    sort(Csps);     // (use lexicographical order of 'Pair's here)
    Int     sum = 0;
    for (int i = 0; i < Csps.size(); i++){
        Cs[i] = Csps[i].fst, ps[i] = Csps[i].snd, sum += Cs[i];
        if (sum < 0) fprintf(stderr, "ERROR! Too large constants encountered in constraint.\n"), exit(1);
    }
    ps.shrink(ps.size() - Csps.size());
    Cs.shrink(Cs.size() - Csps.size());

    // Propagate already present consequences:
    //
    bool changed;
    do{
        changed = false;
        while (ps.size() > 0 && sum-Cs.last() < C){
            changed = true;
            if (!addUnit(ps.last())){
                ok = false;
                return false; }
            sum -= Cs.last();
            C   -= Cs.last();
            ps.pop(); Cs.pop();
        }

        // Trivially true or false?
        if (C <= 0)
            return false;
        if (sum < C){
            ok = false;
            return false; }
        assert(sum - Cs[ps.size()-1] >= C);

        // GCD:
        assert(Cs.size() > 0);
        Int     div = Cs[0];
        for (int i = 1; i < Cs.size(); i++)
            div = gcd(div, Cs[i]);
        for (int i = 0; i < Cs.size(); i++)
            Cs[i] /= div;
        C = (C + div-1) / div;
        if (div != 1)
            changed = true;

        // Trim constants:
        for (int i = 0; i < Cs.size(); i++)
            if (Cs[i] > C)
                changed = true,
                Cs[i] = C;
    }while (changed);

    //**/reportf("Normalized constraint:"); for (int i = 0; i < ps.size(); i++) reportf(" %d*%sx%d", Cs[i], sign(ps[i])?"~":"", var(ps[i])); reportf(" >= %d\n", C);

    return true;
}


void PbSolver::storePb(const vec<Lit>& ps, const vec<Int>& Cs, Int lo, Int hi)
{
    assert(ps.size() == Cs.size());
    for (int i = 0; i < ps.size(); i++)
        n_occurs[index(ps[i])]++;
    constrs.push(new (mem.alloc(sizeof(Linear) + ps.size()*(sizeof(Lit) + sizeof(Int)))) Linear(ps, Cs, lo, hi));
    //**/reportf("STORED: "), dump(constrs.last()), reportf("\n");
}


//=================================================================================================


// Returns TRUE if the constraint should be deleted. May set the 'ok' flag to false
bool PbSolver::propagate(Linear& c)
{
    //**/reportf("BEFORE propagate()\n");
    //**/dump(c, sat_solver.assigns_ref()); reportf("\n");

    // Remove assigned literals:
    Int     sum = 0, true_sum = 0;
    int     j = 0;
    for (int i = 0; i < c.size; i++){
        assert(c(i) > 0);
        if (value(c[i]) == l_Undef){
            sum += c(i);
            c(j) = c(i);
            c[j] = c[i];
            j++;
        }else if (value(c[i]) == l_True)
            true_sum += c(i);
    }
    c.size = j;
    if (c.lo != Int_MIN) c.lo -= true_sum;
    if (c.hi != Int_MAX) c.hi -= true_sum;

    // Propagate:
    while (c.size > 0){
        if (c(c.size-1) > c.hi){
            addUnit(~c[c.size-1]);
            sum -= c(c.size-1);
            c.size--;
        }else if (sum - c(c.size-1) < c.lo){
            addUnit(c[c.size-1]);
            sum -= c(c.size-1);
            if (c.lo != Int_MIN) c.lo -= c(c.size-1);
            if (c.hi != Int_MAX) c.hi -= c(c.size-1);
            c.size--;
        }else
            break;
    }

    if (c.lo <= 0)  c.lo = Int_MIN;
    if (c.hi > sum) c.hi = Int_MAX;

    //**/reportf("AFTER propagate()\n");
    //**/dump(c, sat_solver.assigns_ref()); reportf("\n\n");
    if (c.size == 0){
        if (c.lo > 0 || c.hi < 0)
            ok = false;
        return true;
    }else
        return c.lo == Int_MIN && c.hi == Int_MAX;
}


void PbSolver::propagate()
{
    if (nVars() == 0) return;
    if (occur.size() == 0) setupOccurs();

    if (opt_verbosity >= 1) reportf("  -- Unit propagations: ", constrs.size());
    bool found = false;

    while (propQ_head < trail.size()){
        //**/reportf("propagate("); dump(trail[propQ_head]); reportf(")\n");
        Var     x = var(trail[propQ_head++]);
        for (int pol = 0; pol < 2; pol++){
            vec<int>&   cs = occur[index(Lit(x,pol))];
            for (int i = 0; i < cs.size(); i++){
                if (constrs[cs[i]] == NULL) continue;
                int trail_sz = trail.size();
                if (propagate(*constrs[cs[i]]))
                    constrs[cs[i]] = NULL;
                if (opt_verbosity >= 1 && trail.size() > trail_sz) found = true, reportf("p");
                if (!ok) return;
            }
        }
    }

    if (opt_verbosity >= 1) {
        if (!found) reportf("(none)\n");
        else        reportf("\n");
    }

    occur.clear(true);
}


void PbSolver::setupOccurs()
{
    // Allocate vectors of right capacities:
    occur.growTo(nVars()*2);
    assert(nVars() == pb_n_vars);
    for (int i = 0; i < nVars()*2; i++){
        vec<int> tmp(xmalloc<int>(n_occurs[i]), n_occurs[i]); tmp.clear();
        tmp.moveTo(occur[i]); }

    // Fill vectors:
    for (int i = 0; i < constrs.size(); i++){
        if (constrs[i] == NULL) continue;
        for (int j = 0; j < constrs[i]->size; j++)
            assert(occur[index((*constrs[i])[j])].size() < n_occurs[index((*constrs[i])[j])]),
            occur[index((*constrs[i])[j])].push(i);
    }
}


// Left-hand side equal
static bool lhsEq(const Linear& c, const Linear& d) {
    if (c.size == d.size){
        for (int i = 0; i < c.size; i++) if (c[i] != d[i] || c(i) != d(i)) return false;
        return true;
    }else return false; }
// Left-hand side equal complementary (all literals negated)
static bool lhsEqc(const Linear& c, const Linear& d) {
    if (c.size == d.size){
        for (int i = 0; i < c.size; i++) if (c[i] != ~d[i] || c(i) != d(i)) return false;
        return true;
    }else return false; }


void PbSolver::findIntervals()
{
    if (opt_verbosity >= 1)
        reportf("  -- Detecting intervals from adjacent constraints: ");

    bool found = false;
    int i = 0;
    Linear* prev;
    for (; i < constrs.size() && constrs[i] == NULL; i++);
    if (i < constrs.size()){
        prev = constrs[i++];
        for (; i < constrs.size(); i++){
            if (constrs[i] == NULL) continue;
            Linear& c = *prev;
            Linear& d = *constrs[i];

            if (lhsEq(c, d)){
                if (d.lo < c.lo) d.lo = c.lo;
                if (d.hi > c.hi) d.hi = c.hi;
                constrs[i-1] = NULL;
                if (opt_verbosity >= 1) reportf("=");
                found = true;
            }
            if (lhsEqc(c, d)){
                Int sum = 0;
                for (int j = 0; j < c.size; j++)
                    sum += c(j);
                Int lo = (c.hi == Int_MAX) ? Int_MIN : sum - c.hi;
                Int hi = (c.lo == Int_MIN) ? Int_MAX : sum - c.lo;
                if (d.lo < lo) d.lo = lo;
                if (d.hi > hi) d.hi = hi;
                constrs[i-1] = NULL;
                if (opt_verbosity >= 1) reportf("#");
                found = true;
            }

            prev = &d;
        }
    }
    if (opt_verbosity >= 1) {
        if (!found) reportf("(none)\n");
        else        reportf("\n");
    }
}


bool PbSolver::rewriteAlmostClauses()
{
    vec<Lit>    ps;
    vec<Int>    Cs;
    bool        found = false;
    int         n_splits = 0;
    char        buf[20];

    if (opt_verbosity >= 1)
        //reportf("  -- Clauses(.)/Splits(s): ");
    for (int i = 0; i < constrs.size(); i++){
        if (constrs[i] == NULL) continue;
        Linear& c   = *constrs[i];
        assert(c.lo != Int_MIN || c.hi != Int_MAX);

        if (c.hi != Int_MAX) continue;

        int n = c.size;
        for (; n > 0 && c(n-1) == c.lo; n--);

        if (n <= 1){
            // Pure clause:
            //if (opt_verbosity >= 1) reportf(".");
            found = true;
            ps.clear();
            for (int j = n; j < c.size; j++)
                ps.push(c[j]);
            sat_solver.addClause(ps);

            constrs[i] = NULL;      // Remove this clause

        }else if (c.size-n >= 3){
            // Split clause part:
            //if (opt_verbosity >= 1) reportf("s");
            found = true;
            sprintf(buf, "@split%d", n_splits);
            n_splits++;
            Var x = getVar(buf); assert(x == sat_solver.nVars()-1);
            ps.clear();
            ps.push(Lit(x));
            for (int j = n; j < c.size; j++)
                ps.push(c[j]);
            sat_solver.addClause(ps);
            if (!sat_solver.okay()){
                //reportf("\n");
                return false; }

            ps.clear();
            Cs.clear();
            ps.push(~Lit(x));
            Cs.push(c.lo);
            for (int j = 0; j < n; j++)
                ps.push(c[j]),
                Cs.push(c(j));
            if (!addConstr(ps, Cs, c.lo, 1)){
                //reportf("\n");
                return false; }

            constrs[i] = NULL;      // Remove this clause
        }
    }

    /*
    if (opt_verbosity >= 1) {
        if (!found) reportf("(none)\n");
        else        reportf("\n");
    }
    */
    return true;
}


//=================================================================================================
// Main solver/optimizer:

/*
static
Int evalGoal(Linear& goal, vec<lbool>& model)
{
    Int sum = 0;
    for (int i = 0; i < goal.size; i++){
        assert(model[var(goal[i])] != l_Undef);
        if (( sign(goal[i]) && model[var(goal[i])] == l_False)
        ||  (!sign(goal[i]) && model[var(goal[i])] == l_True )
        )
            sum += goal(i);
    }
    return sum;
}
*/

void PbSolver::translateToMaxSAT(vec<Lit>& ps,vec<Int>& Cs,cchar* filename)
{

  int v=-1,cla=0,spare=0,hard=0;
  int & n_vars=v, & n_clauses=cla;
  
  FILE*   out = fopen(filename, "wb"); 
  assert(out != NULL);
  
  for (int i = 0; i < ps.size(); i++)
  {
  	
  	hard+=abs(toint(Cs[i]));
	if(toint(Cs[i])<0) spare+=abs(toint(Cs[i]));
  }
  
  fprintf(out, "c SUBTRACT %d \n", spare);
  sat_solver.countClauses(n_vars,n_clauses);
  n_clauses=n_clauses+ps.size();
  //fprintf(out, "c PSEUDOBOOLEAN %d \n",toint(hard));
  fprintf(out, "p wcnf %d %d %d\n", n_vars, n_clauses, toint(hard));

  for (int i = 0; i < ps.size(); i++)
  {
        if(toint(Cs[i])>=0)
	{
  		fprintf(out, "%d -%d 0\n",toint(Cs[i]),var(ps[i])+1);
	}
	else
	{
		fprintf(out, "%d %d 0\n",abs(toint(Cs[i])),var(ps[i])+1);
	}
  }
  
  sat_solver.exportWcnf(out,hard);
  
  fclose(out);
}

void PbSolver::solve(solve_Command cmd)
{
    if (!ok) return;

    // Convert constraints:
    pb_n_vars = nVars();
    pb_n_constrs = constrs.size();
    if (opt_verbosity >= 1) reportf("Converting %d PB-constraints to clauses...\n", constrs.size());
    propagate();
    if (!convertPbs(true)){ assert(!ok); return; }

    // Freeze goal function variables (for SatELite):
    if (goal != NULL){
        for (int i = 0; i < goal->size; i++)
            sat_solver.freeze(var((*goal)[i]));
    }

    // Solver (optimize):
    sat_solver.setVerbosity(opt_verbosity);

    vec<Lit> goal_ps; if (goal != NULL){ for (int i = 0; i < goal->size; i++)  {goal_ps.push((*goal)[i]);}}
    vec<Int> goal_Cs; if (goal != NULL){ for (int i = 0; i < goal->size; i++) goal_Cs.push((*goal)(i)); }
    assert(best_goalvalue == Int_MAX);

    if (opt_polarity_sug != 0){
        for (int i = 0; i < goal_Cs.size(); i++)
            sat_solver.suggestPolarity(var(goal_ps[i]), ((goal_Cs[i]*opt_polarity_sug > 0 && !sign(goal_ps[i])) || (goal_Cs[i]*opt_polarity_sug < 0 && sign(goal_ps[i]))) ? l_False : l_True);
    }

    if (opt_convert_goal != ct_Undef)
        opt_convert = opt_convert_goal;
    opt_sort_thres *= opt_goal_bias;
    
    // FEDE_NEW

    if (opt_goal != Int_MAX)
        addConstr(goal_ps, goal_Cs, opt_goal, -1),
        convertPbs(false);
	
    // FEDE_NEW
    if (opt_wcnf != NULL)
    {
	//normalizePb(goal_ps, goal_Cs, opt_goal);
        reportf("Exporting WCNF to: \b%s\b\n", opt_wcnf);
	translateToMaxSAT(goal_ps, goal_Cs,opt_wcnf);
        exit(0);
    }
    
    if (opt_cnf != NULL)
        reportf("Exporting CNF to: \b%s\b\n", opt_cnf),
        sat_solver.exportCnf(opt_cnf),
        exit(0);
	
	sat_solver.unitSoftClauses(goal_ps, goal_Cs);
	
	sat_solver.solve();

	/*
    bool    sat = false;
    int     n_solutions = 0;    // (only for AllSolutions mode)
    while (sat_solver.solve()){
        sat = true;
        if (cmd == sc_AllSolutions){
            vec<Lit>    ban;
            n_solutions++;
            reportf("MODEL# %d:", n_solutions);
            for (Var x = 0; x < pb_n_vars; x++){
                assert(sat_solver.model()[x] != l_Undef);
                ban.push(Lit(x, sat_solver.model()[x] == l_True));
                reportf(" %s%s", (sat_solver.model()[x] == l_False)?"-":"", index2name[x]);
            }
            reportf("\n");
            sat_solver.addClause(ban);

        }else{
            best_model.clear();
            for (Var x = 0; x < pb_n_vars; x++)
                assert(sat_solver.model()[x] != l_Undef),
                best_model.push(sat_solver.model()[x] == l_True);

            if (goal == NULL)   // ((fix: moved here Oct 4, 2005))
                break;

            best_goalvalue = evalGoal(*goal, sat_solver.model());
            if (cmd == sc_FirstSolution) break;

            if (opt_verbosity >= 1){
                char* tmp = toString(best_goalvalue);
                reportf("\bFound solution: %s\b\n", tmp);
                xfree(tmp); }
            if (!addConstr(goal_ps, goal_Cs, best_goalvalue, -2))
                break;
            convertPbs(false);
        }
    }
    if (goal == NULL && sat)
        best_goalvalue = Int_MIN;       // (found model, but don't care about it)
	*/
	/*
    if (opt_verbosity >= 1){
        if      (!sat)
            reportf("\bUNSATISFIABLE\b\n");
        else if (goal == NULL)
            reportf("\bSATISFIABLE: No goal function specified.\b\n");
        else if (cmd == sc_FirstSolution){
            char* tmp = toString(best_goalvalue);
            reportf("\bFirst solution found: %s\b\n", tmp);
            xfree(tmp);
        }else{
            char* tmp = toString(best_goalvalue);
            reportf("\bOptimal solution: %s\b\n", tmp);
            xfree(tmp);
        }
    }
    */
}


void PbSolver::solveCnf(solve_Command cmd)
{
    bool sol;
    sol=sat_solver.solve();
    /*
    if(sol) reportf("\bSATISFIABLE\b\n");
    else reportf("\bUNSATISFIABLE\b\n");
    */
}

