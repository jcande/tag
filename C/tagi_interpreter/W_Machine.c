/*
 * +   Mark (set) the square under the head
 * -   Erase (clear) the square under the head
 * >   Move the head to the right by one square
 * <   Move the head to the left by one square
 * jmp n   Branch to the n'th instruction if the square under the head is marked,
 *       otherwise treat as nop
 *       XXX We might want this to be if-then-else style instead of just if-then
 *
 * We can think of this exactly like assembly for an ISA with 5 instructions
 * and an infinitely long tape extending in both directions.
 * For example:
 *
 * 0: +
 * 1: >
 * 2: +
 * 3: jmp 1
 *
 * This is an infinite loop that just sets every bit as it traverses
 * rightwardly on the tape.
 *
 * Now that we have the syntax and semantics of the turing machine outlined
 * let's see how this maps to a tag system (taken from Tag Systems and Lag
 * Systems by Wang 1963).
 *
 * +   (x_i x_i)^m s_i s_i (y_i y_i)^n
 *
 *      x_i -> x_i+1 x_i+1
 *      s0_i -> s1_i+1
 *      s1_i -> s1_i+1
 *      y_i -> y_i+1 y_i+1
 *
 * -    ...
 *      s0_i -> s0_i+1
 *      s1_i -> s0_i+1
 *      ...
 *
 *     This is kinda confusing because we take some abstract symbol (s_i) and
 *     map it to a concrete value of 1_i+1/0_i+1. Obviously we are setting it
 *     but in this representation it is not obvious that 1_i+1 is still an "s"
 *     symbol. This is because subsequent productions must be able to
 *     distinguish the value under the read head.
 *
 *     The other confusing bit is that we encode the value of the tape in a
 *     super clever and indirect way. In our example, both m and n are integers
 *     and the binary representation of these integers corresponds to the cells
 *     themselves (up to the highest set bit and then everything else is
 *     defined to be 0). In other words, the x's and y's are irrelevant aside
 *     from their quantities which defines the bitwise representation of the
 *     cells. Pretty cool.
 *
 * Let's take our small W-machine program and translate it into a tag system.
 * For simplicity, let's assume our W-machine's tape is blank (no initial
 * input). This means our machine representation of
 * (x_i x_i)^m s_i s_i (y_i y_i)^n would be just s0_0 s0_0. To translate the
 * turing machine, each symbol must have its prefix (so that it can be operated on
 * by rules. E.g., x, s, y), and its state (the index that shows where the
 * turing machine is executing). The s symbols are special as they must
 * actually have a concrete value (0 or 1), so we'll have a pair s0_i s1_i to
 * guide our actions.
 *
 * Instruction 0 (+) would be:
 * x_0 -> x_1 x_1
 *
 * s0_0 -> s1_1 s1_1
 * s1_0 -> s1_1 s1_1
 *
 * y_0 -> y_1 y_1
 *
 *
 * 1 (>):
 * (1)
 * x_1 -> x_a1 x_a1 xp_a1
 *
 * s0_1 -> s0_a1 s0_a1
 * s1_1 -> s1_a1 s1_a1
 *
 * y_1 -> y_a1 y_a1
 *
 * (2)
 * x_a1 -> x_b1 x_b1 x_b1
 * xp_a1 -> x_b1 x_b1
 *
 * (3)
 * s0_a1 -> t_1
 * s1_a1 -> x_b1 x_b1 t_1
 *
 * y_a1 -> y_b1
 *
 * (4)
 * x_b1 -> x_c1 x_c1
 *
 * (5)
 * t_1 -> t_a1 tp_a1
 * y_b1 -> y_c1 y_c1
 *
 * (6)
 * x_c1 -> x_2 x_2
 *
 * (7)
 * t_a1 -> s1_2 s1_2
 * tp_a1 -> x_2 s0_2 s0_2
 *
 * y_c1 -> y_2 y_2
 *
 *
 * 2 (+):
 * x_2 -> x_3 x_3
 *
 * s0_2 -> s1_3 s1_3
 * s1_2 -> s1_3 s1_3
 *
 * y_2 -> y_3 y_3
 *
 *
 * 3 (jmp 1):
 * (1)
 * x_3 -> t_3 t_3
 *
 * s0_3 -> sp0_3
 * s1_3 -> sp1_3 sp1_3
 *
 * y_3 -> u_3 up_3
 *
 * (2)
 * t_3 -> xp_3 xpp_3
 *
 * (3)
 * sp0_3 -> xp_3 spp0_3 spp0_3
 * sp1_3 -> spp1_3 spp1_3
 *
 * u_3 -> yp_3 ypp_3
 * up_3 -> ypp_3 yp_3
 *
 * (4)
 * xp_3 -> x_1 x_1
 * xpp_3 -> x_4 x_4
 *
 * spp0_3 -> s0_4 s0_4
 * spp1_3 -> s1_1 s1_1
 *
 * yp_3 -> y_1 y_1
 * ypp_3 -> y_4 y_4
 *
 *
 *
 * Wow! That sucks, you must be thinking. And it does but it's not that bad.
 * Also, the "p" that's in some of those symbols stands for "prime" and
 * represents the tick (') used in the paper.
 *
 * Let's evaluate a few instructions given our initial configuration is simply:
 * s0_0 s0_0
 *
 * First we'll look at the only matching symbol which nicely corresponds to
 * the first W-machine instruction. Applying the rule yields:
 * s1_1 s1_1
 *
 * Continuing at the next instruction:
 * s1_a1 s1_a1
 * => x_b1 x_b1 t_1
 * => t_1 x_c1 x_c1
 * => x_c1 t_a1 tp_a1
 * => tpa_1 x_2 x_2
 * => x_2 x_2 s0_2 s0_2
 *
 * Or, written a bit more explicitly: (x_2 x_2)^1 (s0_2 s0_2). Neat, it does work.
 * Continuing on with instruction 2:
 * => x_3 x_3 s1_3 s1_3
 *
 * And finally, our jump:
 * => s1_3 s1_3 t_3 t_3
 * => t_3 t_3 sp1_3 sp1_3
 * => sp1_3 sp1_3 xp_3 xpp_3
 * => xp_3 xpp_3 spp1_3 spp1_3
 * => spp1_3 spp1_3 x_1 x_1
 * => x_1 x_1 s1_1 s1_1
 *
 * And voila. We are now back at instruction 1. Magic. Who would have thought
 * shifting the read head is as complicated as conditional branching.
 */

enum Inst {
    Now_this_is_not_an_empty_enum_are_you_happy_compiler
};
