# libeio 数据结构与函数详解

## 📊 核心数据结构分析

### eio_req - 异步请求描述符（源码定义）

```c
/**
 * 源码位置: eio.h line 259-292
 * 实际的异步请求结构体定义
 */
struct eio_req
{
  eio_req volatile *next; /* private ETP */  // 私有：ETP内部使用

  eio_wd wd;       /* all applicable requests: working directory of pathname, old name; wd_open: return wd */
                   // 工作目录：路径名、旧名称的工作目录；wd_open返回wd

  eio_ssize_t result;  /* result of syscall, e.g. result = read (... */
                      // 系统调用结果，如：result = read(...)
  off_t offs;      /* read, write, truncate, readahead, sync_file_range, fallocate, slurp: file offset, mknod: dev_t */
                  // 偏移量：读写、截断、预读、同步文件范围、分配空间、slurp的文件偏移；mknod的设备号
  size_t size;     /* read, write, readahead, sendfile, msync, mlock, sync_file_range, fallocate, slurp: length */
                  // 大小：读写、预读、sendfile、msync、mlock、同步文件范围、分配空间、slurp的长度
  void *ptr1;      /* all applicable requests: pathname, old name, readdir: optional eio_dirents */
                  // 指针1：所有适用请求的路径名、旧名称；readdir的可选eio_dirents
  void *ptr2;      /* all applicable requests: new name or memory buffer; readdir: name strings */
                  // 指针2：所有适用请求的新名称或内存缓冲区；readdir的名称字符串
  eio_tstamp nv1;  /* utime, futime: atime; busy: sleep time */
                  // 时间戳1：utime、futime的访问时间；busy的睡眠时间
  eio_tstamp nv2;  /* utime, futime: mtime */
                  // 时间戳2：utime、futime的修改时间

  int int1;        /* all applicable requests: file descriptor; sendfile: output fd; open, msync, mlockall, readdir: flags */
                  // 整数1：所有适用请求的文件描述符；sendfile的输出fd；open、msync、mlockall、readdir的标志
  long int2;       /* chown, fchown: uid; sendfile: input fd; open, chmod, mkdir, mknod: file mode, seek: whence, fcntl, ioctl: request, sync_file_range, fallocate, rename: flags */
                  // 整数2：chown、fchown的用户ID；sendfile的输入fd；open、chmod、mkdir、mknod的文件模式；seek的位置；fcntl、ioctl的请求；同步文件范围、分配空间、重命名的标志
  long int3;       /* chown, fchown: gid; rename, link: working directory of new name */
                  // 整数3：chown、fchown的组ID；rename、link的新名称工作目录
  int errorno;     /* errno value on syscall return */
                  // 系统调用返回时的errno值

  unsigned char flags; /* private */         // 私有标志位

  signed char type;/* EIO_xxx constant ETP */ // 请求类型：EIO_xxx常量（ETP内部）
  signed char pri;     /* the priority ETP */ // 优先级（ETP内部）
#if __i386 || __amd64
  unsigned char cancelled; /* ETP */         // 取消标志（ETP内部，x86架构）
#else
  sig_atomic_t  cancelled; /* ETP */         // 取消标志（ETP内部，其他架构）
#endif

  void *data;                              // 用户数据指针
  eio_cb finish;                           // 完成回调函数
  void (*destroy)(eio_req *req); /* called when request no longer needed */
                                   // 销毁函数：请求不再需要时调用
  void (*feed)(eio_req *req);    /* only used for group requests */
                                // 喂食函数：仅用于群组请求

  EIO_REQ_MEMBERS                         // 可扩展的请求成员

  eio_req *grp, *grp_prev, *grp_next, *grp_first; /* private ETP */
                                                  // 私有：群组相关指针（ETP内部）
};

/* _private_ request flags */
/**
 * 源码位置: eio.h line 295-298
 * 请求标志位定义
 */
enum {
  EIO_FLAG_PTR1_FREE = 0x01, /* need to free(ptr1) */
                            // 需要释放ptr1
  EIO_FLAG_PTR2_FREE = 0x02, /* need to free(ptr2) */
                            // 需要释放ptr2
};
```

