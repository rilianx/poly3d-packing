/*
 * Greedy.cpp
 */

#include <cmath>
#include <limits>
#include <stdexcept>
#include "Greedy.h"

using namespace std;

namespace plp {

Solution greedy(Placer& placer, double alpha, double beta, double gamma) {
	const Instance& inst = placer.inst;
	double vmax = 0;
	for(const Piece& P : inst.pieces) vmax = max(vmax, P.volume);
	double h0 = inst.height_lower_bound();

	Solution S(inst);
	vector<int> left;
	int n_left = 0;
	for(const ItemType& k : inst.types) { left.push_back(k.demand); n_left += k.demand; }
	vector<PlacedPart> placed;
	vector<Vec3> pos;
	double top = 0;

	for(; n_left>0; n_left--){
		int best_k = -1;
		Vec3 best_t;
		double best_val = -numeric_limits<double>::max();
		for(size_t k=0; k<left.size(); k++){
			if(left[k]==0) continue;
			const Piece& P = inst.pieces[inst.types[k].piece];
			pos.clear();
			placer.candidates(inst.types[k].piece, placed, top, pos);
			for(const Vec3& t : pos){
				double ztop = t.z + P.bbox.extent().z;
				double val = alpha*log(P.volume/vmax) - beta*t.z/h0 - gamma*max(0.0, ztop-top)/h0;
				if(best_k < 0 || val > best_val) { best_k = k; best_t = t; best_val = val; }
			}
		}
		if(best_k < 0) throw runtime_error("greedy: no feasible position for the remaining pieces");

		int p = inst.types[best_k].piece;
		for(int a : placer.get_parts(p)){
			PlacedPart pp;
			pp.part = a;
			pp.t = best_t;
			pp.bbox.lo = placer.get_part(a).get_bbox().lo + best_t;
			pp.bbox.hi = placer.get_part(a).get_bbox().hi + best_t;
			placed.push_back(pp);
		}
		Placement pl;
		pl.type = best_k;
		pl.t = best_t;
		S.items.push_back(pl);
		left[best_k]--;
		top = max(top, best_t.z + inst.pieces[p].bbox.extent().z);
	}
	return S;
}

} /* namespace plp */
