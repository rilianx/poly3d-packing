/*
 * Instance.cpp
 */

#include <fstream>
#include <sstream>
#include <map>
#include <cmath>
#include <algorithm>
#include "Instance.h"
#include "Mesh.h"
#include "Json.h"

using namespace std;

namespace plp {

namespace {

string basename(const string& path) {
	size_t p = path.find_last_of("/\\");
	return (p==string::npos)? path : path.substr(p+1);
}

string dirname(const string& path) {
	size_t p = path.find_last_of("/\\");
	return (p==string::npos)? "." : path.substr(0,p);
}

bool exists(const string& file) {
	ifstream in(file.c_str());
	return (bool) in;
}

}

Piece Piece::load(const string& file, const string& parts_file) {
	Mesh M = Mesh::load(parts_file.empty()? file : parts_file);
	if(M.faces.empty()) throw runtime_error("mesh without faces: " + file);

	Piece P;
	P.name = basename(file);
	P.parts_file = parts_file;

	//face ranges of each part
	vector< pair<int,int> > ranges;
	if(M.n_groups()>1){
		for(int k=0; k<M.n_groups(); k++)
			ranges.push_back(make_pair(M.groups[k], (k+1<M.n_groups())? M.groups[k+1] : (int)M.faces.size()));
	}else{
		ranges = M.closed_shells(0, M.faces.size());
		if(ranges.empty()){
			//e.g., T-junctions: the volume (divergence theorem) is still correct
			P.warnings.push_back("the mesh is not topologically closed");
			ranges.push_back(make_pair(0, (int)M.faces.size()));
		}
	}

	//vertices not used by any face (e.g., polytope7 of Stoyan et al. 2005: a vertex is missing from the faces)
	vector<bool> used(M.vertices.size(), false);
	for(const vector<int>& f : M.faces) for(int k : f) used[k]=true;
	int unused = count(used.begin(), used.end(), false);
	if(unused > 0)
		P.warnings.push_back(to_string(unused) + " vertex(es) of the file do not appear in any face (ignored)");

	Box3 bb;
	for(const Vec3& v : M.vertices) bb.add(v);
	double tol = 1e-9 * bb.extent().norm();

	P.convex_parts = true;
	P.volume = 0;
	for(auto& r : ranges){
		vector<Vec3> pts;
		vector<bool> used(M.vertices.size(), false);
		for(int j=r.first; j<r.second; j++)
			for(int k : M.faces[j]) if(!used[k]) { used[k]=true; pts.push_back(M.vertices[k]); }

		ConvexPolytope C = ConvexPolytope::hull(pts);
		double vol = fabs(M.volume(r.first, r.second));
		if(fabs(vol - C.get_volume()) > 1e-6*C.get_volume()) P.convex_parts=false;
		P.volume += vol;
		P.parts.push_back(C);
	}

	//overlapping parts: the volume of the union is not the sum of the volumes
	bool overlap=false;
	for(size_t i=0; i<P.parts.size() && !overlap; i++)
		for(size_t j=i+1; j<P.parts.size() && !overlap; j++)
			overlap = penetration_depth(P.parts[i], P.parts[j], tol) > tol;

	P.mesh_min = bb.lo;
	if(!parts_file.empty()){
		Mesh O = Mesh::load(file);
		Box3 ob;
		for(const Vec3& v : O.vertices) ob.add(v);
		P.mesh_min = ob.lo;
		double vol = fabs(O.volume(0, O.faces.size()));
		if(overlap) P.volume = vol;
		else if(fabs(vol - P.volume) > 1e-6*vol){
			ostringstream ss;
			ss << "volume of the decomposition (" << P.volume << ") differs from that of the original mesh (" << vol << ")";
			P.warnings.push_back(ss.str());
		}
	}else if(overlap)
		P.warnings.push_back("overlapping parts: the volume is overestimated");

	if(P.parts.size()==1) P.convex = P.convex_parts;
	else{
		ConvexPolytope H = ConvexPolytope::hull(M.vertices);
		P.convex = P.convex_parts && fabs(H.get_volume() - P.volume) <= 1e-6*H.get_volume();
	}

	//the bounding box starts at the origin
	P.offset = bb.lo;
	for(ConvexPolytope& C : P.parts) C = C.translated(-bb.lo);
	P.bbox = Box3();
	for(ConvexPolytope& C : P.parts) P.bbox.add(C.get_bbox());
	return P;
}

Instance Instance::load(const string& file, const string& root, const string& dec) {
	Json J = Json::parse_file(file);

	Instance I;
	I.file = file;
	I.name = J.has("name")? J["name"].as_string() : basename(file);
	I.W = J["container"]["size-x"].as_number();
	I.D = J["container"]["size-y"].as_number();

	const Json& types = J["item-types"];
	if(types.size()==0) throw runtime_error("instance without pieces: " + file);

	//root folder: the first folder (going up) from which the first piece exists
	string first = types[0]["path"].as_string();
	I.root = root;
	if(I.root.empty()){
		string dir = dirname(file);
		for(int up=0; up<6; up++){
			if(exists(dir + "/" + first)) { I.root = dir; break; }
			dir += "/..";
		}
		if(I.root.empty()) throw runtime_error("could not find " + first + " (use --root)");
	}

	map<string,int> loaded;
	for(size_t k=0; k<types.size(); k++){
		ItemType t;
		t.path = types[k]["path"].as_string();
		t.demand = (int) types[k]["demand"].as_number();
		if(!loaded.count(t.path)){
			loaded[t.path] = I.pieces.size();
			//decomposition into convex parts (if any)
			string parts = I.root + "/" + dec + "/" + t.path.substr(0, t.path.find_last_of('.')) + ".obj";
			I.pieces.push_back(Piece::load(I.root + "/" + t.path, exists(parts)? parts : ""));
		}
		t.piece = loaded[t.path];
		I.types.push_back(t);
	}
	return I;
}

int Instance::n_items() const {
	int n=0;
	for(const ItemType& t : types) n+=t.demand;
	return n;
}

double Instance::total_volume() const {
	double v=0;
	for(const ItemType& t : types) v += t.demand * pieces[t.piece].volume;
	return v;
}

double Instance::height_lower_bound() const {
	double h = total_volume() / (W*D);
	for(const ItemType& t : types) h = max(h, pieces[t.piece].bbox.extent().z);
	return h;
}

} /* namespace plp */
