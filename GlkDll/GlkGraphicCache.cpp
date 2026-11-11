/////////////////////////////////////////////////////////////////////////////
//
// Windows MFC Glk Libraries
//
// GlkGraphicCache
// LRU cache of previously loaded graphics
//
// This speeds up certain graphics-heavy games,
// if they spend much of their time drawing tilemaps and other structures
// where the same images are repeatedly drawn regularly:
// Normally GlkDll performs a fresh lookup and PNG/JPEG decode of these images
// on every draw call, with this cache both are skipped if the same image
// was recently loaded already.
//
// We still incur memory copies on load from cache, for simplicity.
// A future improvement could add cache pinning and provide for aliasing,
// where loaded graphics could reference the cached pixel buffer in place.
// However, as draws typically iterate the pixel buffer anyway,
// I'm not sure how much that would really buy.
//
/////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GlkGraphicCache.h"

#include <utility>

CWinGlkGraphicCache::CWinGlkGraphicCache()
: m_iTotalCost(0)
{
}

CWinGlkGraphicCache::~CWinGlkGraphicCache()
{
  for (std::map<int, Entry*>::iterator it = m_Cache.begin(); it != m_Cache.end(); ++it)
  {
    delete it->second;
  }
}

CWinGlkGraphic* CWinGlkGraphicCache::GetGraphic(int iNumber, BOOL bLoad)
{
  std::map<int, Entry*>::iterator it = m_Cache.find(iNumber);
  if (it == m_Cache.end())
    return NULL;

  Entry* found = it->second;
  MoveToFront(found);  // Bump LRU: We want most recent use, not just insert.
  return found->Get(bLoad);
}

void CWinGlkGraphicCache::PutGraphic(int iNumber, CWinGlkGraphic* pGraphic)
{
  if (!pGraphic->m_pPixels || !pGraphic->m_pHeader)
    return;  // Programming error: Can only store fully-loaded images.
  if (m_Cache.find(iNumber) != m_Cache.end())
    return;  // Image was already in cache, so why did you go and load it by hand?

  int cost = CostOf(pGraphic);
  if (cost > MAX_COST_TO_STORE)
    return;  // Image is too large to be worth storing.

  // We're committed to inserting this entry. Make room, if needed.
  TrimBelow(MAX_TOTAL_COST - cost);

  Entry* pEntry = new Entry(pGraphic, cost);
  // Add it to the main cache structure.
  std::map<int, Entry*>::iterator it = m_Cache.insert(std::make_pair(iNumber, pEntry)).first;
  // Link it into the LRU: It's the newest entry.
  m_Lru.push_front(it);
  // Update the cross-reference: We know we just put it at the front.
  pEntry->m_LruPos = m_Lru.begin();

  m_iTotalCost += cost;
}

int CWinGlkGraphicCache::CostOf(CWinGlkGraphic* pGraphic)
{
  // For now, we only count the pixel buffers and ignore our struct/stl overhead.
  return pGraphic->m_dwHeight * pGraphic->m_dwWidth * 4;
}

void CWinGlkGraphicCache::MoveToFront(Entry* pEntry)
{
  if (pEntry->m_LruPos == m_Lru.begin())
    return;  // Already at front, nothing to do.

  // Take the entry (which holds an iterator into the map)
  // out of where it currently is in the LRU list,
  // and put it back on the front.
  std::map<int, Entry*>::iterator it = *pEntry->m_LruPos;
  m_Lru.erase(pEntry->m_LruPos);
  m_Lru.push_front(it);
  pEntry->m_LruPos = m_Lru.begin();
}

void CWinGlkGraphicCache::TrimBelow(int iMaxSize)
{
  while (m_iTotalCost > iMaxSize && !m_Lru.empty())
  {
    std::map<int, Entry*>::iterator it = m_Lru.back();
    m_iTotalCost -= it->second->m_iCost;
    ASSERT(it->second->m_LruPos == m_Lru.rbegin());
    delete it->second;
    m_Cache.erase(it);
    m_Lru.pop_back();
  }
}

CWinGlkGraphicCache::Entry::Entry(CWinGlkGraphic* pGraphic, int iCost)
: m_iCost(iCost),
  m_dwWidth(pGraphic->m_dwWidth), m_dwHeight(pGraphic->m_dwHeight), m_bAlpha(pGraphic->m_bAlpha)
{
  m_pHeader = new BITMAPINFOHEADER;
  CopyMemory(m_pHeader, pGraphic->m_pHeader, sizeof(BITMAPINFOHEADER));
  size_t pixels = m_dwHeight * m_dwWidth * 4;
  m_pPixels = new BYTE[pixels];
  CopyMemory(m_pPixels, pGraphic->m_pPixels, pixels);
}

CWinGlkGraphicCache::Entry::~Entry()
{
  delete m_pHeader;
  delete[] m_pPixels;
}

CWinGlkGraphic* CWinGlkGraphicCache::Entry::Get(bool bLoad)
{
  CWinGlkGraphic* pGraphic = new CWinGlkGraphic();
  pGraphic->m_dwWidth = m_dwWidth;
  pGraphic->m_dwHeight = m_dwHeight;
  pGraphic->m_bAlpha = m_bAlpha;
  if (bLoad)
  {
    pGraphic->m_pHeader = new BITMAPINFOHEADER;
    CopyMemory(pGraphic->m_pHeader, m_pHeader, sizeof(BITMAPINFOHEADER));
    size_t pixels = m_dwHeight * m_dwWidth * 4;
    pGraphic->m_pPixels = new BYTE[pixels];
    CopyMemory(pGraphic->m_pPixels, m_pPixels, pixels);
  }
  return pGraphic;
}

