#pragma once

#include <memory>
#include <nonstd/optional.hpp>

#include "ecole/observation/abstract.hpp"

namespace ecole {
namespace observation {

class FocusNodeObs {
public:
	long long number;
	int depth;
	double lowerbound;
	double estimate;
	int n_added_conss;
	long long parent_number;
	double parent_lowerbound;
};

class FocusNode : public ObservationFunction<nonstd::optional<FocusNodeObs>> {
public:
	using Observation = nonstd::optional<FocusNodeObs>;
	Observation obtain_observation(scip::Model& model) override;
};

}  // namespace observation
}  // namespace ecole
