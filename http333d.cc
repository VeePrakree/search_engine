/*
 * Copyright ©2023 Timmy Yang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Summer Quarter 2023 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <list>
#include <filesystem>
#include <sstream>

#include "./ServerSocket.h"
#include "./HttpServer.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::string;

// Print out program usage, and exit() with EXIT_FAILURE.
static void Usage(char* prog_name);

// Parse command-line arguments to get port, path, and indices to use
// for your http333d server.
//
// Params:
// - argc: number of argumnets
// - argv: array of arguments
// - port: output parameter returning the port number to listen on
// - path: output parameter returning the directory with our static files
// - indices: output parameter returning the list of index file names
//
// Calls Usage() on failure. Possible errors include:
// - path is not a readable directory
// - index file names are readable
static void GetPortAndPath(int argc,
                    char** argv,
                    uint16_t* const port,
                    string* const path,
                    list<string>* const indices);

int main(int argc, char** argv) {
  // Print out welcome message.
  cout << "Welcome to http333d, the UW cse333 web server!" << endl;
  cout << "  Copyright 2012 Steven Gribble" << endl;
  cout << "  http://www.cs.washington.edu/homes/gribble" << endl;
  cout << endl;
  cout << "initializing:" << endl;
  cout << "  parsing port number and static files directory..." << endl;

  // Ignore the SIGPIPE signal, otherwise we'll crash out if a client
  // disconnects unexpectedly.
  signal(SIGPIPE, SIG_IGN);

  // Get the port number and list of index files.
  uint16_t port_num;
  string static_dir;
  list<string> indices;
  GetPortAndPath(argc, argv, &port_num, &static_dir, &indices);
  cout << "    port: " << port_num << endl;
  cout << "    path: " << static_dir << endl;

  // Run the server.
  hw4::HttpServer hs(port_num, static_dir, indices);
  if (!hs.Run()) {
    cerr << "  server failed to run!?" << endl;
  }

  cout << "server completed!  Exiting." << endl;
  return EXIT_SUCCESS;
}


static void Usage(char* prog_name) {
  cerr << "Usage: " << prog_name << " port staticfiles_directory indices+";
  cerr << endl;
  exit(EXIT_FAILURE);
}

static void GetPortAndPath(int argc,
                    char** argv,
                    uint16_t* const port,
                    string* const path,
                    list<string>* const indices) {
  // Here are some considerations when implementing this function:
  // - There is a reasonable number of command line arguments
  // - The port number is reasonable
  // - The path (i.e., argv[2]) is a readable directory
  // - You have at least 1 index, and all indices are readable files

  // STEP 1:
  if (argc < 4) {
    cerr << "Incorrect number of command line arguments.\n";
    Usage(argv[0]);
  }

  std::stringstream pnum(argv[1]);
  uint16_t port_num;
  pnum >> port_num;
  if (pnum.bad()) {
    cerr << "Port number could not be recognized.\n";
    Usage(argv[0]);
  }
  // Highest and lowest possible port numbers.
  if (port_num > 65535 || port_num < 0) {
    cerr << "Port number is invalid.\n";
    Usage(argv[0]);
  }
  // 1024 is the lowest port that is not "well-known". We want to allow both
  // dynamic and potentially registered port numbers.
  if (port_num < 1024) {
    cerr << "Port number is registered; cannot be used.\n";
    Usage(argv[0]);
  }
  *port = port_num;

  if (!std::filesystem::is_directory(argv[2])) {
    cerr << "Path to static file directory is invalid.\n";
    Usage(argv[0]);
  }
  *path = argv[2];

  // Because of glob expansion, we can just get all the command-line args
  // after the 2nd.
  list<string> index_paths(argv + 3, argv + argc);
  list<string> vetted_paths;
  for (string path : index_paths) {
    if (!std::filesystem::is_regular_file(path)) {
      cerr << "Skipping index \"" << path << "\", path not recognized";
    } else {
      vetted_paths.push_back(path);
    }
  }
  *indices = vetted_paths;
}
