#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"

/* 加入以下代码, 支持printf函数, 而不需要选择use MicroLIB，并禁用半主机模式避免死机 */
#if 1
#if (__ARMCC_VERSION >= 6010050)            /* 使用AC6编译器时 */
__asm(".global __use_no_semihosting\n\t");  /* 声明不使用半主机模式 */
__asm(".global __ARM_use_no_argv \n\t");    /* AC6下需要声明main函数为无参数格式，否则部分例程可能出现半主机模式 */
#else
/* 使用AC5编译器时, 要在这里定义__FILE 和 不使用半主机模式 */
#pragma import(__use_no_semihosting)
struct __FILE
{
    int handle;
};
#endif

int _ttywrch(int ch)
{
    ch = ch;
    return ch;
}

void _sys_exit(int x)
{
    x = x;
}

char *_sys_command_string(char *cmd, int len)
{
    return NULL;
}

FILE __stdout;

/* MDK下需要重定义fputc函数, 避免半主机模式报错。
   为了不和4G模块冲突，这里的 fputc 为空操作，丢弃 printf 数据 */
int fputc(int ch, FILE *f)
{
    return ch;
}
#endif

/* 留空的初始化函数，防止编译报错，也不去干扰 4G模块在 dx_ct511_uart.c 里对 USART2 的底层配置 */
void usart_init(uint32_t baudrate)
{
    // Do nothing. USART2 is initialized in dx_ct511_uart_init()
}
