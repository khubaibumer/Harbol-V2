#include "objpool.h"

#ifdef OS_WINDOWS
#	define HARBOL_LIB
#endif

typedef union {
	uint8_t *const byte;
	size_t *const next_index;
} ObjInfo_t;


HARBOL_EXPORT struct HarbolObjPool harbol_objpool_create(const size_t objsize, const size_t len)
{
	struct HarbolObjPool objpool = {NULL,NULL,0,0,0};
	if( len==0UL || objsize==0UL )
		return objpool;
	else {
		objpool.ObjSize = harbol_align_size(objsize, sizeof(size_t));
		objpool.Size = objpool.FreeBlocks = len;
		objpool.Mem = calloc(objpool.Size, objpool.ObjSize);
		if( objpool.Mem==NULL ) {
			objpool.Size = 0UL;
			return objpool;
		} else {
			for( uindex_t i=0; i<objpool.FreeBlocks; i++ ) {
				ObjInfo_t block = { .byte = &objpool.Mem[i * objpool.ObjSize] };
				*block.next_index = i + 1;
			}
			objpool.Next = objpool.Mem;
			return objpool;
		}
	}
}

HARBOL_EXPORT struct HarbolObjPool harbol_objpool_from_buffer(void *const buf, const size_t objsize, const size_t len)
{
	struct HarbolObjPool objpool = {NULL,NULL,0,0,0};
	
	// If the object next_index isn't large enough to align to a size_t, then we can't use it.
	if( len==0UL || objsize<sizeof(size_t) || objsize * len != harbol_align_size(objsize, sizeof(size_t)) * len )
		return objpool;
	else {
		objpool.ObjSize = harbol_align_size(objsize, sizeof(size_t));
		objpool.Size = objpool.FreeBlocks = len;
		objpool.Mem = buf;
		for( uindex_t i=0; i<objpool.FreeBlocks; i++ ) {
			ObjInfo_t block = { .byte = &objpool.Mem[i * objpool.ObjSize] };
			*block.next_index = i + 1;
		}
		objpool.Next = objpool.Mem;
		return objpool;
	}
}

HARBOL_EXPORT bool harbol_objpool_clear(struct HarbolObjPool *const objpool)
{
	if( objpool->Mem==NULL )
		return false;
	else {
		free(objpool->Mem);
		*objpool = (struct HarbolObjPool){NULL,NULL,0,0,0};
		return true;
	}
}

HARBOL_EXPORT void *harbol_objpool_alloc(struct HarbolObjPool *const objpool)
{
	if( objpool->FreeBlocks>0UL ) {
		// for first allocation, head points to the very first index.
		// Next = &pool[0];
		// ret = Next == ret = &pool[0];
		ObjInfo_t ret = { .byte = objpool->Next };
		objpool->FreeBlocks--;
		
		// after allocating, we set head to the address of the index that *Next holds.
		// Next = &pool[*Next * pool.objsize];
		objpool->Next = ( objpool->FreeBlocks != 0UL ) ? objpool->Mem + (*ret.next_index * objpool->ObjSize) : NULL;
		memset(ret.byte, 0, objpool->ObjSize);
		return ret.byte;
	}
	else return NULL;
}

HARBOL_EXPORT bool harbol_objpool_free(struct HarbolObjPool *const restrict objpool, void *ptr)
{
	ObjInfo_t p = { .byte = ptr };
	if( ptr==NULL || p.byte < objpool->Mem || p.byte > objpool->Mem + objpool->Size * objpool->ObjSize )
		return false;
	else {
		// when we free our pointer, we recycle the pointer space to store the previous index
		// and then we push it as our new head.
		
		// *p = index of Next in relation to the buffer;
		// Next = p;
		*p.next_index = ( objpool->Next != NULL ) ? (objpool->Next - objpool->Mem) / objpool->ObjSize : objpool->Size;
		objpool->Next = p.byte;
		++objpool->FreeBlocks;
		return true;
	}
}

HARBOL_EXPORT bool harbol_objpool_cleanup(struct HarbolObjPool *const restrict objpool, void **const restrict ptrref)
{
	if( *ptrref==NULL )
		return false;
	else {
		const bool free_result = harbol_objpool_free(objpool, *ptrref);
		*ptrref = NULL;
		return free_result;
	}
}