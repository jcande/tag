use std::fs::File;
use std::path::Path;
use std::collections::HashMap;
use std::str::FromStr;
use std::io::Read;
use std::fmt;

#[macro_use]
extern crate failure_derive;
extern crate failure;

#[macro_use]
extern crate nom;
use nom::types::CompleteByteSlice;
use nom::AsBytes;

extern crate tag;

fn forge_symbol(pos: usize, name: &str) -> String {
    let mut symbol = name.to_string();
    symbol.push('_');
    symbol.push_str(&pos.to_string());
    symbol
}

#[derive(Debug, Fail)]
pub enum Err {
    #[fail(display = "{}", message)]
    GeneralError { message: String },

    #[fail(display = "{}", rule)]
    InvalidRule { rule: String },

    #[fail(display = "{}", label)]
    DuplicateLabel { label: String },

    // this realy should be a LabelId but I don't want to pull it out of the Target
    #[fail(display = "Unknown target {} referenced", target)]
    UnknownTarget {
        target: Target,
    },

    #[fail(display = "{}", message)]
    IoError {
        message: String,
        backtrace: failure::Backtrace,
        #[cause]
        cause: std::io::Error,
    },
}

impl From<std::io::Error> for Err {
    fn from(error: std::io::Error) -> Err {
        Err::IoError {
            // XXX what would make a good message?
            message: "IO Error".to_string(),
            backtrace: failure::Backtrace::new(),
            cause: error,
        }
    }
}

impl From<tag::Err> for Err {
    fn from(error: tag::Err) -> Err {
        Err::GeneralError {
            message: error.to_string(),
        }
    }
}

#[derive(Debug, Clone)]
pub enum Insn {
    Write(WriteOp),
    Seek(SeekOp),
    Io(IoOp),
    Jmp(InsnOffset, InsnOffset),
    Debug,
}

#[derive(Debug, Clone)]
pub enum Stmt {
    Write(WriteOp),
    Seek(SeekOp),
    Io(IoOp),
    Label(LabelId),
    Jmp(Target, Target),
    Debug,
}

#[derive(Debug, Clone)]
pub enum WriteOp {
    Set,
    Unset,
}

#[derive(Debug, Clone)]
pub enum SeekOp {
    Left,
    Right,
}

#[derive(Debug, Clone)]
pub enum IoOp {
    In,
    Out,
}


pub type LabelId = String;
pub type InsnOffset = usize;
pub type LabelMap = HashMap<LabelId, InsnOffset>;
#[derive(Debug, Clone)]
pub enum Target {
    NextAddress,
    Name(LabelId),
}

impl fmt::Display for Target {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Target::NextAddress => write!(f, "<fallthrough>"),
            Target::Name(label) => write!(f, "<{}>", label),
        }
    }
}

pub type Code = Vec<Insn>;

#[derive(Debug)]
pub struct Program {
    pub instructions: Code,
    labels: LabelMap,
}

impl FromStr for Program {
    type Err = failure::Error;

    fn from_str(unparsed: &str) -> Result<Program, failure::Error> {
        let statements = Program::parse_statements(unparsed)?;

        // make jmp table
        let mut jmp_table: LabelMap = HashMap::new();
        let mut offset: InsnOffset = 0;
        for stmt in statements.iter() {
            if let Stmt::Label(label_id) = stmt {
                if jmp_table.contains_key(label_id) {
                    Err(Err::DuplicateLabel{ label: label_id.to_owned() })?
                }

                jmp_table.insert(label_id.to_owned(), offset);
            } else {
                offset += 1;
            }
        }

        // make instructions
        // we should probably rethink this and devise a way to lean on the type system instead of encoding the ordering implicitly in the vector
        let mut insns: Vec<Insn> = Vec::new();
        let mut next: InsnOffset = 1;
        for stmt in statements.iter() {
            if let Stmt::Label(_) = stmt {
                continue;
            }

            let insn = match stmt {
                Stmt::Write(value) => {
                    Insn::Write(value.to_owned())
                },
                Stmt::Seek(direction) => {
                    Insn::Seek(direction.to_owned())
                },
                Stmt::Io(rw) => {
                    Insn::Io(rw.to_owned())
                },
                Stmt::Jmp(branch_t, branch_f) => {
                    let target_address = |branch: &Target| match branch {
                        Target::NextAddress     => Some(&next),
                        Target::Name(label_id)  => jmp_table.get(label_id),
                    };

                    // missing label error
                    let t = target_address(branch_t).ok_or(Err::UnknownTarget {
                        target: branch_t.to_owned(),
                    })?;
                    let f = target_address(branch_f).ok_or(Err::UnknownTarget {
                        target: branch_f.to_owned(),
                    })?;

                    Insn::Jmp(*t, *f)
                },
                Stmt::Debug => Insn::Debug,

                _ => {
                    panic!("Shouldn't reach this");
                },
            };

            insns.push(insn);
            next += 1;
        }

        Ok(Program{
            instructions: insns,
            labels: jmp_table,
        })
    }
}

