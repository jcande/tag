use std::collections::HashMap;
use std::collections::HashSet;
use std::fs::File;
use std::io::Read;
use std::path::Path;
use std::str::FromStr;
use std::fmt;


extern crate bincode;
use bincode::serialize;

extern crate failure;
#[macro_use] extern crate failure_derive;

extern crate pest;
use pest::Parser;
extern crate pest_derive;
use pest_derive::Parser;

extern crate serde;
#[macro_use] extern crate serde_derive;




const _GRAMMAR: &str = include_str!("tag.pest");

#[derive(Parser, Debug)]
#[grammar = "tag.pest"]
struct TagParser;

// XXX These errors are trash. Make them not trash, at least a tiny bit
#[derive(Debug, Fail)]
pub enum Err {
    #[fail(display = "{}", message)]
    GeneralError { message: String },

    // XXX let's find a way to get this working with those stupid lifetimes
/*
    #[fail(display = "pest error: {}", error)]
    PestError {
        error: pest::Error<'a, Rule>,
    },
*/

    #[fail(display = "{}", error)]
    NumParseIntError {
        error: std::num::ParseIntError,
    },

    #[fail(display = "{}", message)]
    IoError {
        message: String,
        backtrace: failure::Backtrace,
        #[cause]
        cause: std::io::Error,
    },

    #[fail(display = "Serde got sad: {}", error)]
    SerializeError {
        error: std::boxed::Box<bincode::ErrorKind>,
    },

    #[fail(display = "Invalid deletion number of {}. Must be >= 2", deletion_number)]
    InvalidDeletionNumber {
        deletion_number: u32,
    },

    #[fail(display = "Invalid queue length of {}. Must be >= deletion number.", queue_length)]
    InvalidQueueLength {
        queue_length: u32,
    },

    #[fail(display = "Duplicate start symbol {}.", start_symbol)]
    DuplicateStartSymbol {
        start_symbol: String,
    },

    #[fail(display = "There are more symbols than can fit in {} bytes.", boundary)]
    TooManySymbols {
        boundary: usize,
    },

    #[fail(display = "There were no symbols found")]
    EmptyProgram,

    #[fail(display = "Did the grammar change? Save the input file!")]
    ParseError,
}

impl Err {
    pub fn serial(error: std::boxed::Box<bincode::ErrorKind>) -> Err {
        Err::SerializeError {
            error: error
        }
    }
}

impl fmt::Display for Program {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.write_fmt(
            format_args!("deletion_number {{\n    {}\n}}\n\n",
                self.deletion_number
            )
        )?;

        fmt.write_fmt(
            format_args!("rules {{\n\n")
        )?;
        // XXX may want to sort rules so we can get all of the start_symbols in the proper
        // order
        for (start_symbol, appendant) in &self.rules {
            fmt.write_fmt(format_args!("{} -> ", start_symbol))?;

            match appendant {
                RuleStyle::Pure(appendant) => {
                    for symbol in appendant {
                        fmt.write_fmt(format_args!("{} ", symbol))?;
                    }
                },
                RuleStyle::Input(app_f, app_t) => {
                    fmt.write_str("{ ")?;
                    for symbol in app_f {
                        fmt.write_fmt(format_args!("{} ", symbol))?;
                    }
                    fmt.write_str(" ; ")?;
                    for symbol in app_t {
                        fmt.write_fmt(format_args!("{} ", symbol))?;
                    }
                    fmt.write_str(" } ")?;
                },
                RuleStyle::Output(appendant, bit) => {
                    let bit = if *bit {
                        1
                    } else {
                        0
                    };
                    fmt.write_fmt(format_args!("{}: ", bit))?;
                    for symbol in appendant {
                        fmt.write_fmt(format_args!("{} ", symbol))?;
                    }
                },
            };
            fmt.write_str("\n")?;
        }
        fmt.write_str("\n}\n\n")?;

        fmt.write_str("queue {\n    ")?;
        for symbol in self.queue.iter() {
            fmt.write_fmt(format_args!("{} ", symbol))?;
        }
        fmt.write_str("\n}\n")?;

