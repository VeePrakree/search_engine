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

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;
using boost::algorithm::split;

namespace hw4 {

static const char* kHeaderEnd = "\r\n\r\n",
                 * kAllowedMethod = "GET",
                 * kLineDelim     = "\r\n",
                 * kKeyValueDelim = ": ";
static const int kHeaderEndLen = 4;

bool HttpConnection::GetNextRequest(HttpRequest* const request) {
  // Use WrappedRead from HttpUtils.cc to read bytes from the files into
  // private buffer_ variable. Keep reading until:
  // 1. The connection drops
  // 2. You see a "\r\n\r\n" indicating the end of the request header.
  //
  // Hint: Try and read in a large amount of bytes each time you call
  // WrappedRead.
  //
  // After reading complete request header, use ParseRequest() to parse into
  // an HttpRequest and save to the output parameter request.
  //
  // Important note: Clients may send back-to-back requests on the same socket.
  // This means WrappedRead may also end up reading more than one request.
  // Make sure to save anything you read after "\r\n\r\n" in buffer_ for the
  // next time the caller invokes GetNextRequest()!

  // STEP 1:
  constexpr size_t kBufSize = 1024;
  // A char array - we are getting actual text, not just bytes
  char buf[kBufSize];
  int num_read = 0;
  do {
    buf[num_read] = '\0';
    buffer_ += buf;
    // We need to check buffer_ here instead of checking buf (or a string
    // thereby created) in the case that buffer_ already has the end of
    // an HTTP request from a previous read
    size_t end_index = buffer_.find(kHeaderEnd);
    if (end_index != string::npos) {
      size_t start_index = buffer_.find(kAllowedMethod);
      if (start_index == string::npos) return false;
      HttpRequest candidate = ParseRequest(
        EscapeHtml(string(buffer_, start_index, end_index - start_index)));
      string newbuf = string(buffer_, end_index + kHeaderEndLen);
      buffer_ = newbuf;
      *request = candidate;
      return true;
    }
    num_read = WrappedRead(fd_, reinterpret_cast<uint8_t*>(buf), kBufSize - 1);
  } while (num_read > 0);

  return false;  // You may want to change this.
}

bool HttpConnection::WriteResponse(const HttpResponse& response) const {
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         reinterpret_cast<const unsigned char*>(str.c_str()),
                         str.length());
  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string& request) const {
  HttpRequest req("/");  // by default, get "/".

  // Plan for STEP 2:
  // 1. Split the request into different lines (split on "\r\n").
  // 2. Extract the URI from the first line and store it in req.URI.
  // 3. For the rest of the lines in the request, track the header name and
  //    value and store them in req.headers_ (e.g. HttpRequest::AddHeader).
  //
  // Hint: Take a look at HttpRequest.h for details about the HTTP header
  // format that you need to parse.
  //
  // You'll probably want to look up boost functions for:
  // - Splitting a string into lines on a "\r\n" delimiter
  // - Trimming whitespace from the end of a string
  // - Converting a string to lowercase.
  //
  // Note: If a header is malformed, skip that line.

  // STEP 2:

  string to_parse = request;
  boost::algorithm::trim_right(to_parse);
  vector<string> result;

  size_t start = 0, end = 0;

  static const int kLineDelimLen = strlen(kLineDelim);
  while ((end = to_parse.find(kLineDelim, start)) != std::string::npos) {
      result.push_back(to_parse.substr(start, end - start));
      start = end + kLineDelimLen;
  }
  result.push_back(to_parse.substr(start));

  auto it = result.begin();

  vector<string> first_line;
  split(first_line, *it, boost::is_any_of(" \t\n"));
  req.set_uri(first_line[1]);

  while (++it != result.end()) {
    string line = *it;
    // Have to use this instead of split because the header body could contain
    // instances of the key value delimiter ": "
    size_t index = line.find(kKeyValueDelim);
    if (index != string::npos) {
      string header = boost::algorithm::to_lower_copy(string(line, 0, index));
      string body = string(line, index + 2);
      req.AddHeader(header, body);
    }
  }
  return req;
}

}  // namespace hw4
