//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2005 aMule Team ( admin@amule.org / http://www.amule.org )
//
// Any parts of this program derived from the xMule, lMule or eMule project,
// or contributed by third-party developers are copyrighted by their
// respective authors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA, 02111-1307, USA
//
/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/listctrl.h
// Purpose:     Generic list control
// Author:      Robert Roebling
// Created:     01/02/97
// Id:
// Copyright:   (c) 1998 Robert Roebling, Julian Smart and Markus Holzem
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef LISTCTRL_GEN_H
#define LISTCTRL_GEN_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "listctrl_gen.h"
#endif

#include <wx/defs.h>
#include <wx/wxchar.h>
#include <wx/object.h>
#include <wx/generic/imaglist.h>
#include <wx/control.h>
#include <wx/timer.h>
#include <wx/dcclient.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/listbase.h>

#define wxLC_OWNERDRAW 0x10000


#if wxUSE_DRAG_AND_DROP
class WXDLLEXPORT wxDropTarget;
#endif

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxListItem;
class WXDLLEXPORT wxListEvent;

#if !defined(__WXMSW__) || defined(__WIN16__) || defined(__WXUNIVERSAL__)
class WXDLLEXPORT wxODListCtrl;
#define wxImageListType wxImageList
#else
#define wxImageListType wxGenericImageList
#endif

#include <wx/hashmap.h>
WX_DECLARE_HASH_MAP(long,void*,wxIntegerHash,wxIntegerEqual,ItemDataMap);

//-----------------------------------------------------------------------------
// internal classes
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxListHeaderData;
class WXDLLEXPORT wxListItemData;
class WXDLLEXPORT wxListLineData;

class WXDLLEXPORT wxODListHeaderWindow;
class WXDLLEXPORT wxODListMainWindow;

class WXDLLEXPORT wxListRenameTimer;
class WXDLLEXPORT wxListTextCtrl;

//-----------------------------------------------------------------------------
// wxODListCtrl
//-----------------------------------------------------------------------------

class WXDLLEXPORT wxODGenericListCtrl: public wxControl
{
public:
    wxODGenericListCtrl();
    wxODGenericListCtrl( wxWindow *parent,
                wxWindowID id = -1,
                const wxPoint &pos = wxDefaultPosition,
                const wxSize &size = wxDefaultSize,
                long style = wxLC_ICON,
                const wxValidator& validator = wxDefaultValidator,
                const wxString &name = wxT("listctrl") )
    {
        Create(parent, id, pos, size, style, validator, name);
    }
    ~wxODGenericListCtrl();

    bool Create( wxWindow *parent,
                 wxWindowID id = -1,
                 const wxPoint &pos = wxDefaultPosition,
                 const wxSize &size = wxDefaultSize,
                 long style = wxLC_ICON,
                 const wxValidator& validator = wxDefaultValidator,
                 const wxString &name = wxT("listctrl") );

    bool GetColumn( int col, wxListItem& item ) const;
    bool SetColumn( int col, wxListItem& item );
    int GetColumnWidth( int col ) const;
    bool SetColumnWidth( int col, int width);
    int GetCountPerPage() const; // not the same in wxGLC as in Windows, I think

    void GetVisibleLines(long* first,long* last);

    bool GetItem( wxListItem& info ) const;
    bool SetItem( wxListItem& info ) ;
    long SetItem( long index, int col, const wxString& label, int imageId = -1 );
    int  GetItemState( long item, long stateMask ) const;
    bool SetItemState( long item, long state, long stateMask);
    bool SetItemImage( long item, int image, int selImage);
    wxString GetItemText( long item ) const;
    void SetItemText( long item, const wxString& str );
    long GetItemData( long item ) const;
    bool SetItemData( long item, long data );
    bool GetItemRect( long item, wxRect& rect, int code = wxLIST_RECT_BOUNDS ) const;
    bool GetItemPosition( long item, wxPoint& pos ) const;
    bool SetItemPosition( long item, const wxPoint& pos ); // not supported in wxGLC
    int GetItemCount() const;
    int GetColumnCount() const;
    void SetItemSpacing( int spacing, bool isSmall = FALSE );
    int GetItemSpacing( bool isSmall ) const;
    void SetItemTextColour( long item, const wxColour& col);
    wxColour GetItemTextColour( long item ) const;
    void SetItemBackgroundColour( long item, const wxColour &col);
    wxColour GetItemBackgroundColour( long item ) const;
    int GetSelectedItemCount() const;
    wxColour GetTextColour() const;
    void SetTextColour(const wxColour& col);
    long GetTopItem() const;
    void SetSortArrow(int col, int mode);

