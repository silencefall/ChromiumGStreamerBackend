// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/binding_generating_native_handler.h"

#include "base/macros.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/v8_helpers.h"

namespace extensions {

using namespace v8_helpers;

BindingGeneratingNativeHandler::BindingGeneratingNativeHandler(
    ScriptContext* context,
    const std::string& api_name,
    const std::string& bind_to)
    : context_(context), api_name_(api_name), bind_to_(bind_to) {}

v8::Local<v8::Object> BindingGeneratingNativeHandler::NewInstance() {
  // This long sequence of commands effectively runs the JavaScript code,
  // such that result[bind_to] is the compiled schema for |api_name|:
  //
  //   var result = {};
  //   result[bind_to] = require('binding').Binding.create(api_name).generate();
  //   return result;
  //
  // Unfortunately using the v8 APIs makes that quite verbose.
  // Each stage is marked with the code it executes.
  v8::Isolate* isolate = context_->isolate();
  v8::EscapableHandleScope scope(isolate);

  // Convert |api_name| and |bind_to| into their v8::Strings to pass
  // through the v8 APIs.
  v8::Local<v8::String> v8_api_name;
  v8::Local<v8::String> v8_bind_to;
  if (!ToV8String(isolate, api_name_, &v8_api_name) ||
      !ToV8String(isolate, bind_to_, &v8_bind_to)) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

  v8::Local<v8::Context> v8_context = context_->v8_context();

  // require('binding');
  v8::Local<v8::Object> binding_module;
  if (!context_->module_system()->Require("binding").ToLocal(&binding_module)) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

  // require('binding').Binding;
  v8::Local<v8::Value> binding_value;
  v8::Local<v8::Object> binding;
  if (!GetProperty(v8_context, binding_module, "Binding", &binding_value) ||
      !binding_value->ToObject(v8_context).ToLocal(&binding)) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

  // require('binding').Binding.create;
  v8::Local<v8::Value> create_binding_value;
  if (!GetProperty(v8_context, binding, "create", &create_binding_value) ||
      !create_binding_value->IsFunction()) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }
  v8::Local<v8::Function> create_binding =
      create_binding_value.As<v8::Function>();

  // require('Binding').Binding.create(api_name);
  v8::Local<v8::Value> argv[] = {v8_api_name};
  v8::Local<v8::Value> binding_instance_value;
  v8::Local<v8::Object> binding_instance;
  if (!CallFunction(v8_context, create_binding, binding, arraysize(argv), argv,
                    &binding_instance_value) ||
      !binding_instance_value->ToObject(v8_context)
           .ToLocal(&binding_instance)) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

  // require('binding').Binding.create(api_name).generate;
  v8::Local<v8::Value> generate_value;
  if (!GetProperty(v8_context, binding_instance, "generate", &generate_value) ||
      !generate_value->IsFunction()) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }
  v8::Local<v8::Function> generate = generate_value.As<v8::Function>();

  // require('binding').Binding.create(api_name).generate();
  v8::Local<v8::Object> object = v8::Object::New(isolate);
  v8::Local<v8::Value> compiled_schema;
  if (!CallFunction(v8_context, generate, binding_instance, 0, nullptr,
                    &compiled_schema)) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }

  // var result = {};
  // result[bind_to] = ...;
  if (!SetProperty(v8_context, object, v8_bind_to, compiled_schema)) {
    NOTREACHED();
    return v8::Local<v8::Object>();
  }
  // return result;
  return scope.Escape(object);
}

}  // namespace extensions