        Ok(())
    }
}


/*
impl ser::Error for Err {
    fn custom<T: std::fmt::Display>(msg: T) -> Self {
        Err::GeneralError {
            message: msg.to_string()
        }
    }
}

impl std::error::Error for Err {
    fn description(&self) -> &str {
        match *self {
            _ => "error",
        }
    }
}
*/

impl From<std::io::Error> for Err {
    fn from(error: std::io::Error) -> Err {
        Err::IoError {
            message: "IO Error".to_string(),
            backtrace: failure::Backtrace::new(), //oh god how do I do this?
            cause: error,
        }
    }
}

pub type Symbol = String;
pub type Series = Vec<Symbol>;
pub type Rules = HashMap<Symbol, RuleStyle>;

#[derive(Debug,Clone)]
pub enum RuleStyle {
    Pure(Series),    // appendant
    Input(Series, Series),    // appendant0, appendant1
    Output(Series, bool),    // appendant, output_bit
}

enum RuleStyleReduced {
    Pure,
    Input,
    Output,
}
pub struct AppendantBuilder {
    style: RuleStyleReduced,
    output_bit: bool,
    // This is used by both Output and Pure styles
    appendant: Series,
    appendant0: Series,
    appendant1: Series,
}
impl AppendantBuilder {
    fn minimal_struct(style: RuleStyleReduced) -> AppendantBuilder {
        AppendantBuilder {
            style: style,
            output_bit: false,
            appendant: Vec::new(),
            appendant0: Vec::new(),
            appendant1: Vec::new(),
        }
    }
    pub fn input_rule() -> AppendantBuilder {
        Self::minimal_struct(RuleStyleReduced::Input)
    }

    pub fn output_rule() -> AppendantBuilder {
        Self::minimal_struct(RuleStyleReduced::Output)
    }

    pub fn pure_rule() -> AppendantBuilder {
        Self::minimal_struct(RuleStyleReduced::Pure)
    }

    pub fn with_output(mut self, bit: bool) -> Self {
        self.output_bit = bit;
        self
    }

    pub fn with_appendant(mut self, appendant: Series) -> Self {
        self.appendant = appendant;
        self
    }

    pub fn with_false_appendant(mut self, appendant0: Series) -> Self {
        self.appendant0 = appendant0;
        self
    }

    pub fn with_true_appendant(mut self, appendant1: Series) -> Self {
        self.appendant1 = appendant1;
        self
    }

    pub fn finalize(self) -> RuleStyle {
        // XXX Should this return result? I think panic'ing here is ok but
        // maybe not.
        match self.style {
            RuleStyleReduced::Pure =>
                RuleStyle::Pure(self.appendant),
            RuleStyleReduced::Input =>
                RuleStyle::Input(self.appendant0, self.appendant1),
            RuleStyleReduced::Output =>
                RuleStyle::Output(self.appendant, self.output_bit),
        }
    }
}

// rule!(x -> a b c)
#[macro_export]
macro_rules! rule {
    // Output rule
    // XXX figure out how to combine these bastards
    ($prog:ident , $symbol:ident -> 0: $($app:expr)*) => {{
        let mut app_vec = Vec::new();
        $(
            app_vec.push($app.to_owned());
        )*
        let appendant = tag::AppendantBuilder::output_rule()
                        .with_output(false)
                        .with_appendant(app_vec)
                        .finalize();
        $prog.insert($symbol.to_owned(), appendant);
    }};
    ($prog:ident , $symbol:ident -> 1: $($app:expr)*) => {{
        let mut app_vec = Vec::new();
        $(
            app_vec.push($app.to_owned());
        )*
        let appendant = tag::AppendantBuilder::output_rule()
                        .with_output(true)
                        .with_appendant(app_vec)
                        .finalize();
        $prog.insert($symbol.to_owned(), appendant);
    }};

    // Input rule
    ($prog:ident , $symbol:ident -> { $($app0:expr)* ; $($app1:expr)* }) => {{
        let mut app0_vec = Vec::new();
        $(
            app0_vec.push($app0.to_owned());
        )*
        let mut app1_vec = Vec::new();
        $(
            app1_vec.push($app1.to_owned());
        )*
        let appendant = tag::AppendantBuilder::input_rule()
                        .with_false_appendant(app0_vec)
                        .with_true_appendant(app1_vec)
                        .finalize();

        $prog.insert($symbol.to_owned(), appendant)
    }};

    // Pure rule
    ($prog:ident , $symbol:ident -> $($app:expr)*) => {{
        let mut app_vec = Vec::new();
        $(
            app_vec.push($app.to_owned());
        )*
        let appendant = tag::RuleStyle::Pure(app_vec);

        $prog.insert($symbol.to_owned(), appendant)
    }};

}