fn is_label_id(input: u8) -> bool {
    // misc = { "'" | '_' }
    // label_id = (alpha | digit | misc)+
    match input {
        b'a' ... b'z'   => true,
        b'A' ... b'Z'   => true,
        b'0' ... b'9'   => true,
        b'\''           => true,
        b'_'            => true,
        _               => false,
    }
}
named!(label_id<CompleteByteSlice, &[u8]>,
        map!(take_while!(is_label_id),
            |b| b.0
        )
);

named!(label<CompleteByteSlice, Stmt>,
   do_parse!(
       label: map_res!(label_id,
                       std::str::from_utf8) >>
       tag!(":")                            >>
       (Stmt::Label(label.to_owned()))
    )
);

named!(jmp_op<CompleteByteSlice, Stmt>,
    do_parse!(
        ws!(tag!("jmp"))                                   >>
        true_branch: map_res!(label_id,
                              std::str::from_utf8)         >>

        false_branch: opt!(do_parse!(
                        ws!(tag!(","))                     >>
                        id: map_res!(label_id,
                                     std::str::from_utf8)  >>
                        (Target::Name(id.to_owned()))
                        ))                                 >>

        (Stmt::Jmp(Target::Name(true_branch.to_owned()),
                    false_branch.unwrap_or(Target::NextAddress)))
));

named!(set_op<CompleteByteSlice, Stmt>,
    do_parse!(ws!(tag!("+")) >> (Stmt::Write(WriteOp::Set)))
);
named!(unset_op<CompleteByteSlice, Stmt>,
    do_parse!(ws!(tag!("-")) >> (Stmt::Write(WriteOp::Unset)))
);

named!(shiftL_op<CompleteByteSlice, Stmt>,
    do_parse!(ws!(tag!("<")) >> (Stmt::Seek(SeekOp::Left)))
);
named!(shiftR_op<CompleteByteSlice, Stmt>,
    do_parse!(ws!(tag!(">")) >> (Stmt::Seek(SeekOp::Right)))
);

named!(input_op<CompleteByteSlice, Stmt>,
    do_parse!(ws!(tag!(",")) >> (Stmt::Io(IoOp::In)))
);
named!(output_op<CompleteByteSlice, Stmt>,
    do_parse!(ws!(tag!(".")) >> (Stmt::Io(IoOp::Out)))
);

named!(debug_op<CompleteByteSlice, Stmt>,
    do_parse!(ws!(tag!("!")) >> (Stmt::Debug))
);

named!(statement<CompleteByteSlice, Stmt>,
    alt!(
        jmp_op    |
        set_op    | unset_op  |
        shiftL_op | shiftR_op |
        input_op  | output_op |
        debug_op
    )
);

named!(comment<CompleteByteSlice, CompleteByteSlice>,
       do_parse!(tag!("/*")                             >>
                 comment: take_until_and_consume!("*/") >>
       (comment))
);

// XXX Yeah, you can't put a comment anywhere. I am willing to live with that for the time being
named!(any_statement<CompleteByteSlice, Stmt>,
    do_parse!(
        opt!(ws!(comment))   >>
        stmt: alt!(
                ws!(label)   |
                statement
        )               >>
        (stmt)
    )
);

named!(program_statements<CompleteByteSlice, Vec<Stmt>>,
       many0!(any_statement)
);

impl Program {
    fn parse_statements(unparsed: &str) -> Result<Vec<Stmt>, failure::Error> {
        let unparsed = CompleteByteSlice(unparsed.as_bytes());
        let (rest, statements) = program_statements(unparsed)
            .map_err(|e| Err::GeneralError {
                message: format!("Nom Error: {}", e),
             })?;

        let rest = String::from_utf8(rest
                                     .as_bytes()
                                     .to_vec())
            .expect("Invalid UTF8");
        if rest.len() > 0 {
            Err(Err::GeneralError {
                message: format!("Left over data: {}", rest),
            })?;
        }

        Ok(statements)
    }

    pub fn from_file(filename: &Path) -> Result<Program, failure::Error> {
        let mut unparsed_file = String::new();
        File::open(filename)?.read_to_string(&mut unparsed_file)?;

        Program::from_str(&unparsed_file)
    }

