// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_TAB_STRIP_MODEL_H_
#define CHROME_BROWSER_TABS_TAB_STRIP_MODEL_H_
#pragma once

#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/tabs/tab_strip_model_observer.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/page_transition_types.h"

class NavigationController;
class Profile;
class TabContents;
class TabStripModelDelegate;
class TabStripModelOrderController;

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModel
//
//  A model & low level controller of a Browser Window tabstrip. Holds a vector
//  of TabContents, and provides an API for adding, removing and shuffling
//  them, as well as a higher level API for doing specific Browser-related
//  tasks like adding new Tabs from just a URL, etc.
//
// Each tab may be any one of the following states:
// . Mini-tab. Mini tabs are locked to the left side of the tab strip and
//   rendered differently (small tabs with only a favicon). The model makes
//   sure all mini-tabs are at the beginning of the tab strip. For example,
//   if a non-mini tab is added it is forced to be with non-mini tabs. Requests
//   to move tabs outside the range of the tab type are ignored. For example,
//   a request to move a mini-tab after non-mini-tabs is ignored.
//   You'll notice there is no explcit api for making a tab a mini-tab, rather
//   there are two tab types that are implicitly mini-tabs:
//   . App. Corresponds to an extension that wants an app tab. App tabs are
//     identified by TabContents::is_app(). App tabs are always pinneded (you
//     can't unpin them).
//   . Pinned. Any tab can be pinned. Non-app tabs whose pinned state is changed
//     are moved to be with other mini-tabs or non-mini tabs.
//
//  A TabStripModel has one delegate that it relies on to perform certain tasks
//  like creating new TabStripModels (probably hosted in Browser windows) when
//  required. See TabStripDelegate above for more information.
//
//  A TabStripModel also has N observers (see TabStripModelObserver above),
//  which can be registered via Add/RemoveObserver. An Observer is notified of
//  tab creations, removals, moves, and other interesting events. The
//  TabStrip implements this interface to know when to create new tabs in
//  the View, and the Browser object likewise implements to be able to update
//  its bookkeeping when such events happen.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModel : public NotificationObserver {
 public:
  // Policy for how new tabs are inserted.
  enum InsertionPolicy {
    // Newly created tabs are created after the selection. This is the default.
    INSERT_AFTER,

    // Newly created tabs are inserted before the selection.
    INSERT_BEFORE,
  };

  // Used to specify what should happen when the tab is closed.
  enum CloseTypes {
    CLOSE_NONE                     = 0,

    // Indicates the tab was closed by the user. If true,
    // TabContents::set_closed_by_user_gesture(true) is invoked.
    CLOSE_USER_GESTURE             = 1 << 0,

    // If true the history is recorded so that the tab can be reopened later.
    // You almost always want to set this.
    CLOSE_CREATE_HISTORICAL_TAB    = 1 << 1,
  };

  // Constants used when adding tabs.
  enum AddTabTypes {
    // Used to indicate nothing special should happen to the newly inserted
    // tab.
    ADD_NONE          = 0,

    // The tab should be selected.
    ADD_SELECTED      = 1 << 0,

    // The tab should be pinned.
    ADD_PINNED        = 1 << 1,

    // If not set the insertion index of the TabContents is left up to the Order
    // Controller associated, so the final insertion index may differ from the
    // specified index. Otherwise the index supplied is used.
    ADD_FORCE_INDEX   = 1 << 2,

    // If set the newly inserted tab inherits the group of the currently
    // selected tab. If not set the tab may still inherit the group under
    // certain situations.
    ADD_INHERIT_GROUP = 1 << 3,

    // If set the newly inserted tab's opener is set to the currently selected
    // tab. If not set the tab may still inherit the group/opener under certain
    // situations.
    // NOTE: this is ignored if ADD_INHERIT_GROUP is set.
    ADD_INHERIT_OPENER = 1 << 4,
  };

  static const int kNoTab = -1;

  // Construct a TabStripModel with a delegate to help it do certain things
  // (See TabStripModelDelegate documentation). |delegate| cannot be NULL.
  TabStripModel(TabStripModelDelegate* delegate, Profile* profile);
  virtual ~TabStripModel();

  // Retrieves the TabStripModelDelegate associated with this TabStripModel.
  TabStripModelDelegate* delegate() const { return delegate_; }

  // Add and remove observers to changes within this TabStripModel.
  void AddObserver(TabStripModelObserver* observer);
  void RemoveObserver(TabStripModelObserver* observer);

  // Retrieve the number of TabContentses/emptiness of the TabStripModel.
  int count() const { return static_cast<int>(contents_data_.size()); }
  bool empty() const { return contents_data_.empty(); }

  // Retrieve the Profile associated with this TabStripModel.
  Profile* profile() const { return profile_; }

  // Retrieve the index of the currently selected TabContents.
  int selected_index() const { return selected_index_; }

  // Returns true if the tabstrip is currently closing all open tabs (via a
  // call to CloseAllTabs). As tabs close, the selection in the tabstrip
  // changes which notifies observers, which can use this as an optimization to
  // avoid doing meaningless or unhelpful work.
  bool closing_all() const { return closing_all_; }

  // Access the order controller. Exposed only for unit tests.
  TabStripModelOrderController* order_controller() const {
    return order_controller_;
  }

  // Sets the insertion policy. Default is INSERT_AFTER.
  void SetInsertionPolicy(InsertionPolicy policy);
  InsertionPolicy insertion_policy() const;

  // Returns true if |observer| is in the list of observers. This is intended
  // for debugging.
  bool HasObserver(TabStripModelObserver* observer);

  // Basic API /////////////////////////////////////////////////////////////////

  // Determines if the specified index is contained within the TabStripModel.
  bool ContainsIndex(int index) const;

  // Adds the specified TabContents in the default location. Tabs opened in the
  // foreground inherit the group of the previously selected tab.
  void AppendTabContents(TabContents* contents, bool foreground);

  // Adds the specified TabContents at the specified location. |add_types| is a
  // bitmask of AddTypes; see it for details.
  //
  // All append/insert methods end up in this method.
  //
  // NOTE: adding a tab using this method does NOT query the order controller,
  // as such the ADD_FORCE_INDEX AddType is meaningless here.  The only time the
  // |index| is changed is if using the index would result in breaking the
  // constraint that all mini-tabs occur before non-mini-tabs.
  // See also AddTabContents.
  void InsertTabContentsAt(int index,
                           TabContents* contents,
                           int add_types);

  // Closes the TabContents at the specified index. This causes the TabContents
  // to be destroyed, but it may not happen immediately (e.g. if it's a
  // TabContents). |close_types| is a bitmask of CloseTypes.
  // Returns true if the TabContents was closed immediately, false if it was not
  // closed (we may be waiting for a response from an onunload handler, or
  // waiting for the user to confirm closure).
  bool CloseTabContentsAt(int index, uint32 close_types);

  // Replaces the entire state of a the tab at index by switching in a
  // different NavigationController. This is used through the recently
  // closed tabs list, which needs to replace a tab's current state
  // and history with another set of contents and history.
  //
  // The old NavigationController is deallocated and this object takes
  // ownership of the passed in controller.
  void ReplaceNavigationControllerAt(int index,
                                     NavigationController* controller);

  // Replaces the tab contents at |index| with |new_contents|. This deletes the
  // TabContents currently at |index|.
  void ReplaceTabContentsAt(int index, TabContents* new_contents);

  // Detaches the TabContents at the specified index from this strip. The
  // TabContents is not destroyed, just removed from display. The caller is
  // responsible for doing something with it (e.g. stuffing it into another
  // strip).
  TabContents* DetachTabContentsAt(int index);

  // Select the TabContents at the specified index. |user_gesture| is true if
  // the user actually clicked on the tab or navigated to it using a keyboard
  // command, false if the tab was selected as a by-product of some other
  // action.
  void SelectTabContentsAt(int index, bool user_gesture);

  // Move the TabContents at the specified index to another index. This method
  // does NOT send Detached/Attached notifications, rather it moves the
  // TabContents inline and sends a Moved notification instead.
  // If |select_after_move| is false, whatever tab was selected before the move
  // will still be selected, but it's index may have incremented or decremented
  // one slot.
  // NOTE: this does nothing if the move would result in app tabs and non-app
  // tabs mixing.
  void MoveTabContentsAt(int index, int to_position, bool select_after_move);

  // Returns the currently selected TabContents, or NULL if there is none.
  TabContents* GetSelectedTabContents() const;

  // Returns the TabContents at the specified index, or NULL if there is none.
  TabContents* GetTabContentsAt(int index) const;

  // Returns the index of the specified TabContents, or TabContents::kNoTab if
  // the TabContents is not in this TabStripModel.
  int GetIndexOfTabContents(const TabContents* contents) const;

  // Returns the index of the specified NavigationController, or -1 if it is
  // not in this TabStripModel.
  int GetIndexOfController(const NavigationController* controller) const;

  // Notify any observers that the TabContents at the specified index has
  // changed in some way. See TabChangeType for details of |change_type|.
  void UpdateTabContentsStateAt(
      int index,
      TabStripModelObserver::TabChangeType change_type);

  // Make sure there is an auto-generated New Tab tab in the TabStripModel.
  // If |force_create| is true, the New Tab will be created even if the
  // preference is set to false (used by startup).
  void EnsureNewTabVisible(bool force_create);

  // Close all tabs at once. Code can use closing_all() above to defer
  // operations that might otherwise by invoked by the flurry of detach/select
  // notifications this method causes.
  void CloseAllTabs();

  // Returns true if there are any TabContents that are currently loading.
  bool TabsAreLoading() const;

  // Returns the controller controller that opened the TabContents at |index|.
  NavigationController* GetOpenerOfTabContentsAt(int index);

  // Returns the index of the next TabContents in the sequence of TabContentses
  // spawned by the specified NavigationController after |start_index|.
  // If |use_group| is true, the group property of the tab is used instead of
  // the opener to find the next tab. Under some circumstances the group
  // relationship may exist but the opener may not.
  int GetIndexOfNextTabContentsOpenedBy(const NavigationController* opener,
                                        int start_index,
                                        bool use_group) const;

  // Returns the index of the first TabContents in the model opened by the
  // specified opener.
  int GetIndexOfFirstTabContentsOpenedBy(const NavigationController* opener,
                                         int start_index) const;

  // Returns the index of the last TabContents in the model opened by the
  // specified opener, starting at |start_index|.
  int GetIndexOfLastTabContentsOpenedBy(const NavigationController* opener,
                                        int start_index) const;

  // Called by the Browser when a navigation is about to occur in the specified
  // TabContents. Depending on the tab, and the transition type of the
  // navigation, the TabStripModel may adjust its selection and grouping
  // behavior.
  void TabNavigating(TabContents* contents, PageTransition::Type transition);

  // Forget all Opener relationships that are stored (but _not_ group
  // relationships!) This is to reduce unpredictable tab switching behavior
  // in complex session states. The exact circumstances under which this method
  // is called are left up to the implementation of the selected
  // TabStripModelOrderController.
  void ForgetAllOpeners();

  // Forgets the group affiliation of the specified TabContents. This should be
  // called when a TabContents that is part of a logical group of tabs is
  // moved to a new logical context by the user (e.g. by typing a new URL or
  // selecting a bookmark). This also forgets the opener, which is considered
  // a weaker relationship than group.
  void ForgetGroup(TabContents* contents);

  // Returns true if the group/opener relationships present for |contents|
  // should be reset when _any_ selection change occurs in the model.
  bool ShouldResetGroupOnSelect(TabContents* contents) const;

  // Changes the blocked state of the tab at |index|.
  void SetTabBlocked(int index, bool blocked);

  // Changes the pinned state of the tab at |index|. See description above
  // class for details on this.
  void SetTabPinned(int index, bool pinned);

  // Returns true if the tab at |index| is pinned.
  // See description above class for details on pinned tabs.
  bool IsTabPinned(int index) const;

  // Is the tab a mini-tab?
  // See description above class for details on this.
  bool IsMiniTab(int index) const;

  // Is the tab at |index| an app?
  // See description above class for details on app tabs.
  bool IsAppTab(int index) const;

  // Returns true if the tab at |index| is blocked by a tab modal dialog.
  bool IsTabBlocked(int index) const;

  // Returns the index of the first tab that is not a mini-tab. This returns
  // |count()| if all of the tabs are mini-tabs, and 0 if none of the tabs are
  // mini-tabs.
  int IndexOfFirstNonMiniTab() const;

  // Returns a valid index for inserting a new tab into this model. |index| is
  // the proposed index and |mini_tab| is true if inserting a tab will become
  // mini (pinned or app). If |mini_tab| is true, the returned index is between
  // 0 and IndexOfFirstNonMiniTab. If |mini_tab| is false, the returned index
  // is between IndexOfFirstNonMiniTab and count().
  int ConstrainInsertionIndex(int index, bool mini_tab);

  // Command level API /////////////////////////////////////////////////////////

  // Adds a TabContents at the best position in the TabStripModel given the
  // specified insertion index, transition, etc. |add_types| is a bitmask of
  // AddTypes; see it for details. This method ends up calling into
  // InsertTabContentsAt to do the actual inertion.
  void AddTabContents(TabContents* contents,
                      int index,
                      PageTransition::Type transition,
                      int add_types);

  // Closes the selected TabContents.
  void CloseSelectedTab();

  // Select adjacent tabs
  void SelectNextTab();
  void SelectPreviousTab();

  // Selects the last tab in the tab strip.
  void SelectLastTab();

  // Swap adjacent tabs.
  void MoveTabNext();
  void MoveTabPrevious();

  // View API //////////////////////////////////////////////////////////////////

  // Context menu functions.
  enum ContextMenuCommand {
    CommandFirst = 0,
    CommandNewTab,
    CommandReload,
    CommandDuplicate,
    CommandCloseTab,
    CommandCloseOtherTabs,
    CommandCloseTabsToRight,
    CommandRestoreTab,
    CommandTogglePinned,
    CommandBookmarkAllTabs,
    CommandUseVerticalTabs,
    CommandLast
  };

  // Returns true if the specified command is enabled.
  bool IsContextMenuCommandEnabled(int context_index,
                                   ContextMenuCommand command_id) const;

  // Returns true if the specified command is checked.
  bool IsContextMenuCommandChecked(int context_index,
                                   ContextMenuCommand command_id) const;

  // Performs the action associated with the specified command for the given
  // TabStripModel index |context_index|.
  void ExecuteContextMenuCommand(int context_index,
                                 ContextMenuCommand command_id);

  // Returns a vector of indices of the tabs that will close when executing the
  // command |id| for the tab at |index|. The returned indices are sorted in
  // descending order.
  std::vector<int> GetIndicesClosedByCommand(int index,
                                             ContextMenuCommand id) const;

  // Overridden from notificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // We cannot be constructed without a delegate.
  TabStripModel();

  // Returns true if the specified TabContents is a New Tab at the end of the
  // TabStrip. We check for this because opener relationships are _not_
  // forgotten for the New Tab page opened as a result of a New Tab gesture
  // (e.g. Ctrl+T, etc) since the user may open a tab transiently to look up
  // something related to their current activity.
  bool IsNewTabAtEndOfTabStrip(TabContents* contents) const;

  // Closes the TabContents at the specified indices. This causes the
  // TabContents to be destroyed, but it may not happen immediately.  If the
  // page in question has an unload event the TabContents will not be destroyed
  // until after the event has completed, which will then call back into this
  // method.
  //
  // Returns true if the TabContents were closed immediately, false if we are
  // waiting for the result of an onunload handler.
  bool InternalCloseTabs(const std::vector<int>& indices, uint32 close_types);

  // Invoked from InternalCloseTabs and when an extension is removed for an app
  // tab. Notifies observers of TabClosingAt and deletes |contents|. If
  // |create_historical_tabs| is true, CreateHistoricalTab is invoked on the
  // delegate.
  //
  // The boolean parameter create_historical_tab controls whether to
  // record these tabs and their history for reopening recently closed
  // tabs.
  void InternalCloseTab(TabContents* contents,
                        int index,
                        bool create_historical_tabs);

  TabContents* GetContentsAt(int index) const;

  // The actual implementation of SelectTabContentsAt. Takes the previously
  // selected contents in |old_contents|, which may actually not be in
  // |contents_| anymore because it may have been removed by a call to say
  // DetachTabContentsAt...
  void ChangeSelectedContentsFrom(
      TabContents* old_contents, int to_index, bool user_gesture);

  // Returns the number of New Tab tabs in the TabStripModel.
  int GetNewTabCount() const;

  // Selects either the next tab (|foward| is true), or the previous tab
  // (|forward| is false).
  void SelectRelativeTab(bool forward);

  // Does the work of MoveTabContentsAt. This has no checks to make sure the
  // position is valid, those are done in MoveTabContentsAt.
  void MoveTabContentsAtImpl(int index,
                             int to_position,
                             bool select_after_move);

  // Returns true if the tab represented by the specified data has an opener
  // that matches the specified one. If |use_group| is true, then this will
  // fall back to check the group relationship as well.
  struct TabContentsData;
  static bool OpenerMatches(const TabContentsData* data,
                            const NavigationController* opener,
                            bool use_group);

  // Our delegate.
  TabStripModelDelegate* delegate_;

  // A hunk of data representing a TabContents and (optionally) the
  // NavigationController that spawned it. This memory only sticks around while
  // the TabContents is in the current TabStripModel, unless otherwise
  // specified in code.
  struct TabContentsData {
    explicit TabContentsData(TabContents* a_contents)
        : contents(a_contents),
          reset_group_on_select(false),
          pinned(false),
          blocked(false) {
      SetGroup(NULL);
    }

    // Create a relationship between this TabContents and other TabContentses.
    // Used to identify which TabContents to select next after one is closed.
    void SetGroup(NavigationController* a_group) {
      group = a_group;
      opener = a_group;
    }

    // Forget the opener relationship so that when this TabContents is closed
    // unpredictable re-selection does not occur.
    void ForgetOpener() {
      opener = NULL;
    }

    TabContents* contents;
    // We use NavigationControllers here since they more closely model the
    // "identity" of a Tab, TabContents can change depending on the URL loaded
    // in the Tab.
    // The group is used to model a set of tabs spawned from a single parent
    // tab. This value is preserved for a given tab as long as the tab remains
    // navigated to the link it was initially opened at or some navigation from
    // that page (i.e. if the user types or visits a bookmark or some other
    // navigation within that tab, the group relationship is lost). This
    // property can safely be used to implement features that depend on a
    // logical group of related tabs.
    NavigationController* group;
    // The owner models the same relationship as group, except it is more
    // easily discarded, e.g. when the user switches to a tab not part of the
    // same group. This property is used to determine what tab to select next
    // when one is closed.
    NavigationController* opener;
    // True if our group should be reset the moment selection moves away from
    // this Tab. This is the case for tabs opened in the foreground at the end
    // of the TabStrip while viewing another Tab. If these tabs are closed
    // before selection moves elsewhere, their opener is selected. But if
    // selection shifts to _any_ tab (including their opener), the group
    // relationship is reset to avoid confusing close sequencing.
    bool reset_group_on_select;

    // Is the tab pinned?
    bool pinned;

    // Is the tab interaction blocked by a modal dialog?
    bool blocked;
  };

  // The TabContents data currently hosted within this TabStripModel.
  typedef std::vector<TabContentsData*> TabContentsDataVector;
  TabContentsDataVector contents_data_;

  // The index of the TabContents in |contents_| that is currently selected.
  int selected_index_;

  // A profile associated with this TabStripModel, used when creating new Tabs.
  Profile* profile_;

  // True if all tabs are currently being closed via CloseAllTabs.
  bool closing_all_;

  // An object that determines where new Tabs should be inserted and where
  // selection should move when a Tab is closed.
  TabStripModelOrderController* order_controller_;

  // Our observers.
  typedef ObserverList<TabStripModelObserver> TabStripModelObservers;
  TabStripModelObservers observers_;

  // A scoped container for notification registries.
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModel);
};

#endif  // CHROME_BROWSER_TABS_TAB_STRIP_MODEL_H_
