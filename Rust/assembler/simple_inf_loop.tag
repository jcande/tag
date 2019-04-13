deletion_number {
        2
}

/*

top:
>
+
jmp top

*/

rules {

    s1_2 -> s1a_t_2 s1a_t_2
    net_2 -> __2
    s0_0 -> net_0 __0 test_0
    s0b_0 -> s0c_0 s0c_0
    y_1 -> y_2 y_2
    xb_t_2 -> x_0 x_0
    s0c_0 -> s0_1 s0_1
    s0b_f_2 -> s0_3 s0_3
    x_1 -> x_2 x_2
    net_0 -> odd_net_0 even_net_0
    test_0 -> s1b_0 s0b_0
    shift_2 -> s0b_f_2 s0b_f_2
    yb_t_2 -> y_0 y_0
    xb_f_2 -> x_3 x_3
    y_2 -> ya_t_2 ya_f_2
    s1a_t_2 -> s1b_t_2 s1b_t_2
    s1b_0 -> s1c_0 s1c_0
    even_net_0 -> pad_0
    ya_0 -> yb_0 yb_0
    xa_0 -> xb_0 xb_0
    xc_0 -> x_1 x_1
    s1c_0 -> s1_1 s1_1
    xb_0 -> xc_0 xc_0
    yb_0 -> yc_0 yc_0
    s0_1 -> s1_2 s1_2
    ya_f_2 -> yb_f_2 yb_f_2
    yb_f_2 -> y_3 y_3
    s1b_t_2 -> s1_0 s1_0
    x_0 -> xa_0 xa_0 xa_0 xa_0
    s1_0 -> xa_0 xa_0 net_0 __0 test_0
    y_0 -> ya_0
    s0_2 -> net_2 __2 shift_2
    s1_1 -> s1_2 s1_2
    yc_0 -> y_1 y_1
    odd_net_0 ->
    ya_t_2 -> yb_t_2 yb_t_2
    xa_2 -> xb_t_2 xb_f_2
    x_2 -> xa_2 xa_2

}

queue {
        s0_0 s0_0
}

