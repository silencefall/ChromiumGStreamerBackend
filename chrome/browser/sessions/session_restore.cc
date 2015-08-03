// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include <algorithm>
#include <list>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore_delegate.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/sessions/tab_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/session_types.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_state.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_recorder.h"
#endif

using content::NavigationController;
using content::RenderWidgetHost;
using content::WebContents;
using RestoredTab = SessionRestoreDelegate::RestoredTab;

namespace {

class SessionRestoreImpl;

// Pointers to SessionRestoreImpls which are currently restoring the session.
std::set<SessionRestoreImpl*>* active_session_restorers = nullptr;

// SessionRestoreImpl ---------------------------------------------------------

// SessionRestoreImpl is responsible for fetching the set of tabs to create
// from SessionService. SessionRestoreImpl deletes itself when done.

class SessionRestoreImpl : public content::NotificationObserver {
 public:
  SessionRestoreImpl(Profile* profile,
                     Browser* browser,
                     chrome::HostDesktopType host_desktop_type,
                     bool synchronous,
                     bool clobber_existing_tab,
                     bool always_create_tabbed_browser,
                     const std::vector<GURL>& urls_to_open,
                     SessionRestore::CallbackList* callbacks)
      : profile_(profile),
        browser_(browser),
        host_desktop_type_(host_desktop_type),
        synchronous_(synchronous),
        clobber_existing_tab_(clobber_existing_tab),
        always_create_tabbed_browser_(always_create_tabbed_browser),
        urls_to_open_(urls_to_open),
        active_window_id_(0),
        restore_started_(base::TimeTicks::Now()),
        on_session_restored_callbacks_(callbacks) {
    // For sanity's sake, if |browser| is non-null: force |host_desktop_type| to
    // be the same as |browser|'s desktop type.
    DCHECK(!browser || browser->host_desktop_type() == host_desktop_type);

    if (active_session_restorers == nullptr)
      active_session_restorers = new std::set<SessionRestoreImpl*>();

    // Only one SessionRestoreImpl should be operating on the profile at the
    // same time.
    std::set<SessionRestoreImpl*>::const_iterator it;
    for (it = active_session_restorers->begin();
         it != active_session_restorers->end(); ++it) {
      if ((*it)->profile_ == profile)
        break;
    }
    DCHECK(it == active_session_restorers->end());

    active_session_restorers->insert(this);

    // When asynchronous its possible for there to be no windows. To make sure
    // Chrome doesn't prematurely exit AddRef the process. We'll release in the
    // destructor when restore is done.
    g_browser_process->AddRefModule();
  }

  bool synchronous() const { return synchronous_; }

  Browser* Restore() {
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile_);
    DCHECK(session_service);
    session_service->GetLastSession(
        base::Bind(&SessionRestoreImpl::OnGotSession, base::Unretained(this)),
        &cancelable_task_tracker_);

    if (synchronous_) {
      {
        base::MessageLoop::ScopedNestableTaskAllower allow(
            base::MessageLoop::current());
        base::RunLoop loop;
        quit_closure_for_sync_restore_ = loop.QuitClosure();
        loop.Run();
        quit_closure_for_sync_restore_ = base::Closure();
      }
      Browser* browser =
          ProcessSessionWindowsAndNotify(&windows_, active_window_id_);
      delete this;
      return browser;
    }

    if (browser_) {
      registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                     content::Source<Browser>(browser_));
    }

