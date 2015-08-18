// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpType;
using base::trace_event::ProcessMemoryDump;
using testing::_;
using testing::Return;

namespace content {

// A mock dump provider, used to check that dump requests actually end up
// creating memory dumps.
class MockDumpProvider : public base::trace_event::MemoryDumpProvider {
 public:
  MOCK_METHOD2(OnMemoryDump, bool(const MemoryDumpArgs& args,
                                  ProcessMemoryDump* pmd));
};

class MemoryTracingTest : public ContentBrowserTest {
 public:
  void DoRequestGlobalDump(const base::trace_event::MemoryDumpCallback& cb) {
    MemoryDumpArgs dump_args = { MemoryDumpArgs::LevelOfDetail::HIGH };
    MemoryDumpManager::GetInstance()->RequestGlobalDump(
        MemoryDumpType::EXPLICITLY_TRIGGERED, dump_args, cb);
  }

  // Used as callback argument for MemoryDumpManager::RequestGlobalDump():
  void OnGlobalMemoryDumpDone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::Closure closure,
      uint64 dump_guid,
      bool success) {
    // Make sure we run the RunLoop closure on the same thread that originated
    // the run loop (which is the IN_PROC_BROWSER_TEST_F main thread).
    if (!task_runner->RunsTasksOnCurrentThread()) {
      task_runner->PostTask(
          FROM_HERE, base::Bind(&MemoryTracingTest::OnGlobalMemoryDumpDone,
                                base::Unretained(this), task_runner, closure,
                                dump_guid, success));
      return;
    }
    ++callback_call_count_;
    last_callback_dump_guid_ = dump_guid;
    last_callback_success_ = success;
    closure.Run();
  }

 protected:
  void SetUp() override {
    callback_call_count_ = 0;
    last_callback_dump_guid_ = 0;
    last_callback_success_ = false;
    MemoryDumpManager::GetInstance()->Initialize();
    mock_dump_provider_.reset(new MockDumpProvider());
    MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        mock_dump_provider_.get());
    // TODO(primiano): This should be done via TraceConfig.
    // See https://goo.gl/5Hj3o0.
    MemoryDumpManager::GetInstance()->DisablePeriodicDumpsForTesting();
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        mock_dump_provider_.get());
    mock_dump_provider_.reset();
    ContentBrowserTest::TearDown();
  }

  void EnableMemoryTracing() {
    base::RunLoop run_loop;
    std::string category_filter = MemoryDumpManager::kTraceCategoryForTesting;
    base::trace_event::TraceConfig trace_config(category_filter, "");
    bool success = TracingController::GetInstance()->EnableRecording(
      trace_config, run_loop.QuitClosure());
    EXPECT_TRUE(success);
    run_loop.Run();
  }

  void DisableTracing() {
    bool success = TracingController::GetInstance()->DisableRecording(NULL);
    EXPECT_TRUE(success);
    base::RunLoop().RunUntilIdle();
  }

  void RequestGlobalDumpAndWait(bool from_renderer_thread) {
    base::RunLoop run_loop;
    base::trace_event::MemoryDumpCallback callback = base::Bind(
        &MemoryTracingTest::OnGlobalMemoryDumpDone, base::Unretained(this),
        base::ThreadTaskRunnerHandle::Get(), run_loop.QuitClosure());
    if (from_renderer_thread) {
      PostTaskToInProcessRendererAndWait(
          base::Bind(&MemoryTracingTest::DoRequestGlobalDump,
                     base::Unretained(this), callback));
    } else {
      DoRequestGlobalDump(callback);
    }
    run_loop.Run();
  }

  void Navigate(Shell* shell) {
    NavigateToURL(shell, GetTestUrl("", "title.html"));
  }

  base::Closure on_memory_dump_complete_closure_;
  scoped_ptr<MockDumpProvider> mock_dump_provider_;
  uint32 callback_call_count_;
  uint64 last_callback_dump_guid_;
  bool last_callback_success_;
};

class SingleProcessMemoryTracingTest : public MemoryTracingTest {
 public:
  SingleProcessMemoryTracingTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
  }
};

// This test is flaky on Mac. See crbug.com/522009.
#if defined(OS_MACOSX)
#define MAYBE_BrowserInitiatedSingleDump DISABLED_BrowserInitiatedSingleDump
#else
#define MAYBE_BrowserInitiatedSingleDump BrowserInitiatedSingleDump
#endif
// Checks that a memory dump initiated from a the main browser thread ends up in
// a single dump even in single process mode.
IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest,
                       MAYBE_BrowserInitiatedSingleDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(false /* from_renderer_thread */);
  EXPECT_EQ(1u, callback_call_count_);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);
  DisableTracing();
}

// This test is flaky on Mac. See crbug.com/522009.
#if defined(OS_MACOSX)
#define MAYBE_RendererInitiatedSingleDump DISABLED_RendererInitiatedSingleDump
#else
#define MAYBE_RendererInitiatedSingleDump RendererInitiatedSingleDump
#endif
// Checks that a memory dump initiated from a renderer thread ends up in a
// single dump even in single process mode.
IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest,
                       MAYBE_RendererInitiatedSingleDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(true /* from_renderer_thread */);
  EXPECT_EQ(1u, callback_call_count_);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);
  DisableTracing();
}

// This test is flaky on Mac. See crbug.com/522009.
#if defined(OS_MACOSX)
#define MAYBE_ManyInterleavedDumps DISABLED_ManyInterleavedDumps
#else
#define MAYBE_ManyInterleavedDumps ManyInterleavedDumps
#endif
IN_PROC_BROWSER_TEST_F(SingleProcessMemoryTracingTest,
                       MAYBE_ManyInterleavedDumps) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_))
      .Times(4)
      .WillRepeatedly(Return(true));

  EnableMemoryTracing();

  RequestGlobalDumpAndWait(true /* from_renderer_thread */);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);

  RequestGlobalDumpAndWait(false /* from_renderer_thread */);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);

  RequestGlobalDumpAndWait(false /* from_renderer_thread */);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);

  RequestGlobalDumpAndWait(true /* from_renderer_thread */);
  EXPECT_EQ(4u, callback_call_count_);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);

  DisableTracing();
}

// Checks that a memory dump initiated from a the main browser thread ends up in
// a successful dump.
IN_PROC_BROWSER_TEST_F(MemoryTracingTest, BrowserInitiatedDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(false /* from_renderer_thread */);
  EXPECT_EQ(1u, callback_call_count_);
  EXPECT_NE(0u, last_callback_dump_guid_);
  EXPECT_TRUE(last_callback_success_);
  DisableTracing();
}

}  // namespace content
