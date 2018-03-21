use std::fs;
use std::path::Path;
#[allow(unused_imports)] use std::io::Read;
use std::io::Error as IoError;
use clap::{App, AppSettings, Arg, ArgMatches};
use failure::Error;
use console::CliError;
use console::bindings;

#[derive(Debug, Fail)]
#[fail(display = "Errors that occur while generating bindings")]
enum BindingError {
    /// Indicates that an io::Error occurred involving a given file.
    /// _0: The name of the file involved.
    /// _1: The io::Error with details.
    #[fail(display = "File error for '{}': {}", _0, _1)]
    FileError(String, IoError),
}

pub fn make_subcommand<'a, 'b>() -> App<'a, 'b> {
    App::new("generate")
        .settings(&[
            AppSettings::ArgRequiredElseHelp,
            AppSettings::DeriveDisplayOrder,
            AppSettings::ColoredHelp,
        ])
        .alias("gen")
        .about("Generate Flipper language bindings")
        .before_help("Generate bindings for the specified module")
        .arg(Arg::with_name("directory")
            .index(1)
            .takes_value(true)
            .required(true)
            .help("The name of output directory"))
        .arg(Arg::with_name("file")
            .index(2)
            .takes_value(true)
            .required(true)
            .help("The module file to generate language bindings for"))
}

pub fn execute(args: &ArgMatches) -> Result<(), Error> {
    let filename = args.value_of("file").unwrap();
    let path = Path::new(args.value_of("directory").unwrap());

    let mut file = fs::File::open(filename).map_err(|e| {
        BindingError::FileError(filename.to_owned(), e)
    })?;

    let module_binary = {
        let mut v = Vec::new();
        let _ = file.read_to_end(&mut v);
        v
    };

    fs::create_dir_all(path)?;

    let modules = bindings::Module::parse(&module_binary)?;

    for module in modules {
        let mut out = fs::File::create(path.join(&module.name)).map_err(|e|{
            BindingError::FileError("Failed to create file".to_owned(), e)
        })?;

        bindings::generators::c::generate_module(&module, &mut out)?;
    };

    Ok(())
}