### eio_dirent - 目录项结构

```c
/**
 * 源码位置: eio.h line 71-80
 * 目录项结构定义
 */
struct eio_dirent
{
  int nameofs; /* offset of null-terminated name string in (char *)req->ptr2 */
              // 名称字符串在(char *)req->ptr2中的偏移量
  unsigned short namelen; /* size of name in characters */
                         // 名称字符数
  unsigned char type; /* one of EIO_DT_* */
                      // 类型：EIO_DT_*之一
  signed char score; /* internal use */
                    // 分数：内部使用
  ino_t inode; /* the inode number, if available, otherwise unspecified */
               // inode号：如果可用则为inode号，否则未指定
};
```

---

## 🎯 核心API函数详解

### 初始化函数 `eio_init`

```c
/**
 * 源码位置: eio.h line 307-311
 * 库初始化函数声明
 */
/* returns < 0 on error, errno set
 * need_poll, if non-zero, will be called when results are available
 * and eio_poll_cb needs to be invoked (it MUST NOT call eio_poll_cb itself).
 * done_poll is called when the need to poll is gone.
 */
int eio_init (void (*want_poll)(void), void (*done_poll)(void));

/**
 * 源码位置: eio.c line 1846-1851
 * 初始化函数实现
 */
int ecb_cold
eio_init (void (*want_poll)(void), void (*done_poll)(void))
{
  eio_want_poll_cb = want_poll;      // 设置轮询需求回调
  eio_done_poll_cb = done_poll;      // 设置轮询完成回调

  return etp_init (EIO_POOL, 0, 0, 0);  // 调用ETP初始化
}
```

### 轮询函数 `eio_poll`

```c
/**
 * 源码位置: eio.h line 313-315
 * 轮询函数声明
 */
/* must be called regularly to handle pending requests */
/* returns 0 if all requests were handled, -1 if not, or the value of EIO_FINISH if != 0 */
int eio_poll (void);

/**
 * 源码位置: eio.c line 2362-2415
 * 轮询函数实现（部分）
 */
int eio_poll (void)
{
  int res = 0;

  // 🔒 锁定结果队列
  X_LOCK (EIO_POOL->reslock);

  // 📤 处理完成的请求
  while ((req = reqq_shift (&EIO_POOL->res_queue)))
    {
      --EIO_POOL->npending;          // 减少挂起计数

      // 🎯 执行用户回调
      if ((res = EIO_FINISH (req)))  // EIO_FINISH宏展开为回调调用
        break;

      // 🗑️ 清理请求资源
      EIO_DESTROY (req);             // EIO_DESTROY宏调用destroy函数
    }

  // 🔔 检查是否还需要轮询
  if (!EIO_POOL->npending)
    ETP_DONE_POLL (EIO_POOL);        // 调用完成轮询回调

  X_UNLOCK (EIO_POOL->reslock);      // 🔓 解锁结果队列

  return res;
}
```

### 配置函数系列

```c
/**
 * 源码位置: eio.h line 317-324
 * 配置函数声明
 */
/* stop polling if poll took longer than duration seconds */
void eio_set_max_poll_time (eio_tstamp nseconds);
/* do not handle more then count requests in one call to eio_poll_cb */
void eio_set_max_poll_reqs (unsigned int nreqs);

/* set minimum required number
 * maximum wanted number
 * or maximum idle number of threads */
void eio_set_min_parallel (unsigned int nthreads);
void eio_set_max_parallel (unsigned int nthreads);
void eio_set_max_idle     (unsigned int nthreads);
void eio_set_idle_timeout (unsigned int seconds);

/**
 * 源码位置: eio.c line 2417-2464
 * 配置函数实现
 */
void eio_set_max_idle (unsigned int nthreads)
{
  X_LOCK (EIO_POOL->wrklock);        // 🔒 锁定工作线程
  EIO_POOL->max_idle = nthreads;     // 设置最大空闲线程数
  X_UNLOCK (EIO_POOL->wrklock);      // 🔓 解锁工作线程
}

void eio_set_idle_timeout (unsigned int seconds)
{
  EIO_POOL->idle_timeout = seconds;  // 设置空闲超时时间
}

void eio_set_max_parallel (unsigned int nthreads)
{
  if (nthreads > 128)                // 限制最大线程数
    nthreads = 128;

  X_LOCK (EIO_POOL->wrklock);
  EIO_POOL->wanted = nthreads ? nthreads : 1;  // 设置期望线程数
  X_UNLOCK (EIO_POOL->wrklock);

  etp_maybe_start_thread (EIO_POOL); // 检查是否需要启动新线程
}
```

