/****
DIAMOND protein aligner
Copyright (C) 2013-2020 Max Planck Society for the Advancement of Science e.V.
                        Benjamin Buchfink
                        Eberhard Karls Universitaet Tuebingen
						
Code developed by Benjamin Buchfink <benjamin.buchfink@tue.mpg.de>

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

#include <math.h>
#include <string>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <sstream>
#include "score_matrix.h"
#include "../basic/config.h"
#include "../stats/cbs.h"

using std::string;
using std::vector;

Score_matrix score_matrix;

Score_matrix::Score_matrix(const string & matrix, int gap_open, int gap_extend, int frameshift, int stop_match_score, uint64_t db_letters, int scale):
	standard_matrix_(&Stats::StandardMatrix::get(matrix)),
	gap_open_ (gap_open == -1 ? standard_matrix_->default_gap_exist : gap_open),
	gap_extend_ (gap_extend == -1 ? standard_matrix_->default_gap_extend : gap_extend),
	frame_shift_(frameshift),
	db_letters_ ((double)db_letters),
	name_(matrix),
	matrix8_(standard_matrix_->scores.data(), stop_match_score),
	bias_((char)(-low_score())),
	matrix8u_(standard_matrix_->scores.data(), stop_match_score, bias_),
	matrix8_low_(standard_matrix_->scores.data(), stop_match_score, 0, 16),
	matrix8_high_(standard_matrix_->scores.data(), stop_match_score, 0, 16, 16),
	matrix8u_low_(standard_matrix_->scores.data(), stop_match_score, bias_, 16),
	matrix8u_high_(standard_matrix_->scores.data(), stop_match_score, bias_, 16, 16),
	matrix16_(standard_matrix_->scores.data(), stop_match_score),
	matrix32_(standard_matrix_->scores.data(), stop_match_score),
	matrix32_scaled_(standard_matrix_->freq_ratios, standard_matrix_->ungapped_constants().Lambda, standard_matrix_->scores.data(), scale)
{
	const Stats::StandardMatrix::Parameters& p = standard_matrix_->constants(gap_open_, gap_extend_), & u = standard_matrix_->ungapped_constants();
	const double G = gap_open_ + gap_extend_, b = 2.0 * G * (u.alpha - p.alpha), beta = 2.0 * G * (u.alpha_v - p.alpha_v);
	Sls::AlignmentEvaluerParameters params{ p.Lambda, p.K, p.alpha, b, p.alpha, b, p.alpha_v, beta, p.alpha_v, beta, p.sigma, 2.0 * G * (u.alpha_v - p.sigma) };
	evaluer.initParameters(params);
	ln_k_ = std::log(evaluer.parameters().K);
}

int8_t Score_matrix::low_score() const
{
	int8_t low = std::numeric_limits<int8_t>::max();
	for (Letter i = 0; i < (Letter)value_traits.alphabet_size; ++i)
		for (Letter j = i + 1; j < (Letter)value_traits.alphabet_size; ++j)
			low = std::min(low, (int8_t)this->operator()(i, j));
	return low;
}

int8_t Score_matrix::high_score() const
{
	int8_t high = std::numeric_limits<int8_t>::min();
	for (Letter i = 0; i < (Letter)value_traits.alphabet_size; ++i)
		for (Letter j = i; j < (Letter)value_traits.alphabet_size; ++j)
			high = std::max(high, (int8_t)this->operator()(i, j));
	return high;
}

double Score_matrix::avg_id_score() const
{
	double s = 0;
	for (int i = 0; i < 20; ++i)
		s += this->operator()(i, i);
	return s / 20;
}

std::ostream& operator<<(std::ostream& s, const Score_matrix &m)
{
	s << "(Matrix=" << m.name_
		<< " Lambda=" << m.lambda()
		<< " K=" << m.k()
		<< " Penalties=" << m.gap_open_
		<< '/' << m.gap_extend_ << ')';
	return s;
}

const signed char* custom_scores(const string &matrix_file)
{
	static signed char scores[AMINO_ACID_COUNT * AMINO_ACID_COUNT];
	string l, s;
	std::stringstream ss;
	vector<Letter> pos;
	unsigned n = 0;
	std::fill(scores, scores + sizeof(scores), int8_t(-1));
	if (matrix_file == "")
		return scores;
	std::ifstream f(matrix_file.c_str());
	while (!f.eof()) {
		std::getline(f, l);
		if (l[0] == '#')
			continue;
		if (pos.size() == 0) {
			for (string::const_iterator i = l.begin(); i != l.end(); ++i)
				if (*i == ' ' || *i == '\t')
					continue;
				else
					pos.push_back(value_traits.from_char(*i));
		}
		else {
			if (n >= pos.size())
				break;
			ss << l;
			if (value_traits.from_char(ss.get()) != pos[n])
				throw std::runtime_error("Invalid custom scoring matrix file format.");
			for (unsigned i = 0; i < pos.size(); ++i) {
				int score;
				ss >> score;
				scores[(int)pos[n] * AMINO_ACID_COUNT + (int)pos[i]] = score;
			}
			ss.clear();
			++n;
		}
	}
	return scores;
}

Score_matrix::Score_matrix(const string& matrix_file, int gap_open, int gap_extend, int stop_match_score, const Custom&, uint64_t db_letters) :
	score_array_(custom_scores(matrix_file)),
	gap_open_(gap_open),
	gap_extend_(gap_extend),
	db_letters_((double)db_letters),
	name_("custom"),
	matrix8_(score_array_, stop_match_score),
	bias_((char)(-low_score())),
	matrix8u_(score_array_, stop_match_score, bias_),
	matrix8_low_(score_array_, stop_match_score, 0, 16),
	matrix8_high_(score_array_, stop_match_score, 0, 16, 16),
	matrix8u_low_(score_array_, stop_match_score, bias_, 16),
	matrix8u_high_(score_array_, stop_match_score, bias_, 16, 16),
	matrix16_(score_array_, stop_match_score),
	matrix32_(score_array_, stop_match_score)
{
	const int N = TRUE_AA;
	long m[N][N];
	long* p[N];
	for (int i = 0; i < N; ++i) {
		for (int j = 0; j < N; ++j)
			m[i][j] = score_array_[i * AMINO_ACID_COUNT + j];
		p[i] = m[i];
	}
	const double* bg = Stats::blosum62.background_freqs.data();
	evaluer.initGapped(N, p, bg, bg, gap_open_, gap_extend_, gap_open_, gap_extend_, false, 0.01, 0.05, 120.0, 1024.0, 1);
	ln_k_ = std::log(evaluer.parameters().K);
}

template<typename _t>
Score_matrix::Scores<_t>::Scores(const double (&freq_ratios)[Stats::NCBI_ALPH][Stats::NCBI_ALPH], double lambda, const int8_t* scores, int scale) {
	const int n = value_traits.alphabet_size;
	for (int i = 0; i < 32; ++i)
		for (int j = 0; j < 32; ++j) {
			if (i < TRUE_AA && j < TRUE_AA)
				data[i * 32 + j] = std::round(std::log(freq_ratios[Stats::ALPH_TO_NCBI[i]][Stats::ALPH_TO_NCBI[j]]) / lambda * scale);
			else if (i < n && j < n)
				data[i * 32 + j] = std::max((int)scores[i * n + j] * scale, SCHAR_MIN);
			else
				data[i * 32 + j] = SCHAR_MIN;
		}
}

template<typename _t>
std::vector<const _t*> Score_matrix::Scores<_t>::pointers() const {
	vector<const _t*> r(32);
	for (int i = 0; i < 32; ++i)
		r[i] = &data[i * 32];
	return r;
}

template struct Score_matrix::Scores<int>;

double Score_matrix::evalue(int raw_score, unsigned query_len, unsigned subject_len) const
{
	return evaluer.evalue(raw_score, query_len, subject_len) * (double)db_letters_ / (double)subject_len;
}

bool Score_matrix::report_cutoff(int score, double evalue) const {
	if (config.min_bit_score != 0)
		return bitscore(score) >= config.min_bit_score;
	else
		return evalue <= config.max_evalue;
}