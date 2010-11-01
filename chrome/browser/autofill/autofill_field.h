// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_FIELD_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_FIELD_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"
#ifdef ANDROID
#include "WebCoreSupport/autofill/FormFieldAndroid.h"
#else
#include "webkit/glue/form_field.h"
#endif

class AutoFillField : public webkit_glue::FormField {
 public:
  AutoFillField();
  AutoFillField(const webkit_glue::FormField& field,
                const string16& unique_name);
  virtual ~AutoFillField();

  const string16& unique_name() const { return unique_name_; }

  AutoFillFieldType heuristic_type() const { return heuristic_type_; }
  AutoFillFieldType server_type() const { return server_type_; }
  const FieldTypeSet& possible_types() const { return possible_types_; }

  // Sets the heuristic type of this field, validating the input.
  void set_heuristic_type(const AutoFillFieldType& type);
  void set_server_type(const AutoFillFieldType& type) { server_type_ = type; }
  void set_possible_types(const FieldTypeSet& possible_types) {
    possible_types_ = possible_types;
  }

  // This function automatically chooses between server and heuristic autofill
  // type, depending on the data available.
  AutoFillFieldType type() const;

  // Returns true if the value of this field is empty.
  bool IsEmpty() const;

  // The unique signature of this field, composed of the field name and the html
  // input type in a 32-bit hash.
  std::string FieldSignature() const;

  // Returns true if the field type has been determined (without the text in the
  // field).
  bool IsFieldFillable() const;

 private:
  // The unique name of this field, generated by AutoFill.
  string16 unique_name_;

  // The type of the field, as determined by the AutoFill server.
  AutoFillFieldType server_type_;

  // The type of the field, as determined by the local heuristics.
  AutoFillFieldType heuristic_type_;

  // The set of possible types for this field.
  FieldTypeSet possible_types_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillField);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_FIELD_H_
