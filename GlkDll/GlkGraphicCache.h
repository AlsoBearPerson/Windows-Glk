/////////////////////////////////////////////////////////////////////////////
//
// Windows MFC Glk Libraries
//
// GlkGraphicCache
// LRU cache of previously loaded graphics
//
/////////////////////////////////////////////////////////////////////////////

#ifndef WINGLK_GRAPHIC_CACHE_H_
#define WINGLK_GRAPHIC_CACHE_H_

#include "GlkGraphic.h"

#include <list>
#include <map>

class CWinGlkGraphicCache
{
public:
  CWinGlkGraphicCache();
  ~CWinGlkGraphicCache();

  CWinGlkGraphic* GetGraphic(int iNumber, BOOL bLoad);
  void PutGraphic(int iNumber, CWinGlkGraphic* pGraphic);

private:
  class Entry
  {
  public:
    Entry(CWinGlkGraphic* pGraphic, int iCost);
    ~Entry();

    CWinGlkGraphic* Get(bool bLoad);

    const int m_iCost;
    // See the big comment below for what nonsense we're up to here.
    std::list<std::map<int, Entry*>::iterator>::iterator m_LruPos;

  private:
    BYTE *m_pPixels;
    BITMAPINFOHEADER *m_pHeader;
    // We only carry the fields that the JPEG/PNG decoders actually populate,
    // the rest of CWinGlkGraphic appears to be mainly used later inside
    // GlkWindowTextBuffer for its purposes.
    DWORD m_dwWidth;
    DWORD m_dwHeight;
    bool m_bAlpha;
  };

  int CostOf(CWinGlkGraphic* pGraphic);
  void MoveToFront(Entry* pEntry);
  void TrimBelow(int iMaxSize);

  // A maze of twisty little STL containers, what's it do?
  // We need the following to be fast:
  // * given a resource ID, get the cache entry, if it exists.
  // * remove the oldest cache entry, reading back its cost.
  // * update an entry to become the newest (most recently-used) entry.
  //
  // We use a map as the main ID->entry index,
  // and a std::list for age tracking, with the newest entry at the front of the list,
  // and the oldest entry at the back.
  // Each entry knows its relative age via the m_LruPos saved iterator,
  // and the age list points back at the entries via a map iterator.
  // Using list iterators in this way is essential for this LRU strategy to work,
  // so that entries can be moved without having to worry about their neighbors.
  // Using map iterators is a small optimization to save a map lookup on expiry.
  //
  // We could alternatively use a classic intrusive linked list through Entry,
  // but that's more code to write.
  //
  // Note that this REQUIRES both containers to provide full iterator stability,
  // i.e. iterators to an element remain valid on container mutation,
  // except for mutations involving the element itself.
  // Check your favorite STL doc for the word "invalidated" around iterators.
  //
  // If we were willing to require C++11, we could change the map to an
  // std::unordered_map for faster lookups, but note: That container can NOT
  // provide iterator stability, and thus the list would need to contain
  // keys. Still, it would likely be faster.
  std::map<int, Entry*> m_Cache;
  std::list<std::map<int, Entry*>::iterator> m_Lru;
  int m_iTotalCost;

  // Graphics with pixel buffers larger than this will not be stored
  static const int MAX_COST_TO_STORE = 4 * 1024 * 1024;
  // Once the cache grows to this size, old entries get evicted as new entries are added.
  // This only counts pixel data for now, the actually used RAM is a bit higher.
  static const int MAX_TOTAL_COST = 256 * 1024 * 1024;
};

#endif // WINGLK_GRAPHIC_CACHE_H_