    return browser_;
  }

  // Restore window(s) from a foreign session. Returns newly created Browsers.
  std::vector<Browser*> RestoreForeignSession(
      std::vector<const sessions::SessionWindow*>::const_iterator begin,
      std::vector<const sessions::SessionWindow*>::const_iterator end) {
    std::vector<Browser*> browsers;
    std::vector<RestoredTab> created_contents;
    // Create a browser instance to put the restored tabs in.
    for (std::vector<const sessions::SessionWindow*>::const_iterator i = begin;
         i != end; ++i) {
      Browser* browser =
          CreateRestoredBrowser(BrowserTypeForWindowType((*i)->type),
                                (*i)->bounds, (*i)->show_state, (*i)->app_name);
      browsers.push_back(browser);

      // Restore and show the browser.
      const int initial_tab_count = 0;
      int selected_tab_index =
          std::max(0, std::min((*i)->selected_tab_index,
                               static_cast<int>((*i)->tabs.size()) - 1));
      RestoreTabsToBrowser(*(*i), browser, initial_tab_count,
                           selected_tab_index, &created_contents);
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
    }

    // Always create in a new window.
    FinishedTabCreation(true, true, &created_contents);

    on_session_restored_callbacks_->Notify(
        static_cast<int>(created_contents.size()));

    return browsers;
  }

  // Restore a single tab from a foreign session.
  // Opens in the tab in the last active browser, unless disposition is
  // NEW_WINDOW, in which case the tab will be opened in a new browser. Returns
  // the WebContents of the restored tab.
  WebContents* RestoreForeignTab(const sessions::SessionTab& tab,
                                 WindowOpenDisposition disposition) {
    DCHECK(!tab.navigations.empty());
    int selected_index = tab.current_navigation_index;
    selected_index = std::max(
        0,
        std::min(selected_index, static_cast<int>(tab.navigations.size() - 1)));

    bool use_new_window = disposition == NEW_WINDOW;

    Browser* browser =
        use_new_window
            ? new Browser(Browser::CreateParams(profile_, host_desktop_type_))
            : browser_;

    RecordAppLaunchForTab(browser, tab, selected_index);

    WebContents* web_contents;
    if (disposition == CURRENT_TAB) {
      DCHECK(!use_new_window);
      web_contents = chrome::ReplaceRestoredTab(
          browser, tab.navigations, selected_index, true, tab.extension_app_id,
          nullptr, tab.user_agent_override);
    } else {
      int tab_index =
          use_new_window ? 0 : browser->tab_strip_model()->active_index() + 1;
      web_contents = chrome::AddRestoredTab(
          browser, tab.navigations, tab_index, selected_index,
          tab.extension_app_id,
          disposition == NEW_FOREGROUND_TAB,  // selected
          tab.pinned, true, nullptr, tab.user_agent_override);
      // Start loading the tab immediately.
      web_contents->GetController().LoadIfNecessary();
    }

    if (use_new_window) {
      browser->tab_strip_model()->ActivateTabAt(0, true);
      browser->window()->Show();
    }
    NotifySessionServiceOfRestoredTabs(browser,
                                       browser->tab_strip_model()->count());

    // Since FinishedTabCreation() is not called here, |this| will leak if we
    // are not in sychronous mode.
    DCHECK(synchronous_);

    on_session_restored_callbacks_->Notify(1);

    return web_contents;
  }

  ~SessionRestoreImpl() override {
    STLDeleteElements(&windows_);

    active_session_restorers->erase(this);
    if (active_session_restorers->empty()) {
      delete active_session_restorers;
      active_session_restorers = nullptr;
    }

    g_browser_process->ReleaseModule();
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case chrome::NOTIFICATION_BROWSER_CLOSED:
        delete this;
        return;

      default:
        NOTREACHED();
        break;
    }
  }

  Profile* profile() { return profile_; }

 private:
  // Invoked when done with creating all the tabs/browsers.
  //
  // |created_tabbed_browser| indicates whether a tabbed browser was created,
  // or we used an existing tabbed browser.
  //
  // If successful, this begins loading tabs and deletes itself when all tabs
  // have been loaded.
  //
  // Returns the Browser that was created, if any.
  Browser* FinishedTabCreation(bool succeeded,
                               bool created_tabbed_browser,
                               std::vector<RestoredTab>* contents_created) {
    Browser* browser = nullptr;
    if (!created_tabbed_browser && always_create_tabbed_browser_) {
      browser =
          new Browser(Browser::CreateParams(profile_, host_desktop_type_));
      if (urls_to_open_.empty()) {
        // No tab browsers were created and no URLs were supplied on the command
        // line. Open the new tab page.
        urls_to_open_.push_back(GURL(chrome::kChromeUINewTabURL));
      }
      AppendURLsToBrowser(browser, urls_to_open_);
      browser->window()->Show();
    }

    if (succeeded) {
      if (SessionRestore::GetSmartRestoreMode() !=
          SessionRestore::SMART_RESTORE_MODE_OFF) {
        std::stable_sort(contents_created->begin(), contents_created->end());
      }
      // Start Loading tabs.
      SessionRestoreDelegate::RestoreTabs(*contents_created, restore_started_);
    }

    if (!synchronous_) {
      // If we're not synchronous we need to delete ourself.
      // NOTE: we must use DeleteLater here as most likely we're in a callback
      // from the history service which doesn't deal well with deleting the
      // object it is notifying.
      base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);

      // The delete may take a while and at this point we no longer care about
      // if the browser is deleted. Don't listen to anything. This avoid a
      // possible double delete too (if browser is closed before DeleteSoon() is
      // processed).
      registrar_.RemoveAll();
    }

