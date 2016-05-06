// #include <desc.h> -- commented out (with the stuff in create()) to fix thread 1754
#include <object.h>

#define READ_FLAG   0x01
#define IM_FLAG     0x02
#define ALL_FLAGS   0xFF

#define MAIL_PROMPT "[Mail] "

/// To be documented.

string folder_name, *subject, *sender, *text, *whom;
int *flags;
int *mtime;
int newmsgs;

static string param1, param2, temp;
static int zfileindex;
static string tstr;
static string tstr2;
static cmdline_switch;

int query_num_msgs() { return sizeof(sender); }

int query_im_flag(int i) { return (flags[i] & IM_FLAG) != 0; }
int query_read_flag(int i) { return (flags[i] & READ_FLAG) != 0; }

void set_im_flag(int i, int f) {
   if( f )
      flags[i] |= IM_FLAG;
   else
      flags[i] &= (ALL_FLAGS - IM_FLAG);
}

void set_read_flag( int i, int f ) {
   if( f )
      flags[i] |= READ_FLAG;
   else
      flags[i] &= ( ALL_FLAGS - READ_FLAG );
}


/** This is the create func, initializes vars, etc....
 */
create() {
   // short = "a Mail reader";
   //name = "mail_rdr";
   //set_name(0);
   //set_distant(0);
   //set_long(0);
   //set_look(0);
   // long = "This is a mail reader.";
   // distant = "a Mail Reader";
   // look = "A mail reader object.";
   folder_name = "unnamed";
   subject = ({ });
   sender = ({ });
   text = ({ });
   mtime = ({ });
   whom = ({ });
   flags = ({ });
   newmsgs = 0;
   tstr = "";
   tstr2 = "";
   cmdline_switch = 0;
   ::create();
}

set_tstr( string str ) { tstr = str; }

set_tstr2( string str ) { tstr2 = str; }

init() {
   set_folder_name(this_player()->query_name());
   load_mail_folder(this_player()->query_name());
   newmsgs = 0;
   save_mail_folder();

   if( tstr == "" ) {
      int unread, x;
      foreach( x : flags )
         if (!(x & READ_FLAG))
            unread++;
      if (unread)
         msg("You have ~CBRT"+unread+"~CDEF unread message"+(unread==1?"":"s")+".");
      add_interface("viewmail", MAIL_PROMPT);
   } else
   if( tstr == "mail" ) {
      param2 = tstr2;
      temp = "";
      msg_bare("To finish your message, put . on a blank line.\n");
      msg_bare("To cancel, put *Q on a blank line.\n");
      write("\n");
      cmdline_switch = 1;
      add_interface( "writemail", "" );
   } else
   if( tstr == "instant" ) {
      param2 = tstr2;
      temp = "";
      msg_bare( "To finish your message, put . on a blank line.\n" );
      msg_bare( "To cancel, put a *Q on a blank line.\n" );
      write( "\n" );
      cmdline_switch = 1;
      add_interface( "writeim", "" );
   }
   ::init();
}

query_newmsgs() { return newmsgs; }

set_newmsgs() {
   newmsgs = 1;
}

writeim( str ) {
   if( str ) {
      if( str == "." || str == "**" ) {
         call_other( "/secure/mail_daemon", "mail_send_im", param2, this_player()->query_cap_name(), temp );
         msg_bare( "Message sent.\n" );
         write( "\n" );
         if( !cmdline_switch ) {
            shift_interface( "viewmail", MAIL_PROMPT );
         } else {
            msg_bare("You have exited the mail interface.\n");
            drop_interface();
            destruct( this_object() );
         }

         return 1;
      }
      if( str == "*Q" ) {
         write( "\n" );
         msg_bare( "Message canceled.\n" );
         write( "\n" );
         if( !cmdline_switch ) {
            shift_interface( "viewmail", MAIL_PROMPT );
         } else {
            msg_bare("You have exited the mail interface.\n");
            drop_interface();
            destruct( this_object() );
         }
         return 1;
      }
      temp = temp + str;
      temp += "\n";
   }
}

writemail( str ) {
   if( str ) {
      if( str == "." || str == "**" ) {
         shift_interface("subjectenter", " Subject: ");
         return 1;
      }
      if(str=="*Q") {
         write("\n");
         msg_bare("Message canceled.\n");
         write("\n");
         shift_interface("viewmail", MAIL_PROMPT);
         return 1;
      }
      temp = temp + str;
      temp += "\n";
   }
}

