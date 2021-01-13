/****
DIAMOND protein aligner
Copyright (C) 2020 Max Planck Society for the Advancement of Science e.V.

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

#include "cbs.h"
#include "../basic/config.h"
#include "score_matrix.h"

namespace Stats {

std::vector<double> composition(const sequence& s) {
    std::vector<double> r(20, 0.0);
    int n = 0;
    for (size_t i = 0; i < s.length(); ++i) {
        int l = s[i];
        if (l < 20) {
            ++r[l];
            ++n;
        }
    }
    if (n == 0)
        return r;
    for (int i = 0; i < 20; ++i)
        r[i] /= n;
    return r;
}

TargetMatrix::TargetMatrix(const double* query_comp, int query_len, const sequence& target)
{
    if (!CBS::matrix_adjust(config.comp_based_stats))
        return;

    auto c = composition(target);
    if (CBS::conditioned(config.comp_based_stats)) {
        auto r = s_TestToApplyREAdjustmentConditional(query_len, (int)target.length(), query_comp, c.data(), score_matrix.background_freqs());
        if (r == eCompoScaleOldMatrix)
            return;
    }

    scores.resize(32 * AMINO_ACID_COUNT);
    scores32.resize(32 * AMINO_ACID_COUNT);
    score_min = INT_MAX;
    score_max = INT_MIN;
    vector<int> s = CompositionMatrixAdjust(query_len, (int)target.length(), query_comp, c.data(), config.cbs_matrix_scale, score_matrix.ungapped_lambda(), score_matrix.joint_probs(), score_matrix.background_freqs());
    
    for (size_t i = 0; i < AMINO_ACID_COUNT; ++i) {
        for (size_t j = 0; j < AMINO_ACID_COUNT; ++j)
            if (i < 20 && j < 20) {
                scores[i * 32 + j] = s[j * 20 + i];
                scores32[i * 32 + j] = s[j * 20 + i];
                score_min = std::min(score_min, s[j * 20 + i]);
                score_max = std::max(score_max, s[j * 20 + i]);
                //std::cerr << s2[j * 20 + i] << ' ';
            }
            else {
                scores[i * 32 + j] = std::max(score_matrix(i, j) * config.cbs_matrix_scale, SCHAR_MIN);
                scores32[i * 32 + j] = score_matrix(i, j) * config.cbs_matrix_scale;
                score_min = std::min(score_min, scores32[i * 32 + j]);
                score_max = std::max(score_max, scores32[i * 32 + j]);
            }
        //std::cerr << endl;
    }
}

TargetMatrix::TargetMatrix(const int16_t* query_matrix, const int16_t* target_matrix) :
    scores(32 * AMINO_ACID_COUNT),
    scores32(32 * AMINO_ACID_COUNT),
    score_min(INT_MAX),
    score_max(INT_MIN)
{
    const double f = (double)config.cbs_matrix_scale / (double)CBS::AVG_MATRIX_SCALE;
    for (size_t i = 0; i < AMINO_ACID_COUNT; ++i) {
        for (size_t j = 0; j < AMINO_ACID_COUNT; ++j)
            if (i < 20 && j < 20) {
                const int score = std::round(((double)query_matrix[i * 20 + j] + (double)target_matrix[i * 20 + j]) * f / 2.0);
                scores[i * 32 + j] = score;
                scores32[i * 32 + j] = score;
                score_min = std::min(score_min, score);
                score_max = std::max(score_max, score);
            }
            else {
                const int score = score_matrix(i, j) * config.cbs_matrix_scale;
                scores[i * 32 + j] = score;
                scores32[i * 32 + j] = score;
                score_min = std::min(score_min, score);
                score_max = std::max(score_max, score);
            }
    }
}

}