pub enum BinRuleStyle {
    Pure = 2,
    Input = 1,
    Output = 0,
}

pub type BinSymbol = u64;

#[derive(Debug)]
pub struct Program {
    deletion_number: u32,
    rules: Rules,
    queue: Series,
}

impl FromStr for Program {
    type Err = failure::Error;

    //
    // Read tag.pest to understand why this code does what it does. We
    // essentially start with the rule named "system" and descend to each
    // sub-rule to extract the data we care about.
    //

    fn from_str(unparsed: &str) -> Result<Program, failure::Error> {
        let mut system = TagParser::parse(Rule::system, &unparsed)
                            .map_err(|e| Err::GeneralError {
                                message: format!("Pest Error: {}", e)
                            })?;

        let system = system.next().ok_or(Err::ParseError)?;
        if system.as_rule() != Rule::system {
            Err(Err::ParseError)?;
        }
        let mut system = system.into_inner();

        let next_rule = system.next().ok_or(Err::ParseError)?;
        if next_rule.as_rule() != Rule::deletion_number {
            Err(Err::ParseError)?;
        }

        // into_inner() peels the onion, gives us an iterator because we can walk over ALL of the subfields.
        // next pulls apart the iterator and gives us the first (and in this case only) value
        let deletion_number = next_rule
                                .into_inner()
                                .next()
                                .ok_or(Err::ParseError)?
                                .as_str()
                                .parse::<u32>()
                                .map_err(|e| Err::NumParseIntError {
                                    error: e
                                })?;

        if deletion_number < 2 {
            Err(Err::InvalidDeletionNumber {
                deletion_number: deletion_number
            })?;
        }

        let next_rule = system.next().ok_or(Err::ParseError)?;
        if next_rule.as_rule() != Rule::rules {
            Err(Err::ParseError)?;
        }
        let rules = next_rule;
        let mut tag_rules: Rules = HashMap::new();
        for rule in rules.into_inner() {
            let mut rule = rule.into_inner();

            let start_symbol = rule.next().ok_or(Err::ParseError)?.as_str();

            let style = rule.next().ok_or(Err::ParseError)?;
            let body = match style.as_rule() {
                Rule::pure_rule => {
                    let pure = style.into_inner()
                        .next()
                        .ok_or(Err::ParseError)?
                        .into_inner()
                        .map(|s| s.as_str().to_owned())
                        .collect::<Vec<_>>();

                    RuleStyle::Pure(pure)
                },
                Rule::input_rule => {
                    let mut in_rule = style.into_inner();
                    let app0 = in_rule
                        .next()
                        .ok_or(Err::ParseError)?
                        .into_inner()
                        .map(|s| s.as_str().to_owned())
                        .collect::<Vec<_>>();
                    let app1 = in_rule
                        .next()
                        .ok_or(Err::ParseError)?
                        .into_inner()
                        .map(|s| s.as_str().to_owned())
                        .collect::<Vec<_>>();

                    RuleStyle::Input(app0, app1)
                },
                Rule::output_rule => {
                    let mut out_rule = style.into_inner();
                    let bit = out_rule
                        .next()
                        .ok_or(Err::ParseError)?
                        .as_str()
                        .parse::<u32>()
                        .map_err(|e| Err::NumParseIntError {
                            error: e
                        })?;
                    let bit = bit != 0;
                    let app = out_rule
                        .next()
                        .ok_or(Err::ParseError)?
                        .into_inner()
                        .map(|s| s.as_str().to_owned())
                        .collect::<Vec<_>>();

                    RuleStyle::Output(app, bit)
                },
                _ => Err(Err::ParseError)?
            };

            if tag_rules.contains_key(start_symbol) {
                Err(Err::DuplicateStartSymbol {
                    start_symbol: start_symbol.to_owned()
                })?;
            }
            tag_rules.insert(start_symbol.to_owned(), body);
        }

        let next_rule = system.next().ok_or(Err::ParseError)?;
        if next_rule.as_rule() != Rule::queue {
            Err(Err::ParseError)?;
        }
        let queue = next_rule;

        let queue_data = queue.into_inner().next().ok_or(Err::ParseError)?;
        let tag_queue = queue_data
            .into_inner()
            .map(|s| s.as_str().to_owned())
            .collect::<Vec<_>>();
        if (tag_queue.len() as u32) < deletion_number {
            Err(Err::InvalidQueueLength {
                queue_length: tag_queue.len() as u32
            })?;
        }

        Ok(Program {
            deletion_number: deletion_number,
            rules: tag_rules,
            queue: tag_queue,
        })
    }
}

