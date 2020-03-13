#include <array>
#include <cstddef>
#include <limits>

#include <xtensor/xview.hpp>

#include "ecole/observation/nodebipartite.hpp"
#include "ecole/scip/type.hpp"

namespace ecole {
namespace observation {

auto NodeBipartite::clone() const -> std::unique_ptr<Base> {
	return std::make_unique<NodeBipartite>(*this);
}

using tensor = decltype(NodeBipartiteObs::col_feat);
using value_type = tensor::value_type;

static value_type constexpr cste = 5.;
static value_type constexpr nan = std::numeric_limits<value_type>::quiet_NaN();
static auto constexpr n_row_feat = 5;
static auto constexpr n_col_feat =
	11 + scip::enum_size<scip::var_type>::value + scip::enum_size<scip::base_stat>::value;

static value_type get_obj_norm(scip::Model const& model) {
	auto norm = SCIPgetObjNorm(model.get_scip_ptr());
	return norm > 0 ? norm : 1.;
}

static auto extract_col_feat(scip::Model const& model) {
	tensor col_feat{{model.lp_columns().size, n_col_feat}, 0.};

	value_type const n_lps = SCIPgetNLPs(model.get_scip_ptr());
	value_type const obj_l2_norm = get_obj_norm(model);

	auto iter = col_feat.begin();
	for (auto const col : model.lp_columns()) {
		auto const var = col.var();
		*(iter++) = static_cast<value_type>(col.lb().has_value());
		*(iter++) = static_cast<value_type>(col.ub().has_value());
		*(iter++) = static_cast<value_type>(col.reduced_cost()) / obj_l2_norm;
		*(iter++) = static_cast<value_type>(col.obj()) / obj_l2_norm;
		*(iter++) = static_cast<value_type>(col.prim_sol());
		if (var.type_() == SCIP_VARTYPE_CONTINUOUS)
			*(iter++) = 0.;
		else
			*(iter++) = static_cast<value_type>(col.prim_sol_frac());
		*(iter++) = static_cast<value_type>(col.is_prim_sol_at_lb());
		*(iter++) = static_cast<value_type>(col.is_prim_sol_at_ub());
		*(iter++) = static_cast<value_type>(col.age()) / (n_lps + cste);
		iter[static_cast<std::size_t>(col.basis_status())] = 1.;
		iter += scip::enum_size<scip::base_stat>::value;
		*(iter++) = static_cast<value_type>(var.best_sol_val().value_or(nan));
		*(iter++) = static_cast<value_type>(var.avg_sol().value_or(nan));
		iter[static_cast<std::size_t>(var.type_())] = 1.;
		iter += scip::enum_size<scip::var_type>::value;
	}

	// Make sure we iterated over as many element as there are in the tensor
	assert(static_cast<std::size_t>(iter - col_feat.begin()) == col_feat.size());

	return col_feat;
}

/**
 * Number of inequality rows.
 *
 * Row are counted once per right hand side and once per left hand side.
 */
static std::size_t get_n_ineq_rows(scip::Model const& model) {
	std::size_t count = 0;
	for (auto row : model.lp_rows()) {
		count += static_cast<std::size_t>(row.rhs().has_value());
		count += static_cast<std::size_t>(row.lhs().has_value());
	}
	return count;
}

static auto extract_row_feat(scip::Model const& model) {
	tensor row_feat{{get_n_ineq_rows(model), n_row_feat}, 0.};

	value_type const n_lps = SCIPgetNLPs(model.get_scip_ptr());
	value_type const obj_l2_norm = get_obj_norm(model);

	auto extract_row = [n_lps, obj_l2_norm](auto& iter, auto const row, bool const lhs) {
		value_type const sign = lhs ? -1. : 1.;
		value_type const row_l2_norm = static_cast<value_type>(row.l2_norm());
		if (lhs) {
			*(iter++) = row.lhs().value() / row_l2_norm;
			*(iter++) = static_cast<value_type>(row.is_at_lhs());
		} else {
			*(iter++) = row.rhs().value() / row_l2_norm;
			*(iter++) = static_cast<value_type>(row.is_at_rhs());
		}
		*(iter++) = static_cast<value_type>(row.age()) / (n_lps + cste);
		*(iter++) = sign * row.obj_cos_sim();
		*(iter++) = sign * row.dual_sol() / (row_l2_norm * obj_l2_norm);
	};

	auto iter = row_feat.begin();
	for (auto const row : model.lp_rows()) {
		// Rows are counted once per rhs and once per lhs
		if (row.lhs().has_value()) extract_row(iter, row, true);
		if (row.rhs().has_value()) extract_row(iter, row, false);
	}

	// Make sure we iterated over as many element as there are in the tensor
	auto diff = iter - row_feat.begin();
	(void)diff;
	assert(static_cast<std::size_t>(iter - row_feat.begin()) == row_feat.size());

	return row_feat;
}

auto NodeBipartite::get(environment::State const& state) -> NodeBipartiteObs {
	return {
		extract_col_feat(state.model),
		extract_row_feat(state.model),
		state.model.lp_matrix(),
	};
}

}  // namespace observation
}  // namespace ecole