    fn mk_write(pos: InsnOffset, op: &WriteOp) -> tag::Rules {
        let mut rules = HashMap::new();

        // x_i -> x_j x_j
        let x_start = forge_symbol(pos, "x");
        let x = forge_symbol(pos + 1, "x");
        tag::rule!(rules, x_start -> x x);

        // y_i -> y_j y_j
        let y_start = forge_symbol(pos, "y");
        let y = forge_symbol(pos + 1, "y");
        tag::rule!(rules, y_start -> y y);


        let s1_start = forge_symbol(pos, "s1");
        let s1_next = forge_symbol(pos + 1, "s1");

        let s0_start = forge_symbol(pos, "s0");
        let s0_next = forge_symbol(pos + 1, "s0");

        match op {
            // s0_i -> s1_j s1_j
            // s1_i -> s1_j s1_j
            WriteOp::Set    => {
                tag::rule!(rules, s0_start -> s1_next s1_next);
                tag::rule!(rules, s1_start -> s1_next s1_next);
            },
            // s0_i -> s0_j s0_j
            // s1_i -> s0_j s0_j
            WriteOp::Unset  => {
                tag::rule!(rules, s0_start -> s0_next s0_next);
                tag::rule!(rules, s1_start -> s0_next s0_next);
            },
        };

        rules
    }

    fn mk_seek(pos: InsnOffset, direction: &SeekOp) -> tag::Rules {
        let mut rules = HashMap::new();

        let blank = forge_symbol(pos, "_");

        let x = forge_symbol(pos, "x");
        let x_next = forge_symbol(pos + 1, "x");
        let xa = forge_symbol(pos, "xa");
        let xb = forge_symbol(pos, "xb");
        let xc = forge_symbol(pos, "xc");

        let s0 = forge_symbol(pos, "s0");
        let s1 = forge_symbol(pos, "s1");
        let net = forge_symbol(pos, "net");
        let test = forge_symbol(pos, "test");
        let s0b = forge_symbol(pos, "s0b");
        let s1b = forge_symbol(pos, "s1b");
        let even_net = forge_symbol(pos, "even_net");
        let odd_net = forge_symbol(pos, "odd_net");
        let pad = forge_symbol(pos, "pad");
        let s0c = forge_symbol(pos, "s0c");
        let s1c = forge_symbol(pos, "s1c");
        let s0_next = forge_symbol(pos + 1, "s0");
        let s1_next = forge_symbol(pos + 1, "s1");

        let y = forge_symbol(pos, "y");
        let ya = forge_symbol(pos, "ya");
        let yb = forge_symbol(pos, "yb");
        let yc = forge_symbol(pos, "yc");
        let y_next = forge_symbol(pos + 1, "y");


        match direction {
            SeekOp::Right       => {
                //
                // (1)
                //

                // x_i -> xa xa xa xa
                // Double the xs
                tag::rule!(rules, x -> xa xa xa xa);

                // s0_i -> net _ test
                tag::rule!(rules, s0 -> net blank test);
                // s1_i -> xa xa net _ test
                tag::rule!(rules, s1 -> xa xa net blank test);

                // y_i -> ya
                // Divide by 2 to obtain the unary representation of the ys
                tag::rule!(rules, y -> ya);


                //
                // (2)
                //

                // xa -> xb xb
                // Maintain the status quo so we can do a parity check
                tag::rule!(rules, xa -> xb xb);

                // net -> odd_net even_net
                // Cast the net! This let's us safely operate even if there are no xs or ys
                tag::rule!(rules, net -> odd_net even_net);
                // test -> s1b s0b
                // This senses the parity of the ys. If they're odd, then the new head has a 1,
                // if it is even then it gets a 0.
                tag::rule!(rules, test -> s1b s0b);


                // ya -> yb yb
                tag::rule!(rules, ya -> yb yb);


                //
                // (3)
                //

                // xb -> xc xc
                tag::rule!(rules, xb -> xc xc);

                // s0b -> s0c s0c
                // Looks like it was even
                tag::rule!(rules, s0b -> s0c s0c);
                // s1b -> s1c s1c
                // Turns out we had an odd amount of ys
                tag::rule!(rules, s1b -> s1c s1c);
                // odd_net ->
                // Print out the intermediate queue, trust me. Keep the odd parity going.
                tag::rule!(rules, odd_net -> );
                // even_net -> pad
                // Print out the intermediate queue, trust me. Balance the parity.
                tag::rule!(rules, even_net -> pad);

                // yb -> yc yc
                tag::rule!(rules, yb -> yc yc);


                //
                // (4)
                //

                // xc -> x_j x_j
                tag::rule!(rules, xc -> x_next x_next);

                // s0c -> s0_j s0_j
                tag::rule!(rules, s0c -> s0_next s0_next);
                // s1c -> s1_j s1_j
                tag::rule!(rules, s1c -> s1_next s1_next);

                // yc -> y_j y_j
                tag::rule!(rules, yc -> y_next y_next);
            },
            SeekOp::Left        => {
                // SeekOp::Right was done first so the comments there will be better. This is
                // the same exact idea only mirrored.

                //
                // (1)
                //

                // x_i -> xa xa
                // Maintain the status quo
                tag::rule!(rules, x -> xa xa);

                // s0_i -> net _ test
                tag::rule!(rules, s0 -> net blank test);
                // s1_i -> net _ test ya ya
                tag::rule!(rules, s1 -> net blank test ya ya);

                // y_i -> ya ya ya ya
                // Double
                tag::rule!(rules, y -> ya ya ya ya);


                //
                // (2)
                //

                // xa -> xb
                // Halve
                tag::rule!(rules, xa -> xb);

                // net -> odd_net even_net
                // Cast the net so we can operate even with no xs or ys.
                tag::rule!(rules, net -> odd_net even_net);
                // test -> s1b s0b
                tag::rule!(rules, test -> s1b s0b);

                // ya -> yb yb
                tag::rule!(rules, ya -> yb yb);


                //
                // (3)
                //

                // xb -> xc xc
                tag::rule!(rules, xb -> xc xc);

                // s0b -> s0c s0c
                tag::rule!(rules, s0b -> s0c s0c);
                // s1b -> s1c s1c
                tag::rule!(rules, s1b -> s1c s1c);
                // odd_net ->
                tag::rule!(rules, odd_net -> );
                // even_net -> pad
                tag::rule!(rules, even_net -> pad);

                // yb -> yc yc
                tag::rule!(rules, yb -> yc yc);


                //
                // (4)
                //

                // xc -> x_j x_j
                tag::rule!(rules, xc -> x_next x_next);

                // s0c -> s0_j s0_j
                tag::rule!(rules, s0c -> s0_next s0_next);
                // s1c -> s1_j s1_j
                tag::rule!(rules, s1c -> s1_next s1_next);

                // yc -> y_j y_j
                tag::rule!(rules, yc -> y_next y_next);
            },
        };

        rules
    }