impl Program {
	pub fn from_components(deletion_number: u32, rules: Rules, queue: Series) -> Result<Program, failure::Error> {
        if deletion_number < 2 {
            Err(Err::InvalidDeletionNumber {
                deletion_number: deletion_number
            })?;
        }

        if (queue.len() as u32) < deletion_number {
            Err(Err::InvalidQueueLength {
                queue_length: queue.len() as u32
            })?;
        }

        Ok(Program {
            deletion_number: deletion_number,
            rules: rules,
            queue: queue,
        })
	}

    pub fn from_file(filename: &Path) -> Result<Program, failure::Error> {
        let mut unparsed_file = String::new();
        File::open(filename)?
            .read_to_string(&mut unparsed_file)?;

        Program::from_str(&unparsed_file)
    }

    fn make_symbol_map(&self) -> HashMap<Symbol, BinSymbol> {

        //
        // Grab all unique symbols
        //

        let mut all_symbols = HashSet::new();
        for start_symbol in self.rules.keys() {
            all_symbols.insert(start_symbol.to_owned());
        }
        for series in self.rules.values() {
            match series {
                RuleStyle::Pure(appendant) => {
                    for symbol in appendant {
                        all_symbols.insert(symbol.to_owned());
                    }
                },
                RuleStyle::Input(appendant0, appendant1) => {
                    for symbol in appendant0 {
                        all_symbols.insert(symbol.to_owned());
                    }

                    for symbol in appendant1 {
                        all_symbols.insert(symbol.to_owned());
                    }
                },
                RuleStyle::Output(appendant, _) => {
                    for symbol in appendant {
                        all_symbols.insert(symbol.to_owned());
                    }
                },
            };
        }
        for symbol in self.queue.iter() {
            all_symbols.insert(symbol.to_owned());
        }


        //
        // Now give each symbol name a corresponding numeric value
        //

        let mut translation_map = HashMap::new();
        for (i, symbol) in all_symbols.iter().enumerate() {
            translation_map.insert(symbol.to_owned(), i as u64);
        }

        translation_map
    }

    fn calculate_byte_boundary(elements: usize) -> usize {
        let mut highest_bit = 0;
        while (elements >> highest_bit) > 0 {
            highest_bit += 1
        }

        //
        // Round up to next byte-boundary because we want to know how many
        // bytes it take to represent elements.
        //
        let bits_in_byte = 8;
        ((highest_bit + bits_in_byte - 1) / bits_in_byte)
    }

