/*
 * Placer.cpp
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include "Placer.h"

using namespace std;

namespace plp {

Placer::Placer(const Instance& inst, int grid, double slide, double eps) :
		inst(inst), grid(max(grid,2)), slide(slide), eps(eps), seed(0) {
	double scale = max(max(inst.W, inst.D), inst.height_lower_bound());
	tol = 1e-9*scale;
	for(const Piece& P : inst.pieces){
		piece_parts.push_back(vector<int>());
		for(const ConvexPolytope& C : P.parts){
			piece_parts.back().push_back(parts.size());
			parts.push_back(&C);
		}
	}
}

const ConvexPolytope& Placer::nfp(int a, int b) {
	if(nfp_idx.size() != parts.size()) nfp_idx.resize(parts.size());
	vector<const ConvexPolytope*>& row = nfp_idx[a];
	if(row.empty()) row.assign(parts.size(), NULL);
	if(row[b]) return *row[b];
	auto it = nfps.find(make_pair(a,b));
	if(it==nfps.end()){
		vector<Vec3> pts;
		for(const Vec3& vb : parts[b]->get_vertices())
			for(const Vec3& va : parts[a]->get_vertices())
				pts.push_back(vb - va);
		it = nfps.insert(make_pair(make_pair(a,b), ConvexPolytope::hull(pts, 1e-9, false))).first;
	}
	row[b] = &it->second;
	return it->second;
}

bool Placer::feasible(int p, const Vec3& t, const vector<PlacedPart>& placed) const {
	for(int a : piece_parts[p]){
		ConvexPolytope A = parts[a]->translated(t);
		for(const PlacedPart& B : placed){
			if(!A.get_bbox().overlaps(B.bbox, eps)) continue;
			if(penetration_depth(A, parts[B.part]->translated(B.t), eps) > eps) return false;
		}
	}
	return true;
}

Vec3 Placer::descend(int p, Vec3 t, const Vec3& c, const vector<PlacedPart>& placed) {
	Vec3 e = inst.pieces[p].bbox.extent();
	double delta = max(max(e.x, e.y), e.z);
	Vec3 g = -c / c.norm();

	for(int round=0; round<100; round++){
		//trust region: box around t inside the container (z does not increase)
		Box3 R;
		R.lo = Vec3(max(0.0, t.x-delta), max(0.0, t.y-delta), max(0.0, t.z-delta));
		R.hi = Vec3(min(inst.W-e.x, t.x+delta), min(inst.D-e.y, t.y+delta), t.z);

		lp.clear();
		for(int k=0; k<3; k++){
			Vec3 u; u[k]=1;
			lp.add(u, R.hi[k]);
			lp.add(-u, -R.lo[k]);
		}

		for(int a : piece_parts[p]){
			const Box3& ab = parts[a]->get_bbox();
			for(const PlacedPart& B : placed){
				//box of the translations for which a and b may overlap
				Box3 nb;
				nb.lo = B.bbox.lo - ab.hi;
				nb.hi = B.bbox.hi - ab.lo;
				if(!nb.overlaps(R, -tol)) continue;

				const ConvexPolytope& N = nfp(a, B.part);
				Vec3 rel = t - B.t;
				//facet satisfied at t that allows the longest step along the descent direction g:
				//slack/(-n.g) if n.g < 0 (infinite if n.g >= 0; then the largest n.g is preferred)
				int best=-1, worst=-1;
				double best_step=-1, best_g=0, worst_s=-numeric_limits<double>::max();
				const vector<Plane>& F = N.get_facets();
				for(int f=0; f<(int)F.size(); f++){
					double s = dot(F[f].n, rel) - F[f].d;
					if(s > worst_s) { worst_s=s; worst=f; }
					if(s < -tol) continue;
					double ng = dot(F[f].n, g);
					double step = (ng >= -1e-12)? numeric_limits<double>::max() : max(0.0,s)/(-ng);
					if(step > best_step || (step==best_step && ng > best_g)) { best_step=step; best_g=ng; best=f; }
				}
				if(best<0) best=worst;
				//n.(t - tb) >= d  <=>  -n.t <= -d - n.tb
				lp.add(-F[best].n, -F[best].d - dot(F[best].n, B.t));
			}
		}

		Vec3 t2 = lp.minimize(c, t);
		//with nearly dependent active rows the LP may return a point outside its region (no valid descent)
		bool inside = true;
		for(int k=0; k<3; k++)
			inside = inside && std::isfinite(t2[k]) && t2[k] >= R.lo[k] - 1e3*tol && t2[k] <= R.hi[k] + 1e3*tol;
		if(!inside || dot(c, t2) >= dot(c, t) - tol) break;
		t = t2;
	}
	return t;
}

void Placer::candidates(int p, const vector<PlacedPart>& placed, double top, vector<Vec3>& out) {
	Vec3 e = inst.pieces[p].bbox.extent();
	if(e.x > inst.W + eps || e.y > inst.D + eps) return;
	double X = max(0.0, inst.W - e.x), Y = max(0.0, inst.D - e.y);

	for(int i=0; i<grid; i++)
		for(int j=0; j<grid; j++){
			Vec3 t0(X*i/(grid-1), Y*j/(grid-1), top);
			if(seed){
				uniform_real_distribution<double> u(0.0, 100.0*tol);
				t0.x = min(X, t0.x + ((i==0)? u(rng) : -u(rng)));
				t0.y = min(Y, t0.y + ((j==0)? u(rng) : -u(rng)));
				t0.x = max(0.0, t0.x); t0.y = max(0.0, t0.y);
			}
			//gravity and slide towards the nearest walls
			Vec3 c((t0.x <= X/2)? slide : -slide, (t0.y <= Y/2)? slide : -slide, 1.0);
			Vec3 t = descend(p, t0, c, placed);

			//small numerical violations of the container bounds
			t.x = min(max(t.x, 0.0), X);
			t.y = min(max(t.y, 0.0), Y);
			t.z = max(t.z, 0.0);

			bool repeated=false;
			for(const Vec3& u : out) if((u-t).norm() <= 1e3*tol) { repeated=true; break; }
			if(repeated || !feasible(p, t, placed)) continue;
			out.push_back(t);
		}
}

void Placer::place_all(const vector<int>& pieces, const vector<Vec3>& t, int skip, vector<PlacedPart>& out) const {
	out.clear();
	for(size_t i=0; i<pieces.size(); i++){
		if((int)i==skip) continue;
		for(int k : piece_parts[pieces[i]]){
			PlacedPart pp;
			pp.part = k;
			pp.t = t[i];
			pp.bbox.lo = parts[k]->get_bbox().lo + t[i];
			pp.bbox.hi = parts[k]->get_bbox().hi + t[i];
			out.push_back(pp);
		}
	}
}

double Placer::improve(const vector<int>& pieces, vector<Vec3>& t, int max_passes) {
	int n = pieces.size();
	auto height = [&](int skip) {
		double h=0;
		for(int i=0; i<n; i++) if(i!=skip) h = max(h, t[i].z + inst.pieces[pieces[i]].bbox.extent().z);
		return h;
	};

	double H = height(-1);
	vector<PlacedPart> others;
	vector<Vec3> cand;
	for(int pass=0; pass<max_passes; pass++){
		//bottom-up
		vector<int> order(n);
		for(int i=0; i<n; i++) order[i]=i;
		sort(order.begin(), order.end(), [&](int a, int b){ return t[a].z < t[b].z; });

		for(int i : order){
			int p = pieces[i];
			double X = max(0.0, inst.W - inst.pieces[p].bbox.extent().x);
			double Y = max(0.0, inst.D - inst.pieces[p].bbox.extent().y);
			place_all(pieces, t, i, others);

			//the piece descends again: gravity and slide towards the nearest walls
			Vec3 c((t[i].x <= X/2)? slide : -slide, (t[i].y <= Y/2)? slide : -slide, 1.0);
			Vec3 u = descend(p, t[i], c, others);
			if(dot(c,u) < dot(c,t[i]) - tol && feasible(p, u, others)) t[i] = u;

			//a piece that defines the height moves to its lowest candidate position
			double ztop = t[i].z + inst.pieces[p].bbox.extent().z;
			if(ztop < H - tol) continue;
			cand.clear();
			candidates(p, others, height(i), cand);
			Vec3 best = t[i];
			for(const Vec3& v : cand)
				if(v.z < best.z - tol) best = v;
			t[i] = best;
		}

		double H2 = height(-1);
		if(H2 >= H - tol) { H = H2; break; }
		H = H2;
	}
	return H;
}

double Placer::overlap(const vector<int>& pieces, const vector<Vec3>& t) const {
	int n = pieces.size();
	vector< vector<ConvexPolytope> > P(n);
	vector<Box3> box(n);
	for(int i=0; i<n; i++)
		for(int a : piece_parts[pieces[i]]){
			P[i].push_back(parts[a]->translated(t[i]));
			box[i].add(P[i].back().get_bbox());
		}
	double total=0;
	for(int i=0; i<n; i++)
		for(int j=i+1; j<n; j++){
			if(!box[i].overlaps(box[j], eps)) continue;
			double d=0;
			for(const ConvexPolytope& A : P[i])
				for(const ConvexPolytope& B : P[j])
					if(A.get_bbox().overlaps(B.get_bbox(), eps)) d = max(d, penetration_depth(A, B));
			if(d > eps) total += d;
		}
	return total;
}

namespace {

/** penetration depth of rel in the NFP (0 if rel is outside) */
inline double nfp_depth(const ConvexPolytope& N, const Vec3& rel) {
	double m = numeric_limits<double>::max();
	for(const Plane& f : N.get_facets()){
		m = min(m, f.d - dot(f.n, rel));
		if(m <= 0) return 0;
	}
	return m;
}

}