    fn mk_io(pos: InsnOffset, rw: &IoOp) -> tag::Rules {
        let mut rules = HashMap::new();

        // x_i -> x_j x_j
        let x = forge_symbol(pos, "x");
        let x_next = forge_symbol(pos + 1, "x");
        tag::rule!(rules, x -> x_next x_next);

        // y_i -> y_j y_j
        let y = forge_symbol(pos, "y");
        let y_next = forge_symbol(pos + 1, "y");
        tag::rule!(rules, y -> y_next y_next);


        let s1 = forge_symbol(pos, "s1");
        let s1_next = forge_symbol(pos + 1, "s1");

        let s0 = forge_symbol(pos, "s0");
        let s0_next = forge_symbol(pos + 1, "s0");

        match rw {
            IoOp::In    => {
                // s0_i -> { s0_j s0_j ; s1_j s1_j }
                tag::rule!(rules, s0 -> { s0_next s0_next ; s1_next s1_next });
                // s1_i -> { s0_j s0_j ; s1_j s1_j }
                tag::rule!(rules, s1 -> { s0_next s0_next ; s1_next s1_next });
            },
            IoOp::Out   => {
                // s0_i -> 0: s0_j s0_j
                tag::rule!(rules, s0 -> 0: s0_next s0_next);

                // s1_i -> 1: s1_j s1_j
                tag::rule!(rules, s1 -> 1: s1_next s1_next);
            },
        };

        rules
    }