subjectenter(str) {
  if(str) {
    call_other("/secure/mail_daemon","mail_send", param2, this_player()->query_cap_name(), str, temp);
    msg_bare("Message sent.\n");
    write("\n");
    if( !cmdline_switch ) {
       shift_interface("viewmail", MAIL_PROMPT);
    } else {
       drop_interface();
       destruct( this_object() );
    }
    return 1;
  }
}

viewmail(str) {
   int i;
   int c;
   string s;
   string strtemp;

   if( str ) {
      if( str == "q" || str == "quit" ) {
         msg_bare("You have exited the mail interface.\n");
         drop_interface();
         drop_interface();
         destruct(this_object());
         return 1;
      }

   if( str ) {
      param1 = "";
      param2 = "";
      sscanf( str, "%s %s", param1, param2 );
   }

   if( str == "help" || str == "?" ) {
     msg( "~[060Mail commands are as follows:~CDEF" );
     msg("view/list    List the mail msgs in your folder, and the subjects.");
     msg("read #       Read the mail msg number #.");
     msg("mail who     Send a mail msg to 'who'. Use a '.' to end the msg on a blank line.");
     msg("del/remove # Remove the msg number # from the mail folder.");
     msg("purge im     Delete all instant message posts from your mail folder.");
     msg("quit         What do you think this one does?\n");
   }

   if(param1=="m" || param1=="mail") {
     param2 = lower_case(param2);
     if(param2 != this_player()->query_name()) {
       temp = "";
       msg_bare("To finish your message, put . on a blank line.\n");
       msg_bare("To cancel, put *Q on a blank line.\n");
       write("\n");
       shift_interface("writemail", "");
     } else {
       msg_bare("~[090You cannot mail yourself!~CDEF\n");
       write("\n");
     }
   }

   if( param1 == "im" ) {
      param2 = lower_case( param2 );
      temp = "";
      msg_bare( "To finish your instant message, put a . on a blank line.\n" );
      msg_bare( "To cancel, put *Q on a blank line.\n" );
      write( "\n" );
      shift_interface( "writeim", "" );
   }

   if(param1=="d" || param1=="del" || param1=="delete" || param1=="remove" || param1=="rm") {
     i = to_int(param2);
     if(query_to_int_fail()) {
       write("ERROR ====> MSG # NOT AN INTEGER!\n");
       return 1;
     }
     if(!query_valid_msg(i)) {
       msg_bare("~[020Whoops! I can't find that one.~CDEF\n");
       write("\n");
       return 1;
     }
     call_other("/secure/mail_daemon","mail_remove",i);
     msg_bare( "~[090Message deleted.~CDEF\n" );
   }

   if (param1 == "u" || param1 == "unread") {
     i = to_int(param2);
     if(query_to_int_fail()) {
       write("ERROR ====> MSG # NOT AN INTEGER!\n");
       return 1;
     }
     if(!query_valid_msg(i)) {
       msg_bare("~[020Whoops! I can't find that one.~CDEF\n");
       write("\n");
       return 1;
     }
     set_read_flag( i, 0 );
     save_mail_folder();
     msg_bare( "Message marked as unread.\n" );
   }

   if (str == "r" || str == "read") {
     int x, next = -1;
     for( x=0; x < sizeof(flags); x++ )
       if (!(flags[x] & READ_FLAG)) {
         next = x;
         break;
       }
     if (next >= 0) {
       param1 = str;
       param2 = (string)next;
     } else {
       msg("You have no more unread messages.");
       return 1;
     }
   }

   if(param1=="r" || param1=="read") {
     i = to_int(param2);
     if(query_to_int_fail()) {
       write("ERROR ====> MSG # NOT AN INTEGER!\n");
       return 1;
     }
     if(!query_valid_msg(i)) {
       msg_bare("~[020Whoops! I can't find that one.~CDEF\n");
       write("\n");
       return 1;
     }
     set_read_flag( i, 1 );
     save_mail_folder();
     msg_bare( "~[150From: ~[140" + sender[i]+ "\n" );
     msg_bare( "~[150Date: ~[140" + ctime( mtime[i] ) + "\n" );
     msg_bare("~[150Subject: ~[140"+subject[i]+"\n");
     msg_bare("~[130Message Reads: \n");
     write("\n");
     rm("/temp/listswap."+folder_name);
     write_file("/temp/listswap."+folder_name, text[i]);
     shift_interface("msg_scroll", "(PrEsS aNy KeY, or hit 'q' to quit)");
     zfileindex = 0;
     s = read_file("/temp/listswap."+folder_name, zfileindex, 19);
     write(process_string("~CDEF",0));
     s = process_string(s,0);
     write(s);
     write(process_string("~[150",0));
     return 1;
   }
   if(str=="l" || str=="list" || str=="view") {
    write("Your mail msgs: \n" );
    for(i=0;i<sizeof(subject);i++) {
       //msg_bare("~[120<"+i+"> ~[140From: ~[150"+query_sender(i)+" ~[040Subject: ~[060"+query_subject(i)+"~CDEF\n");
       //pad( str, 20, 0 );
       strtemp = pad( "~[120<"+i+">" , 10, 0 );
       strtemp += stime( mtime[i] );
       strtemp +=  query_read_flag(i)?"   ":" U ";
       //strtemp +=  " ~[140From: ~[150"+query_sender(i)+" ~[040Subject: ~[060"+query_subject(i)+"~CDEF\n";
       strtemp +=  " ~[140From: ~[150"+pad( query_sender(i), 14, 0 )+" ~[040Subject: ~[060"+pad( query_subject(i), 24, 0 )+"~CDEF";
       msg( strtemp );
    }
    write("\n");
   }
   if( str=="purge im" ) {
      call_other( "/secure/mail_daemon", "mail_purge_instants" );
      msg_bare( "~[090All instant messages purged.~CDEF\n" );
   }
  }
}

