#pragma once

#include <optional>
#include <string>
#include <vector>

#include "vrv_includes.h"

#include "verosim/model/fraction.h"

namespace verosim::extract_detail {

char StepFromPname(vrv::data_PITCHNAME pname);
int DiatonicIndex(char step);
std::string TypeNameFromDur(vrv::data_DURATION dur);
bool AccidWrittenInfo(vrv::data_ACCIDENTAL_WRITTEN accid, std::string &name, int &alter);
bool AccidGesturalAlter(vrv::data_ACCIDENTAL_GESTURAL accid, int &alter);
std::string ClefSign(vrv::data_CLEFSHAPE shape);
void AppendArticulations(const vrv::Object *obj, std::vector<std::string> &out);
std::string StripIdRef(std::string ref);
bool StaffMatches(const vrv::Object *obj, const std::string &staffN);
bool HasStaffIdent(const vrv::Object *obj);
std::optional<std::string> ModernMensurSymbol(const vrv::Mensur &mensur);
bool IsVisibleMeterSig(const vrv::MeterSig &metersig);
bool MeterQLFromMeterSig(const vrv::MeterSig &metersig, Fraction &ql);
bool MeterQLFromMensur(const vrv::Mensur &mensur, Fraction &ql);
Fraction BeatQLFromMeterSig(const vrv::MeterSig &metersig);
Fraction FractionFromDouble(double value);

} // namespace verosim::extract_detail