#if defined(OS_CHROMEOS)
    chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker("SessionRestore-End",
                                                           false);
#endif
    return browser;
  }

  void OnGotSession(ScopedVector<sessions::SessionWindow> windows,
                    SessionID::id_type active_window_id) {
#if defined(OS_CHROMEOS)
    chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
        "SessionRestore-GotSession", false);
#endif
    if (synchronous_) {
      // See comment above windows_ as to why we don't process immediately.
      windows_.swap(windows.get());
      active_window_id_ = active_window_id;
      CHECK(!quit_closure_for_sync_restore_.is_null());
      quit_closure_for_sync_restore_.Run();
      return;
    }

    ProcessSessionWindowsAndNotify(&windows.get(), active_window_id);
  }

  Browser* ProcessSessionWindowsAndNotify(
      std::vector<sessions::SessionWindow*>* windows,
      SessionID::id_type active_window_id) {
    std::vector<RestoredTab> contents;
    Browser* result =
        ProcessSessionWindows(windows, active_window_id, &contents);
    on_session_restored_callbacks_->Notify(static_cast<int>(contents.size()));
    return result;
  }

  Browser* ProcessSessionWindows(std::vector<sessions::SessionWindow*>* windows,
                                 SessionID::id_type active_window_id,
                                 std::vector<RestoredTab>* created_contents) {
    DVLOG(1) << "ProcessSessionWindows " << windows->size();

    if (windows->empty()) {
      // Restore was unsuccessful. The DOM storage system can also delete its
      // data, since no session restore will happen at a later point in time.
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetDOMStorageContext()
          ->StartScavengingUnusedSessionStorage();
      return FinishedTabCreation(false, false, created_contents);
    }

#if defined(OS_CHROMEOS)
    chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
        "SessionRestore-CreatingTabs-Start", false);
#endif

    // After the for loop this contains the last TABBED_BROWSER. Is null if no
    // tabbed browsers exist.
    Browser* last_browser = nullptr;
    bool has_tabbed_browser = false;

    // After the for loop, this contains the browser to activate, if one of the
    // windows has the same id as specified in active_window_id.
    Browser* browser_to_activate = nullptr;

    // Determine if there is a visible window.
    bool has_visible_browser = false;
    for (std::vector<sessions::SessionWindow*>::iterator i = windows->begin();
         i != windows->end(); ++i) {
      if ((*i)->show_state != ui::SHOW_STATE_MINIMIZED)
        has_visible_browser = true;
    }

    for (std::vector<sessions::SessionWindow*>::iterator i = windows->begin();
         i != windows->end(); ++i) {
      Browser* browser = nullptr;
      if (!has_tabbed_browser &&
          (*i)->type == sessions::SessionWindow::TYPE_TABBED)
        has_tabbed_browser = true;
      if (i == windows->begin() &&
          (*i)->type == sessions::SessionWindow::TYPE_TABBED && browser_ &&
          browser_->is_type_tabbed() &&
          !browser_->profile()->IsOffTheRecord()) {
        // The first set of tabs is added to the existing browser.
        browser = browser_;
      } else {
#if defined(OS_CHROMEOS)
        chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
            "SessionRestore-CreateRestoredBrowser-Start", false);
#endif
        // Show the first window if none are visible.
        ui::WindowShowState show_state = (*i)->show_state;
        if (!has_visible_browser) {
          show_state = ui::SHOW_STATE_NORMAL;
          has_visible_browser = true;
        }
        browser =
            CreateRestoredBrowser(BrowserTypeForWindowType((*i)->type),
                                  (*i)->bounds, show_state, (*i)->app_name);
#if defined(OS_CHROMEOS)
        chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
            "SessionRestore-CreateRestoredBrowser-End", false);
