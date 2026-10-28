/*
 * Solution.cpp
 */

#include <fstream>
#include <sstream>
#include <map>
#include <cmath>
#include "Solution.h"
#include "Json.h"

using namespace std;

namespace plp {

namespace {

string basename(const string& path) {
	size_t p = path.find_last_of("/\\");
	return (p==string::npos)? path : path.substr(p+1);
}

ConvexPolytope place(const ConvexPolytope& C, const Placement& p) {
	return (p.R.is_identity())? C.translated(p.t) : C.transformed(p.R, p.t);
}

Box3 placed_bbox(const Piece& P, const Placement& p) {
	if(p.R.is_identity()) { Box3 b; b.add(P.bbox.lo+p.t); b.add(P.bbox.hi+p.t); return b; }
	Box3 b;
	for(const ConvexPolytope& C : P.parts)
		for(const Vec3& v : C.get_vertices()) b.add(p.R*v + p.t);
	return b;
}

}

Solution Solution::load(const string& file, const Instance& inst) {
	Json J = Json::parse_file(file);
	if(J.has("itemOrigin") && J["itemOrigin"].as_string()!="AlignToMinimum")
		throw runtime_error("unsupported itemOrigin: " + J["itemOrigin"].as_string());

	//types by file name
	map<string, vector<int> > by_name;
	for(size_t k=0; k<inst.types.size(); k++)
		by_name[basename(inst.types[k].path)].push_back(k);
	vector<int> used(inst.types.size(), 0);

	Solution S(inst);
	const Json& items = J["items"];
	for(size_t i=0; i<items.size(); i++){
		string name = items[i]["name"].as_string();
		if(!by_name.count(name)) throw runtime_error("unknown piece in the solution: " + name);

		Placement p;
		p.type = by_name[name][0];
		for(int k : by_name[name])
			if(used[k] < inst.types[k].demand) { p.type=k; break; }
		used[p.type]++;

		//4x4 matrix in row-vector convention: v' = v M (translation in the last row)
		const Json& T = items[i]["transformation"];
		if(T.has("matrix")){
			const Json& M = T["matrix"];
			for(int r=0;r<3;r++) for(int c=0;c<3;c++) p.R.m[c][r] = M[r][c].as_number();
			for(int c=0;c<3;c++) p.t[c] = M[3][c].as_number();
		}else{
			const Json& pos = T["position"];
			for(int c=0;c<3;c++) p.t[c] = pos[c].as_number();
		}
		//the translation in the file places the original mesh; t places the bounding box of the parts
		const Piece& P = inst.pieces[inst.types[p.type].piece];
		p.t = p.t - (P.mesh_min - P.offset);
		S.items.push_back(p);
	}
	return S;
}

Vec3 Solution::mesh_translation(const Placement& p) const {
	const Piece& P = inst.pieces[inst.types[p.type].piece];
	return p.t + (P.mesh_min - P.offset);
}

void Solution::save(const string& file) const {
	Json J = Json::object();
	J.set("instancePath", inst.file);
	J.set("totalHeight", height());
	J.set("itemOrigin", "AlignToMinimum");
	J.set("gap", 0.0);
	Json items = Json::array();
	for(const Placement& p : this->items){
		Json it = Json::object();
		it.set("name", basename(inst.types[p.type].path));
		Json T = Json::object();
		Json pos = Json::array();
		Vec3 tm = mesh_translation(p);
		for(int c=0;c<3;c++) pos.push_back(tm[c]);
		T.set("position", pos);
		Json M = Json::array();
		for(int r=0;r<4;r++){
			Json row = Json::array();
			for(int c=0;c<3;c++) row.push_back((r<3)? p.R.m[c][r] : tm[c]);
			row.push_back((r<3)? 0.0 : 1.0);
			M.push_back(row);
		}
		T.set("matrix", M);
		it.set("transformation", T);
		items.push_back(it);
	}
	J.set("items", items);

	ofstream out(file.c_str());
	if(!out) throw runtime_error("could not write " + file);
	J.write(out);
	out << endl;
}

double Solution::height() const {
	double h=0;
	for(const Placement& p : items)
		h = max(h, placed_bbox(inst.pieces[inst.types[p.type].piece], p).hi.z);
	return h;
}

Report Solution::verify(double eps, bool partial) const {
	Report r;
	r.n_placed = items.size();
	r.max_overlap = 0;
	r.max_outside = 0;
	r.height = height();

	//number of pieces
	vector<int> count(inst.types.size(), 0);
	for(const Placement& p : items) count[p.type]++;
	for(size_t k=0; k<inst.types.size(); k++){
		if(count[k] > inst.types[k].demand || (!partial && count[k] < inst.types[k].demand)){
			ostringstream ss;
			ss << "type " << k << " (" << inst.types[k].path << "): " << count[k]
			   << " pieces placed, demand " << inst.types[k].demand;
			r.errors.push_back(ss.str());
		}
	}

	//pieces at their positions
	vector< vector<ConvexPolytope> > placed(items.size());
	vector<Box3> bbox(items.size());
	double vol=0;
	for(size_t i=0; i<items.size(); i++){
		const Placement& p = items[i];
		const Piece& P = inst.pieces[inst.types[p.type].piece];
		vol += P.volume;
		if(!p.R.is_rotation()){
			r.errors.push_back("piece " + to_string(i) + ": the matrix is not a rotation");
			continue;
		}
		for(const ConvexPolytope& C : P.parts){
			placed[i].push_back(place(C, p));
			bbox[i].add(placed[i].back().get_bbox());
		}

		//inside the container
		const Box3& b = bbox[i];
		double out = max(max(max(-b.lo.x, -b.lo.y), -b.lo.z), max(b.hi.x-inst.W, b.hi.y-inst.D));
		r.max_outside = max(r.max_outside, out);
		if(out > eps){
			ostringstream ss;
			ss << "piece " << i << " (" << P.name << ") outside the container by " << out;
			r.errors.push_back(ss.str());
		}
		if(!P.convex_parts)
			r.errors.push_back("piece " + to_string(i) + " (" + P.name + "): non-convex parts, the check is not exact");
	}

	//overlap between pieces
	for(size_t i=0; i<items.size(); i++)
		for(size_t j=i+1; j<items.size(); j++){
			if(!bbox[i].overlaps(bbox[j], eps)) continue;
			double depth=0;
			for(const ConvexPolytope& A : placed[i])
				for(const ConvexPolytope& B : placed[j])
					if(A.get_bbox().overlaps(B.get_bbox(), eps))
						depth = max(depth, penetration_depth(A, B, eps));
			r.max_overlap = max(r.max_overlap, depth);
			if(depth > eps){
				ostringstream ss;
				ss << "pieces " << i << " and " << j << " overlap (penetration " << depth << ")";
				r.errors.push_back(ss.str());
			}
		}

	r.density = (r.height>0)? vol / (inst.W*inst.D*r.height) : 0;
	r.feasible = r.errors.empty();
	return r;
}

ostream& operator<<(ostream& os, const Report& r) {
	os << "feasible: " << (r.feasible? "yes":"no") << endl;
	os << "items: " << r.n_placed << endl;
	os << "height: " << r.height << endl;
	os << "density: " << r.density << endl;
	os << "max_overlap: " << r.max_overlap << endl;
	os << "max_outside: " << r.max_outside << endl;
	for(const string& e : r.errors) os << "ERROR: " << e << endl;
	return os;
}

} /* namespace plp */