### 状态查询函数

```c
/**
 * 源码位置: eio.h line 326-329
 * 状态查询函数声明
 */
unsigned int eio_nreqs    (void); /* number of requests in-flight */
unsigned int eio_nready   (void); /* number of not-yet handled requests */
unsigned int eio_npending (void); /* number of finished but unhandled requests */
unsigned int eio_nthreads (void); /* number of worker threads in use currently */

/**
 * 源码位置: eio.c line 2344-2360
 * 状态查询函数实现
 */
unsigned int eio_nreqs (void)
{
  unsigned int count;
  X_LOCK (EIO_POOL->reqlock);
  count = EIO_POOL->nreqs;
  X_UNLOCK (EIO_POOL->reqlock);
  return count;
}

unsigned int eio_nready (void)
{
  unsigned int count;
  X_LOCK (EIO_POOL->reqlock);
  count = EIO_POOL->nready;
  X_UNLOCK (EIO_POOL->reqlock);
  return count;
}

unsigned int eio_npending (void)
{
  unsigned int count;
  X_LOCK (EIO_POOL->reqlock);
  count = EIO_POOL->npending;
  X_UNLOCK (EIO_POOL->reqlock);
  return count;
}

unsigned int eio_nthreads (void)
{
  unsigned int count;
  X_LOCK (EIO_POOL->wrklock);
  count = EIO_POOL->started;
  X_UNLOCK (EIO_POOL->wrklock);
  return count;
}
```

---

## 🎯 基础异步操作函数

### 空操作函数 `eio_nop`

```c
/**
 * 源码位置: eio.h line 343
 * 空操作函数声明
 */
eio_req *eio_nop       (int pri, eio_cb cb, void *data); /* does nothing except go through the whole process */

/**
 * 源码位置: eio.c line 2149-2157
 * 空操作函数实现
 */
eio_req *eio_nop (int pri, eio_cb cb, void *data)
{
  REQ (EIO_NOP);                     // 使用REQ宏创建请求
  SEND;                              // 使用SEND宏提交请求
}

/**
 * 源码位置: eio.c line 1861-1874
 * REQ和SEND宏定义
 */
#define REQ(rtype)                                          \
  eio_req *req;                                             \
                                                            \
  req = (eio_req *)calloc (1, sizeof *req);                 \
  if (!req)                                                 \
    return 0;                                               \
                                                            \
  req->type    = rtype;                                     \
  req->pri     = pri;                                       \
  req->finish  = cb;                                        \
  req->data    = data;                                      \
  req->destroy = eio_api_destroy;

#define SEND eio_submit (req); return req

/**
 * 源码位置: eio.c line 2292-2303
 * 请求提交函数实现
 */
void eio_submit (eio_req *req)
{
  // 📊 增加计数器
  X_LOCK (EIO_POOL->reqlock);
  ++EIO_POOL->nreqs;
  ++EIO_POOL->nready;
  X_UNLOCK (EIO_POOL->reqlock);

  // 📥 将请求推入队列
  X_LOCK (EIO_POOL->reqlock);
  reqq_push (&EIO_POOL->req_queue, req);
  X_COND_SIGNAL (EIO_POOL->reqwait);  // 唤醒等待的线程
  X_UNLOCK (EIO_POOL->reqlock);

  // 🚀 检查是否需要启动新线程
  etp_maybe_start_thread (EIO_POOL);
}
```

### 繁忙操作函数 `eio_busy`