#endif
      }
      if ((*i)->type == sessions::SessionWindow::TYPE_TABBED)
        last_browser = browser;
      WebContents* active_tab =
          browser->tab_strip_model()->GetActiveWebContents();
      int initial_tab_count = browser->tab_strip_model()->count();
      bool close_active_tab =
          clobber_existing_tab_ && i == windows->begin() &&
          (*i)->type == sessions::SessionWindow::TYPE_TABBED && active_tab &&
          browser == browser_ && (*i)->tabs.size() > 0;
      if (close_active_tab)
        --initial_tab_count;
      int selected_tab_index =
          initial_tab_count > 0
              ? browser->tab_strip_model()->active_index()
              : std::max(0, std::min((*i)->selected_tab_index,
                                     static_cast<int>((*i)->tabs.size()) - 1));
      if ((*i)->window_id.id() == active_window_id)
        browser_to_activate = browser;

      RestoreTabsToBrowser(*(*i), browser, initial_tab_count,
                           selected_tab_index, created_contents);
      NotifySessionServiceOfRestoredTabs(browser, initial_tab_count);
      // This needs to be done after restore because closing the last tab will
      // close the whole window.
      if (close_active_tab)
        chrome::CloseWebContents(browser, active_tab, true);
    }

    if (browser_to_activate && browser_to_activate->is_type_tabbed())
      last_browser = browser_to_activate;

    if (last_browser && !urls_to_open_.empty())
      AppendURLsToBrowser(last_browser, urls_to_open_);
#if defined(OS_CHROMEOS)
    chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
        "SessionRestore-CreatingTabs-End", false);