    fn mk_jmp(pos: InsnOffset, br_t: &InsnOffset, br_f: &InsnOffset) -> tag::Rules {
        let mut rules = HashMap::new();

        let x = forge_symbol(pos, "x");
        let xa = forge_symbol(pos, "xa");
        let xb_t = forge_symbol(pos, "xb_t");
        let xb_f = forge_symbol(pos, "xb_f");
        let x_t = forge_symbol(*br_t, "x");
        let x_f = forge_symbol(*br_f, "x");

        let s0 = forge_symbol(pos, "s0");
        let s1 = forge_symbol(pos, "s1");
        let net = forge_symbol(pos, "net");
        let blank = forge_symbol(pos, "_");
        let shift = forge_symbol(pos, "shift");
        let s1a_t = forge_symbol(pos, "s1a_t");
        let s0b_f = forge_symbol(pos, "s0b_f");
        let s1b_t = forge_symbol(pos, "s1b_t");
        let s0_f = forge_symbol(*br_f, "s0");
        let s1_t = forge_symbol(*br_t, "s1");

        let y = forge_symbol(pos, "y");
        let ya_t = forge_symbol(pos, "ya_t");
        let ya_f = forge_symbol(pos, "ya_f");
        let yb_t = forge_symbol(pos, "yb_t");
        let yb_f = forge_symbol(pos, "yb_f");
        let y_t = forge_symbol(*br_t, "y");
        let y_f = forge_symbol(*br_f, "y");

        //
        // (1)
        //

        // x_i -> xa xa
        // Since the xs appear before the head (s*), we maintain the status quo for a cycle so
        // that we can be impacted by the parity flip.
        tag::rule!(rules, x -> xa xa);

        tag::rule!(rules, s0 -> net blank shift);
        tag::rule!(rules, s1 -> s1a_t s1a_t);

        tag::rule!(rules, y -> ya_t ya_f);


        //
        // (2)
        //

        // Ok, now we can participate in the parity flip.
        tag::rule!(rules, xa -> xb_t xb_f);

        tag::rule!(rules, s1a_t -> s1b_t s1b_t);
        tag::rule!(rules, ya_t -> yb_t yb_t);


        tag::rule!(rules, net -> blank);
        tag::rule!(rules, shift -> s0b_f s0b_f);
        tag::rule!(rules, ya_f -> yb_f yb_f);


        //
        // (3)
        //

        tag::rule!(rules, xb_t -> x_t x_t);
        tag::rule!(rules, s1b_t -> s1_t s1_t);
        tag::rule!(rules, yb_t -> y_t y_t);

        tag::rule!(rules, xb_f -> x_f x_f);
        tag::rule!(rules, s0b_f -> s0_f s0_f);
        tag::rule!(rules, yb_f -> y_f y_f);

        rules
    }

    // XXX nop for now
    fn mk_debug(pos: InsnOffset) -> tag::Rules {
        let mut rules = HashMap::new();

        let x = forge_symbol(pos, "x");
        let x_dbg = forge_symbol(pos, "x_dbg");
        let x_next = forge_symbol(pos + 1, "x");

        let s0 = forge_symbol(pos, "s0");
        let s0_dbg = forge_symbol(pos, "s0_dbg");
        let s0_next = forge_symbol(pos + 1, "s0");
        let s1 = forge_symbol(pos, "s1");
        let s1_dbg = forge_symbol(pos, "s1_dbg");
        let s1_next = forge_symbol(pos + 1, "s1");

        let y = forge_symbol(pos, "y");
        let y_dbg = forge_symbol(pos, "y_dbg");
        let y_next = forge_symbol(pos + 1, "y");

        tag::rule!(rules, x -> x_dbg x_dbg);
        tag::rule!(rules, s0 -> s0_dbg s0_dbg);
        tag::rule!(rules, s1 -> s1_dbg s1_dbg);
        tag::rule!(rules, y -> y_dbg y_dbg);

        tag::rule!(rules, x_dbg -> x_next x_next);
        tag::rule!(rules, s0_dbg -> s0_next s0_next);
        tag::rule!(rules, s1_dbg -> s1_next s1_next);
        tag::rule!(rules, y_dbg -> y_next y_next);

        rules
    }

    // XXX should also return some debug symbols (jmp_table?)
    pub fn compile(&self) -> Result<tag::Program, failure::Error> {
        let mut rules: tag::Rules = HashMap::new();

        for (i, insn) in self.instructions.iter().enumerate() {
            let translated = match insn {
                Insn::Write(value) => {
                    Self::mk_write(i, &value)
                },
                Insn::Seek(direction) => {
                    Self::mk_seek(i, &direction)
                },
                Insn::Io(rw) => {
                    Self::mk_io(i, &rw)
                },
                Insn::Jmp(branch_t, branch_f) => {
                    Self::mk_jmp(i, &branch_t, &branch_f)
                },
                Insn::Debug => {
                    Self::mk_debug(i)   // XXX need to think about how to do this
                },
            };

            rules.extend(translated);
        }

        // XXX start start? This can then generate .data
        let default_queue = vec!["s0_0".to_owned(), "s0_0".to_owned()];
        tag::Program::from_components(2, rules, default_queue)
    }
}









#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