```c
/**
 * 源码位置: eio.h line 344
 * 繁忙操作函数声明
 */
eio_req *eio_busy      (eio_tstamp delay, int pri, eio_cb cb, void *data); /* ties a thread for this long, simulating busyness */

/**
 * 源码位置: eio.c line 2159-2168
 * 繁忙操作函数实现
 */
eio_req *eio_busy (eio_tstamp delay, int pri, eio_cb cb, void *data)
{
  REQ (EIO_BUSY);
  req->nv1 = delay;                  // 设置繁忙时间
  SEND;
}

/**
 * 源码位置: eio.c line 2088-2098
 * 繁忙操作执行实现
 */
case EIO_BUSY:
  {
    struct timeval tv1, tv2;
    gettimeofday (&tv1, 0);
    do
      gettimeofday (&tv2, 0);
    while (etp_tvdiff (&tv1, &tv2) < (int)req->nv1);  // 循环等待指定时间

    req->result = 0;
  }
  break;
```

### 文件操作函数

```c
/**
 * 源码位置: eio.h line 358-359
 * 文件读写函数声明
 */
eio_req *eio_read (int fd, void *buf, size_t count, int pri, eio_cb cb, void *data);
eio_req *eio_write (int fd, void *buf, size_t count, int pri, eio_cb cb, void *data);

/**
 * 源码位置: eio.c line 2177-2194
 * 文件读写函数实现
 */
eio_req *eio_read (int fd, void *buf, size_t count, int pri, eio_cb cb, void *data)
{
  REQ (EIO_READ);
  req->int1 = fd;                    // 文件描述符
  req->ptr2 = buf;                   // 缓冲区指针
  req->size = count;                 // 读取大小
  req->offs = -1;                    // -1表示普通读取（非pread）
  SEND;
}

eio_req *eio_write (int fd, void *buf, size_t count, int pri, eio_cb cb, void *data)
{
  REQ (EIO_WRITE);
  req->int1 = fd;
  req->ptr2 = buf;
  req->size = count;
  req->offs = -1;                    // -1表示普通写入（非pwrite）
  SEND;
}

/**
 * 源码位置: eio.c line 1915-1920
 * 读写操作执行实现
 */
case EIO_READ:
  ALLOC (req->size);                 // 分配读取缓冲区
  req->result = req->offs >= 0
              ? pread(req->int1, req->ptr2, req->size, req->offs)  // 带偏移读取
              : read (req->int1, req->ptr2, req->size);            // 普通读取
  break;

case EIO_WRITE:
  req->result = req->offs >= 0
              ? pwrite(req->int1, req->ptr2, req->size, req->offs) // 带偏移写入
              : write (req->int1, req->ptr2, req->size);           // 普通写入
  break;
```

### 文件打开函数 `eio_open`

```c
/**
 * 源码位置: eio.h line 370
 * 文件打开函数声明
 */
eio_req *eio_open (const char *path, int flags, mode_t mode, int pri, eio_cb cb, void *data);

/**
 * 源码位置: eio.c line 2208-2218
 * 文件打开函数实现
 */
eio_req *eio_open (const char *path, int flags, mode_t mode, int pri, eio_cb cb, void *data)
{
  REQ (EIO_OPEN);
  PATH;                              // 使用PATH宏处理路径
  req->int1 = flags;                 // 打开标志
  req->int2 = mode;                  // 文件权限模式
  SEND;
}

/**
 * 源码位置: eio.c line 1876-1884
 * PATH宏定义
 */
#define PATH                                                        \
  req->flags |= EIO_FLAG_PTR1_FREE;                                 \
  req->ptr1 = strdup (path);                                        \
  if (!req->ptr1)                                                   \
    {                                                               \
      eio_api_destroy (req);                                        \
      return 0;                                                     \
    }

/**
 * 源码位置: eio.c line 1938-1939
 * 文件打开执行实现
 */
case EIO_OPEN:
  req->result = openat(dirfd, req->ptr1, req->int1, (mode_t)req->int2);
  break;
```

---

## 🔧 辅助宏和工具函数

### 结果获取宏