double Placer::pair_penetration(const vector<int>& pieces, const vector<Vec3>& t, int i, int j) {
	double s=0;
	Vec3 rel = t[i] - t[j];
	for(int a : piece_parts[pieces[i]])
		for(int b : piece_parts[pieces[j]]){
			const Box3& ab = parts[a]->get_bbox();
			const Box3& bb = parts[b]->get_bbox();
			bool near=true;
			for(int c=0; c<3 && near; c++) near = rel[c] > bb.lo[c]-ab.hi[c] && rel[c] < bb.hi[c]-ab.lo[c];
			if(near) s += nfp_depth(nfp(a, b), rel);
		}
	return s;
}

double Placer::axis_move(const vector<int>& pieces, vector<Vec3>& t, int i, int axis, double H, const Weights& w) {
	int n = pieces.size();
	Vec3 e = inst.pieces[pieces[i]].bbox.extent();
	double up[3] = { max(0.0, inst.W-e.x), max(0.0, inst.D-e.y), max(0.0, H-e.z) };
	double lo = 0, hi = up[axis], s0 = t[i][axis];

	//terms (pairs of parts whose NFP crosses the line). Inside the interval (L,U) the penetration is
	//min(cap, min_f (C_f - K_f s)): the facets with K_f = n_f[axis] != 0 are stored in C and K (from off, len)
	struct Term { double w, L, U, cap; int off, len; };
	vector<Term> terms;
	vector<double> C, K;
	vector<double> cand = { lo, hi, min(max(s0, lo), hi) };
	for(int j=0; j<n; j++){
		if(j==i) continue;
		Vec3 rel = t[i] - t[j];
		//filter with the boxes of the pieces
		const Box3& bi = inst.pieces[pieces[i]].bbox;
		const Box3& bj = inst.pieces[pieces[j]].bbox;
		bool cross=true;
		for(int c=0; c<3 && cross; c++)
			if(c!=axis) cross = rel[c] > bj.lo[c]-bi.hi[c] && rel[c] < bj.hi[c]-bi.lo[c];
			else cross = rel[c]+(hi-s0) > bj.lo[c]-bi.hi[c] && rel[c]+(lo-s0) < bj.hi[c]-bi.lo[c];
		if(!cross) continue;
		auto it = w.find(make_pair(min(i,j), max(i,j)));
		double wij = 1.0 + ((it==w.end())? 0.0 : it->second);
		for(int a : piece_parts[pieces[i]])
			for(int b : piece_parts[pieces[j]]){
				const Box3& ab = parts[a]->get_bbox();
				const Box3& bb = parts[b]->get_bbox();
				//the line (rel with the coordinate axis free) must cross the box of the NFP in the other coordinates
				bool near=true;
				for(int c=0; c<3 && near; c++)
					if(c!=axis) near = rel[c] > bb.lo[c]-ab.hi[c] && rel[c] < bb.hi[c]-ab.lo[c];
					else near = rel[c]+(hi-s0) > bb.lo[c]-ab.hi[c] && rel[c]+(lo-s0) < bb.hi[c]-ab.lo[c];
				if(!near) continue;
				//interval of s in which rel lies inside the NFP: n.rel(s) <= d for all the facets
				const ConvexPolytope& N = nfp(a, b);
				double L = -numeric_limits<double>::max(), U = numeric_limits<double>::max(), cap = numeric_limits<double>::max();
				size_t off = C.size();
				for(const Plane& f : N.get_facets()){
					double k = f.n[axis];
					double r = f.d - dot(f.n, rel) + k*s0;   //penetration through the facet: r - k*s
					if(fabs(k) < 1e-12) { cap = min(cap, r); if(cap < 0) break; continue; }
					if(k > 0) U = min(U, r/k); else L = max(L, r/k);
					C.push_back(r);
					K.push_back(k);
				}
				if(cap < 0 || L >= U || U <= lo || L >= hi) { C.resize(off); K.resize(off); continue; }
				terms.push_back(Term{wij, L, U, cap, (int) off, (int) (C.size()-off)});
				if(L > lo) cand.push_back(L);
				if(U < hi) cand.push_back(U);
			}
	}
	if(terms.empty()) return 0;

	auto depth = [&](const Term& tm, double s) {
		double m = tm.cap;
		const double* c = &C[tm.off];
		const double* k = &K[tm.off];
		for(int f=0; f<tm.len; f++) m = min(m, c[f] - k[f]*s);
		return max(0.0, m);
	};
	//each term adds only at the breakpoints inside its interval (sorted breakpoints)
	sort(cand.begin(), cand.end());
	cand.erase(unique(cand.begin(), cand.end()), cand.end());
	vector<double> val(cand.size(), 0.0);
	double f0 = 0;
	for(const Term& tm : terms){
		size_t k0 = upper_bound(cand.begin(), cand.end(), tm.L) - cand.begin();
		size_t k1 = lower_bound(cand.begin(), cand.end(), tm.U) - cand.begin();
		for(size_t k=k0; k<k1; k++) val[k] += tm.w * depth(tm, cand[k]);
		if(s0 > tm.L && s0 < tm.U) f0 += tm.w * depth(tm, s0);
	}
	double best = f0, bs = s0;
	for(size_t k=0; k<cand.size(); k++){
		//ties: the smallest coordinate (gravity / towards the walls)
		if(val[k] < best - 1e-12 || (fabs(val[k]-best) <= 1e-12 && cand[k] < bs)) { best=val[k]; bs=cand[k]; }
	}
	t[i][axis] = bs;
	return f0 - best;
}