#endif
    if (browser_to_activate)
      browser_to_activate->window()->Activate();

    // If last_browser is NULL and urls_to_open_ is non-empty,
    // FinishedTabCreation will create a new TabbedBrowser and add the urls to
    // it.
    Browser* finished_browser =
        FinishedTabCreation(true, has_tabbed_browser, created_contents);
    if (finished_browser)
      last_browser = finished_browser;

    // sessionStorages needed for the session restore have now been recreated
    // by RestoreTab. Now it's safe for the DOM storage system to start
    // deleting leftover data.
    content::BrowserContext::GetDefaultStoragePartition(profile_)
        ->GetDOMStorageContext()
        ->StartScavengingUnusedSessionStorage();
    return last_browser;
  }

  // Record an app launch event (if appropriate) for a tab which is about to
  // be restored. Callers should ensure that selected_index is within the
  // bounds of tab.navigations before calling.
  void RecordAppLaunchForTab(Browser* browser,
                             const sessions::SessionTab& tab,
                             int selected_index) {
    DCHECK(selected_index >= 0 &&
           selected_index < static_cast<int>(tab.navigations.size()));
    GURL url = tab.navigations[selected_index].virtual_url();
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile())
            ->enabled_extensions()
            .GetAppByURL(url);
    if (extension) {
      extensions::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_SESSION_RESTORE, extension->GetType());
    }
  }

  // Adds the tabs from |window| to |browser|. Normal tabs go after the existing
  // tabs but pinned tabs will be pushed in front.
  // If there are no existing tabs, the tab at |selected_tab_index| will be
  // selected. Otherwise, the tab selection will remain untouched.
  void RestoreTabsToBrowser(const sessions::SessionWindow& window,
                            Browser* browser,
                            int initial_tab_count,
                            int selected_tab_index,
                            std::vector<RestoredTab>* created_contents) {
    DVLOG(1) << "RestoreTabsToBrowser " << window.tabs.size();
    DCHECK(!window.tabs.empty());
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeTicks highest_time = base::TimeTicks::UnixEpoch();
    if (initial_tab_count == 0) {
      if (SessionRestore::GetSmartRestoreMode() ==
          SessionRestore::SMART_RESTORE_MODE_MRU) {
        // The last active time of a WebContents is initially set to the
        // creation time of the tab, which is not necessarly the same as the
        // loading time, so we have to restore the values. Also, since TimeTicks
        // only make sense in their current session, these values have to be
        // sanitized first. To do so, we need to first figure out the largest
        // time. This will then be used to set the last active time of
        // each tab where the most recent tab will have its time set to |now|
        // and the rest of the tabs will have theirs set earlier by the same
        // delta as they originally had.
        for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
          const sessions::SessionTab& tab = *(window.tabs[i]);
          if (tab.last_active_time > highest_time)
            highest_time = tab.last_active_time;
        }
      }
      for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
        const sessions::SessionTab& tab = *(window.tabs[i]);

        // Loads are scheduled for each restored tab unless the tab is going to
        // be selected as ShowBrowser() will load the selected tab.
        bool is_selected_tab = (i == selected_tab_index);
        WebContents* contents = RestoreTab(tab, i, browser, is_selected_tab);

        // RestoreTab can return nullptr if |tab| doesn't have valid data.
        if (!contents)
          continue;

        // Sanitize the last active time.
        if (SessionRestore::GetSmartRestoreMode() ==
            SessionRestore::SMART_RESTORE_MODE_MRU) {
          base::TimeDelta delta = highest_time - tab.last_active_time;
          contents->SetLastActiveTime(now - delta);
        }

        RestoredTab restored_tab(contents, is_selected_tab,
                                 tab.extension_app_id.empty(), tab.pinned);
        created_contents->push_back(restored_tab);

        // If this isn't the selected tab, there's nothing else to do.
        if (!is_selected_tab)
          continue;

        ShowBrowser(browser, browser->tab_strip_model()->GetIndexOfWebContents(
                                 contents));
        // TODO(sky): remove. For debugging 368236.
        CHECK_EQ(browser->tab_strip_model()->GetActiveWebContents(), contents);
      }
    } else {
      // If the browser already has tabs, we want to restore the new ones after
      // the existing ones. E.g. this happens in Win8 Metro where we merge
      // windows or when launching a hosted app from the app launcher.
      int tab_index_offset = initial_tab_count;
      for (int i = 0; i < static_cast<int>(window.tabs.size()); ++i) {
        const sessions::SessionTab& tab = *(window.tabs[i]);
        // Always schedule loads as we will not be calling ShowBrowser().
        WebContents* contents =
            RestoreTab(tab, tab_index_offset + i, browser, false);
        if (contents) {
          // Sanitize the last active time.
          if (SessionRestore::GetSmartRestoreMode() ==
              SessionRestore::SMART_RESTORE_MODE_MRU) {
            base::TimeDelta delta = highest_time - tab.last_active_time;
            contents->SetLastActiveTime(now - delta);
          }
          RestoredTab restored_tab(contents, false,
                                   tab.extension_app_id.empty(), tab.pinned);
          created_contents->push_back(restored_tab);
        }
      }
    }
  }

  // |tab_index| is ignored for pinned tabs which will always be pushed behind
  // the last existing pinned tab.
  // |tab_loader_| will schedule this tab for loading if |is_selected_tab| is
  // false.
  WebContents* RestoreTab(const sessions::SessionTab& tab,
                          const int tab_index,
                          Browser* browser,
                          bool is_selected_tab) {
    // It's possible (particularly for foreign sessions) to receive a tab
    // without valid navigations. In that case, just skip it.
    // See crbug.com/154129.
    if (tab.navigations.empty())
      return nullptr;
    int selected_index = tab.current_navigation_index;
    selected_index = std::max(
        0,
        std::min(selected_index, static_cast<int>(tab.navigations.size() - 1)));

    RecordAppLaunchForTab(browser, tab, selected_index);

    // Associate sessionStorage (if any) to the restored tab.
    scoped_refptr<content::SessionStorageNamespace> session_storage_namespace;
    if (!tab.session_storage_persistent_id.empty()) {
      session_storage_namespace =
          content::BrowserContext::GetDefaultStoragePartition(profile_)
              ->GetDOMStorageContext()
              ->RecreateSessionStorage(tab.session_storage_persistent_id);
    }

    WebContents* web_contents = chrome::AddRestoredTab(
        browser, tab.navigations, tab_index, selected_index,
        tab.extension_app_id,
        false,  // select
        tab.pinned, true, session_storage_namespace.get(),
        tab.user_agent_override);
    // Regression check: check that the tab didn't start loading right away. The
    // focused tab will be loaded by Browser, and TabLoader will load the rest.
    DCHECK(web_contents->GetController().NeedsReload());

    return web_contents;
  }

  Browser* CreateRestoredBrowser(Browser::Type type,
                                 gfx::Rect bounds,
                                 ui::WindowShowState show_state,
                                 const std::string& app_name) {
    Browser::CreateParams params(type, profile_, host_desktop_type_);
    if (!app_name.empty()) {
      const bool trusted_source = true;  // We only store trusted app windows.
      params = Browser::CreateParams::CreateForApp(
          app_name, trusted_source, bounds, profile_, host_desktop_type_);
    } else {
      params.initial_bounds = bounds;
    }
    params.initial_show_state = show_state;
    params.is_session_restore = true;
    return new Browser(params);
  }

  void ShowBrowser(Browser* browser, int selected_tab_index) {
    DCHECK(browser);
    DCHECK(browser->tab_strip_model()->count());
    browser->tab_strip_model()->ActivateTabAt(selected_tab_index, true);

    if (browser_ == browser)
      return;

    browser->window()->Show();
    browser->set_is_session_restore(false);

    // TODO(jcampan): http://crbug.com/8123 we should not need to set the
    //                initial focus explicitly.
    browser->tab_strip_model()->GetActiveWebContents()->SetInitialFocus();
  }

  // Appends the urls in |urls| to |browser|.
  void AppendURLsToBrowser(Browser* browser, const std::vector<GURL>& urls) {
    for (size_t i = 0; i < urls.size(); ++i) {
      int add_types = TabStripModel::ADD_FORCE_INDEX;
      if (i == 0)
        add_types |= TabStripModel::ADD_ACTIVE;
      chrome::NavigateParams params(browser, urls[i],
                                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
      params.disposition = i == 0 ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
      params.tabstrip_add_types = add_types;
      chrome::Navigate(&params);
    }
  }

  // Invokes TabRestored on the SessionService for all tabs in browser after
  // initial_count.
  void NotifySessionServiceOfRestoredTabs(Browser* browser, int initial_count) {
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile_);
    if (!session_service)
      return;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int i = initial_count; i < tab_strip->count(); ++i)
      session_service->TabRestored(tab_strip->GetWebContentsAt(i),
                                   tab_strip->IsTabPinned(i));
  }

  // The profile to create the sessions for.
  Profile* profile_;

  // The first browser to restore to, may be null.
  Browser* browser_;

  // The desktop on which all new browsers should be created (browser_, if it is
  // not NULL, must be of this desktop type as well).
  chrome::HostDesktopType host_desktop_type_;

  // Whether or not restore is synchronous.
  const bool synchronous_;

  // The quit-closure to terminate the nested message-loop started for
  // synchronous session-restore.
  base::Closure quit_closure_for_sync_restore_;

  // See description of CLOBBER_CURRENT_TAB.
  const bool clobber_existing_tab_;

  // If true and there is an error or there are no windows to restore, we
  // create a tabbed browser anyway. This is used on startup to make sure at
  // at least one window is created.
  const bool always_create_tabbed_browser_;

  // Set of URLs to open in addition to those restored from the session.
  std::vector<GURL> urls_to_open_;

  // Used to get the session.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Responsible for loading the tabs.
  scoped_refptr<TabLoader> tab_loader_;

  // When synchronous we run a nested message loop. To avoid creating windows
  // from the nested message loop (which can make exiting the nested message
  // loop take a while) we cache the SessionWindows here and create the actual
  // windows when the nested message loop exits.
  std::vector<sessions::SessionWindow*> windows_;
  SessionID::id_type active_window_id_;

  content::NotificationRegistrar registrar_;

  // The time we started the restore.
  base::TimeTicks restore_started_;

  // List of callbacks for session restore notification.
  SessionRestore::CallbackList* on_session_restored_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreImpl);
};

}  // namespace