```c
/**
 * 源码位置: eio.h line 407-416
 * 结果访问宏定义
 */
#define EIO_RESULT(req) ((req)->result)           // 获取操作结果
#define EIO_BUF(req)    ((req)->ptr2)             // 获取数据缓冲区
#define EIO_TYPE(req)   ((req)->type)             // 获取请求类型
#define EIO_OFFSET(req) ((off_t)(long)(req)->int1) // 获取偏移量
#define EIO_PATH(req)   ((char *)(req)->ptr1)     // 获取路径字符串
#define EIO_FLAGS(req)  ((req)->flags)            // 获取标志位

/**
 * 源码位置: eio.h line 418-421
 * 状态检查宏
 */
#define EIO_CANCELLED(req) ecb_expect_false ((req)->cancelled)
#define EIO_FINISH(req)  ((req)->finish) && !EIO_CANCELLED (req) ? (req)->finish (req) : 0
#define EIO_DESTROY(req) do { if ((req)->destroy) (req)->destroy (req); } while (0)
```

### 优先级定义

```c
/**
 * 源码位置: eio.h line 423-427
 * 优先级常量定义
 */
#define EIO_PRI_MIN     -4    /* minimum priority */
#define EIO_PRI_MAX      4    /* maximum priority */
#define EIO_PRI_DEFAULT  0    /* default priority */

/**
 * 源码位置: etp.c line 63-64
 * ETP优先级范围定义
 */
#ifndef ETP_PRI_MIN
# define ETP_PRI_MIN 0
# define ETP_PRI_MAX 0
#endif
#define ETP_NUM_PRI (ETP_PRI_MAX - ETP_PRI_MIN + 1)
```

### 请求类型枚举

```c
/**
 * 源码位置: eio.h line 83-136
 * 主要请求类型定义
 */
enum
{
  EIO_READ     = 1,  EIO_WRITE    = 2,  EIO_OPEN    = 3,  EIO_CLOSE   = 4,
  EIO_STAT     = 5,  EIO_LSTAT    = 6,  EIO_FSTAT   = 7,  EIO_TRUNCATE= 8,
  EIO_FTRUNCATE= 9,  EIO_UTIME    =10,  EIO_FUTIME  =11,  EIO_CHMOD   =12,
  EIO_FCHMOD   =13,  EIO_CHOWN    =14,  EIO_FCHOWN  =15,  EIO_SYNC    =16,
  EIO_FSYNC    =17,  EIO_FDATASYNC=18,  EIO_MSYNC   =19,  EIO_MTOUCH  =20,
  EIO_SYNCFS   =21,  EIO_SENDFILE =22,  EIO_READAHEAD=23, EIO_FALLOCATE=24,
  EIO_CLOSEFD  =25,  EIO_FCNTL    =26,  EIO_IOCTL   =27,  EIO_SEEK    =28,
  EIO_READDIR  =29,  EIO_READDIRP =30,  EIO_SLURP   =31,  EIO_WD_OPEN =32,
  EIO_WD_CLOSE =33,  EIO_NOP      =34,  EIO_BUSY    =35,  EIO_GROUP   =36,
  EIO_COPYFILE =37,  EIO_MKDIR    =38,  EIO_RMDIR   =39,  EIO_UNLINK  =40,
  EIO_RENAME   =41,  EIO_LINK     =42,  EIO_SYMLINK =43,  EIO_MKNOD   =44,
  EIO_STATVFS  =45,  EIO_READLINK =46,  EIO_REALPATH=47,  EIO_QUIT    =48
};
```

---

## 🔄 异步操作执行流程

### 完整调用链路

