use std::fs::File;
use std::path::Path;
use std::io::Write;


extern crate failure;
extern crate tag;

fn main() -> Result<(), failure::Error> {
    let program = tag::Program::from_file(Path::new("working.tag"))?; // this is a compiled cat
    //let program = tag::Program::from_file(Path::new("working.tag"))?; // this is a compiled cat
    //let program = tag::Program::from_file(Path::new("cat.tag"))?;
    //let program = tag::Program::from_file(Path::new("dolla_dolla_bill.tag"))?; // prints '$'
    //let program = tag::Program::from_file(Path::new("simple_inf_loop.tag"))?;
    //let program = tag::Program::from_file(Path::new("shift.tag"))?;
    //let program = tag::Program::from_file(Path::new("big.tag"))?;
    //let program = tag::Program::from_file(Path::new("new.tag"))?; // pretty sure this is the eir -> wmc -> tag test (should print a char)

    let (trans, bin) = program.assemble()?;
    let mut out = File::create("out.bin")?;
    out.write_all(&bin)?;

    let mut up = File::create("up.dbg")?;
    for symbol in trans.keys() {
        up.write_fmt(format_args!("{} {:x}\n", symbol, trans[symbol]))?;
    }

    Ok(())
}
