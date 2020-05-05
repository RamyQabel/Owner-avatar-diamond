/****
DIAMOND protein aligner
Copyright (C) 2013-2018 Benjamin Buchfink <buchfink@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#pragma once
#include <Eigen/Sparse>
#include <Eigen/Eigenvalues>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <stdio.h>
#include <memory>
#include <fstream>
#include <limits>
#include "../util/system/system.h"
#include "../util/util.h"
#include "../basic/config.h"
#include "../data/reference.h"
#include "../run/workflow.h"
#include "disjoint_set.h"
#include "../util/io/consumer.h"
#include "../util/algo/algo.h"
#include "../basic/statistics.h"
#include "../util/log_stream.h"
#include "../dp/dp.h"
#include "cluster.h"

using namespace std;

namespace Workflow { namespace Cluster{
class MCL: public ClusteringAlgorithm {
private: 
	void get_exp(Eigen::SparseMatrix<float>* in, Eigen::SparseMatrix<float>* out, float r);
	void get_exp(Eigen::MatrixXf* in, Eigen::MatrixXf* out, float r);
	void get_gamma(Eigen::SparseMatrix<float>* in, Eigen::SparseMatrix<float>* out, float r);
	void get_gamma(Eigen::MatrixXf* in, Eigen::MatrixXf* out, float r);
	vector<unordered_set<uint32_t>> get_list(Eigen::SparseMatrix<float>* m);
	vector<unordered_set<uint32_t>> get_list(Eigen::MatrixXf* m);
	vector<unordered_set<uint32_t>> markov_process(Eigen::SparseMatrix<float>* m, float inflation, float expansion);
	vector<unordered_set<uint32_t>> markov_process(Eigen::MatrixXf* m, float inflation, float expansion);
public:
	void run();
	string get_key();
	string get_description();
};

template <typename T>
class SparseMatrixStream : public Consumer {
	size_t n;
	vector<Eigen::Triplet<T>> data;
	LazyDisjointSet<size_t>* disjointSet;
	virtual void consume(const char *ptr, size_t n) override {
		size_t query, subject, count;
		float qcov, scov, bitscore, id;
		const char *end = ptr + n;
		while (ptr < end) {
			if (sscanf(ptr, "%lu\t%lu\t%f\t%f\t%f\t%f\n%ln", &query, &subject, &qcov, &scov, &bitscore, &id, &count) != 6) 
				throw runtime_error("Cluster format error.");
			const float value = (qcov/100.0f) * (scov/100.0f) * (id/100.0f);
			data.emplace_back(query, subject, value);
			disjointSet->merge(query, subject);
			ptr += count;
		}
	}
public:
	SparseMatrixStream(size_t n){
		this->n = n;
		disjointSet = new LazyDisjointIntegralSet<size_t>(n); 
	}

	~SparseMatrixStream(){
		delete disjointSet;
	}
	pair<vector<vector<uint32_t>>, vector<vector<Eigen::Triplet<T>>>> getComponents(){
		vector<unordered_set<size_t>> sets = disjointSet->getListOfSets();
		vector<vector<Eigen::Triplet<T>>> split(sets.size());
		vector<uint32_t> indexToSetId(this->n, sets.size());
		for(uint32_t iset = 0; iset < sets.size(); iset++){
			split.push_back(vector<Eigen::Triplet<T>>());
			for(size_t index : sets[iset]){
				indexToSetId[index] = iset;
			}
		}
		for(Eigen::Triplet<T> t : data){
			uint32_t iset = indexToSetId[t.row()];
			assert( iset == indexToSetId[t.col()]);
			split[iset].emplace_back(t.row(), t.col(), t.value());
		}
		vector<vector<uint32_t>> indices(sets.size());
		vector<vector<Eigen::Triplet<T>>> components(sets.size());
		for(uint32_t iset = 0; iset < sets.size(); iset++){
			vector<uint32_t> order(sets[iset].begin(), sets[iset].end());
			map<uint32_t, uint32_t> index_map;
			uint32_t iel = 0;
			for(uint32_t const & el: order){
				index_map.emplace(el, iel++);
			}
			vector<Eigen::Triplet<T>> remapped;
			for(Eigen::Triplet<T> const & t : split[iset]){
				remapped.emplace_back(index_map[t.row()], index_map[t.col()], t.value());
			}
			remapped.shrink_to_fit();
			components.emplace_back(move(remapped));
			indices.emplace_back(move(order));
		}
		return make_pair(move(indices), move(components));
	}
	Eigen::SparseMatrix<T> getMatrix(){
		Eigen::SparseMatrix<T> m(n,n);
		m.setFromTriplets(data.begin(), data.end());
		return m;
	}
};
}}