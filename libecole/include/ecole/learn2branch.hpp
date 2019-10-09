#pragma once

#include <string>

#include "ecole/scip/model.hpp"

namespace ecole {

class BranchEnv {
public:
	scip::Model model;

	BranchEnv(scip::Model&& model) noexcept;
	static BranchEnv from_file(std::string const& filename) {
		return BranchEnv{scip::Model::from_file(filename)};
	}

	void run(scip::Model::BranchFunc const& func);
};
} // namespace ecole
