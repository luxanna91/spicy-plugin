// Copyright (c) 2020-2021 by the Zeek Project. See LICENSE for details.

#include <compiler/debug.h>
#include <compiler/driver.h>
#include <getopt.h>

#include <hilti/base/result.h>
#include <hilti/base/util.h>

#include <zeek-spicy/autogen/config.h>

const ::hilti::logging::DebugStream ZeekPlugin("zeek");

void ::spicy::zeek::debug::do_log(const std::string& msg) { HILTI_DEBUG(ZeekPlugin, std::string(msg)); }

static struct option long_driver_options[] = {{"abort-on-exceptions", required_argument, nullptr, 'A'},
                                              {"show-backtraces", required_argument, nullptr, 'B'},
                                              {"compiler-debug", required_argument, nullptr, 'D'},
                                              {"debug", no_argument, nullptr, 'd'},
                                              {"debug-addl", required_argument, nullptr, 'X'},
                                              {"dump-code", no_argument, nullptr, 'C'},
                                              {"help", no_argument, nullptr, 'h'},
                                              {"keep-tmps", no_argument, nullptr, 'T'},
                                              {"library-path", required_argument, nullptr, 'L'},
                                              {"optimize", no_argument, nullptr, 'O'},
                                              {"output", required_argument, nullptr, 'o'},
                                              {"output-c++", required_argument, nullptr, 'c'},
                                              {"print-zeek-config", no_argument, nullptr, 'z'},
                                              {"print-module-path", no_argument, nullptr, 'M'},
                                              {"report-times", required_argument, nullptr, 'R'},
                                              {"version", no_argument, nullptr, 'v'},
                                              {"version-number", no_argument, nullptr, 'V'},
                                              {nullptr, 0, nullptr, 0}};

static void usage() {
    std::cerr << "Usage: spicyz [options] <inputs>\n"
                 "\n"
                 "  -c | --output-c++ <prefix>      Output generated C++ code.\n"
                 "  -d | --debug                    Include debug instrumentation into generated code.\n"
                 "  -o | --output-to <path>         Path for saving output.\n"
                 "  -v | --version                  Print version information.\n"
                 "  -z | --print-zeek-config        Print path to zeek-config.\n"
                 "  -A | --abort-on-exceptions      When executing compiled code, abort() instead of throwing HILTI "
                 "exceptions.\n"
                 "  -B | --show-backtraces          Include backtraces when reporting unhandled exceptions.\n"
                 "  -C | --dump-code                Dump all generated code to disk for debugging.\n"
                 "  -D | --compiler-debug <streams> Activate compile-time debugging output for given debug streams "
                 "(comma-separated; 'help' for list).\n"
                 "  -L | --library-path <path>      Add path to list of directories to search when importing modules.\n"
                 "  -M | --print-module-path        Print the Zeek plugin's default module search path.\n"
                 "  -O | --optimize                 Build optimized release version of generated code.\n"
                 "  -R | --report-times             Report a break-down of compiler's execution time.\n"
                 "  -T | --keep-tmps                Do not delete any temporary files created.\n"
                 "  -X | --debug-addl <addl>        Implies -d and adds selected additional instrumentation "
                 "(comma-separated; see 'help' for list).\n"
                 "\n"
                 "Inputs can be *.spicy, *.evt, *.hlt, .cc/.cxx\n"
                 "\n";
}

using hilti::Nothing;

