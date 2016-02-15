#ifndef KERNEL_BUILTINS_H
#define KERNEL_BUILTINS_H

#define CL_DEVICE_ADDRESS_BITS 64

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int  uint;
typedef unsigned long ulong;

#if CL_DEVICE_ADDRESS_BITS == 32
typedef uint size_t;
typedef int ptrdiff_t;
typedef int intptr_t;
typedef uint uintptr_t;

#else

typedef ulong size_t;
typedef long ptrdiff_t;
typedef long intptr_t;
typedef ulong uintptr_t;

#endif

typedef uint bitfield_t;
typedef bitfield_t cl_mem_fence_flags;

typedef enum {
  memory_scope_work_item,
  memory_scope_work_group,
  memory_scope_device,
  memory_scope_all_svm_devices
} memory_scope;

#define CLK_LOCAL_MEM_FENCE 1
#define CLK_GLOBAL_MEM_FENCE 2
#define CLK_IMAGE_MEM_FENCE 4

unsigned int get_work_dim ();
size_t get_global_size ( uint dimindx );
size_t get_global_id ( uint dimindx );
size_t get_local_size ( uint dimindx );
size_t get_enqueued_local_size ( uint dimindx );
size_t get_local_id (uint dimindx );
size_t get_num_groups ( uint dimindx );
size_t get_group_id ( uint dimindx );
size_t get_global_offset ( uint dimindx );

//void work_group_barrier( cl_mem_fence_flags flags );
void work_group_barrier( cl_mem_fence_flags flags, memory_scope scope );

#endif /* KERNEL_BUILTINS_H */