// SessionRestore -------------------------------------------------------------

// static
Browser* SessionRestore::RestoreSession(
    Profile* profile,
    Browser* browser,
    chrome::HostDesktopType host_desktop_type,
    uint32 behavior,
    const std::vector<GURL>& urls_to_open) {
#if defined(OS_CHROMEOS)
  chromeos::BootTimesRecorder::Get()->AddLoginTimeMarker(
      "SessionRestore-Start", false);
#endif
  DCHECK(profile);
  // Always restore from the original profile (incognito profiles have no
  // session service).
  profile = profile->GetOriginalProfile();
  if (!SessionServiceFactory::GetForProfile(profile)) {
    NOTREACHED();
    return nullptr;
  }
  profile->set_restored_last_session(true);
  // SessionRestoreImpl takes care of deleting itself when done.
  SessionRestoreImpl* restorer = new SessionRestoreImpl(
      profile, browser, host_desktop_type, (behavior & SYNCHRONOUS) != 0,
      (behavior & CLOBBER_CURRENT_TAB) != 0,
      (behavior & ALWAYS_CREATE_TABBED_BROWSER) != 0,
      urls_to_open,
      SessionRestore::on_session_restored_callbacks());
  return restorer->Restore();
}

// static
void SessionRestore::RestoreSessionAfterCrash(Browser* browser) {
   uint32 behavior = 0;
  if (browser->tab_strip_model()->count() == 1) {
    const content::WebContents* active_tab =
        browser->tab_strip_model()->GetWebContentsAt(0);
    if (active_tab->GetURL() == GURL(chrome::kChromeUINewTabURL) ||
        search::IsInstantNTP(active_tab)) {
      // There is only one tab and its the new tab page, make session restore
      // clobber it.
      behavior = SessionRestore::CLOBBER_CURRENT_TAB;
    }
  }
  SessionRestore::RestoreSession(browser->profile(), browser,
                                 browser->host_desktop_type(), behavior,
                                 std::vector<GURL>());
}

