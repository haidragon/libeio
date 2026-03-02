#include <stdio.h>
#include <eio.h>
#include <unistd.h>

// 全局变量用于控制轮询
static int need_poll = 0;

// 当需要轮询时被调用
void want_poll_callback(void) {
    printf("需要轮询\n");
    need_poll = 1;
}

// 当不再需要轮询时被调用
void done_poll_callback(void) {
    printf("轮询完成\n");
    need_poll = 0;
}

int my_cb(eio_req *req) {
    printf("异步任务完成，结果: %zd\n", EIO_RESULT(req));
    return 0;
}

int main() {
    printf("初始化 libeio\n");
    if (eio_init(want_poll_callback, done_poll_callback)) {
        printf("eio_init 失败\n");
        return 1;
    }
    printf("eio_init 成功\n");
    
    printf("提交异步 nop 任务\n");
    eio_req *req = eio_nop(EIO_PRI_DEFAULT, my_cb, NULL);
    if (req == NULL) {
        printf("eio_nop 提交失败\n");
        return 1;
    }
    printf("任务已提交，等待完成...\n");
    
    int count = 0;
    while (eio_nreqs() > 0) {
        printf("轮询中... (第%d次), 待处理请求数: %d\n", ++count, eio_nreqs());
        
        // 如果需要轮询，则执行轮询
        if (need_poll) {
            int result = eio_poll();
            printf("eio_poll 返回: %d\n", result);
        }
        
        usleep(10000); // 等待10ms
        
        if (count > 100) {  // 防止无限循环
            printf("超时退出\n");
            break;
        }
    }
    printf("主循环结束\n");
    return 0;
}