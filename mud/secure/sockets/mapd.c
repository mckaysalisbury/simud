#include "socket.h"

#define Debug(x)


int sock;
//mapping post_requests;
mapping incoming_requests;
mixed *hex_table;

static string parse_request(string file);
static void listen_call(int fd, int act, mixed a, mixed b);
static void callback(int fd, int act, mixed a, mixed b);

void create()
{
    int i, j;

    sock=SOCKETD->socket_listen(4250, #'listen_call);
    write("mapd started.\n");
    incoming_requests = ([ ]);

}

static void listen_call(int fd, int act, mixed a, mixed b)
{
    debug(sprintf("listen_call(%d,%d,%O,%O)\n",fd,act,a,b));
    switch(act) {
      case SOCKET_READY:
   return;
      case SOCKET_ACCEPT:
   SOCKETD->socket_accept(fd, #'callback);
   return;
      case SOCKET_ERROR:
   sock=-1;
      case SOCKET_CLOSE:
   destruct(this_object());
   write("MAPD daemon failed to start -- unable to open socket on port 4250!\n");
   return;
    }
}

/// Convert text in a URL format into an array of fields
string *unhash_url( string url_text ) {
   string onechar, str, *arr;
   int i;

   if( !stringp(url_text) ) return ({ });

   str = implode( explode(url_text, "+"), " ");
   str = implode( explode(str, "&"), "\255");

   arr = explode( str, "%" );
   str = arr[0];

   for( i = 1; i < sizeof(arr); i++ ) {
      onechar = " ";
      if( hex_table[arr[i][0]][arr[i][1]] <= 0 ||
          hex_table[arr[i][0]][arr[i][1]] > 255 ) debug("At arr[i][0..1] = "+arr[i][0..1]+" and hex_table is "+hex_table[arr[i][0]][arr[i][1]]);
      onechar[0] = hex_table[arr[i][0]][arr[i][1]]; //from_hex(arr[i][0..1]);
      if( onechar == "\r" ) onechar = ""; // Converts \r\n -> \n
      if( onechar == "\255" ) onechar = "?";
      str += onechar + arr[i][2..];
   }

   debug( "unhash_url returns: "+as_string(explode(str, "\255")) );
   return explode( str, "\255" );
}

/** A function used for receiving socket events.
 * Events for this daemon should be formatted as follows:
 *
 * receiver\n
 * return_file_name\n
 * data (in CGI form)
 *
 * Answers will be placed in /secure/extern, and named return_file_name.
 *
 * \param fd The socket number on which the event occurs
 * \param act The event type
 * \param a First parameter (varying types)
 * \param b Second parameter (varying types)
 */
static void callback(int fd, int act, mixed a, mixed b)
{
   string answer, first_bytes;

   log_file("mapd", sprintf("\ncallback(\n   %d,\n   %d,\n   %O,\n   %O\n)\n",fd,act,a,b));
   switch(act) {
      case SOCKET_READ: {
         string url;

         answer = 0;
         // Add the incoming request to the current list of known information.
         if( !incoming_requests[fd] )
            incoming_requests[fd] = a;
         else
            incoming_requests[fd] += a;

         answer = "RESPONSE";
         if( answer ) {
            SOCKETD->socket_write(fd, answer);
            SOCKETD->socket_close(fd);
            incoming_requests[fd] = 0;
         }
         break;
      }
      case SOCKET_ERROR: {
         SOCKETD->socket_close(fd);
         break;
      }
    }
}

void destructor()
{
    write("MAPD daemon shutting down.\n");
    if (sock<0) return;
    SOCKETD->socket_close(sock);
}
