#pragma once

#include <memory>
#include <string>

#include "verosim/model/evaluation_profile.h"

namespace verosim {

// Loads and validates one immutable v1 profile. The digest is lowercase
// SHA-256 over the exact bytes read from path, including insignificant JSON
// whitespace, so audit output identifies the precise external artifact.
std::shared_ptr<const EvaluationProfile> LoadEvaluationProfileFile(
    const std::string &path, std::string &error);

} // namespace verosim