msg_scroll(str) {
  string s;

  write(process_string("~CDEF", 0));

  if(str=="q" || str=="quit" || !str || str == "") {
    shift_interface("viewmail", MAIL_PROMPT);
    zfileindex = 0;
    rm("/temp/listswap."+folder_name);
    write(process_string("~CDEF", 0));
    return 1;
  }

  if(str=="d" && str!="q") zfileindex += 19;
  if(str=="u" && (zfileindex - 19) >= 0) {
    zfileindex -= 19;
  }

  s = read_file("/temp/listswap."+folder_name, zfileindex, 19);
  if(!s) {
    shift_interface("viewmail", MAIL_PROMPT);
    zfileindex = 0;
    rm("/temp/listswap."+folder_name);
    write(process_string("~CDEF", 0));
    return 1;
  }

  s = process_string(s, 0);
  write(s);

  write(process_string("~[150", 0));
}

string query_folder_name() { return folder_name; }
string query_subject(int i) { return subject[i]; }
string query_sender(int i) { return sender[i]; }
string query_text(int i) { return text[i]; }
int query_mtime(int i) { return mtime[i]; }
string query_whom(int i) { return whom[i]; }


set_folder_name(string str) {
  folder_name = str;
  return 1;
}

int add_mailmsg( string subj, string send, string txt, string wh, int mtim ) {
   subject += ({ subj });
   sender += ({ send });
   text += ({ txt });
   mtime += ({ mtim });
   whom += ({ wh });
   flags += ({ 0 });
   return 1;
}

int add_instantmsg( string send, string txt, string wh, int mtim ) {
   add_mailmsg( "Instant Message", send, txt, wh, mtim );
   //set_im_flag( sizeof(subject) - 1, 1 );
   return 1;
}

int query_valid_msg(int i) {
  if(i<0 || i>=sizeof(subject) || !subject) return 0;
  return 1;
}

int query_num_messages() { return sizeof(subject); }

int delete_mailmsg( int which ) {
  /* This code will search for the mentioned array index, and delete the msg
     at that point, and all the peripherals like subject, sender, etc....
  */
  if( sizeof(subject) < which ) return 0;
  subject = array_delete(subject, which);
  sender = array_delete(sender, which);
  text = array_delete(text, which);
  mtime = array_delete(mtime, which);
  whom = array_delete(whom, which);
  flags = array_delete( flags, which );
  return 1;
}

load_mail_folder(string str) {
  return restore_object("/mail/"+str);
}

save_mail_folder() {
  save_object("/mail/"+folder_name);
}

void on_suspend() {
   destruct( this_object() );
}
