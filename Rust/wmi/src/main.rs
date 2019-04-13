use std::fmt;
use std::io::{Read, Write};
use std::path::Path;
use std::time::SystemTime;

extern crate failure;
extern crate getopts;
extern crate wmach;

struct BitBuffer {
    data: u8,
    offset: u8,
}

#[derive(PartialEq)]
enum BbStatus {
    Ok,
    Max,
    Min,
}

impl BitBuffer {
    pub fn new() -> BitBuffer {
        BitBuffer {
            data: 0,
            offset: 0,
        }
    }

    pub fn set(&mut self) {
        self.data |= 1 << self.offset;
    }
    pub fn unset(&mut self) {
        self.data &= !(1 << self.offset);
    }

    pub fn write(&mut self, bit: bool) {
        if bit {
            self.set();
        } else {
            self.unset();
        }
    }
    pub fn read(&self) -> bool {
        return (self.data & (1 << self.offset)) != 0;
    }

    pub fn left(&mut self) -> BbStatus {
        let head_bits = 8; // currently using u8
        if self.offset < head_bits - 1 {
            self.offset += 1;
            return BbStatus::Ok;
        } else {
            return BbStatus::Max;
        }
    }
    pub fn right(&mut self) -> BbStatus {
        if self.offset > 0 {
            self.offset -= 1;
            return BbStatus::Ok;
        } else {
            return BbStatus::Min;
        }
    }

    pub fn get(&self) -> u8 {
        return self.data;
    }
    pub fn restore(&mut self, data: u8, direction: BbStatus) {
        // XXX this is just too fucking cute... fix this
        self.data = data;
        if direction == BbStatus::Max {
            self.offset = 7;
        } else {
            self.offset = 0;
        }
    }

    pub fn offset(&self) -> u8 {
        self.offset
    }
}

struct Memory {
    left: Vec<u8>,
    head: BitBuffer,
    right: Vec<u8>,

    position: i64,
}
enum Direction {
    Left,
    Right,
}

impl Memory {
    pub fn new() -> Memory {
        Memory {
            left: Vec::new(),
            head: BitBuffer::new(),
            right: Vec::new(),
            position: 0,
        }
    }

    pub fn read(&self) -> bool {
        self.head.read()
    }
    pub fn write(&mut self, bit: bool) {
        self.head.write(bit);
    }

    pub fn set(&mut self) {
        self.head.set();
        //eprintln!("{}", self);
    }
    pub fn unset(&mut self) {
        self.head.unset();
        //eprintln!("{}", self);
    }

    /*
    fn seek(&mut self, dir: Direction) {
        // probably a better way to do this
        let mut seeker = || match dir {
            Direction::Left => self.head.left(),
            Direction::Right => self.head.right(),
        };
        let (ref init, ref mut mem) = match dir {
            Direction::Left => (BbStatus::Min, self.left),
            Direction::Right => (BbStatus::Max, self.right),
        };

        if seeker() != BbStatus::Ok {
            let old = self.head.get();
            let new = match mem.pop() {
                None => 0,
                Some(value) => value,
            };

            self.head.restore(new, init);
            if old != 0 || mem.len() > 0 {
                mem.push(old);
            }
        }
    }
    */

    pub fn seek_left_unop(&mut self) {
        self.position -= 1;

        let old = self.head.get();
        if self.head.left() != BbStatus::Ok {
            let new = match self.left.pop() {
                None => 0,
                Some(value) => value,
            };
            self.head.restore(new, BbStatus::Min);

            self.right.push(old);
        }
    }
    pub fn seek_left(&mut self) {
        //self.seek(Direction::Left);

        self.seek_left_unop();

        /*
        let old = self.head.get();

        let bit_anywhere = old != 0 || self.left.len() > 0 || self.right.len() > 0;
        if bit_anywhere {
            if self.head.left() != BbStatus::Ok {

                let new = match self.left.pop() {
                    None => 0,
                    Some(value) => value,
                };
                self.head.restore(new, BbStatus::Min);

                if old != 0 || self.right.len() > 0 {
                    self.right.push(old);
                }
            }
        }

        self.position -= 1;
        */
    }
    pub fn seek_right_unop(&mut self) {
        self.position += 1;

        let old = self.head.get();
        if self.head.right() != BbStatus::Ok {
            let new = match self.right.pop() {
                None => 0,
                Some(value) => value,
            };
            self.head.restore(new, BbStatus::Max);

            self.left.push(old);
        }
    }
    pub fn seek_right(&mut self) {
        self.seek_right_unop();

        /*
        let old = self.head.get();

        let bit_anywhere = old != 0 || self.left.len() > 0 || self.right.len() > 0;
        if bit_anywhere {
            if self.head.right() != BbStatus::Ok {
                let new = match self.right.pop() {
                    None => 0,
                    Some(value) => value,
                };
                self.head.restore(new, BbStatus::Max);

                if old != 0 || self.left.len() > 0 {
                    self.left.push(old);
                }
            }
        }

        self.position += 1;
        */
    }
}

struct State {
    //
    // Memory
    //
    mem: Memory,

    //
    // Code
    //
    pc: wmach::InsnOffset,
    instructions: wmach::Code,

    //
    // IO
    //
    output: BitBuffer,
    input: BitBuffer,
}

impl fmt::Display for BitBuffer {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_fmt(format_args!("{{"))?;
        let head_bits = 8; // we're currently using u8
        for offset in (0..head_bits).rev() {
            if offset == self.offset {
                fmt.write_fmt(format_args!("["))?;
            }

            if self.data & (1 << offset) != 0 {
                fmt.write_fmt(format_args!("1"))?;
            } else {
                fmt.write_fmt(format_args!("0"))?;
            }

            if offset == self.offset {
                fmt.write_fmt(format_args!("]"))?;
            }
        }
        fmt.write_fmt(format_args!("}}"))?;