// static
std::vector<Browser*> SessionRestore::RestoreForeignSessionWindows(
    Profile* profile,
    chrome::HostDesktopType host_desktop_type,
    std::vector<const sessions::SessionWindow*>::const_iterator begin,
    std::vector<const sessions::SessionWindow*>::const_iterator end) {
  std::vector<GURL> gurls;
  SessionRestoreImpl restorer(profile, static_cast<Browser*>(nullptr),
                              host_desktop_type, true, false, true, gurls,
                              on_session_restored_callbacks());
  return restorer.RestoreForeignSession(begin, end);
}

// static
WebContents* SessionRestore::RestoreForeignSessionTab(
    content::WebContents* source_web_contents,
    const sessions::SessionTab& tab,
    WindowOpenDisposition disposition) {
  Browser* browser = chrome::FindBrowserWithWebContents(source_web_contents);
  Profile* profile = browser->profile();
  std::vector<GURL> gurls;
  SessionRestoreImpl restorer(profile, browser, browser->host_desktop_type(),
                              true, false, false, gurls,
                              on_session_restored_callbacks());
  return restorer.RestoreForeignTab(tab, disposition);
}

// static
bool SessionRestore::IsRestoring(const Profile* profile) {
  if (active_session_restorers == nullptr)
    return false;
  for (std::set<SessionRestoreImpl*>::const_iterator it =
           active_session_restorers->begin();
       it != active_session_restorers->end(); ++it) {
    if ((*it)->profile() == profile)
      return true;
  }
  return false;
}

// static
bool SessionRestore::IsRestoringSynchronously() {
  if (!active_session_restorers)
    return false;
  for (std::set<SessionRestoreImpl*>::const_iterator it =
           active_session_restorers->begin();
       it != active_session_restorers->end(); ++it) {
    if ((*it)->synchronous())
      return true;
  }
  return false;
}

// static
SessionRestore::CallbackSubscription
    SessionRestore::RegisterOnSessionRestoredCallback(
        const base::Callback<void(int)>& callback) {
  return on_session_restored_callbacks()->Add(callback);
}

// static
SessionRestore::SmartRestoreMode SessionRestore::GetSmartRestoreMode() {
  std::string prioritize_tabs = variations::GetVariationParamValue(
      "IntelligentSessionRestore", "PrioritizeTabs");
  if (prioritize_tabs == "mru")
    return SMART_RESTORE_MODE_MRU;
  if (prioritize_tabs == "simple")
    return SMART_RESTORE_MODE_SIMPLE;
  return SMART_RESTORE_MODE_OFF;
}

// static
base::CallbackList<void(int)>*
    SessionRestore::on_session_restored_callbacks_ = nullptr;