    void SetSingleStyle( long style, bool add = TRUE ) ;
    void SetWindowStyleFlag( long style );
    void RecreateWindow() {}
    long GetNextItem( long item, int geometry = wxLIST_NEXT_ALL, int state = wxLIST_STATE_DONTCARE ) const;
    wxImageListType *GetImageList( int which ) const;
    void SetImageList( wxImageListType *imageList, int which );
    void AssignImageList( wxImageListType *imageList, int which );
    bool Arrange( int flag = wxLIST_ALIGN_DEFAULT ); // always wxLIST_ALIGN_LEFT in wxGLC

    void ClearAll();
    bool DeleteItem( long item );
    bool DeleteAllItems();
    bool DeleteAllColumns();
    bool DeleteColumn( int col );

    void SetItemCount(long count);

    void EditLabel( long item ) { Edit(item); }
    void Edit( long item );

    bool EnsureVisible( long item );
    long FindItem( long start, const wxString& str, bool partial = FALSE );
    long FindItem( long start, long data );
    long FindItem( long start, const wxPoint& pt, int direction ); // not supported in wxGLC
    long HitTest( const wxPoint& point, int& flags);
    long InsertItem(wxListItem& info);
    long InsertItem( long index, const wxString& label );
    long InsertItem( long index, int imageIndex );
    long InsertItem( long index, const wxString& label, int imageIndex );
    long InsertColumn( long col, wxListItem& info );
    long InsertColumn( long col, const wxString& heading, int format = wxLIST_FORMAT_LEFT,
      int width = -1 );
    bool ScrollList( int dx, int dy );
    bool SortItems( wxListCtrlCompare fn, long data );
    bool Update( long item );

    // returns true if it is a virtual list control
    bool IsVirtual() const { return (GetWindowStyle() & wxLC_VIRTUAL) != 0; }

    // refresh items selectively (only useful for virtual list controls)
    void RefreshItem(long item);
    void RefreshItems(long itemFrom, long itemTo);

    // implementation only from now on
    // -------------------------------

    void OnIdle( wxIdleEvent &event );
    void OnSize( wxSizeEvent &event );

    // We have to hand down a few functions

    virtual void Freeze();
    virtual void Thaw();

    virtual void OnDrawItem(int item,wxDC* dc,const wxRect& rect,const wxRect& rectHL,bool highlighted);

    virtual bool SetBackgroundColour( const wxColour &colour );
    virtual bool SetForegroundColour( const wxColour &colour );
    virtual wxColour GetBackgroundColour() const;
    virtual wxColour GetForegroundColour() const;
    virtual bool SetFont( const wxFont &font );
    virtual bool SetCursor( const wxCursor &cursor );

#if wxUSE_DRAG_AND_DROP
    virtual void SetDropTarget( wxDropTarget *dropTarget );
    virtual wxDropTarget *GetDropTarget() const;
#endif

    virtual bool DoPopupMenu( wxMenu *menu, int x, int y );

    virtual void SetFocus();
    virtual bool GetFocus();

    // implementation
    // --------------

    wxImageListType         *m_imageListNormal;
    wxImageListType         *m_imageListSmall;
    wxImageListType         *m_imageListState;  // what's that ?
    bool                 m_ownsImageListNormal,
                         m_ownsImageListSmall,
                         m_ownsImageListState;
    wxODListHeaderWindow  *m_headerWin;
    wxODListMainWindow    *m_mainWin;

protected:
    // return the text for the given column of the given item
    virtual wxString OnGetItemText(long item, long column) const;

    // return the icon for the given item
    virtual int OnGetItemImage(long item) const;

    // return the attribute for the item (may return NULL if none)
    virtual wxListItemAttr *OnGetItemAttr(long item) const;

    // it calls our OnGetXXX() functions
    friend class WXDLLEXPORT wxODListMainWindow;

private:
    // Virtual function hiding supression
    virtual void Update() { wxWindow::Update(); }

    // create the header window
    void CreateHeaderWindow();

    // reposition the header and the main window in the report view depending
    // on whether it should be shown or not
    void ResizeReportView(bool showHeader);

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxODGenericListCtrl);
};

#if !defined(__WXMSW__) || defined(__WIN16__) || defined(__WXUNIVERSAL__)
/*
 * wxODListCtrl has to be a real class or we have problems with
 * the run-time information.
 */

class WXDLLEXPORT wxODListCtrl: public wxODGenericListCtrl
{
    DECLARE_DYNAMIC_CLASS(wxODListCtrl)

public:
    wxODListCtrl() {}

    wxODListCtrl(wxWindow *parent, wxWindowID id = -1,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxLC_ICON,
               const wxValidator &validator = wxDefaultValidator,
               const wxString &name = wxT("listctrl") )
    : wxODGenericListCtrl(parent, id, pos, size, style, validator, name)
    {
    }
};
#endif // !__WXMSW__ || __WIN16__ || __WXUNIVERSAL__

#endif // LISTCTRL_GEN_H
