// Copyright (c) 2020-2021 by the Zeek Project. See LICENSE for details.

#include <string>

#include <hilti/rt/logging.h>

#include <zeek-spicy/autogen/config.h>
#include <zeek-spicy/zeek-compat.h>
#include <zeek-spicy/zeek-reporter.h>

using namespace spicy::zeek;

void reporter::error(const std::string& msg) { ::zeek::reporter->Error("%s", msg.c_str()); }

void reporter::fatalError(const std::string& msg) { ::zeek::reporter->FatalError("%s", msg.c_str()); }

void reporter::warning(const std::string& msg) { ::zeek::reporter->Warning("%s", msg.c_str()); }

void reporter::internalError(const std::string& msg) { ::zeek::reporter->InternalError("%s", msg.c_str()); }

void reporter::weird(::zeek::Connection* conn, const std::string& msg) {
    if ( conn )
        ::zeek::reporter->Weird(conn, msg.c_str());
    else
        ::zeek::reporter->Weird(msg.c_str());
}

void reporter::weird(::zeek::file_analysis::File* f, const std::string& msg) {
    if ( f )
        ::zeek::reporter->Weird(f, msg.c_str());
    else
        ::zeek::reporter->Weird(msg.c_str());
}

void reporter::weird(const std::string& msg) { ::zeek::reporter->Weird(msg.c_str()); }

static std::unique_ptr<::zeek::detail::Location> _makeLocation(const std::string& location) {
    static std::set<std::string> filenames; // see comment below in parse_location

    auto parse_location = [](const auto& s) -> std::unique_ptr<::zeek::detail::Location> {
        // This is not so great; In the HILTI runtome we pass locations
        // around as string. To pass them to Zeek, we need to unsplit the
        // strings into file name and line number. Zeek also won't clean up
        // the file names, so we need to track them ourselves.
        auto x = hilti::rt::split(s, ":");
        if ( x[0].empty() )
            return nullptr;

        auto loc = std::make_unique<::zeek::detail::Location>();
        loc->filename = filenames.insert(std::string(x[0])).first->c_str(); // we retain ownership

        if ( x.size() >= 2 ) {
            auto y = hilti::rt::split(x[1], "-");
            if ( y.size() >= 2 ) {
                loc->first_line = std::stoi(std::string(y[0]));
                loc->last_line = std::stoi(std::string(y[1]));
            }
            else if ( y[0].size() )
                loc->first_line = loc->last_line = std::stoi(std::string(y[0]));
        }

        return loc;
    };

    if ( location.size() )
        return parse_location(location);
    else if ( auto hilti_location = hilti::rt::debug::location() )
        return parse_location(hilti_location);
    else
        return nullptr;
}

void reporter::analyzerError(::zeek::analyzer::Analyzer* a, const std::string& msg, const std::string& location) {
    // Zeek's AnalyzerError() prints a location, so set that.
    auto zeek_location = _makeLocation(location);
    ::zeek::reporter->PushLocation(zeek_location.get());
    ::zeek::reporter->AnalyzerError(a, "%s", msg.c_str());
    ::zeek::reporter->PopLocation();
}

void reporter::analyzerError(::zeek::file_analysis::Analyzer* a, const std::string& msg, const std::string& location) {
    // Zeek's AnalyzerError() prints a location, so set that.
    auto zeek_location = _makeLocation(location);
    ::zeek::reporter->PushLocation(zeek_location.get());

    // Zeek doesn't have an reporter error for file analyzers, so we log this
    // as a weird instead.
    if ( a && a->GetFile() )
        ::zeek::reporter->Weird(a->GetFile(), "file_error", msg.c_str());
    else
        ::zeek::reporter->Weird("file_error", msg.c_str());

    ::zeek::reporter->PopLocation();

    if ( a )
        a->SetSkip(1); // Imitate what AnalyzerError() does for protocol analyzers.
}

void reporter::analyzerError(::zeek::packet_analysis::Analyzer* a, const std::string& msg,
                             const std::string& location) {
    // Zeek's AnalyzerError() prints a location, so set that.
    auto zeek_location = _makeLocation(location);
    ::zeek::reporter->PushLocation(zeek_location.get());
    // Zeek doesn't have an reporter error for packet analyzers, so we log
    // this as a weird instead.
    ::zeek::reporter->Weird("packet_error", msg.c_str());
    ::zeek::reporter->PopLocation();
}

int reporter::numberErrors() { return ::zeek::reporter->Errors(); }
