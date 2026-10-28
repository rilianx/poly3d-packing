/*
 * main.cpp
 *
 * 3D irregular strip packing of polyhedra (translations only), Algorithm 1 of the paper:
 * 1. greedy construction and Improve;
 * 2. strategic oscillation: the height is lowered by a step rho and MinimizeOverlap removes the overlap
 *    (guided local search with exact axis moves); on success, Improve and a new best solution; on failure, the
 *    height grows a quarter of the step and, once back at the best height, the search restarts from the best
 *    solution with one random piece moved to a random position (kick).
 *
 * example (from benchs/): poly3d "Stoyan et al. 2005/Example1.json" -t 120 --seed 1 -o sol.json
 */

#include <iostream>
#include <random>
#include <algorithm>
#include <cmath>
#include <ctime>
#include "args.hxx"
#include "Greedy.h"

using namespace std;
using namespace plp;

int main(int argc, char** argv){

	args::ArgumentParser parser("poly3d", "Solver for 3D irregular strip packing of polyhedra (ESICUP JSON format).");
	args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
	args::ValueFlag<int> _maxtime(parser, "int", "Time limit (CPU seconds). Default: 30", {'t', "timelimit"});
	args::ValueFlag<unsigned> _seed(parser, "int", "Seed (0: no perturbation of the starting points). Default: 0", {"seed"});
	args::ValueFlag<string> _out(parser, "file", "Output solution (ESICUP JSON)", {'o', "output"});
	args::ValueFlag<string> _root(parser, "dir", "Root folder of the piece paths (default: searched from the instance folder)", {"root"});
	args::ValueFlag<string> _dec(parser, "dir", "Folder (under the root) with the convex decompositions of the pieces. Default: decompositions", {"dec"});
	args::ValueFlag<string> _start(parser, "file", "Start from this solution (ESICUP JSON) instead of the construction, e.g., a solution found with a coarser decomposition (--dec)", {"start"});
	args::Group construction(parser, "Construction:");
	args::ValueFlag<double> _alpha(construction, "double", "Weight of the volume of the piece. Default: 1", {"alpha"});
	args::ValueFlag<double> _beta(construction, "double", "Weight of the height of the position. Default: 5.53", {"beta"});
	args::ValueFlag<double> _gamma(construction, "double", "Weight of the increase of the height of the load. Default: 0.77", {"gamma"});
	args::ValueFlag<int> _grid(construction, "int", "Starting points per axis (x, y). Default: 4", {"grid"});
	args::ValueFlag<double> _slide(construction, "double", "Weight sigma of x, y (towards the nearest walls) in Descend. Default: 0.065", {"slide"});
	args::Flag _greedy(construction, "greedy", "Only the construction and Improve", {"greedy"});
	args::Group search(parser, "Search:");
	args::ValueFlag<double> _rho(search, "double", "Step rho of the oscillation (proportion of the best height). Default: 0.01", {"rho"});
	args::ValueFlag<int> _passes(search, "int", "Maximum passes of MinimizeOverlap. Default: 500", {"passes"});
	args::Flag _additive(search, "additive", "Additive GLS weights (+1 for the overlapping pairs) instead of multiplicative", {"additive"});
	args::Flag _no_kick(search, "no_kick", "The restarts do not move any piece", {"no_kick"});
	args::Flag _no_improve(search, "no_improve", "No Improve after the construction and after each separation", {"no_improve"});
	args::Positional<std::string> _file(parser, "instance", "Instance (ESICUP JSON)");

	cout.precision(8);
	try{
		parser.ParseCLI(argc, argv);
	}catch (args::Help&){
		std::cout << parser;
		return 0;
	}catch (args::ParseError& e){
		std::cerr << e.what() << std::endl;
		std::cerr << parser;
		return 1;
	}
	if(!_file){
		std::cerr << parser;
		return 1;
	}

	int maxtime = (_maxtime)? _maxtime.Get() : 30;
	//alpha, beta, gamma and sigma tuned with Optuna
	double alpha = (_alpha)? _alpha.Get() : 1.0;
	double beta = (_beta)? _beta.Get() : 5.53;
	double gamma = (_gamma)? _gamma.Get() : 0.77;
	int grid = (_grid)? _grid.Get() : 4;
	double slide = (_slide)? _slide.Get() : 0.065;
	double rho = (_rho)? _rho.Get() : 0.01;
	int passes = (_passes)? _passes.Get() : 500;
	bool mult = !_additive;

	clock_t begin_time=clock();
	auto elapsed = [&]() { return double(clock()-begin_time)/CLOCKS_PER_SEC; };

	try{
		Instance inst = Instance::load(_file.Get(), (_root)? _root.Get() : "", (_dec)? _dec.Get() : "decompositions");
		cout << "instance: " << inst.name << endl;
		cout << "items: " << inst.n_items() << endl;
		cout << "height_lb: " << inst.height_lower_bound() << endl;
		for(const Piece& P : inst.pieces)
			if(!P.convex_parts)
				cout << "WARNING: " << P.name << " is not decomposed into convex parts (the hull of each part is used)" << endl;

		Placer placer(inst, grid, slide);
		if(_seed) placer.set_seed(_seed.Get());
		double lb = inst.height_lower_bound();
		mt19937 rng((_seed)? _seed.Get() : 0);

		//a new best solution is accepted only if it is feasible (guard against numerical errors)
		long rejected = 0;
		auto accept = [&](const Solution& S2) {
			if(S2.verify().feasible) return true;
			rejected++;
			cout << "WARNING: infeasible solution rejected" << endl;
			return false;
		};
		auto improve = [&](const vector<int>& pieces, vector<Vec3>& t) -> double {
			if(!_no_improve) return placer.improve(pieces, t);
			double h = 0;
			for(size_t i=0; i<pieces.size(); i++) h = max(h, t[i].z + inst.pieces[pieces[i]].bbox.extent().z);
			return h;
		};
		//pieces inside the container of height h (the ones that stick out are moved down, even if they overlap)
		auto clamp = [&](const vector<int>& ps, vector<Vec3>& t, double h) {
			for(size_t i=0; i<ps.size(); i++){
				Vec3 e = inst.pieces[ps[i]].bbox.extent();
				t[i].x = min(max(t[i].x, 0.0), max(0.0, inst.W-e.x));
				t[i].y = min(max(t[i].y, 0.0), max(0.0, inst.D-e.y));
				t[i].z = min(max(t[i].z, 0.0), max(0.0, h-e.z));
			}
		};

		//1. construction, or the given solution
		Solution S = (_start)? Solution::load(_start.Get(), inst) : greedy(placer, alpha, beta, gamma);
		vector<int> ps;
		vector<Vec3> ts;
		for(const Placement& p : S.items) { ps.push_back(inst.types[p.type].piece); ts.push_back(p.t); }
		int n = ps.size();
		if(_start && (int)inst.n_items() != n) { cerr << "ERROR: the starting solution does not have all the items" << endl; return 1; }
		double H = S.height();
		if(_start && !S.verify().feasible){
			//with another decomposition the solution may have small overlaps: they are removed raising the height
			Weights w0;
			double h = H;
			for(int k=0; k<50; k++, h *= 1.005){
				vector<Vec3> t2 = ts;
				clamp(ps, t2, h);
				if(placer.minimize_overlap(ps, t2, h, passes, w0, rng, mult)){
					Solution S2 = S;
					for(int i=0; i<n; i++) S2.items[i].t = t2[i];
					if(S2.verify().feasible) { S.items = S2.items; ts = t2; break; }
				}
			}
			if(!S.verify().feasible) { cerr << "ERROR: the starting solution could not be separated" << endl; return 1; }
			H = S.height();
		}
		cout << "construction_height (" << ((_start)? "start" : "greedy") << ", " << elapsed() << " s): " << H << endl;
		auto set_best = [&](const vector<Vec3>& t, double H2) {
			Solution S2 = S;
			for(int i=0; i<n; i++) S2.items[i].t = t[i];
			if(!accept(S2)) return false;
			S.items = S2.items;
			H = H2;
			return true;
		};
		set_best(ts, improve(ps, ts));
		cout << "improved_height (" << elapsed() << " s): " << H << endl;

		//2. strategic oscillation
		auto current = [&]() { vector<Vec3> t; for(const Placement& p : S.items) t.push_back(p.t); return t; };
		long separations = 0;
		int restarts = 0;
		Weights weights;   //GLS weights (persist during the oscillation)
		double hc = max(lb, H*(1-rho));
		ts = current();
		clamp(ps, ts, hc);
		uniform_int_distribution<int> pick(0, n-1);
		while(!_greedy && elapsed() < maxtime && H > lb*(1+1e-6)){
			separations++;
			if(placer.minimize_overlap(ps, ts, hc, passes, weights, rng, mult)){
				double H2 = improve(ps, ts);
				if(H2 < H - 1e-9*H && set_best(ts, H2))
					cout << "[best] (" << elapsed() << " s): height " << H << endl;
				ts = current();
				hc = max(lb, H*(1-rho));
				clamp(ps, ts, hc);
				weights.clear();
				continue;
			}
			//the height grows a little and the search continues from the overlapping layout
			hc += 0.25*rho*H;
			if(hc < H*(1-1e-6)) { clamp(ps, ts, hc); continue; }

			//restart from the best solution, with one random piece at a random position
			restarts++;
			ts = current();
			hc = max(lb, H*(1-rho));
			clamp(ps, ts, hc);
			int a = pick(rng);
			if(!_no_kick){
				Vec3 e = inst.pieces[ps[a]].bbox.extent();
				uniform_real_distribution<double> ux(0, max(0.0, inst.W-e.x)), uy(0, max(0.0, inst.D-e.y)), uz(0, max(0.0, hc-e.z));
				ts[a] = Vec3(ux(rng), uy(rng), uz(rng));
			}
			weights.clear();
			clamp(ps, ts, hc);
		}

		Report r = S.verify();
		cout << "separations: " << separations << endl;
		cout << "restarts: " << restarts << endl;
		cout << "rejected: " << rejected << endl;
		cout << "time: " << elapsed() << endl;
		cout << r;
		if(_out) S.save(_out.Get());
		return r.feasible? 0 : 2;
	}catch(exception& e){
		cerr << "ERROR: " << e.what() << endl;
		return 1;
	}
}
