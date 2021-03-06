// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

#include <cmath>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "third_party/libaddressinput/chromium/addressinput_util.h"
#include "third_party/libaddressinput/chromium/input_suggester.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_normalizer.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace autofill {
namespace {

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressNormalizer;
using ::i18n::addressinput::BuildCallback;
using ::i18n::addressinput::FieldProblemMap;
using ::i18n::addressinput::PreloadSupplier;
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;

using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::POSTAL_CODE;

// The maximum number attempts to load rules.
static const int kMaxAttemptsNumber = 8;

}  // namespace

AddressValidator::AddressValidator(std::unique_ptr<Source> source,
                                   std::unique_ptr<Storage> storage,
                                   LoadRulesListener* load_rules_listener)
    : supplier_(new PreloadSupplier(source.release(), storage.release())),
      input_suggester_(new InputSuggester(supplier_.get())),
      normalizer_(new AddressNormalizer(supplier_.get())),
      validator_(new ::i18n::addressinput::AddressValidator(supplier_.get())),
      validated_(BuildCallback(this, &AddressValidator::Validated)),
      rules_loaded_(BuildCallback(this, &AddressValidator::RulesLoaded)),
      load_rules_listener_(load_rules_listener),
      weak_factory_(this) {}

AddressValidator::~AddressValidator() {}

void AddressValidator::LoadRules(const std::string& region_code) {
  attempts_number_[region_code] = 0;
  supplier_->LoadRules(region_code, *rules_loaded_);
}

AddressValidator::Status AddressValidator::ValidateAddress(
    const AddressData& address,
    const FieldProblemMap* filter,
    FieldProblemMap* problems) const {
  if (supplier_->IsPending(address.region_code)) {
    if (problems)
      addressinput::ValidateRequiredFields(address, filter, problems);
    return RULES_NOT_READY;
  }

  if (!supplier_->IsLoaded(address.region_code)) {
    if (problems)
      addressinput::ValidateRequiredFields(address, filter, problems);
    return RULES_UNAVAILABLE;
  }

  if (!problems)
    return SUCCESS;

  validator_->Validate(address,
                       true,  // Allow postal office boxes.
                       true,  // Require recipient name.
                       filter,
                       problems,
                       *validated_);

  return SUCCESS;
}

AddressValidator::Status AddressValidator::GetSuggestions(
    const AddressData& user_input,
    AddressField focused_field,
    size_t suggestion_limit,
    std::vector<AddressData>* suggestions) const {
  if (supplier_->IsPending(user_input.region_code))
    return RULES_NOT_READY;

  if (!supplier_->IsLoaded(user_input.region_code))
    return RULES_UNAVAILABLE;

  if (!suggestions)
    return SUCCESS;

  suggestions->clear();

  if (focused_field == POSTAL_CODE ||
      (focused_field >= ADMIN_AREA && focused_field <= DEPENDENT_LOCALITY)) {
    input_suggester_->GetSuggestions(
        user_input, focused_field, suggestion_limit, suggestions);
  }

  return SUCCESS;
}

bool AddressValidator::CanonicalizeAdministrativeArea(
    AddressData* address) const {
  if (!supplier_->IsLoaded(address->region_code))
    return false;

  // TODO: It would probably be beneficial to use the full canonicalization.
  AddressData tmp(*address);
  normalizer_->Normalize(&tmp);
  address->administrative_area = tmp.administrative_area;

  return true;
}

AddressValidator::AddressValidator()
    : load_rules_listener_(NULL), weak_factory_(this) {}

base::TimeDelta AddressValidator::GetBaseRetryPeriod() const {
  return base::TimeDelta::FromSeconds(8);
}

void AddressValidator::Validated(bool success,
                                 const AddressData&,
                                 const FieldProblemMap&) {
  DCHECK(success);
}

void AddressValidator::RulesLoaded(bool success,
                                   const std::string& region_code,
                                   int) {
  if (load_rules_listener_)
    load_rules_listener_->OnAddressValidationRulesLoaded(region_code, success);

  // Count the first failed attempt to load rules as well.
  if (success || attempts_number_[region_code] + 1 >= kMaxAttemptsNumber)
    return;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AddressValidator::RetryLoadRules,
                 weak_factory_.GetWeakPtr(),
                 region_code),
      GetBaseRetryPeriod() * pow(2, attempts_number_[region_code]++));
}

void AddressValidator::RetryLoadRules(const std::string& region_code) {
  // Do not reset retry count.
  supplier_->LoadRules(region_code, *rules_loaded_);
}

}  // namespace autofill