bool Placer::minimize_overlap(const vector<int>& pieces, vector<Vec3>& t, double H, int max_passes,
		Weights& weights, mt19937& rng, bool mult) {
	int n = pieces.size();
	vector<int> order(n);
	for(int i=0; i<n; i++) order[i]=i;

	//penetrations between pairs (the row of a piece is recomputed when it moves)
	vector< vector<double> > P(n, vector<double>(n, 0.0));
	for(int i=0; i<n; i++)
		for(int j=i+1; j<n; j++) P[i][j] = P[j][i] = pair_penetration(pieces, t, i, j);

	for(int pass=0; pass<max_passes; pass++){
		//overlapping pairs
		vector< pair<int,int> > ov;
		vector<double> pen;
		vector<bool> involved(n, false);
		double pmax = 0;
		for(int i=0; i<n; i++)
			for(int j=i+1; j<n; j++)
				if(P[i][j] > eps) { ov.push_back(make_pair(i,j)); pen.push_back(P[i][j]); involved[i]=involved[j]=true; pmax = max(pmax, P[i][j]); }
		if(ov.empty()) return overlap(pieces, t) <= eps;

		//one pass of axis moves over the overlapping pieces, in random order
		shuffle(order.begin(), order.end(), rng);
		double gain=0;
		for(int i : order){
			if(!involved[i]) continue;
			Vec3 old = t[i];
			int axes[3] = {0,1,2};
			shuffle(axes, axes+3, rng);
			for(int ax : axes) gain += axis_move(pieces, t, i, ax, H, weights);
			if(t[i].x!=old.x || t[i].y!=old.y || t[i].z!=old.z)
				for(int j=0; j<n; j++)
					if(j!=i) P[i][j] = P[j][i] = pair_penetration(pieces, t, i, j);
		}
		//local minimum: guided local search (the pairs that still overlap weigh more)
		if(gain <= 1e-9){
			if(mult){
				for(auto& e : weights) e.second = max(0.0, 0.95*(1+e.second) - 1);
				for(size_t k=0; k<ov.size(); k++){
					double& w = weights[ov[k]];
					//undo the decay of the overlapping pairs and multiply according to the severity
					double m = (1+w)/0.95;
					w = max(1.0, m) * (1.2 + 0.8*pen[k]/pmax) - 1;
				}
			}else
				for(const auto& pr : ov) weights[pr] += 1.0;
		}
	}
	return false;
}

} /* namespace plp */
