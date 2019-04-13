deletion_number {
    2
}

rules {
    start -> read_bit read_bit

    read_bit -> { print0 print0 ; print1 print1 }

    print0 -> 0: read_bit read_bit
    print1 -> 1: read_bit read_bit
}

queue {
    start start
}