    // XXX we should probably be a bit stricter with checking for int overflows
    // This should probably only fail if we run out of memory, yolo
    pub fn assemble(&self) -> Result<(HashMap<Symbol, BinSymbol>, Vec<u8>), failure::Error> {
        let trans = self.make_symbol_map();
        let boundary = Program::calculate_byte_boundary(trans.len());
        // XXX check for overflow?
        let queue_size = self.queue.len() * boundary;
        let mut bin = Vec::new();

        if boundary == 0 {
            Err(Err::EmptyProgram)?;
        }

        #[derive(Serialize, Debug)]
        struct BinHeader {
            rule_count: u64,
            symbol_size: u32, // size in bytes per symbol (1..8)
            queue_size: u32, // size in bytes of the queue
            deletion_number: u32,
        };
        let mut header = serialize(&BinHeader {
            rule_count: self.rules.keys().len() as u64,
            symbol_size: boundary as u32,
            queue_size: queue_size as u32,
            deletion_number: self.deletion_number,
        }).map_err(Err::serial)?;
        bin.append(&mut header);

        fn serialize_symbol(symbol: &Symbol,
                            boundary: usize,
                            trans: &HashMap<Symbol, BinSymbol>)
                            -> Result<Vec<u8>, Err>
        {
            let symbol = trans[symbol];
            let mut symbol = match boundary {
                1 ... 8 => serialize(&(symbol as u64))
                    .map_err(Err::serial)?,
                _ => Err(Err::TooManySymbols {
                        boundary: boundary,
                    })?
            };
            symbol.truncate(boundary);
            Ok(symbol)
        }

        for start_symbol in self.rules.keys() {
            match &self.rules[start_symbol] {
                RuleStyle::Pure(appendant) => {
                    let mut style = serialize(&(BinRuleStyle::Pure as u8))
                        .map_err(Err::serial)?;
                    let mut appendant_len = serialize(&((appendant.len() * boundary) as u16))
                        .map_err(Err::serial)?;

                    //
                    // Header
                    //
                    bin.append(&mut style);
                    bin.append(&mut appendant_len);

                    //
                    // Symbol
                    //
                    bin.append(&mut serialize_symbol(start_symbol, boundary, &trans)?);

                    //
                    // Appendants
                    //
                    for symbol in appendant {
                        bin.append(&mut serialize_symbol(symbol, boundary, &trans)?);
                    }
                },
                RuleStyle::Input(appendant0, appendant1) => {
                    let mut style = serialize(&(BinRuleStyle::Input as u8))
                        .map_err(Err::serial)?;
                    let mut appendant0_len = serialize(&((appendant0.len() * boundary) as u16))
                        .map_err(Err::serial)?;
                    let mut appendant1_len = serialize(&((appendant1.len() * boundary) as u16))
                        .map_err(Err::serial)?;

                    //
                    // Header
                    //
                    bin.append(&mut style);
                    bin.append(&mut appendant0_len);
                    bin.append(&mut appendant1_len);

                    //
                    // Symbol
                    //
                    bin.append(&mut serialize_symbol(start_symbol, boundary, &trans)?);

                    //
                    // Appendants
                    //
                    for symbol in appendant0 {
                        bin.append(&mut serialize_symbol(symbol, boundary, &trans)?);
                    }
                    for symbol in appendant1 {
                        bin.append(&mut serialize_symbol(symbol, boundary, &trans)?);
                    }
                },
                RuleStyle::Output(appendant, bit) => {
                    let mut style = serialize(&(BinRuleStyle::Output as u8))
                        .map_err(Err::serial)?;
                    let mut appendant_len = serialize(&((appendant.len() * boundary) as u16))
                        .map_err(Err::serial)?;

                    //
                    // Header
                    //
                    bin.append(&mut style);
                    bin.append(&mut appendant_len);

                    //
                    // Symbol
                    //
                    bin.append(&mut serialize_symbol(start_symbol, boundary, &trans)?);

                    //
                    // Appendants
                    //
                    for symbol in appendant {
                        bin.append(&mut serialize_symbol(symbol, boundary, &trans)?);
                    }

                    if *bit {
                        bin.append(&mut serialize(&(1 as u8))
                                   .map_err(Err::serial)?);
                    } else {
                        bin.append(&mut serialize(&(0 as u8))
                                   .map_err(Err::serial)?);
                    }
                },
            };
        }

        for symbol in self.queue.iter() {
            bin.append(&mut serialize_symbol(symbol, boundary, &trans)?);
        }

        Ok((trans, bin))
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}