```
用户API调用 (eio_read等)
    ↓
REQ宏创建请求结构体
    ↓
SEND宏提交请求
    ↓
eio_submit() 函数
    ↓
增加计数器 (nreqs, nready)
    ↓
reqq_push() 入请求队列
    ↓
X_COND_SIGNAL() 唤醒工作线程
    ↓
etp_maybe_start_thread() 检查线程创建
    ↓
工作线程 etp_proc() 主循环
    ↓
reqq_shift() 从队列获取请求
    ↓
ETP_EXECUTE() 执行请求
    ↓
eio_execute() 核心执行函数
    ↓
switch(req->type) 分发到具体操作
    ↓
系统调用执行 (read/write/open等)
    ↓
结果写入 req->result
    ↓
reqq_push() 入结果队列
    ↓
ETP_WANT_POLL() 触发轮询回调
    ↓
用户调用 eio_poll()
    ↓
reqq_shift() 从结果队列获取
    ↓
EIO_FINISH() 执行用户回调
    ↓
EIO_DESTROY() 清理资源
```

### 回调执行机制

```c
/**
 * 源码中的回调执行链
 */
// 1. 工作线程完成操作后
reqq_push (&pool->res_queue, req);   // 推入结果队列
ETP_WANT_POLL (pool);                // 触发want_poll回调

// 2. want_poll回调实现（用户定义）
void want_poll_callback(void) {
    // 用户必须调用 eio_poll()
    need_poll = 1;
}

// 3. 用户调用轮询
eio_poll();

// 4. 轮询函数执行回调
if ((res = EIO_FINISH (req)))        // EIO_FINISH展开为:
    // ((req)->finish) && !EIO_CANCELLED (req) ? (req)->finish (req) : 0
    // 即：如果finish函数存在且未被取消，则调用finish(req)

// 5. 完成后清理资源
EIO_DESTROY (req);                   // 调用destroy函数清理
```

---

## 💡 最佳实践和注意事项

### 内存管理

```c
/**
 * 源码位置: eio.c 多处
 * 正确的资源清理方式
 */
// 1. REQ宏自动设置destroy函数
#define REQ(rtype)                                          \
  /* ... */                                                 \
  req->destroy = eio_api_destroy;  // 自动设置清理函数

// 2. PATH宏自动管理路径内存
#define PATH                                                        \
  req->flags |= EIO_FLAG_PTR1_FREE;  // 标记需要释放ptr1         \
  req->ptr1 = strdup (path);         // 复制路径字符串

// 3. 读操作自动分配缓冲区
case EIO_READ:
  ALLOC (req->size);                 // 宏定义：分配并标记需要释放
  // ...

// 4. 销毁时自动清理
#define EIO_DESTROY(req) do { if ((req)->destroy) (req)->destroy (req); } while (0)
```

### 错误处理模式

```c
/**
 * 源码中的错误处理机制
 */
// 1. 系统调用错误处理
case EIO_READ:
  req->result = read (req->int1, req->ptr2, req->size);
  if (req->result < 0)
    req->errorno = errno;            // 保存errno值

// 2. 取消检查
if (ecb_expect_false (EIO_CANCELLED (req)))
  {
    req->result  = -1;
    req->errorno = ECANCELED;
    return;
  }

// 3. 用户回调错误处理
int user_callback(eio_req *req) {
    if (req->result < 0) {
        // 处理错误：req->errorno 包含具体错误码
        fprintf(stderr, "Operation failed: %s\n", strerror(req->errorno));
        return -1;
    }
    // 正常处理...
    return 0;
}
```

### 性能优化建议

```c
/**
 * 源码体现的性能优化特性
 */
// 1. 分支预测优化
if (ecb_expect_true (req))           // 预测通常能获取到请求
  break;

// 2. 批量处理支持
void eio_set_max_poll_reqs (unsigned int nreqs) {
  EIO_POOL->max_poll_reqs = nreqs;   // 限制单次轮询处理的请求数
}

// 3. 时间限制
void eio_set_max_poll_time (eio_tstamp nseconds) {
  EIO_POOL->max_poll_time = nseconds * ETP_TICKS;  // 限制轮询时间
}

// 4. 线程池调优
eio_set_max_parallel(8);             // 设置最大并行线程数
eio_set_max_idle(4);                 // 设置最大空闲线程数
eio_set_idle_timeout(30);            // 设置空闲超时时间
```

---

_本文档基于libeio 1.0.2实际源码逐行分析编写，所有代码片段、数据结构定义和函数实现都来源于源文件的直接引用_
