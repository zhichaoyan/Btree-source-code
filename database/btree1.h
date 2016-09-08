#pragma once

#define Btree_maxkey		4096				// maximum key size in bytes
#define Btree_maxbits		29					// maximum page size in bits
#define Btree_minbits		9					// minimum page size in bits
#define Btree_minpage		(1 << Btree_minbits)	// minimum page size
#define Btree_maxpage		(1 << Btree_maxbits)	// maximum page size

//	There are six lock types for each node in four independent sets: 
//	1. (set 1) AccessIntent: Sharable. Going to Read the node. Incompatible with NodeDelete. 
//	2. (set 1) NodeDelete: Exclusive. About to release the node. Incompatible with AccessIntent. 
//	3. (set 2) ReadLock: Sharable. Read the node. Incompatible with WriteLock. 
//	4. (set 2) WriteLock: Exclusive. Modify the node. Incompatible with ReadLock and other WriteLocks. 
//	5. (set 3) ParentModification: Exclusive. Change the node's parent keys. Incompatible with another ParentModification. 
//	6. (set 4) LinkModification: Exclusive. Update of a node's left link is underway. Incompatible with another LinkModification. 

typedef enum {
	Btree_lockAccess = 1,
	Btree_lockDelete = 2,
	Btree_lockRead   = 4,
	Btree_lockWrite  = 8,
	Btree_lockParent = 16,
	Btree_lockLink   = 32
} BtreeLock;

//	types of btree pages/allocations

typedef enum{
	Btree_rootPage,
	Btree_interior,
	Btree_leafPage,
	Btree_maxType
} BtreePageType;

//	BtreeIndex global data on disk

typedef struct {
	uint32_t pageSize;
	uint32_t pageBits;
	uint32_t leafXtra;
	uint64_t numEntries[1];	// number of keys in btree
	DbAddr root;
	DbAddr leaf;
} BtreeIndex;

//	Btree page layout

//	This structure is immediately
//	followed by the key slots

typedef struct {
	RWLock3 readwr[1];	// read/write access lock
	RWLock3 access[1];	// waiting for delete lock
	RWLock3 parent[1];	// posting of fence key
	RWLock3 link[1];		// left link update
} LatchSet;

typedef struct {
	LatchSet latch[1];	// page latches
	uint32_t cnt;		// count of keys in page
	uint32_t act;		// count of active keys
	uint32_t min;		// next page key offset
	uint32_t garbage;	// page garbage in bytes
	uint8_t lvl:6;		// level of page
	uint8_t free:1;		// page is on free chain
	uint8_t kill:1;		// page is being deleted
	DbAddr right;		// page to right
	DbAddr left;		// page to left
} BtreePage;

typedef struct {
	DbAddr pageNo;		// current page address
	BtreePage *page;	// selected page
	uint32_t slotIdx;	// slot on page
} BtreeSet;

//	Page key slot definition.

//	Keys are marked dead, but remain on the page until
//	it cleanup is called.

//	Slot types

//	In addition to the Unique keys that occupy slots
//	there are Librarian slots in the key slot array.

//	The Librarian slots are dead keys that
//	serve as filler, available to add new keys.

typedef enum {
	Btree_indexed,		// key was indexed
	Btree_deleted,		// key was deleted
	Btree_librarian,	// librarian slot
	Btree_stopper		// stopper slot
} BtreeSlotType;

typedef union {
	struct {
		uint32_t off:Btree_maxbits;	// page offset for key start
		uint32_t type:2;			// type of key slot
		uint32_t dead:1;			// dead/librarian slot
	};
	uint32_t bits;
} BtreeSlot;

typedef struct {
	Handle *hndl;		// index handle
	ObjId objId;		// current cursor object ID
	BtreePage *page;	// current cursor page
	uint32_t slotIdx;	// current cache index
} BtreeCursor;

#define btreeIndex(index) ((BtreeIndex *)(index->arena + 1))

BtreeCursor *btreeCursor(Handle *hndl);
uint8_t *btreeCursorKey(BtreeCursor *cursor, uint32_t *len);

uint64_t btreeNewPage (Handle *hndl, uint8_t lvl);
DbAddr *btreeFindKey(DbMap  *map, BtreeCursor *cursor, uint8_t *key, uint32_t keylen);
bool btreeSeekKey (BtreeCursor *cursor, uint8_t *key, uint32_t keylen);
uint64_t btreeNextKey (BtreeCursor *cursor);
uint64_t btreePrevKey (BtreeCursor *cursor);
uint64_t btreeObjId(BtreeCursor *cursor);

#define slotptr(page, slot) (((BtreeSlot *)(page+1)) + (((int)slot)-1))

#define keyaddr(page, off) ((uint8_t *)((unsigned char*)(page) + off))
#define keyptr(page, slot) ((uint8_t *)((unsigned char*)(page) + slotptr(page, slot)->off))
#define keylen(key) ((key[0] & 0x80) ? ((key[0] & 0x7f) << 8 | key[1]) : key[0])
#define keystr(key) ((key[0] & 0x80) ? (key + 2) : (key + 1))
#define keypre(key) ((key[0] & 0x80) ? 2 : 1)

Status btreeInit(Handle *hndl);
Status btreeInsertKey(Handle *hndl, uint8_t *key, uint32_t keyLen, uint8_t lvl, BtreeSlotType type);

Status btreeLoadPage(Handle *hndl, BtreeSet *set, uint8_t *key, uint32_t keyLen, uint8_t lvl, BtreeLock lock, bool stopper);
Status btreeCleanPage(Handle *hndl, BtreeSet *set, uint32_t totKeyLen);
Status btreeSplitPage (Handle *hndl, BtreeSet *set);
Status btreeFixKey (Handle *hndl, uint8_t *fenceKey, uint8_t lvl, bool stopper);

void btreeLockPage(BtreePage *page, BtreeLock mode);
void btreeUnlockPage(BtreePage *page, BtreeLock mode);