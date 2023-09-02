# Bug 1

## A) How is your program acting differently than you expect it to?
- When testing ServerSocket::Accept, it always displays that the server dns name
  resolves to the wildcard ip :: (which seems like it comes from the server dns
  name not being found).

## B) Brainstorm a few possible causes of the bug
- Incorrectly getting the server sockaddr_storage struct from getsockname
- Wrong arguments to getnameinfo (do we need specific flags?)
- Bad code duplication (the code for client and server is duplicated right now
  and it's likely there's at least one place where we are using a variable that
  refers to client)

## C) How you fixed the bug and why the fix was necessary
- First, we refactored ServerSocket::Accept into two functions, one to get the IP
  address of a sockaddr_storage and one to get the DNS name. This solved the code
  duplication variable reference issues (there were, in fact, several of those).
  After fiddling with arguments to getnameinfo for a while, we then tried looking
  into getsockname. Ultimately, we were getting the name of the socket which was
  to listen for clients rather than the client socket file descriptor. We're still
  not entirely sure why using the client socket file descriptor in getsockname
  doesn't return the client DNS name, but it certainly does return localhost as a
  DNS name where using the listener file descriptor just did not (potentially it
  itself was unbound).


# Bug 2

## A) How is your program acting differently than you expect it to?
- When testing HTTPConnection::ParseRequest, the string is parsing irregularly -
  sometimes header values will have random data in the middle or at the end.

## B) Brainstorm a few possible causes of the bug
- Handling read in such a way that the multiple threads conflict? (Not exactly sure
  how this would come about, but generally concurrency issues caused by improper use
  of WrappedRead)
- Not correctly resetting the buffer each time after parsing a request (maybe not
  including data or overwriting it somehow)
- Out-of-bounds read from the WrappedRead buffer causing mystery data

## C) How you fixed the bug and why the fix was necessary
- In the end, the problem was the great enemy of C: null-terminated strings. We were
  not explicitly null-terminating strings in the buffer after WrappedRead, and because
  of the way the method was written (as a do-while loop), this caused random cruft to
  accumulate at the beginning of the buffer which would get added to the string on
  every loop. This caused random mystery data to appear within header values or at the
  end of header values.


# Bug 3

## A) How is your program acting differently than you expect it to?
- For results in the search engine, clicking on external links (eg - to Wikipedia) is
  fine, but clicking on filepaths brings us to back to the home page of the search
  engine instead of that given file. 

## B) Brainstorm a few possible causes of the bug
- Only including the relative path in the href link to the document instead of the absolute
  path (perhaps href requires the absolute path?)
- The document name only includes the name of the document itself and not any path references at all
- Something else we don't understand about the href tag and the way that it accepts document
  paths vs links to http sites

## C) How you fixed the bug and why the fix was necessary
- To get the links to the files working, we added in the /static/ prefix before the 
  path in the href tag to the file to indicate that they are located in a static directory. However, this didn't
  work for the http:// links to Wikipedia and the like, so we only added this keyword for file paths
  by parsing the start of the doc_name using boost (seeing if it starts with http:// or https://). 
