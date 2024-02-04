// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#include<stdbool.h>

#define NBUCKETS 13

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  struct buf buckets[NBUCKETS];
  struct spinlock buckets_lock[NBUCKETS];
} bcache;

int get_key(uint blockno) {
  return blockno % NBUCKETS;
}

void write_buffer(struct buf *buf, uint blockno, uint dev) {
  buf->blockno = blockno;
  buf->dev = dev;
  buf->valid = 0;
  buf->refcnt = 1;
  buf->time_stamp = ticks;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i = 0 ; i < NBUCKETS ; i++) {
    initlock(&bcache.buckets_lock[i],"bcache.hash");
  }
  bcache.buckets[0].next = &bcache.buf[0];

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF - 1; b++){
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    b->next = b + 1;
    initsleeplock(&b->lock, "buffer");
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
  }
  initsleeplock(&b->lock, "buffer");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  int id = get_key(blockno);
  struct buf *b;
  struct buf *lst = &bcache.buckets[id];
  struct buf *take_buf = 0, *lst_buf = 0;
  uint time = __UINT32_MAX__;
  // acquire(&bcache.lock);

  // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  
  bool has_buffer = false;
  acquire(&bcache.buckets_lock[id]);
  for( b = bcache.buckets[id].next; b ; b = b->next, lst = lst->next) {
    if(b->blockno == blockno && b->dev == dev) {
      b->refcnt++;
      b->time_stamp = ticks;
      release(&bcache.buckets_lock[id]);
      // release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
    if(b->refcnt == 0) {
      has_buffer = true;
      take_buf = b;
    }
  }
  if(has_buffer) {
    write_buffer(take_buf, blockno, dev);
    release(&bcache.buckets_lock[id]);
    // release(&bcache.lock);
    acquiresleep(&take_buf->lock);
    return take_buf;
  }

  int num = -1; // 记录有空Buffer的桶序号

  for(int i = 0 ; i < NBUCKETS ; i++) {
    if(i == id)continue;
    acquire(&bcache.buckets_lock[i]);
    lst = &bcache.buckets[i];
    for(b = bcache.buckets[i].next ; b ; b = b->next, lst = lst->next) {
      // 找到可以替换的buffer后，将其，移动到相应的桶
      if(b->refcnt == 0 && b->time_stamp < time) {
        time = b->time_stamp;
        take_buf = b;
        lst_buf = lst;
        has_buffer = true;
        num = i;
      }
    }
    release(&bcache.buckets_lock[i]);
  }

  if(has_buffer == false) panic("bio.c assert fail");

  acquire(&bcache.buckets_lock[num]);
  write_buffer(take_buf,blockno,dev);
  lst_buf->next = take_buf->next;
  take_buf->next = bcache.buckets[id].next;
  bcache.buckets[id].next = take_buf;
  release(&bcache.buckets_lock[num]);
  release(&bcache.buckets_lock[id]);
  // release(&bcache.lock);
  acquiresleep(&take_buf->lock);
  return take_buf;

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  // acquire(&bcache.lock);
  acquire(&bcache.buckets_lock[get_key(b->blockno)]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    
  }
  release(&bcache.buckets_lock[get_key(b->blockno)]);
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  int id = get_key(b->blockno);
  // acquire(&bcache.lock);
  // b->refcnt++;
  // release(&bcache.lock);
  acquire(&bcache.buckets_lock[id]);
  b->refcnt++;
  release(&bcache.buckets_lock[id]);
  
}

void
bunpin(struct buf *b) {
  int id = get_key(b->blockno);
  // acquire(&bcache.lock);
  // b->refcnt--;
  // release(&bcache.lock);
  acquire(&bcache.buckets_lock[id]);
  b->refcnt--;
  release(&bcache.buckets_lock[id]);

}


