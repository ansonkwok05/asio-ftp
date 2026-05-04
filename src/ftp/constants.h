#pragma once

#include <cstddef>
#include <array>
#include <string_view>

enum class CONNECTION_STAGE
{
    UNAUTHENTICATED,
    LOGGED_IN
};

// buffer size in Bytes for receiving ftp messages
constexpr size_t MESSAGE_BUFFER_SIZE = 512;

// buffer size in Bytes for receiving files
constexpr size_t RECEIVE_BUFFER_SIZE = 1024 * 512;

// buffer size in Bytes for sending files
constexpr size_t SEND_BUFFER_SIZE = 1024 * 512;

static constexpr int DATA_CHANNEL_BEGIN_PORT = 7000;

// list of commands supported
static constexpr std::array<std::string_view, 20> FTP_COMMANDS = {
    // Authentication username
    "USER",

    // Authentication password
    "PASS",

    // Return system type
    "SYST",

    // Disconnect.
    "QUIT",

    // Print working directory. Returns the current directory of the host.
    "PWD",

    // Sets the transfer mode (ASCII/Binary).
    "TYPE",

    // Enter passive mode.
    "PASV",

    // Returns information of a file or directory if specified, else information of the current working
    // directory is returned.
    "LIST",

    // Get the feature list implemented by the server.
    "FEAT",

    // Change to Parent Directory.
    "CDUP",

    // Change working directory.
    "CWD",

    // Make directory.
    "MKD",

    // Remove a directory.
    "RMD",

    // Delete file.
    "DELE",

    // Accept the data and to store the data as a file at the server site
    "STOR",

    // Retrieve a copy of the file
    "RETR",

    // Return the size of a file.
    "SIZE",

    // Protection Buffer Size
    "PBSZ",

    // Data Channel Protection Level.
    "PROT",

    // Select options for a feature (for example OPTS UTF8 ON).
    "OPTS",
};