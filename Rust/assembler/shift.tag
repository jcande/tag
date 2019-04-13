deletion_number {
        2
}

/*

>

*/

rules {

x_0 -> xa_0 xa_0 xa_0 xa_0
net_0 -> odd_net_0 even_net_0
s1_0 -> xa_0 xa_0 net_0 __0 test_0
s0b_0 -> s0c_0 s0c_0
s0c_0 -> s0_1 s0_1
xb_0 -> xc_0 xc_0
s1b_0 -> s1c_0 s1c_0
odd_net_0 ->
even_net_0 -> pad_0
test_0 -> s1b_0 s0b_0
xa_0 -> xb_0 xb_0
s1c_0 -> s1_1 s1_1
yc_0 -> y_1 y_1
xc_0 -> x_1 x_1
yb_0 -> yc_0 yc_0
y_0 -> ya_0
ya_0 -> yb_0 yb_0
s0_0 -> net_0 __0 test_0

}

queue {
        s0_0 s0_0
}

