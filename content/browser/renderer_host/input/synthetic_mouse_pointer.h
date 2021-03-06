// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_MOUSE_POINTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_MOUSE_POINTER_H_

#include "base/macros.h"
#include "content/browser/renderer_host/input/synthetic_pointer.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_web_input_event_builders.h"

namespace content {

class CONTENT_EXPORT SyntheticMousePointer : public SyntheticPointer {
 public:
  SyntheticMousePointer();
  ~SyntheticMousePointer() override;

  void DispatchEvent(SyntheticGestureTarget* target,
                     const base::TimeTicks& timestamp) override;

  int Press(float x,
            float y,
            SyntheticGestureTarget* target,
            const base::TimeTicks& timestamp) override;
  void Move(int index,
            float x,
            float y,
            SyntheticGestureTarget* target,
            const base::TimeTicks& timestamp) override;
  void Release(int index,
               SyntheticGestureTarget* target,
               const base::TimeTicks& timestamp) override;

 private:
  blink::WebMouseEvent mouse_event_;
  DISALLOW_COPY_AND_ASSIGN(SyntheticMousePointer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_MOUSE_POINTER_H_
