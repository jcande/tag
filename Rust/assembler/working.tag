deletion_number {
        2
}

/*

// This is a wm cat program
top:
,.
jmp top, top

*/

rules {

    ya_t_2 -> yb_t_2 yb_t_2
    y_0 -> y_1 y_1
    x_1 -> x_2 x_2
    s1b_t_2 -> s1_0 s1_0
    ya_f_2 -> yb_f_2 yb_f_2
    xb_t_2 -> x_0 x_0
    yb_t_2 -> y_0 y_0
    s1_2 -> s1a_t_2 s1a_t_2
    s1a_t_2 -> s1b_t_2 s1b_t_2
    s1_0 -> { s0_1 s0_1  ; s1_1 s1_1  }
    s0b_f_2 -> s0_0 s0_0
    s1_1 -> 1: s1_2 s1_2
    s0_2 -> net_2 __2 shift_2
    x_2 -> xa_2 xa_2
    shift_2 -> s0b_f_2 s0b_f_2
    x_0 -> x_1 x_1
    s0_1 -> 0: s0_2 s0_2
    net_2 -> __2
    yb_f_2 -> y_0 y_0
    xb_f_2 -> x_0 x_0
    s0_0 -> { s0_1 s0_1  ; s1_1 s1_1  }
    y_1 -> y_2 y_2
    y_2 -> ya_t_2 ya_f_2
    xa_2 -> xb_t_2 xb_f_2

}

queue {
        s0_0 s0_0
}