static hilti::Result<Nothing> parseOptions(int argc, char** argv, hilti::driver::Options* driver_options,
                                           hilti::Options* compiler_options) {
    while ( true ) {
        int c = getopt_long(argc, argv, "ABc:CdX:D:L:Mo:ORTvhz", long_driver_options, nullptr);

        if ( c == -1 )
            break;

        switch ( c ) {
            case 'A': driver_options->abort_on_exceptions = true; break;

            case 'B': driver_options->show_backtraces = true; break;

            case 'c':
                driver_options->output_cxx = true;
                driver_options->output_cxx_prefix = optarg;
                driver_options->execute_code = false;
                break;

            case 'C': {
                driver_options->dump_code = true;
                break;
            }

            case 'd': {
                compiler_options->debug = true;
                break;
            }

            case 'X': {
                auto arg = std::string(optarg);

                if ( arg == "help" ) {
                    std::cerr << "Additional debug instrumentation:\n";
                    std::cerr << "   flow:     log function calls to debug stream \"hilti-flow\"\n";
                    std::cerr << "   location: track current source code location for error reporting\n";
                    std::cerr << "   trace:    log statements to debug stream \"hilti-trace\"\n";
                    std::cerr << "\n";
                    exit(0);
                }

                compiler_options->debug = true;

                if ( auto r = compiler_options->parseDebugAddl(arg); ! r )
                    return hilti::result::Error("nothing to do");

                break;
            }

            case 'D': {
                auto arg = std::string(optarg);

                if ( arg == "help" ) {
                    std::cerr << "Debug streams:\n";

                    for ( const auto& s : hilti::logging::DebugStream::all() )
                        std::cerr << "  " << s << "\n";

                    std::cerr << "\n";
                    return Nothing();
                }

                for ( const auto& s : hilti::util::split(arg, ",") ) {
                    if ( ! driver_options->logger->debugEnable(s) )
                        return hilti::result::Error(
                            hilti::util::fmt("Unknown debug stream '%s', use 'help' for list", arg));
                }

                break;
            }

            case 'L': compiler_options->library_paths.emplace_back(std::string(optarg)); break;

            case 'M': std::cout << spicy::zeek::configuration::PluginModuleDirectory << std::endl; return Nothing();

            case 'o': driver_options->output_path = std::string(optarg); break;

            case 'O': compiler_options->optimize = true; break;

            case 'R': driver_options->report_times = true; break;

            case 'T': driver_options->keep_tmps = true; break;

            case 'v': std::cout << spicy::zeek::configuration::PluginVersion << std::endl; return Nothing();

            case 'V': std::cout << spicy::zeek::configuration::PluginVersionNumber << std::endl; return Nothing();

            case 'z': std::cout << spicy::zeek::configuration::ZeekConfig << std::endl; return Nothing();

            case 'h': usage(); return Nothing();

            default: usage(); return hilti::result::Error("could not parse options");
        }
    }

    while ( optind < argc )
        driver_options->inputs.emplace_back(argv[optind++]);

    if ( driver_options->inputs.empty() )
        return hilti::result::Error("no input file given");

    if ( driver_options->output_path.empty() && ! driver_options->output_cxx )
        return hilti::result::Error("no output file for object code given, use -o <file>.hlto");

    return Nothing();
}

int main(int argc, char** argv) {
    auto plugin_path = hilti::util::currentExecutable().parent_path().parent_path();
    spicy::zeek::Driver driver("", plugin_path, spicy::zeek::configuration::ZeekVersionNumber);

    hilti::driver::Options driver_options;
    driver_options.execute_code = true;
    driver_options.include_linker = true;

    auto compiler_options = driver.hiltiOptions();

    if ( auto rc = parseOptions(argc, argv, &driver_options, &compiler_options); ! rc ) {
        hilti::logger().error(rc.error().description());
        return 1;
    }

    driver.setDriverOptions(std::move(driver_options));
    driver.setCompilerOptions(std::move(compiler_options));
    driver.initialize();

    for ( auto p : driver.driverOptions().inputs ) {
        if ( auto rc = driver.loadFile(p); ! rc ) {
            hilti::logger().error(rc.error().description());
            return 1;
        }
    }

    if ( auto rc = driver.compile(); ! rc ) {
        hilti::logger().error(rc.error().description());

        if ( rc.error().context().size() )
            hilti::logger().error(rc.error().context());

        return 1;
    }

    return 0;
}