        Ok(())
    }
}

// XXX make this ONLY print out the registers (everything up to MemoryBase) so that we can
// use this while debugging memory accesses
impl fmt::Display for Memory {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_fmt(format_args!("{}: ", self.position))?;

        for byte in self.left.iter() {
            fmt.write_fmt(
                format_args!("{:02x}", byte)
            )?;
        }
        fmt.write_fmt(format_args!("{}", self.head))?;
        for byte in self.right.iter().rev() {
            fmt.write_fmt(
                format_args!("{:02x}", byte)
            )?;
        }
        //fmt.write_fmt(format_args!("\n"))?;

        Ok(())
    }
}

impl fmt::Display for State {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_fmt(format_args!("{{\n"))?;

        fmt.write_fmt(format_args!("    memory: {}\n", self.mem))?;

        if self.pc < self.instructions.len() {
            fmt.write_fmt(format_args!("    code: {}: {:?}\n", self.pc, self.instructions[self.pc]))?;
        } else {
            fmt.write_fmt(format_args!("    code: {}: INVALID\n", self.pc))?;
        }

        fmt.write_fmt(format_args!("    input: {}\n", self.input))?;
        fmt.write_fmt(format_args!("    output: {}\n", self.output))?;

        fmt.write_fmt(format_args!("}}"))?;

        Ok(())
    }
}

impl State {
    pub fn new(instructions: wmach::Code) -> State {
        State {
            mem: Memory::new(),

            pc: 0,
            instructions: instructions,

            output: BitBuffer::new(),
            input: BitBuffer::new(),
        }
    }

    pub fn pc(&self) -> wmach::Insn {
        self.instructions[self.pc].clone()
    }
    pub fn halt(&self) -> bool {
        self.pc >= self.instructions.len()
    }

    pub fn set(&mut self) {
        self.mem.set();
        self.pc += 1;
    }
    pub fn unset(&mut self) {
        self.mem.unset();
        self.pc += 1;
    }
    pub fn left(&mut self) {
        self.mem.seek_left();
        self.pc += 1;
    }
    pub fn right(&mut self) {
        self.mem.seek_right();
        self.pc += 1;
    }

    pub fn put(&mut self) {
        self.output.write(self.mem.read());
        if self.output.left() == BbStatus::Max {
            let out = [self.output.get()];

            // if we can't write to the screen, such is life
            let _ = std::io::stdout().write(&out);
            std::io::stdout().flush();

            self.output.restore(0, BbStatus::Min);
        }
        //eprintln!("{}", self);
        self.pc += 1;
    }
    pub fn get(&mut self) {
        if self.input.offset() == 0 {
            let input = std::io::stdin()
                .bytes() 
                .next()
                .and_then(|result| result.ok())
                .map(|byte| byte as u8)
                .unwrap_or(0xff);
            if input == 0xff {
                // halt on eof
                // according to the elvm spec we should return 0 but fuck that
                self.pc = std::usize::MAX;
                return;
            }

            self.input.restore(input, BbStatus::Min);
        }

        self.mem.write(self.input.read());
        //eprintln!("{}", self);
        if self.input.left() != BbStatus::Ok {
            self.input.restore(0xff, BbStatus::Min);
        }

        self.pc += 1;
    }

    pub fn branch(&mut self, set: wmach::InsnOffset, unset: wmach::InsnOffset) {
        if self.mem.read() {
            self.pc = set;
        } else {
            self.pc = unset;
        }
//eprintln!("branched: {}", self);
    }

    // XXX Could probably make this interactive or something
    pub fn debug(&mut self) {
        eprintln!("DBG: {}", self);

        self.pc += 1;
    }
}

fn interpret(program: wmach::Program) {
    let mut s = State::new(program.instructions);
    loop {
        if s.halt() {
            break;
        }

        match s.pc() {
            wmach::Insn::Seek(wmach::SeekOp::Left) => {
                s.left();
            }
            wmach::Insn::Seek(wmach::SeekOp::Right) => {
                s.right();
            }

            wmach::Insn::Write(wmach::WriteOp::Set) => s.set(),
            wmach::Insn::Write(wmach::WriteOp::Unset) => s.unset(),

            wmach::Insn::Io(wmach::IoOp::In) => s.get(),
            wmach::Insn::Io(wmach::IoOp::Out) => s.put(),

            wmach::Insn::Jmp(true_addr, false_addr) => s.branch(true_addr, false_addr),

            wmach::Insn::Debug => s.debug(),
        }
    }
    //eprintln!("fin: {}", s);
}

#[derive(Debug, failure::Fail)]
enum WmiError {
    #[fail(display = "missing file to interpret")]
    MissingFile,

    #[fail(display = "printed help message")]
    Help,
}

fn usage(_opts: getopts::Options) {
    eprintln!("gitguud");
}

fn main() -> Result<(), failure::Error> {
    let args: Vec<String> = std::env::args().collect();
    let mut opts = getopts::Options::new();
    opts.optopt("f", "", "source file to interpret", "NAME");
    opts.optflag("h", "help", "print this help menu");

    let matches = opts.parse(&args[1..])?;
    if matches.opt_present("h") {
        usage(opts);
        Err(WmiError::Help)?;
    }

    eprintln!("Loading...");
    let start = SystemTime::now();

    let file = matches.opt_str("f").ok_or(WmiError::MissingFile)?;
    let program = wmach::Program::from_file(Path::new(&file))?;

    let diff = start.elapsed()
          .expect("SystemTime::elapsed failed");
    eprintln!("Initialized in {:?}", diff);

//println!("Program: {:?}", program);
    interpret(program);

    Ok(())
}
