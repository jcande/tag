deletion_number {
    2
}

rules {
/* 0: + */
x_0 -> x_1 x_1

s0_0 -> s1_1 s1_1
s1_0 -> s1_1 s1_1

y_0 -> y_1 y_1


/* 1: > */
x_1 -> x_a1 x_a1 xp_a1

s0_1 -> s0_a1 s0_a1
s1_1 -> s1_a1 s1_a1

y_1 -> y_a1 y_a1

x_a1 -> x_b1 x_b1 x_b1
xp_a1 -> x_b1 x_b1

s0_a1 -> t_1
s1_a1 -> x_b1 x_b1 t_1

y_a1 -> y_b1

x_b1 -> x_c1 x_c1

t_1 -> t_a1 tp_a1
y_b1 -> y_c1 y_c1

x_c1 -> x_2 x_2

t_a1 -> s1_2 s1_2
tp_a1 -> x_2 s0_2 s0_2

y_c1 -> y_2 y_2


/* 2: + */
x_2 -> x_3 x_3

s0_2 -> s1_3 s1_3
s1_2 -> s1_3 s1_3

y_2 -> y_3 y_3


/* 3: jmp 1 */
x_3 -> t_3 t_3

s0_3 -> sp0_3
s1_3 -> sp1_3 sp1_3

y_3 -> u_3 up_3

t_3 -> xp_3 xpp_3

sp0_3 -> xp_3 spp0_3 spp0_3
sp1_3 -> spp1_3 spp1_3

u_3 -> yp_3 ypp_3
up_3 -> ypp_3 yp_3

xp_3 -> x_1 x_1
xpp_3 -> x_4 x_4

spp0_3 -> s0_4 s0_4
spp1_3 -> s1_1 s1_1

yp_3 -> y_1 y_1
ypp_3 -> y_4 y_4

}

queue {
    /* an empty tape is invalid! We need to represent (at least) the head of
     * the turing machine */
    s0_0 s0_0
}
