use std::path::Path;

// TODO make a usable main

extern crate failure;
extern crate tag;
extern crate wmach;

fn main() -> Result<(), failure::Error> {
    //let program = wmach::Program::from_file(Path::new("/tmp/in.wm"))?;
    //let program = wmach::Program::from_file(Path::new("cat.wm"))?;
    //let program = wmach::Program::from_file(Path::new("../../../others/elvm/assembled.wm"))?;
    let program = wmach::Program::from_file(Path::new("test.wm"))?;
    let tag = program.compile()?;
    println!("{}", tag);

    println!("{:?}", program);
    Ok(())
}
