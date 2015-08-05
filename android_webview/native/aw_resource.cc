// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_resource.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "jni/AwResource_jni.h"

namespace android_webview {
namespace AwResource {

std::vector<std::string> GetConfigKeySystemUuidMapping() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::string> key_system_uuid_mappings;
  ScopedJavaLocalRef<jobjectArray> mappings =
      Java_AwResource_getConfigKeySystemUuidMapping(env);
  base::android::AppendJavaStringArrayToStringVector(env, mappings.obj(),
                                                     &key_system_uuid_mappings);
  return key_system_uuid_mappings;
}

bool RegisterAwResource(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace AwResource
}  // namespace android_webview
