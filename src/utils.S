/* src/utils.S */
/* src/utils.S */
.global put32
.global get32
.global timer_get_freq
.global timer_set_tval
.global timer_set_ctl
.global set_vbar_el1

put32:
    str w1, [x0]
    ret

get32:
    ldr w0, [x0]
    ret

timer_get_freq:
    mrs x0, CNTFRQ_EL0
    ret

timer_set_tval:
    msr cntp_tval_el0, x0  /* <--- OJO: msr */
    ret

timer_set_ctl:
    msr cntp_ctl_el0, x0
    ret

set_vbar_el1:
    msr vbar_el1, x0
    ret