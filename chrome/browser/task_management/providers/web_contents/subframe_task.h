// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_SUBFRAME_TASK_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_SUBFRAME_TASK_H_

#include "base/macros.h"
#include "chrome/browser/task_management/providers/web_contents/renderer_task.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace task_management {

// Defines a concrete renderer task that can represent processes hosting
// out-of-process iframes.
class SubframeTask : public RendererTask {
 public:
  SubframeTask(content::RenderFrameHost* render_frame_host,
               content::WebContents* web_contents,
               RendererTask* main_task);
  ~SubframeTask() override;

  // task_management::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
  void Activate() override;

  // task_management::Task:
  Task* GetParentTask() const override;

 private:
  // The task for the main frame of this WebContents.
  RendererTask* main_task_;
  DISALLOW_COPY_AND_ASSIGN(SubframeTask);
};

}  // namespace task_management


#endif  // CHROME_BROWSER_TASK_MANAGEMENT_PROVIDERS_WEB_CONTENTS_SUBFRAME_TASK_H_
