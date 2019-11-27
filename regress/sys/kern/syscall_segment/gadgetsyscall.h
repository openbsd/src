/*	$OpenBSD: gadgetsyscall.h,v 1.1 2019/11/27 17:15:36 mortimer Exp $	*/

pid_t gadget_getpid() {
	pid_t ans = 0;
#if defined(__amd64__)
	asm("mov $0x14, %%eax; syscall; mov %%eax, %0;" :"=r"(ans)::"%eax", "%ecx", "%r11");
#endif
	return ans;
}
